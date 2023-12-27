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

#include <linux/uaccess.h> /* copy_*_user in <= 2.6.24 */
#include <linux/version.h> /* LINUX_VERSION_CODE, KERNEL_VERSION */
#include <linux/workqueue.h>

#include "config.h" /* DEVICE_NAME, DEFAULT_* */
#include "frame.h"  /* struct synccom_frame */
#include "port.h"
#include "sysfs.h" /* port_*_attribute_group */
#include "utils.h" /* return_{val_}_if_untrue, chars_to_u32, ... */

void synccom_port_execute_GO_R(struct synccom_port *port);
void synccom_port_execute_STOP_R(struct synccom_port *port);
void synccom_port_execute_STOP_T(struct synccom_port *port);
void synccom_port_execute_RST_R(struct synccom_port *port);
static void read_data_callback(struct urb *urb);
void frame_count_worker(struct work_struct *port);

int initialize(struct synccom_port *port) {
  int i;
  char clock_bits[20] = DEFAULT_CLOCK_BITS;

  port->device = &port->udev->dev;
  mutex_init(&port->register_access_mutex);
  mutex_init(&port->running_bc_mutex);

  sema_init(&port->write_semaphore, 1);
  sema_init(&port->read_semaphore, 1);
  sema_init(&port->poll_semaphore, 1);

  init_waitqueue_head(&port->input_queue);
  init_waitqueue_head(&port->output_queue);

  port->memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
  port->memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;

  spin_lock_init(&port->board_rx_spinlock);
  spin_lock_init(&port->board_tx_spinlock);

  spin_lock_init(&port->istream_spinlock);
  spin_lock_init(&port->pending_iframe_spinlock);
  spin_lock_init(&port->pending_oframe_spinlock);

  spin_lock_init(&port->queued_oframes_spinlock);
  spin_lock_init(&port->queued_iframes_spinlock);
  spin_lock_init(&port->pending_iframes_spinlock);

  synccom_port_set_append_status(port, DEFAULT_APPEND_STATUS_VALUE);
  synccom_port_set_ignore_timeout(port, DEFAULT_IGNORE_TIMEOUT_VALUE);
  synccom_port_set_tx_modifiers(port, DEFAULT_TX_MODIFIERS_VALUE);
  synccom_port_set_rx_multiple(port, DEFAULT_RX_MULTIPLE_VALUE);

  SYNCCOM_REGISTERS_INIT(port->register_storage);
  port->register_storage.FIFOT = DEFAULT_FIFOT_VALUE;
  port->register_storage.CCR0 = DEFAULT_CCR0_VALUE;
  port->register_storage.CCR1 = DEFAULT_CCR1_VALUE;
  port->register_storage.CCR2 = DEFAULT_CCR2_VALUE;
  port->register_storage.BGR = DEFAULT_BGR_VALUE;
  port->register_storage.SSR = DEFAULT_SSR_VALUE;
  port->register_storage.SMR = DEFAULT_SMR_VALUE;
  port->register_storage.TSR = DEFAULT_TSR_VALUE;
  port->register_storage.TMR = DEFAULT_TMR_VALUE;
  port->register_storage.RAR = DEFAULT_RAR_VALUE;
  port->register_storage.RAMR = DEFAULT_RAMR_VALUE;
  port->register_storage.PPR = DEFAULT_PPR_VALUE;
  port->register_storage.TCR = DEFAULT_TCR_VALUE;
  port->register_storage.IMR = DEFAULT_IMR_VALUE;
  port->register_storage.DPLLR = DEFAULT_DPLLR_VALUE;
  port->register_storage.FCR = DEFAULT_FCR_VALUE;
  synccom_port_set_registers(port, &port->register_storage);
  synccom_port_set_clock_bits(port, clock_bits);

  synccom_port_create_urbs(port);

  INIT_LIST_HEAD(&port->list);
  synccom_flist_init(&port->queued_oframes);
  synccom_flist_init(&port->queued_iframes);
  synccom_flist_init(&port->pending_iframes);
  port->istream = synccom_frame_new(port);
  port->pending_iframe = 0;
  port->pending_oframe = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
  init_timer(&port->timer);
  port->timer.data = (unsigned long)port;
  port->timer.function = &timer_handler;
#else
  timer_setup(&port->timer, &timer_handler, 0);
#endif

  INIT_WORK(&port->bclist_worker, frame_count_worker);

  tasklet_init(&port->send_oframe_tasklet, oframe_worker, (unsigned long)port);

  synccom_port_execute_RRES(port, 1);
  synccom_port_execute_TRES(port, 1);

  mod_timer(&port->timer, jiffies + msecs_to_jiffies(20));

  for (i = 0; i < NUMBER_OF_URBS; i++) {
    usb_submit_urb(port->bulk_in_urbs[i], GFP_ATOMIC);
  }

  return 0;
}

int synccom_port_create_urbs(struct synccom_port *port) {
  int i, buffer_size;
  buffer_size = URB_BUFFER_SIZE * sizeof(unsigned char);

  // read urbs
  port->bulk_in_urbs =
      kmalloc(NUMBER_OF_URBS * sizeof(struct urb *), GFP_KERNEL);
  port->bulk_in_buffers =
      kmalloc(NUMBER_OF_URBS * sizeof(unsigned char *), GFP_KERNEL);

  for (i = 0; i < NUMBER_OF_URBS; i++) {
    port->bulk_in_urbs[i] = usb_alloc_urb(0, GFP_KERNEL);
    port->bulk_in_buffers[i] = kmalloc(buffer_size, GFP_KERNEL);
    usb_fill_bulk_urb(
        port->bulk_in_urbs[i], port->udev, usb_rcvbulkpipe(port->udev, 0x82),
        port->bulk_in_buffers[i], buffer_size, read_data_callback, port);
  }

  return 0;
}

int synccom_port_destroy_urbs(struct synccom_port *port) {
  int i;

  // read urbs
  for (i = 0; i < NUMBER_OF_URBS; i++) {
    usb_free_urb(port->bulk_in_urbs[i]);
    kfree(port->bulk_in_buffers[i]);
  }

  kfree(port->bulk_in_urbs);
  kfree(port->bulk_in_buffers);

  return 0;
}

void frame_count_worker(struct work_struct *port) {
  struct synccom_port *sport =
      container_of(port, struct synccom_port, bclist_worker);
  update_bc_buffer(sport);
}

void synccom_port_reset_timer(struct synccom_port *port) {
  if (mod_timer(&port->timer, jiffies + msecs_to_jiffies(1)))
    dev_err(port->device, "mod_timer\n");
}

unsigned synccom_port_timed_out(struct synccom_port *port, int need_lock) {
  __u32 star_value = 0;
  unsigned i = 0;

  return_val_if_untrue(port, 0);

  for (i = 0; i < DEFAULT_TIMEOUT_VALUE; i++) {
    star_value = synccom_port_get_register(port, 0, STAR_OFFSET, need_lock);

    if ((star_value & CE_BIT) == 0)
      return 0;
  }

  return 1;
}

int synccom_port_write(struct synccom_port *port, const char *data,
                       unsigned length) {
  unsigned long queued_flags = 0;
  struct synccom_frame *frame = 0;

  return_val_if_untrue(port, 0);

  frame = synccom_frame_new(port);
  if (!frame)
    return -ENOMEM;

  synccom_frame_add_data_from_user(frame, data, length);
  frame->frame_size = length;

  spin_lock_irqsave(&port->queued_oframes_spinlock, queued_flags);
  synccom_flist_add_frame(&port->queued_oframes, frame);
  spin_unlock_irqrestore(&port->queued_oframes_spinlock, queued_flags);

  tasklet_schedule(&port->send_oframe_tasklet);

  return 0;
}

ssize_t synccom_port_stream_read(struct synccom_port *port, char *buf,
                                 size_t length) {
  unsigned out_length = 0;
  unsigned long istream_flags = 0;

  spin_lock_irqsave(&port->istream_spinlock, istream_flags);
  out_length = min(length, (size_t)synccom_frame_get_length(port->istream));
  synccom_frame_remove_data(port->istream, buf, out_length);
  spin_unlock_irqrestore(&port->istream_spinlock, istream_flags);

  return out_length;
}

ssize_t synccom_port_frame_read(struct synccom_port *port, char *buf,
                                size_t buf_length) {
  struct synccom_frame *frame = 0;
  unsigned remaining_buf_length = 0;
  int max_frame_length = 0;
  unsigned current_frame_length = 0;
  unsigned stream_length = 0;
  unsigned out_length = 0;
  unsigned long queued_flags = 0;

  do {
    remaining_buf_length = buf_length - out_length;

    if (port->append_status && port->append_timestamp)
      max_frame_length = remaining_buf_length - sizeof(synccom_timestamp);
    else if (port->append_status)
      max_frame_length = remaining_buf_length;
    else if (port->append_timestamp)
      max_frame_length = remaining_buf_length + 2 - sizeof(synccom_timestamp);
    else
      max_frame_length = remaining_buf_length + 2; // Status length

    if (max_frame_length < 0)
      break;

    spin_lock_irqsave(&port->queued_iframes_spinlock, queued_flags);
    frame = synccom_flist_peek_front(&port->queued_iframes);
    if(!frame) {
        spin_unlock_irqrestore(&port->queued_iframes_spinlock, queued_flags);
        break;
    }
    current_frame_length = synccom_frame_get_frame_size(frame);
    stream_length = synccom_frame_get_length(port->istream);
    if((current_frame_length > max_frame_length) || (stream_length < current_frame_length)) {
        spin_unlock_irqrestore(&port->queued_iframes_spinlock, queued_flags);
        break;
    }
    frame = synccom_flist_remove_frame(&port->queued_iframes);
    spin_unlock_irqrestore(&port->queued_iframes_spinlock, queued_flags);

    current_frame_length -= (!port->append_status) ? 2 : 0;
    spin_lock_irqsave(&port->istream_spinlock, queued_flags);
    synccom_frame_remove_data(port->istream, buf + out_length, current_frame_length);
    out_length += current_frame_length;
    if(!port->append_status) {
        synccom_frame_remove_data(port->istream, NULL, 2);
    }
    spin_unlock_irqrestore(&port->istream_spinlock, queued_flags);

    if (port->append_timestamp) {
      memcpy(buf + out_length, &frame->timestamp, sizeof(frame->timestamp));
      current_frame_length += sizeof(frame->timestamp);
      out_length += sizeof(frame->timestamp);
    }

    synccom_frame_delete(frame);
  } while (port->rx_multiple);

  if (out_length == 0)
    return -ENOBUFS;

  return out_length;
}

ssize_t synccom_port_read(struct synccom_port *port, char *buf, size_t count) {
  return_val_if_untrue(port, 0);

  if (synccom_port_is_streaming(port))
    return synccom_port_stream_read(port, buf, count);
  else
    return synccom_port_frame_read(port, buf, count);
}

unsigned synccom_port_has_incoming_data(struct synccom_port *port) {
  unsigned status = 0;
  unsigned long irq_flags = 0, irq_flags2 = 0;

  return_val_if_untrue(port, 0);

  if (synccom_port_is_streaming(port)) {
    spin_lock_irqsave(&port->istream_spinlock, irq_flags);
    status = (synccom_frame_is_empty(port->istream)) ? 0 : 1;
    spin_unlock_irqrestore(&port->istream_spinlock, irq_flags);
  } else {
    struct synccom_frame *frame = 0;
    spin_lock_irqsave(&port->istream_spinlock, irq_flags);
    spin_lock_irqsave(&port->queued_iframes_spinlock, irq_flags2);
    frame = synccom_flist_peek_front(&port->queued_iframes);
    if (!frame || (frame->frame_size > synccom_frame_get_length(port->istream)))
        status = 0;
    else
        status = 1;
    spin_unlock_irqrestore(&port->queued_iframes_spinlock, irq_flags2);
    spin_unlock_irqrestore(&port->istream_spinlock, irq_flags);
  }

  return status;
}

static void write_data_callback(struct urb *urb) {
  struct synccom_port *port;
  int transfer_size = 0;

  port = urb->context;
  if (urb->status) {
    if (!(urb->status == -ENOENT || urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN))
      dev_err(&port->interface->dev,
              "%s - nonzero write bulk status received: %d\n", __func__,
              urb->status);

    spin_lock(&port->err_lock);
    port->errors = urb->status;
    spin_unlock(&port->err_lock);
    return;
  }
  transfer_size = urb->actual_length;
  dev_dbg(port->device, "Actually wrote %d bytes.", transfer_size);
  kfree(urb->transfer_buffer);
  usb_free_urb(urb);
}

static void write_register_callback(struct urb *urb) {
  struct synccom_port *port;
  int transfer_size = 0;

  port = urb->context;
  if (urb->status) {
    if (!(urb->status == -ENOENT || urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN))
      dev_err(&port->interface->dev,
              "%s - nonzero write register bulk status received: %d\n",
              __func__, urb->status);

    spin_lock(&port->err_lock);
    port->errors = urb->status;
    spin_unlock(&port->err_lock);
    return;
  }
  transfer_size = urb->actual_length;
  usb_free_urb(urb);
}

static void read_data_callback(struct urb *urb) {
  struct synccom_port *port;
  int transfer_size = 0;
  unsigned payload = 0;
  int i = 0;
  unsigned char temp = 0;
  unsigned char *data_buffer = 0;
  unsigned long istream_flags = 0;
  static unsigned char errorcheck1=0, errorcheck2=0;

  port = urb->context;
  data_buffer = urb->transfer_buffer;

  if (urb->status) {
    // killed, unlinked, config changed or bad, someone else should resubmit
    if (!(urb->status == -ENOENT || urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN))
      dev_err(&port->interface->dev,
              "%s - nonzero read bulk status received: %d\n", __func__,
              urb->status);

    spin_lock(&port->err_lock);
    port->errors = urb->status;
    spin_unlock(&port->err_lock);

    if (urb->status == -ESHUTDOWN)
      return;
    // actually maybe I shouldn't resubmit
    // usb_submit_urb(urb, GFP_ATOMIC);
    return;
  }
  transfer_size = urb->actual_length;
  if (transfer_size == 0) {
    // try again
    usb_submit_urb(urb, GFP_ATOMIC);
    return;
  }
  payload = data_buffer[0] << 8;
  payload |= data_buffer[1];
  if(errorcheck1 == data_buffer[0]
    && errorcheck2 == data_buffer[1]
    && errorcheck1 == errorcheck2) {
      // There's a bug where for some reason sometimes the first
      // two bytes of the buffer are a repeat of the last two of a previous
      // read, instead of the payload size.
      // For now, we're saving the last two bytes of every chunk of received
      // data, and comparing it to the first two bytes. It's not perfect.
      // This needs to be resolved in the firmware.
      dev_info(port->device,
        "Payload wrong, using buffer_size! Payload: %d, Size: %d, first two bytes 0x%2.2x 0x%2.2x",
        payload, transfer_size, data_buffer[0], data_buffer[1]);
      payload = transfer_size - 2;
  }

  errorcheck1 = data_buffer[transfer_size-1];
  errorcheck2 = data_buffer[transfer_size-2];

  if (synccom_port_get_input_memory_usage(port) + payload >
      synccom_port_get_input_memory_cap(port)) {
    dev_warn(port->device, "Input memory overflow - discarding data. Cap: %d, Size: %d", synccom_port_get_input_memory_cap(port), synccom_port_get_input_memory_usage(port) + payload);
    usb_submit_urb(urb, GFP_ATOMIC);
    return;
  }

  for (i = 0; i < (payload + 2); i += 2) {
    temp = data_buffer[i];
    data_buffer[i] = data_buffer[i + 1];
    data_buffer[i + 1] = temp;
  }
  spin_lock_irqsave(&port->istream_spinlock, istream_flags);
  synccom_frame_add_data(port->istream, data_buffer + 2, payload);
  spin_unlock_irqrestore(&port->istream_spinlock, istream_flags);

  if (synccom_port_is_streaming(port))
    wake_up_interruptible(&port->input_queue);
  else
    schedule_work(&port->bclist_worker);

  usb_submit_urb(urb, GFP_ATOMIC);
}

__u32 synccom_port_get_register(struct synccom_port *port, unsigned bar,
                                unsigned register_offset, int need_lock) {
  unsigned offset;
  __u32 *value = NULL;
  __u32 fvalue = 0;
  int command = 0x6b;
  int count;
  char *msg = NULL;

  struct synccom_port *dev;
  dev = port;

  return_val_if_untrue(port, 0);
  return_val_if_untrue(bar <= 2, 0);

  offset = port_offset(port, bar, register_offset);

  msg = kmalloc(3, GFP_KERNEL);
  value = kmalloc(sizeof(__u32) * 1, GFP_KERNEL);

  msg[0] = command;
  msg[1] = (offset >> 8) & 0xFF;
  msg[2] = offset & 0xFF;

  if (need_lock) {
    mutex_lock(&port->register_access_mutex);
  }
  usb_bulk_msg(port->udev, usb_sndbulkpipe(port->udev, 1), msg, 3, &count,
               HZ * 10);
  usb_bulk_msg(port->udev, usb_rcvbulkpipe(port->udev, 1), value,
               sizeof(*value), &count, HZ * 10);
  if (need_lock) {
    mutex_unlock(&port->register_access_mutex);
  }
  fvalue = ((*value >> 24) & 0xff) | ((*value << 8) & 0xff0000) |
           ((*value >> 8) & 0xff00) | ((*value << 24) & 0xff000000);

  kfree(msg);
  kfree(value);

  return fvalue;
}

int synccom_port_set_register(struct synccom_port *port, unsigned bar,
                              unsigned register_offset, __u32 value,
                              int need_lock) {
  struct urb *register_urb;
  unsigned offset = 0;
  int command = 0x6a;
  char *msg;
  int retval;

  // TODO just temporary until I get rid of need_lock
  retval = need_lock;
  return_val_if_untrue(port, 0);
  return_val_if_untrue(bar <= 2, 0);

  register_urb = usb_alloc_urb(0, GFP_ATOMIC);
  if (!register_urb)
    return -1;
  offset = port_offset(port, bar, register_offset);

  msg = kmalloc(7, GFP_KERNEL);
  msg[0] = command;
  msg[1] = (offset >> 8) & 0xFF;
  msg[2] = offset & 0xFF;
  msg[3] = (value >> 24) & 0xFF;
  msg[4] = (value >> 16) & 0xFF;
  msg[5] = (value >> 8) & 0xFF;
  msg[6] = value & 0xFF;
  usb_fill_bulk_urb(register_urb, port->udev, usb_sndbulkpipe(port->udev, 1),
                    msg, 7, write_register_callback, port);
  retval = usb_submit_urb(register_urb, GFP_ATOMIC);
  kfree(msg);

  if (bar == 0) {
    synccom_register old_value = ((synccom_register *)&port->register_storage)[register_offset / 4];
    ((synccom_register *)&port->register_storage)[register_offset / 4] = value;

    if (old_value != value) {
      dev_dbg(port->device, "%i:%02x 0x%08x => 0x%08x\n", bar, register_offset, (unsigned int)old_value, value);
    }
    else {
      dev_dbg(port->device, "%i:%02x 0x%08x\n", bar, register_offset, value);
    }
  }
  else if (register_offset == FCR_OFFSET) {
    synccom_register old_value = port->register_storage.FCR;
    port->register_storage.FCR = value;

    if (old_value != value) {
      dev_dbg(port->device, "2:00 0x%08x => 0x%08x\n", (unsigned int)old_value, value);
    }
    else {
      dev_dbg(port->device, "2:00 0x%08x\n", value);
    }
  }

  return 1;
  // return retval;
}

int synccom_port_write_data(struct synccom_port *port, char *data,
                            unsigned byte_count) {
  struct urb *write_urb;
  unsigned char *urb_buffer;

  return_val_if_untrue(port, -1);
  return_val_if_untrue(data, -1);
  return_val_if_untrue(byte_count > 0, -1);

  write_urb = usb_alloc_urb(0, GFP_ATOMIC);
  if (!write_urb) {
    dev_dbg(port->device, "%s: Couldn't alloc urb!", __func__);
    return -1;
  }
  urb_buffer = kmalloc(byte_count, GFP_ATOMIC);
  if (!urb_buffer) {
    dev_dbg(port->device, "%s: Couldn't alloc buffer!", __func__);
    usb_free_urb(write_urb);
    return -1;
  }

  memcpy(urb_buffer, data, byte_count);
  dev_dbg(port->device, "Attempting to write %d bytes.", byte_count);
  usb_fill_bulk_urb(write_urb, port->udev, usb_sndbulkpipe(port->udev, 6),
                    urb_buffer, byte_count, write_data_callback, port);
  return usb_submit_urb(write_urb, GFP_ATOMIC);
}

void synccom_port_set_clock(struct synccom_port *port, unsigned bar,
                            unsigned register_offset, char *data,
                            unsigned byte_count) {

  unsigned offset = 0;
  int count;
  char *msg = NULL;

  return_if_untrue(port);
  return_if_untrue(bar <= 2);
  return_if_untrue(data);
  return_if_untrue(byte_count > 0);

  msg = kmalloc(7, GFP_KERNEL);

  offset = port_offset(port, bar, register_offset);

  msg[0] = 0x6a;
  msg[1] = 0x00;
  msg[2] = 0x40;
  msg[3] = data[0];
  msg[4] = data[1];
  msg[5] = data[2];
  msg[6] = data[3];

  usb_bulk_msg(port->udev, usb_sndbulkpipe(port->udev, 1), msg, 7, &count,
               HZ * 10);

  kfree(msg);
}

int synccom_port_set_registers(struct synccom_port *port,
                               const struct synccom_registers *regs) {
  unsigned stalled = 0;
  unsigned i = 0;

  return_val_if_untrue(port, 0);
  return_val_if_untrue(regs, 0);

  for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
    unsigned register_offset = i * 4;

    if (is_read_only_register(register_offset) ||
        ((synccom_register *)regs)[i] < 0) {
      continue;
    }

    if (register_offset <= MAX_OFFSET) {
      if (synccom_port_set_register(port, 0, register_offset,
                                    ((synccom_register *)regs)[i],
                                    1) == -ETIMEDOUT)
        stalled = 1;
    } else {
      synccom_port_set_register(port, 2, FCR_OFFSET,
                                ((synccom_register *)regs)[i], 1);
    }
  }

  return (stalled) ? -ETIMEDOUT : 1;
}

void synccom_port_get_registers(struct synccom_port *port,
                                struct synccom_registers *regs) {
  unsigned i = 0;

  return_if_untrue(port);
  return_if_untrue(regs);

  for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
    if (((synccom_register *)regs)[i] != SYNCCOM_UPDATE_VALUE)
      continue;

    if (i * 4 <= MAX_OFFSET) {
      ((synccom_register *)regs)[i] =
          synccom_port_get_register(port, 0, i * 4, 1);
    } else {
      ((synccom_register *)regs)[i] =
          synccom_port_get_register(port, 2, FCR_OFFSET, 1);
    }
  }
}

__u32 synccom_port_get_TXCNT(struct synccom_port *port) {
  __u32 fifo_bc_value = 0;

  return_val_if_untrue(port, 0);

  fifo_bc_value = synccom_port_get_register(port, 0, FIFO_BC_OFFSET, 1);

  return (fifo_bc_value & 0x1FFF0000) >> 16;
}

unsigned synccom_port_get_RXCNT(struct synccom_port *port) {

  __u32 fifo_bc_value = 0;

  return_val_if_untrue(port, 0);

  fifo_bc_value = synccom_port_get_register(port, 0, FIFO_BC_OFFSET, 1);

  /* Not sure why, but this can be larger than 8192. We add
 the 8192 check here so other code can count on the value
 not being larger than 8192. */
  return min(fifo_bc_value & 0x00003FFF, (__u32)8192);
}

__u8 synccom_port_get_FREV(struct synccom_port *port) {
  __u32 vstr_value = 0;

  return_val_if_untrue(port, 0);

  vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET, 1);

  return (__u8)((vstr_value & 0x000000FF));
}

__u8 synccom_port_get_PREV(struct synccom_port *port) {
  __u32 vstr_value = 0;

  return_val_if_untrue(port, 0);

  vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET, 1);

  return (__u8)((vstr_value & 0x0000FF00) >> 8);
}

__u16 synccom_port_get_PDEV(struct synccom_port *port) {
  __u32 vstr_value = 0;

  return_val_if_untrue(port, 0);

  vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET, 1);

  return (__u16)((vstr_value & 0xFFFF0000) >> 16);
}

unsigned synccom_port_get_CE(struct synccom_port *port) {
  __u32 star_value = 0;

  return_val_if_untrue(port, 0);

  star_value = synccom_port_get_register(port, 0, STAR_OFFSET, 1);

  return (unsigned)((star_value & 0x00040000) >> 18);
}

int synccom_port_purge_rx(struct synccom_port *port) {
  int error_code = 0;
  unsigned long flags = 0;
  return_val_if_untrue(port, 0);

  dev_dbg(port->device, "purge_rx\n");

  mutex_lock(&port->running_bc_mutex);

  mutex_lock(&port->register_access_mutex);
  spin_lock_irqsave(&port->board_rx_spinlock, flags);
  error_code = synccom_port_execute_RRES(port, 0);
  spin_unlock_irqrestore(&port->board_rx_spinlock, flags);
  mutex_unlock(&port->register_access_mutex);

  spin_lock_irqsave(&port->queued_iframes_spinlock, flags);
  synccom_flist_clear(&port->queued_iframes);
  spin_unlock_irqrestore(&port->queued_iframes_spinlock, flags);

  spin_lock_irqsave(&port->pending_iframes_spinlock, flags);
  synccom_flist_clear(&port->pending_iframes);
  spin_unlock_irqrestore(&port->pending_iframes_spinlock, flags);

  spin_lock_irqsave(&port->istream_spinlock, flags);
  synccom_frame_clear(port->istream);
  spin_unlock_irqrestore(&port->istream_spinlock, flags);

  spin_lock_irqsave(&port->pending_iframe_spinlock, flags);
  if (port->pending_iframe) {
    synccom_frame_delete(port->pending_iframe);
    port->pending_iframe = 0;
  }
  spin_unlock_irqrestore(&port->pending_iframe_spinlock, flags);

  mutex_unlock(&port->running_bc_mutex);

  return 1;
}

int synccom_port_purge_tx(struct synccom_port *port) {
  int error_code = 0;
  unsigned long flags = 0;

  return_val_if_untrue(port, 0);

  dev_dbg(port->device, "purge_tx\n");

  mutex_lock(&port->register_access_mutex);
  spin_lock_irqsave(&port->board_tx_spinlock, flags);
  error_code = synccom_port_execute_TRES(port, 0);
  spin_unlock_irqrestore(&port->board_tx_spinlock, flags);
  mutex_unlock(&port->register_access_mutex);

  spin_lock_irqsave(&port->queued_oframes_spinlock, flags);
  synccom_flist_clear(&port->queued_oframes);
  spin_unlock_irqrestore(&port->queued_oframes_spinlock, flags);

  spin_lock_irqsave(&port->pending_oframe_spinlock, flags);
  if (port->pending_oframe) {
    synccom_frame_delete(port->pending_oframe);
    port->pending_oframe = 0;
  }
  spin_unlock_irqrestore(&port->pending_oframe_spinlock, flags);

  wake_up_interruptible(&port->output_queue);

  return 1;
}

unsigned synccom_port_get_input_memory_usage(struct synccom_port *port) {
  unsigned value = 0;
  unsigned long pending_flags;
  unsigned long queued_flags;
  unsigned long stream_flags;

  return_val_if_untrue(port, 0);

  spin_lock_irqsave(&port->queued_iframes_spinlock, queued_flags);
  value = port->queued_iframes.estimated_memory_usage;
  spin_unlock_irqrestore(&port->queued_iframes_spinlock, queued_flags);

  spin_lock_irqsave(&port->istream_spinlock, stream_flags);
  value += synccom_frame_get_length(port->istream);
  spin_unlock_irqrestore(&port->istream_spinlock, stream_flags);

  spin_lock_irqsave(&port->pending_iframe_spinlock, pending_flags);
  if (port->pending_iframe)
    value += synccom_frame_get_length(port->pending_iframe);
  spin_unlock_irqrestore(&port->pending_iframe_spinlock, pending_flags);

  return value;
}

unsigned synccom_port_get_output_memory_usage(struct synccom_port *port) {

  unsigned value = 0;
  unsigned long pending_flags;
  unsigned long queued_flags;

  return_val_if_untrue(port, 0);

  spin_lock_irqsave(&port->queued_oframes_spinlock, queued_flags);
  value = port->queued_oframes.estimated_memory_usage;
  spin_unlock_irqrestore(&port->queued_oframes_spinlock, queued_flags);

  spin_lock_irqsave(&port->pending_oframe_spinlock, pending_flags);
  if (port->pending_oframe)
    value += synccom_frame_get_length(port->pending_oframe);
  spin_unlock_irqrestore(&port->pending_oframe_spinlock, pending_flags);

  return value;
}

unsigned synccom_port_get_input_memory_cap(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return port->memory_cap.input;
}

unsigned synccom_port_get_output_memory_cap(struct synccom_port *port) {

  return_val_if_untrue(port, 0);

  return port->memory_cap.output;
}

void synccom_port_set_memory_cap(struct synccom_port *port,
                                 struct synccom_memory_cap *value) {
  return_if_untrue(port);
  return_if_untrue(value);

  if (value->input >= 0) {
    if (port->memory_cap.input != value->input) {
      dev_dbg(port->device, "memory cap (input) %i => %i\n",
              port->memory_cap.input, value->input);
    } else {
      dev_dbg(port->device, "memory cap (input) %i\n", value->input);
    }

    port->memory_cap.input = value->input;
  }

  if (value->output >= 0) {
    if (port->memory_cap.output != value->output) {
      dev_dbg(port->device, "memory cap (output) %i => %i\n",
              port->memory_cap.output, value->output);
    } else {
      dev_dbg(port->device, "memory cap (output) %i\n", value->output);
    }

    port->memory_cap.output = value->output;
  }
}

#define STRB_BASE 0x00000008
#define DTA_BASE 0x00000001
#define CLK_BASE 0x00000002
void synccom_port_set_clock_bits(struct synccom_port *port,
                                 unsigned char *clock_data) {

  __u32 orig_fcr_value = 0;
  __u32 new_fcr_value = 0;
  int j = 0; // Must be signed because we are going backwards through the array
  int i = 0; // Must be signed because we are going backwards through the array
  unsigned strb_value = STRB_BASE;
  unsigned dta_value = DTA_BASE;
  unsigned clk_value = CLK_BASE;
  char buf_data[4];
  __u32 *data = 0;
  unsigned data_index = 0;

  return_if_untrue(port);

  clock_data[15] |= 0x04;

  data = kmalloc(sizeof(__u32) * 323, GFP_KERNEL);
  if (data == NULL) {
    dev_warn(port->device, "%s: kmalloc failed.", __func__);
    return;
  }

  if (port->channel == 1) {
    strb_value <<= 0x08;
    dta_value <<= 0x08;
    clk_value <<= 0x08;
  }

  mutex_lock(&port->register_access_mutex);
  // Don't spinlock here because usb_bulk_msg may sleep.
  // spin_lock_irqsave(&port->board_settings_spinlock, flags);
  orig_fcr_value = synccom_port_get_register(port, 2, FCR_OFFSET, 0);

  data[data_index++] = new_fcr_value = orig_fcr_value & 0xfffff0f0;

  for (i = 19; i >= 0; i--) {
    for (j = 7; j >= 0; j--) {
      int bit = ((clock_data[i] >> j) & 1);

      if (bit)
        new_fcr_value |= dta_value; /* Set data bit */
      else
        new_fcr_value &= ~dta_value; /* Clear data bit */

      data[data_index++] = new_fcr_value |= clk_value;  /* Set clock bit */
      data[data_index++] = new_fcr_value &= ~clk_value; /* Clear clock bit */

      new_fcr_value = orig_fcr_value & 0xfffff0f0;
    }
  }

  new_fcr_value = orig_fcr_value & 0xfffff0f0;

  new_fcr_value |= strb_value; /* Set strobe bit */
  new_fcr_value &= ~clk_value; /* Clear clock bit	*/

  data[data_index++] = new_fcr_value;
  data[data_index++] = orig_fcr_value;

  for (i = 0; i < 323; i++) {
    buf_data[0] = data[i] >> 24;
    buf_data[1] = data[i] >> 16;
    buf_data[2] = data[i] >> 8;
    buf_data[3] = data[i] >> 0;

    synccom_port_set_clock(port, 0, FCR_OFFSET, &buf_data[0], data_index);
  }
  // spin_unlock_irqrestore(&port->board_settings_spinlock, flags);
  mutex_unlock(&port->register_access_mutex);

  kfree(data);
}

int synccom_port_set_append_status(struct synccom_port *port, unsigned value) {
  return_val_if_untrue(port, 0);

  if (value && synccom_port_is_streaming(port))
    return -EOPNOTSUPP;

  if (port->append_status != value) {
    dev_dbg(port->device, "append status %i => %i", port->append_status, value);
  } else {
    dev_dbg(port->device, "append status = %i", value);
  }

  port->append_status = (value) ? 1 : 0;

  return 1;
}

unsigned synccom_port_get_append_status(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return !synccom_port_is_streaming(port) && port->append_status;
}

int synccom_port_set_append_timestamp(struct synccom_port *port,
                                      unsigned value) {
  // TODO: implement
  return -EOPNOTSUPP;
}

unsigned synccom_port_get_append_timestamp(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return !synccom_port_is_streaming(port) && port->append_timestamp;
}

void synccom_port_set_ignore_timeout(struct synccom_port *port,
                                     unsigned value) {
  return_if_untrue(port);

  if (port->ignore_timeout != value) {
    dev_dbg(port->device, "ignore timeout %i => %i", port->ignore_timeout,
            value);
  } else {
    dev_dbg(port->device, "ignore timeout = %i", value);
  }

  port->ignore_timeout = (value) ? 1 : 0;
}

unsigned synccom_port_get_ignore_timeout(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return port->ignore_timeout;
}

void synccom_port_set_rx_multiple(struct synccom_port *port, unsigned value) {
  return_if_untrue(port);

  if (port->rx_multiple != value) {
    dev_dbg(port->device, "receive multiple %i => %i", port->rx_multiple,
            value);
  } else {
    dev_dbg(port->device, "receive multiple = %i", value);
  }

  port->rx_multiple = (value) ? 1 : 0;
}

unsigned synccom_port_get_rx_multiple(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return port->rx_multiple;
}

int synccom_port_execute_TRES(struct synccom_port *port, int need_lock) {
  return_val_if_untrue(port, 0);

  return synccom_port_set_register(port, 0, CMDR_OFFSET, 0x08000000, need_lock);
}

int synccom_port_execute_RRES(struct synccom_port *port, int need_lock) {
  return_val_if_untrue(port, 0);

  return synccom_port_set_register(port, 0, CMDR_OFFSET, 0x00020000, need_lock);
}

unsigned synccom_port_is_streaming(struct synccom_port *port) {
  unsigned transparent_mode = 0;
  unsigned xsync_mode = 0;
  unsigned rlc_mode = 0;
  unsigned fsc_mode = 0;
  unsigned ntb = 0;

  return_val_if_untrue(port, 0);

  transparent_mode = ((port->register_storage.CCR0 & 0x3) == 0x2) ? 1 : 0;
  xsync_mode = ((port->register_storage.CCR0 & 0x3) == 0x1) ? 1 : 0;
  rlc_mode = (port->register_storage.CCR2 & 0xffff0000) ? 1 : 0;
  fsc_mode = (port->register_storage.CCR0 & 0x700) ? 1 : 0;
  ntb = (port->register_storage.CCR0 & 0x70000) >> 16;

  return ((transparent_mode || xsync_mode) && !(rlc_mode || fsc_mode || ntb))
             ? 1
             : 0;
}

int synccom_port_set_tx_modifiers(struct synccom_port *port, int value) {
  return_val_if_untrue(port, 0);

  switch (value) {
  case XF:
  case XF | TXT:
  case XF | TXEXT:
  case XREP:
  case XREP | TXT:
    if (port->tx_modifiers != value) {
      dev_dbg(port->device, "transmit modifiers 0x%x => 0x%x\n",
              port->tx_modifiers, value);
    } else {
      dev_dbg(port->device, "transmit modifiers 0x%x\n", value);
    }

    port->tx_modifiers = value;

    break;

  default:
    dev_warn(port->device, "tx modifiers (invalid value 0x%x)\n", value);

    return -EINVAL;
  }

  return 1;
}

unsigned synccom_port_get_tx_modifiers(struct synccom_port *port) {
  return_val_if_untrue(port, 0);

  return port->tx_modifiers;
}

void synccom_port_execute_transmit(struct synccom_port *port, unsigned dma) {

  unsigned command_register = 0;
  unsigned command_value = 0;
  unsigned command_bar = 0;

  return_if_untrue(port);

  command_bar = 0;
  command_register = CMDR_OFFSET;
  command_value = 0x01000000;

  if (port->tx_modifiers & XREP)
    command_value |= 0x02000000;

  if (port->tx_modifiers & TXT)
    command_value |= 0x10000000;

  if (port->tx_modifiers & TXEXT)
    command_value |= 0x20000000;

  synccom_port_set_register(port, command_bar, command_register, command_value,
                            1);
}

#define TX_FIFO_SIZE 100000
int prepare_frame_for_fifo(struct synccom_port *port,
                           struct synccom_frame *frame, unsigned *length) {
  unsigned current_length = 0;
  unsigned fifo_space = 0;
  unsigned transmit_length = 0;
  unsigned frame_size = 0;
  unsigned size_in_fifo = 0;

  current_length = synccom_frame_get_length(frame);
  size_in_fifo = ((current_length % 4) == 0)
                     ? current_length
                     : current_length + (4 - current_length % 4);
  frame_size = synccom_frame_get_frame_size(frame);
  fifo_space = TX_FIFO_SIZE - 1;
  fifo_space -= fifo_space % 4;
  /* Determine the maximum amount of data we can send this time around. */
  transmit_length = (size_in_fifo > fifo_space) ? fifo_space : size_in_fifo;

  if (transmit_length < 1)
    return 0;

  if(synccom_port_write_data(port, frame->buffer, transmit_length)==0)
    synccom_frame_remove_data(frame, NULL, transmit_length);

  *length = transmit_length;

  /* If this is the first time we add data to the FIFO for this frame we
     tell the port how much data is in this frame. */
  if (current_length == frame_size)
    synccom_port_set_register(port, 0, BC_FIFO_L_OFFSET, frame_size, 1);

  /* We still have more data to send. */
  if (!synccom_frame_is_empty(frame))
    return 1;
  return 2;
}

unsigned synccom_port_transmit_frame(struct synccom_port *port,
                                     struct synccom_frame *frame) {
  unsigned transmit_length = 0;
  int result;

  result = prepare_frame_for_fifo(port, frame, &transmit_length);

  if (result)
    synccom_port_execute_transmit(port, 0);

  dev_dbg(port->device, "F#%i => %i byte%s%s\n", frame->number, transmit_length,
          (transmit_length == 1) ? "" : "s",
          (result == 2) ? " (finished)" : "");

  return result;
}

void program_synccom(struct synccom_port *port, char *line) {
  int count;
  char *msg = NULL;
  int i;
  msg = kmalloc(50, GFP_KERNEL);
  msg[0] = 0x06;

  for (i = 0; line[i] != 13; i++) {
    msg[i + 1] = line[i];
  }

  usb_bulk_msg(port->udev, usb_sndbulkpipe(port->udev, 1), msg, i + 1, &count,
               HZ * 10);

  kfree(msg);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
void timer_handler(unsigned long data) {
  struct synccom_port *port = (struct synccom_port *)data;
#else
void timer_handler(struct timer_list *t) {
  struct synccom_port *port = from_timer(port, t, timer);
#endif
  tasklet_schedule(&port->send_oframe_tasklet);
}

void oframe_worker(unsigned long data) {
  struct synccom_port *port = 0;
  int result = 0;

  unsigned long board_flags = 0;
  unsigned long frame_flags = 0;
  unsigned long queued_flags = 0;

  port = (struct synccom_port *)data;

  return_if_untrue(port);

  spin_lock_irqsave(&port->board_tx_spinlock, board_flags);
  spin_lock_irqsave(&port->pending_oframe_spinlock, frame_flags);

  /* Check if exists and if so, grabs the frame to transmit. */
  if (!port->pending_oframe) {
    spin_lock_irqsave(&port->queued_oframes_spinlock, queued_flags);
    port->pending_oframe = synccom_flist_remove_frame(&port->queued_oframes);
    spin_unlock_irqrestore(&port->queued_oframes_spinlock, queued_flags);

    /* No frames in queue to transmit */
    if (!port->pending_oframe) {
      spin_unlock_irqrestore(&port->pending_oframe_spinlock, frame_flags);
      spin_unlock_irqrestore(&port->board_tx_spinlock, board_flags);
      return;
    }
  }

  result = synccom_port_transmit_frame(port, port->pending_oframe);

  if (result == 2) {
    synccom_frame_delete(port->pending_oframe);
    port->pending_oframe = 0;
  }

  spin_unlock_irqrestore(&port->pending_oframe_spinlock, frame_flags);
  spin_unlock_irqrestore(&port->board_tx_spinlock, board_flags);

  if (result) {
    wake_up_interruptible(&port->output_queue);
    tasklet_schedule(&port->send_oframe_tasklet);
  }
}
