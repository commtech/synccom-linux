/*
Copyright 2022 Commtech, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "config.h"
#include "port.h"
#include "utils.h"
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/poll.h>

/* Define these values to match your devices */
#define SYNCCOM_VENDOR_ID 0x2eb0
#define SYNCCOM_PRODUCT_ID 0x0030

LIST_HEAD(synccom_cards);

/* table of devices that work with this driver */
static const struct usb_device_id synccom_table[] = {
    {USB_DEVICE(SYNCCOM_VENDOR_ID, SYNCCOM_PRODUCT_ID)},
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, synccom_table);

/* Get a minor range for your devices from the usb maintainer */
#define USB_synccom_MINOR_BASE 192

#define WRITES_IN_FLIGHT 8
/* arbitrarily chosen */

/* Structure to hold all device specific stuff */

#define to_synccom_dev(d) container_of(d, struct synccom_port, kref)

static struct usb_driver synccom_driver;
static void synccom_draw_down(struct synccom_port *port);
unsigned synccom_poll(struct file *file, struct poll_table_struct *wait);
long synccom_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static void synccom_delete(struct kref *kref) {
  struct synccom_port *port = to_synccom_dev(kref);

  synccom_port_destroy_urbs(port);
  usb_put_dev(port->udev);
  kfree(port);
}

static int synccom_open(struct inode *inode, struct file *file) {

  struct synccom_port *port;
  struct usb_interface *interface;
  int subminor;
  int retval = 0;

  subminor = iminor(inode);

  interface = usb_find_interface(&synccom_driver, subminor);
  if (!interface) {
    pr_err("%s - error, can't find device for minor %d\n", __func__, subminor);
    retval = -ENODEV;
    goto exit;
  }

  port = usb_get_intfdata(interface);
  if (!port) {
    retval = -ENODEV;
    goto exit;
  }

  retval = usb_autopm_get_interface(interface);
  if (retval)
    goto exit;

  /* increment our usage count for the device */
  kref_get(&port->kref);
  /* save our object in the file's private structure */
  file->private_data = port;

exit:
  return retval;
}

static int synccom_release(struct inode *inode, struct file *file) {
  struct synccom_port *port;

  port = file->private_data;
  if (port == NULL)
    return -ENODEV;

  /* allow the device to be autosuspended */
  mutex_lock(&port->io_mutex);
  if (port->interface)
    usb_autopm_put_interface(port->interface);
  mutex_unlock(&port->io_mutex);

  /* decrement the count on our device */
  kref_put(&port->kref, synccom_delete);
  return 0;
}

static int synccom_flush(struct file *file, fl_owner_t id) {
  struct synccom_port *port;
  int res;
  unsigned long err_flags = 0;

  port = file->private_data;
  if (port == NULL)
    return -ENODEV;

  /* wait for io to stop */
  mutex_lock(&port->io_mutex);
  synccom_draw_down(port);

  /* read out errors, leave subsequent opens a clean slate */
  spin_lock_irqsave(&port->err_lock, err_flags);
  res = port->errors ? (port->errors == -EPIPE ? -EPIPE : -EIO) : 0;
  port->errors = 0;
  spin_unlock_irqrestore(&port->err_lock, err_flags);

  mutex_unlock(&port->io_mutex);

  return res;
}

unsigned synccom_poll(struct file *file, struct poll_table_struct *wait) {
  struct synccom_port *port = 0;
  unsigned mask = 0;
  int discard = 0;

  port = file->private_data;
  discard = down_interruptible(&port->poll_semaphore);

  poll_wait(file, &port->input_queue, wait);
  poll_wait(file, &port->output_queue, wait);

  if(synccom_port_has_incoming_data(port))
    mask |= POLLIN | POLLRDNORM;

  if(synccom_port_get_output_memory_usage(port) < synccom_port_get_output_memory_cap(port))
    mask |= POLLOUT | POLLWRNORM;

  up(&port->poll_semaphore);

  return mask;
}

static ssize_t synccom_read(struct file *file, char *buf, size_t count,
                            loff_t *ppos) {

  struct synccom_port *port = 0;
  ssize_t read_count;

  port = file->private_data;

  if (count == 0)
    return count;

  if (down_interruptible(&port->read_semaphore))
    return -ERESTARTSYS;

  while (!synccom_port_has_incoming_data(port)) {
    up(&port->read_semaphore);

    if (file->f_flags & O_NONBLOCK)
      return -EAGAIN;

    if (wait_event_interruptible(port->input_queue,
                                 synccom_port_has_incoming_data(port))) {
      return -ERESTARTSYS;
    }

    if (down_interruptible(&port->read_semaphore))
      return -ERESTARTSYS;
  }

  read_count = synccom_port_read(port, buf, count);

  up(&port->read_semaphore);

  return read_count;
}

static ssize_t synccom_write(struct file *file, const char *buf, size_t count,
                             loff_t *ppos) {

  struct synccom_port *port = 0;
  int error_code = 0;

  port = file->private_data;

  if (count == 0)
    return count;

  if (count > synccom_port_get_output_memory_cap(port))
    return -ENOBUFS;

  if (down_interruptible(&port->write_semaphore))
    return -ERESTARTSYS;

  while (synccom_port_get_output_memory_usage(port) + count >
         synccom_port_get_output_memory_cap(port)) {
    up(&port->write_semaphore);

    if (file->f_flags & O_NONBLOCK)
      return -EAGAIN;

    if (wait_event_interruptible(
            port->output_queue,
            synccom_port_get_output_memory_usage(port) + count <=
                synccom_port_get_output_memory_cap(port))) {
      dev_dbg(port->device, "output_queue popped.. %d usage, %d cap",
              synccom_port_get_output_memory_usage(port),
              synccom_port_get_output_memory_cap(port));
      return -ERESTARTSYS;
    }

    if (down_interruptible(&port->write_semaphore))
      return -ERESTARTSYS;
  }

  error_code = synccom_port_write(port, buf, count);

  up(&port->write_semaphore);

  return (error_code < 0) ? error_code : count;
}

long synccom_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  struct synccom_port *port = 0;
  long error_code = 0;
  char clock_bits[20];
  char firmware_line[50];
  unsigned int tmp_int = 0;
  struct synccom_registers regs;
  struct synccom_memory_cap tmp_memcap;

  port = file->private_data;

  switch (cmd) {
  case TEST:
    break;

  case SYNCCOM_GET_REGISTERS:
    if (copy_from_user(&regs, (struct synccom_registers *)arg,
                       sizeof(struct synccom_registers))) {
      return -EFAULT;
    }
    synccom_port_get_registers(port, &regs);
    if (copy_to_user((struct synccom_registers *)arg, &regs,
                     sizeof(struct synccom_registers))) {
      return -EFAULT;
    }
    break;

  case SYNCCOM_SET_REGISTERS:
    if (copy_from_user(&regs, (struct synccom_registers *)arg,
                       sizeof(struct synccom_registers))) {
      return -EFAULT;
    }
    synccom_port_set_registers(port, &regs);
    break;

  case SYNCCOM_PURGE_TX:
    error_code = synccom_port_purge_tx(port);
    break;

  case SYNCCOM_PURGE_RX:
    error_code = synccom_port_purge_rx(port);
    break;

  case SYNCCOM_ENABLE_APPEND_STATUS:
    error_code = synccom_port_set_append_status(port, 1);
    break;

  case SYNCCOM_DISABLE_APPEND_STATUS:
    synccom_port_set_append_status(port, 0);
    break;

  case SYNCCOM_GET_APPEND_STATUS:
    tmp_int = synccom_port_get_append_status(port);
    if (copy_to_user((void *)arg, &tmp_int, sizeof(tmp_int))) {
      return -EFAULT;
    }
    break;

  case SYNCCOM_ENABLE_APPEND_TIMESTAMP:
    error_code = synccom_port_set_append_timestamp(port, 1);
    break;

  case SYNCCOM_DISABLE_APPEND_TIMESTAMP:
    synccom_port_set_append_timestamp(port, 0);
    break;

  case SYNCCOM_GET_APPEND_TIMESTAMP:
    tmp_int = synccom_port_get_append_timestamp(port);
    if (copy_to_user((void *)arg, &tmp_int, sizeof(tmp_int))) {
      return -EFAULT;
    }
    break;

  case SYNCCOM_SET_MEMORY_CAP:
    if (copy_from_user(&tmp_memcap, (void *)arg, sizeof(tmp_memcap))) {
      return -EFAULT;
    }
    synccom_port_set_memory_cap(port, &tmp_memcap);
    break;

  case SYNCCOM_GET_MEMORY_CAP:
    tmp_memcap.input = synccom_port_get_input_memory_cap(port);
    tmp_memcap.output = synccom_port_get_output_memory_cap(port);
    if (copy_to_user(&(((struct synccom_memory_cap *)arg)->input),
                     &tmp_memcap.input, sizeof(tmp_memcap.input))) {
      return -EFAULT;
    }
    if (copy_to_user(&(((struct synccom_memory_cap *)arg)->output),
                     &tmp_memcap.output, sizeof(tmp_memcap.output))) {
      return -EFAULT;
    }
    break;

  case SYNCCOM_SET_CLOCK_BITS:
    if (copy_from_user(clock_bits, (char *)arg, 20)) {
      return -EFAULT;
    }
    synccom_port_set_clock_bits(port, clock_bits);
    break;

  case SYNCCOM_ENABLE_IGNORE_TIMEOUT:
    synccom_port_set_ignore_timeout(port, 1);
    break;

  case SYNCCOM_DISABLE_IGNORE_TIMEOUT:
    synccom_port_set_ignore_timeout(port, 0);
    break;

  case SYNCCOM_GET_IGNORE_TIMEOUT:
    tmp_int = synccom_port_get_ignore_timeout(port);
    if (copy_to_user((void *)arg, &tmp_int, sizeof(tmp_int))) {
      return -EFAULT;
    }
    break;

  case SYNCCOM_SET_TX_MODIFIERS:
    tmp_int = (unsigned int)arg;
    error_code = synccom_port_set_tx_modifiers(port, (unsigned int)tmp_int);
    break;

  case SYNCCOM_GET_TX_MODIFIERS:
    *(unsigned *)arg = synccom_port_get_tx_modifiers(port);
    break;

  case SYNCCOM_ENABLE_RX_MULTIPLE:
    synccom_port_set_rx_multiple(port, 1);
    break;

  case SYNCCOM_DISABLE_RX_MULTIPLE:
    synccom_port_set_rx_multiple(port, 0);
    break;

  case SYNCCOM_GET_RX_MULTIPLE:
    tmp_int = synccom_port_get_rx_multiple(port);
    if (copy_to_user((void *)arg, &tmp_int, sizeof(tmp_int))) {
      return -EFAULT;
    }
    break;
  case SYNCCOM_SET_NONVOLATILE:
    if(!synccom_port_can_support_nonvolatile(port)) {
      error_code = -EINVAL;
      break;
    }
    tmp_int = (unsigned int)arg;
    error_code = synccom_port_set_nonvolatile(port, tmp_int, 1);
    break;
  case SYNCCOM_GET_NONVOLATILE:
    if(!synccom_port_can_support_nonvolatile(port)) {
      error_code = -EINVAL;
      break;
    }
    tmp_int = synccom_port_get_nonvolatile(port, 1);
    if (copy_to_user((void *)arg, &tmp_int, sizeof(tmp_int))) {
      return -EFAULT;
    }
    break;
  case SYNCCOM_REPROGRAM:
    if (copy_from_user(firmware_line, (char *)arg, 50)) {
      return -EFAULT;
    }
    program_synccom(port, firmware_line);
    break;
  default:
    dev_dbg(port->device, "%s - unknown ioctl 0x%x\n", __func__, cmd);
    return -ENOTTY;
  }

  return error_code;
}

static const struct file_operations synccom_fops = {
    .owner = THIS_MODULE,
    .read = synccom_read,
    .write = synccom_write,
    .open = synccom_open,
    .release = synccom_release,
    .flush = synccom_flush,
    //.llseek =	noop_llseek,
    .unlocked_ioctl = synccom_ioctl,
    .poll = synccom_poll,

};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver synccom_class = {
    .name = "synccom%d",
    .fops = &synccom_fops,
    .minor_base = USB_synccom_MINOR_BASE,
};

static int synccom_probe(struct usb_interface *interface,
                         const struct usb_device_id *id) {
  struct synccom_port *port;
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint;
  size_t buffer_size;
  int i;
  int retval = -ENOMEM;

  /* allocate memory for our device state and initialize it */
  port = kzalloc(sizeof(*port), GFP_KERNEL);
  if (!port) {
    dev_err(&interface->dev, "%s - Out of memory\n", __func__);
    goto error;
  }
  kref_init(&port->kref);
  sema_init(&port->limit_sem, WRITES_IN_FLIGHT);
  mutex_init(&port->io_mutex);
  spin_lock_init(&port->err_lock);
  init_usb_anchor(&port->submitted);
  init_waitqueue_head(&port->bulk_in_wait);

  port->udev = usb_get_dev(interface_to_usbdev(interface));
  port->interface = interface;
  port->device = &port->udev->dev;

  iface_desc = interface->cur_altsetting;
  for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
    endpoint = &iface_desc->endpoint[i].desc;

    if (!port->bulk_in_endpointAddr && usb_endpoint_is_bulk_in(endpoint)) {
      buffer_size = endpoint->wMaxPacketSize;
      port->bulk_in_size = buffer_size;
      port->bulk_in_endpointAddr = endpoint->bEndpointAddress;
      dev_dbg(port->device, "%s - Endpoint found: 0x%2.2x", __func__,
              port->bulk_in_endpointAddr);
    }
    if (!port->bulk_out_endpointAddr && usb_endpoint_is_bulk_out(endpoint)) {
      port->bulk_out_endpointAddr = endpoint->bEndpointAddress;
      dev_dbg(port->device, "%s - Endpoint found: 0x%2.2x", __func__,
              port->bulk_out_endpointAddr);
    }
    if (port->bulk_out_endpointAddr && usb_endpoint_is_bulk_out(endpoint)) {
      port->bulk_out_endpointAddr2 = endpoint->bEndpointAddress;
      dev_dbg(port->device, "%s - Endpoint found: 0x%2.2x", __func__,
              port->bulk_out_endpointAddr2);
    }
  }
  if (!(port->bulk_in_endpointAddr && port->bulk_out_endpointAddr)) {
    dev_err(port->device,
            "%s - Could not find both bulk-in and bulk-out endpoints\n",
            __func__);
    goto error;
  }

  /* save our data pointer in this interface device */
  usb_set_intfdata(interface, port);

  /* we can register the device now, as it is ready */
  retval = usb_register_dev(interface, &synccom_class);
  if (retval) {
    /* something prevented us from registering this driver */
    dev_err(&interface->dev, "Not able to get a minor for this device.\n");
    usb_set_intfdata(interface, NULL);
    goto error;
  }

  /* let the user know what node this device is now attached to */
  dev_info(port->device, "%s - USB synccom device now attached to synccom%d\n",
           __func__, interface->minor);

  initialize(port);

  return 0;

error:
  if (port)
    /* this frees allocated memory */
    kref_put(&port->kref, synccom_delete);
  return retval;
}

static void synccom_disconnect(struct usb_interface *interface) {
  struct synccom_port *port;
  int minor = interface->minor;

  port = usb_get_intfdata(interface);

  usb_set_intfdata(interface, NULL);

  /* give back our minor */
  usb_deregister_dev(interface, &synccom_class);

  /* prevent more I/O from starting */
  mutex_lock(&port->io_mutex);
  port->interface = NULL;
  mutex_unlock(&port->io_mutex);

  usb_kill_anchored_urbs(&port->submitted);

  /* decrement our usage count */
  kref_put(&port->kref, synccom_delete);

  del_timer(&port->timer);
  dev_info(port->device, "%s - USB synccom #%d now disconnected", __func__,
           minor);
}

static void synccom_draw_down(struct synccom_port *port) {
  int time;

  time = usb_wait_anchor_empty_timeout(&port->submitted, 1000);
  if (!time)
    usb_kill_anchored_urbs(&port->submitted);
}

static int synccom_suspend(struct usb_interface *intf, pm_message_t message) {
  struct synccom_port *port = usb_get_intfdata(intf);

  if (!port)
    return 0;
  synccom_draw_down(port);
  return 0;
}

static int synccom_resume(struct usb_interface *intf) { return 0; }

static int synccom_pre_reset(struct usb_interface *intf) {
  struct synccom_port *port = usb_get_intfdata(intf);

  mutex_lock(&port->io_mutex);
  synccom_draw_down(port);

  return 0;
}

static int synccom_post_reset(struct usb_interface *intf) {
  struct synccom_port *port = usb_get_intfdata(intf);

  /* we are sure no URBs are active - no locking needed */
  port->errors = -EPIPE;
  mutex_unlock(&port->io_mutex);

  return 0;
}

static struct usb_driver synccom_driver = {
    .name = "synccom",
    .probe = synccom_probe,
    .disconnect = synccom_disconnect,
    .suspend = synccom_suspend,
    .resume = synccom_resume,
    .pre_reset = synccom_pre_reset,
    .post_reset = synccom_post_reset,
    .id_table = synccom_table,
    .supports_autosuspend = 1,
};

module_usb_driver(synccom_driver);

MODULE_LICENSE("GPL");
MODULE_VERSION("1.1.2");
MODULE_AUTHOR("Landon Unruh <landonu@commtech-fastcom.com>");

MODULE_DESCRIPTION(
    "Driver for the Sync Com series of cards from Commtech, Inc.");
