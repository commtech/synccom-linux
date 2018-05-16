/*
	Copyright (C) 2016 Commtech, Inc.

	This file is part of synccom-linux.

	synccom-linux is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	synccom-linux is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with synccom-linux.	If not, see <http://www.gnu.org/licenses/>.

*/

#include <linux/version.h> /* LINUX_VERSION_CODE, KERNEL_VERSION */
#include <linux/workqueue.h>
#include <linux/uaccess.h> /* copy_*_user in <= 2.6.24 */

#include "port.h"
#include "frame.h" /* struct synccom_frame */
#include "utils.h" /* return_{val_}_if_untrue, chars_to_u32, ... */
#include "config.h" /* DEVICE_NAME, DEFAULT_* */
#include "sysfs.h" /* port_*_attribute_group */


void synccom_port_execute_GO_R(struct synccom_port *port);
void synccom_port_execute_STOP_R(struct synccom_port *port);
void synccom_port_execute_STOP_T(struct synccom_port *port);
void synccom_port_execute_RST_R(struct synccom_port *port);
__u32 synccom_port_cont_read(struct synccom_port *port, unsigned bar,
							 unsigned register_offset);
__u32 synccom_port_cont_read2(struct synccom_port *port);
__u32 synccom_port_cont_read3(struct synccom_port *port);
__u32 synccom_port_cont_read4(struct synccom_port *port);
extern unsigned force_fifo;

void frame_count_worker(struct work_struct *port);


/*
	This handles initialization on a port level. So things that each port have
	will be initialized in this function. /port/ nodes, registers, clock,
	and interrupts all happen here because it is specific to the port.
*/


int initialize(struct synccom_port *port){
	
	char clock_bits[20] = DEFAULT_CLOCK_BITS;
	
	mutex_init(&port->register_access_mutex);
	mutex_init(&port->running_bc_mutex);
	
	sema_init(&port->write_semaphore, 1);
	sema_init(&port->read_semaphore, 1);
	
	init_waitqueue_head(&port->input_queue);
	init_waitqueue_head(&port->output_queue);
	
	
	port->memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
	port->memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;

	port->pending_iframe = 0;
	port->pending_oframe = 0;

	spin_lock_init(&port->board_settings_spinlock);
	spin_lock_init(&port->board_rx_spinlock);
	spin_lock_init(&port->board_tx_spinlock);

	spin_lock_init(&port->istream_spinlock);
	spin_lock_init(&port->pending_iframe_spinlock);
	spin_lock_init(&port->pending_oframe_spinlock);
	spin_lock_init(&port->sent_oframes_spinlock);
	spin_lock_init(&port->queued_oframes_spinlock);
	spin_lock_init(&port->queued_iframes_spinlock);
	spin_lock_init(&port->register_concurrency_spinlock);
	
	
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
    
	port->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	port->bulk_in_buffer = kmalloc(514, GFP_KERNEL);
	port->bulk_in_urb2 = usb_alloc_urb(0, GFP_KERNEL);
	port->bulk_in_buffer2 = kmalloc(514, GFP_KERNEL);
	port->bulk_in_urb3 = usb_alloc_urb(0, GFP_KERNEL);
	port->bulk_in_buffer3 = kmalloc(514, GFP_KERNEL);
	port->bulk_in_urb4 = usb_alloc_urb(0, GFP_KERNEL);
	port->bulk_in_buffer4 = kmalloc(514, GFP_KERNEL);
	port->masterbuf = kmalloc(1000000, GFP_KERNEL);
	port->bc_buffer = kmalloc(4000, GFP_KERNEL);
	
    memset(port->bc_buffer, 0, 4000);

	port->mbsize = 0;
	port->running_frame_count = 0;
	
	synccom_port_set_clock_bits(port, clock_bits);
	
	synccom_port_set_registers(port, &port->register_storage);
	
    INIT_LIST_HEAD(&port->list);
	synccom_flist_init(&port->queued_oframes);
	synccom_flist_init(&port->sent_oframes);
	synccom_flist_init(&port->queued_iframes);
	
	setup_timer(&port->timer, &timer_handler, (unsigned long)port);
	
	INIT_WORK(&port->bclist_worker, frame_count_worker);
	
	synccom_port_execute_RRES(port);
	synccom_port_execute_TRES(port);
	
	mod_timer(&port->timer, jiffies + msecs_to_jiffies(20));
	
	return 0;
}

void frame_count_worker(struct work_struct *port)
{
	struct synccom_port *sport = container_of(port, struct synccom_port, bclist_worker);
	update_bc_buffer(sport);
}


void synccom_port_reset_timer(struct synccom_port *port)
{
	
	if (mod_timer(&port->timer, jiffies + msecs_to_jiffies(1)))
		dev_err(port->device, "mod_timer\n");
}

/* Basic check to see if the CE bit is set. */
unsigned synccom_port_timed_out(struct synccom_port *port)
{
	__u32 star_value = 0;
	unsigned i = 0;

	return_val_if_untrue(port, 0);

	for (i = 0; i < DEFAULT_TIMEOUT_VALUE; i++) {
		star_value = synccom_port_get_register(port, 0, STAR_OFFSET);

		if ((star_value & CE_BIT) == 0)
			return 0;
	}

	return 1;
}

/* Create the data structures the work horse functions use to send data. */
int synccom_port_write(struct synccom_port *port, const char *data, unsigned length)
{

	struct synccom_frame *frame = 0;
	
	return_val_if_untrue(port, 0);
    
	frame = synccom_frame_new(port);

	if (!frame)
		return -ENOMEM;

	synccom_frame_add_data_from_user(frame, data, length);
   
    synccom_port_transmit_frame(port, frame);
	
	synccom_frame_delete(frame);
	
	return 0;
}

/*
	Handles taking the streams already retrieved from the card and giving them
	to the user. This is purely a helper for the synccom_port_read function.
*/
ssize_t synccom_port_stream_read(struct synccom_port *port, char *buf,
							  size_t buf_length)
{
    /*streaming mode will return all available data to the user, no framing 
	  mechanism is used. If there is more data available than the size of 
	  users buffer, the user buffer will be filled and returned.*/
	
	unsigned out_length = 0;
	int length;
	
	length = buf_length;

	return_val_if_untrue(port, 0);

	spin_lock_irq(&port->istream_spinlock);
	
		out_length = min(length, port->mbsize);
		copy_to_user(buf, port->masterbuf, out_length);
		port->mbsize -= out_length;
		memmove(port->masterbuf, port->masterbuf + out_length, port->mbsize);
	
	spin_unlock_irq(&port->istream_spinlock);

	return out_length;
}

/*
	Returns -ENOBUFS if count is smaller than pending frame size
	Buf needs to be a user buffer
*/
ssize_t synccom_port_read(struct synccom_port *port, char *buf, size_t count)
{
	
	int framesize = 0;
	int finalsize = 0;
	if (synccom_port_is_streaming(port))
		return synccom_port_stream_read(port, buf, count);
		
				
    framesize = port->bc_buffer[0];
	
	finalsize = framesize;
	finalsize -= (!port->append_status) ? 2 : 0;
	
	//make sure frames of data are available
	
	if((framesize > port->mbsize) || (framesize < 1) || (port->running_frame_count < 1) || (count < finalsize))
	  return 0;
	  
	mutex_lock(&port->running_bc_mutex);
	port->running_frame_count -= 1;
	
	memmove(port->bc_buffer, port->bc_buffer + 1, ((port->running_frame_count > 0) ? port->running_frame_count : 1) * 4);
	mutex_unlock(&port->running_bc_mutex);
	
	//remove or keep status bytes
	
		  
	spin_lock_irq(&port->queued_iframes_spinlock);
	
		copy_to_user(buf, port->masterbuf, finalsize);
		port->mbsize -= (framesize);
		   
		memmove(port->masterbuf, port->masterbuf + framesize, port->mbsize);
	
	spin_unlock_irq(&port->queued_iframes_spinlock); 
	
	return finalsize;
	
}

/* Count is for streaming mode where we need to check if there is enough
   streaming data.
*/
unsigned synccom_port_has_incoming_data(struct synccom_port *port)
{
	unsigned status = 0;
	

	return_val_if_untrue(port, 0);

	if (synccom_port_is_streaming(port)) {
		status =  (synccom_frame_is_empty(port->istream)) ? 0 : 1;
	}
	else {
		spin_lock_irq(&port->queued_iframes_spinlock);
		if (port->mbsize >= port->bc_buffer[0])
			status = 1;
		spin_unlock_irq(&port->queued_iframes_spinlock);
	}

	return status;
}


static void usb_callback(struct urb *urb)
{
	struct synccom_port *dev;
	int transfer_size = 0;
	size_t payload = 0;
	int i = 0;
	
    unsigned char temp = 0;
	
	
	dev = urb->context;
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);

		spin_lock(&dev->err_lock);
		dev->errors = urb->status;
		spin_unlock(&dev->err_lock);
    

		if(urb->status == -ESHUTDOWN)
		   return;
		   
		synccom_port_cont_read(dev, 0, CCR0_OFFSET);
		return;
		
	}
	transfer_size = urb->actual_length;
	
	payload = dev->bulk_in_buffer[0] << 8;
	payload |= dev->bulk_in_buffer[1];
	

	if ((dev->mbsize + payload) > 1000000){
		printk(KERN_INFO "max mem reached!!!");
		synccom_port_cont_read(dev, 0, CCR0_OFFSET);
		return;
	}
	
	for(i = 0; i < (payload + 2); i += 2){ 
		temp = dev->bulk_in_buffer[i];
		dev->bulk_in_buffer[i] = dev->bulk_in_buffer[i + 1];
		dev->bulk_in_buffer[i+1] = temp;
		
	}
	
	spin_lock(&dev->queued_iframes_spinlock);
	spin_lock(&dev->istream_spinlock);
	
	memcpy(dev->masterbuf + dev->mbsize, dev->bulk_in_buffer + 2, payload);
	
	dev->mbsize += payload;
	
	spin_unlock(&dev->istream_spinlock);
	spin_unlock(&dev->queued_iframes_spinlock);
	schedule_work(&dev->bclist_worker);
	wake_up_interruptible(&dev->input_queue);
	
	synccom_port_cont_read(dev, 0, CCR0_OFFSET);
}

static void usb_callback1(struct urb *urb)
{
	struct synccom_port *dev;
	int transfer_size = 0;
	size_t payload = 0;
	int i = 0;
    unsigned char temp = 0;
	
	
	dev = urb->context;
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);

		spin_lock(&dev->err_lock);
		dev->errors = urb->status;
		spin_unlock(&dev->err_lock);
    	
		if(urb->status == -ESHUTDOWN)
		   return;
		   
		synccom_port_cont_read2(dev);
		return;
		
	}
	transfer_size = urb->actual_length;
	
	

	payload = dev->bulk_in_buffer2[0] << 8;
	payload |= dev->bulk_in_buffer2[1];
	
	
	if ((dev->mbsize + payload) > 1000000){
		printk(KERN_INFO "max mem reached!!!");
		synccom_port_cont_read2(dev);
		return;
	}
	
	for(i = 0; i < (payload + 2); i += 2){ 
		temp = dev->bulk_in_buffer2[i];
		dev->bulk_in_buffer2[i] = dev->bulk_in_buffer2[i + 1];
		dev->bulk_in_buffer2[i+1] = temp;
		
	}
	
	spin_lock(&dev->queued_iframes_spinlock);
	spin_lock(&dev->istream_spinlock);
	
	memcpy(dev->masterbuf + dev->mbsize, dev->bulk_in_buffer2 + 2, payload);
	
	dev->mbsize += payload;
	
	
	spin_unlock(&dev->istream_spinlock);
	spin_unlock(&dev->queued_iframes_spinlock);
	schedule_work(&dev->bclist_worker);
	wake_up_interruptible(&dev->input_queue);
	
	synccom_port_cont_read2(dev);
}

static void usb_callback2(struct urb *urb)
{
	struct synccom_port *dev;
	int transfer_size = 0;
	size_t payload = 0;
	int i = 0;
    unsigned char temp = 0;
	
	
	dev = urb->context;
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);

		spin_lock(&dev->err_lock);
		dev->errors = urb->status;
		spin_unlock(&dev->err_lock);
        
		
		
		if(urb->status == -ESHUTDOWN)
		   return;
		   
		synccom_port_cont_read3(dev);
		return;
		
	}
	transfer_size = urb->actual_length;
	
	
	payload = dev->bulk_in_buffer3[0] << 8;
	payload |= dev->bulk_in_buffer3[1];
	
	
	if ((dev->mbsize + payload) > 1000000){
		printk(KERN_INFO "max mem reached!!!");
		synccom_port_cont_read3(dev);
		return;
	}
	
	for(i = 0; i < (payload + 2); i += 2){ 
		temp = dev->bulk_in_buffer3[i];
		dev->bulk_in_buffer3[i] = dev->bulk_in_buffer3[i + 1];
		dev->bulk_in_buffer3[i+1] = temp;
		
	}
	
	
	spin_lock(&dev->queued_iframes_spinlock);
	spin_lock(&dev->istream_spinlock);
	
	memcpy(dev->masterbuf + dev->mbsize, dev->bulk_in_buffer3 + 2, payload);
	
	dev->mbsize += payload;

	
	spin_unlock(&dev->istream_spinlock);
	spin_unlock(&dev->queued_iframes_spinlock);
	schedule_work(&dev->bclist_worker);
	wake_up_interruptible(&dev->input_queue);
	
	synccom_port_cont_read3(dev);
}


static void usb_callback3(struct urb *urb)
{
	struct synccom_port *dev;
	int transfer_size = 0;
	size_t payload = 0;
	int i = 0;
    unsigned char temp = 0;
	
	dev = urb->context;
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);

		spin_lock(&dev->err_lock);
		dev->errors = urb->status;
		spin_unlock(&dev->err_lock);
        
		
		
		if(urb->status == -ESHUTDOWN)
		   return;
		   
		synccom_port_cont_read4(dev);
		return;
		
	}
	transfer_size = urb->actual_length;
   
	payload = dev->bulk_in_buffer4[0] << 8;
	payload |= dev->bulk_in_buffer4[1];
	
	
	if ((dev->mbsize + payload) > 1000000){
		printk(KERN_INFO "max mem reached!!!");
		synccom_port_cont_read4(dev);
		return;
	}
	
	for(i = 0; i < (payload + 2); i += 2){ 
		temp = dev->bulk_in_buffer4[i];
		dev->bulk_in_buffer4[i] = dev->bulk_in_buffer4[i + 1];
		dev->bulk_in_buffer4[i+1] = temp;
		
	}

	spin_lock(&dev->queued_iframes_spinlock);
	spin_lock(&dev->istream_spinlock);
	
	memcpy(dev->masterbuf + dev->mbsize, dev->bulk_in_buffer4 + 2, payload);

	dev->mbsize += payload;
	
	
	spin_unlock(&dev->istream_spinlock);
	spin_unlock(&dev->queued_iframes_spinlock);
	
	schedule_work(&dev->bclist_worker);
	wake_up_interruptible(&dev->input_queue);

	synccom_port_cont_read4(dev);
}


__u32 synccom_port_cont_read(struct synccom_port *port, unsigned bar,
							 unsigned register_offset)
{
	
	struct synccom_port *dev;
	dev = port;
	
    usb_fill_bulk_urb(dev->bulk_in_urb, dev->udev, usb_rcvbulkpipe(dev->udev,0x82),
				dev->bulk_in_buffer, 512 , usb_callback, dev);   
		
	usb_submit_urb(dev->bulk_in_urb, GFP_ATOMIC);
	
	return 0;

}

__u32 synccom_port_cont_read2(struct synccom_port *port)
							
{
	
	struct synccom_port *dev;
	dev = port;
	
	 usb_fill_bulk_urb(dev->bulk_in_urb2, dev->udev, usb_rcvbulkpipe(dev->udev,0x82),
				dev->bulk_in_buffer2, 512 , usb_callback1, dev);
	
	usb_submit_urb(dev->bulk_in_urb2, GFP_ATOMIC);
	
	return 0;
}

__u32 synccom_port_cont_read3(struct synccom_port *port)
							
{
	
	struct synccom_port *dev;
	dev = port;
	
	 usb_fill_bulk_urb(dev->bulk_in_urb3, dev->udev, usb_rcvbulkpipe(dev->udev,0x82),
				dev->bulk_in_buffer3, 512 , usb_callback2, dev);
	
	usb_submit_urb(dev->bulk_in_urb3, GFP_ATOMIC);
	
	return 0;
}

__u32 synccom_port_cont_read4(struct synccom_port *port)
							
{
	
	struct synccom_port *dev;
	dev = port;
	
	
	 usb_fill_bulk_urb(dev->bulk_in_urb4, dev->udev, usb_rcvbulkpipe(dev->udev,0x82),
				dev->bulk_in_buffer4, 512 , usb_callback3, dev);
	
	usb_submit_urb(dev->bulk_in_urb4, GFP_ATOMIC);
	
	
	return 0;
}
/*
	At the port level the offset will automatically be converted to the port
	specific offset.
*/


__u32 synccom_port_get_register(struct synccom_port *port, unsigned bar,
							 unsigned register_offset)
{
	
	unsigned offset;
	__u32 value = 0;
	__u32 fvalue = 0;
    int command = 0x6b;
	int count;
	char msg[3];
	
	struct synccom_port *dev;
	dev = port;
	
	return_val_if_untrue(port, 0);
	return_val_if_untrue(bar <= 2, 0);
	
	offset = port_offset(port, bar, register_offset);
		
	msg[0] = command;
	msg[1] = (offset >> 8) & 0xFF;
	msg[2] = offset & 0xFF;
	
	
	mutex_lock(&port->register_access_mutex);
	        usb_bulk_msg(port->udev, 
	        usb_sndbulkpipe(port->udev, 1), &msg, 
		    sizeof(msg), &count, HZ*10);
	
            usb_bulk_msg(port->udev, 
	        usb_rcvbulkpipe(port->udev, 1), &value, 
		    sizeof(value), &count, HZ*10);	
	mutex_unlock(&port->register_access_mutex);

	
    fvalue = ((value>>24)&0xff) | ((value<<8)&0xff0000) | ((value>>8)&0xff00) | ((value<<24)&0xff000000);
		
			
return fvalue;	
}




int synccom_port_set_register(struct synccom_port *port, unsigned bar,
						   unsigned register_offset, __u32 value)
{
	
	unsigned offset = 0;
    int command = 0x6a;
	char msg[7]; 
	int count;
	
	return_val_if_untrue(port, 0);
	return_val_if_untrue(bar <= 2, 0);

	offset = port_offset(port, bar, register_offset);
   
	/* Checks to make sure there is a clock present. */
	 if (register_offset == CMDR_OFFSET && port->ignore_timeout == 0
		&& synccom_port_timed_out(port)) {
		return -ETIMEDOUT;
	}

	//construct a message string to send to the synccom
	msg[0] = command;
	msg[1] = (offset >> 8) & 0xFF;
	msg[2] = offset & 0xFF;
	msg[3] = (value >> 24) & 0xFF;
    msg[4] = (value >> 16) & 0xFF;
	msg[5] = (value >> 8) & 0xFF;
	msg[6] =  value & 0xFF;
	
	//send the message to the synccom
	mutex_lock(&port->register_access_mutex);
         usb_bulk_msg(port->udev, 
	     usb_sndbulkpipe(port->udev, 1), &msg, 
		 sizeof(msg), &count, HZ*10);		  
	mutex_unlock(&port->register_access_mutex);
	
	return 1;
}

/*
	At the port level the offset will automatically be converted to the port
	specific offset.
*/

void synccom_port_send_data(struct synccom_port *port, unsigned bar,
								unsigned register_offset, char *data,
								unsigned byte_count)
{
	
	unsigned offset = 0;
    int count;
	
	return_if_untrue(port);
	return_if_untrue(bar <= 2);
	return_if_untrue(data);
	return_if_untrue(byte_count > 0);
	
	offset = port_offset(port, bar, register_offset);
	
	     usb_bulk_msg(port->udev, 
	     usb_sndbulkpipe(port->udev, 6), data, 
		 byte_count, &count, 0);		
	
}

void synccom_port_set_clock(struct synccom_port *port, unsigned bar,
								unsigned register_offset, char *data,
								unsigned byte_count)
{
	
	unsigned offset = 0;
    int count;
    char msg[7];
	
	return_if_untrue(port);
	return_if_untrue(bar <= 2);
	return_if_untrue(data);
	return_if_untrue(byte_count > 0);
	
	
	offset = port_offset(port, bar, register_offset);
	
	msg[0] = 0x6a;
	msg[1] = 0x00;
	msg[2] = 0x40;
	msg[3] = data[0];
    msg[4] = data[1];
	msg[5] = data[2];
	msg[6] = data[3];
	
	     usb_bulk_msg(port->udev, 
	     usb_sndbulkpipe(port->udev, 1), &msg, 
		 7, &count, HZ*10);		
	
}

/*
	At the port level the offset will automatically be converted to the port
	specific offset.
*/
int synccom_port_set_registers(struct synccom_port *port,
							const struct synccom_registers *regs)
{
	unsigned stalled = 0;
	unsigned i = 0;

	return_val_if_untrue(port, 0);
	return_val_if_untrue(regs, 0);

	for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
		unsigned register_offset = i * 4;

		if (is_read_only_register(register_offset)
			|| ((synccom_register *)regs)[i] < 0) {
			continue;
			
		}

		if (register_offset <= MAX_OFFSET) {
			if (synccom_port_set_register(port, 0, register_offset, ((synccom_register *)regs)[i]) == -ETIMEDOUT)
				stalled = 1;
				
		}
		else {
			synccom_port_set_register(port, 2, FCR_OFFSET,
								   ((synccom_register *)regs)[i]);
				
		}
	}

	return (stalled) ? -ETIMEDOUT : 1;
}

/*
  synccom_port_get_registers reads only the registers that are specified with SYNCCOM_UPDATE_VALUE.
  If the requested register is larger than the maximum FCore register it jumps to the FCR register.
*/
void synccom_port_get_registers(struct synccom_port *port, struct synccom_registers *regs)
{
	unsigned i = 0;

	return_if_untrue(port);
	return_if_untrue(regs);

	for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
		if (((synccom_register *)regs)[i] != SYNCCOM_UPDATE_VALUE)
			continue;

		if (i * 4 <= MAX_OFFSET) {
			((synccom_register *)regs)[i] = synccom_port_get_register(port, 0, i * 4);
		}
		else {
			((synccom_register *)regs)[i] = synccom_port_get_register(port, 2,FCR_OFFSET);
		}
	}
}

__u32 synccom_port_get_TXCNT(struct synccom_port *port)
{
	__u32 fifo_bc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_bc_value = synccom_port_get_register(port, 0, FIFO_BC_OFFSET);

	return (fifo_bc_value & 0x1FFF0000) >> 16;
}

unsigned synccom_port_get_RXCNT(struct synccom_port *port)
{
	
	__u32 fifo_bc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_bc_value = synccom_port_get_register(port, 0, FIFO_BC_OFFSET);

	/* Not sure why, but this can be larger than 8192. We add
       the 8192 check here so other code can count on the value
       not being larger than 8192. */
	return min(fifo_bc_value & 0x00003FFF, (__u32)8192);
}
__u8 synccom_port_get_FREV(struct synccom_port *port)
{
	__u32 vstr_value = 0;

	return_val_if_untrue(port, 0);

	vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET);

	return (__u8)((vstr_value & 0x000000FF));
}

__u8 synccom_port_get_PREV(struct synccom_port *port)
{
	__u32 vstr_value = 0;

	return_val_if_untrue(port, 0);

	vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET);

	return (__u8)((vstr_value & 0x0000FF00) >> 8);
}

__u16 synccom_port_get_PDEV(struct synccom_port *port)
{
	__u32 vstr_value = 0;

	return_val_if_untrue(port, 0);

	vstr_value = synccom_port_get_register(port, 0, VSTR_OFFSET);

	return (__u16)((vstr_value & 0xFFFF0000) >> 16);
}

unsigned synccom_port_get_CE(struct synccom_port *port)
{
	__u32 star_value = 0;

	return_val_if_untrue(port, 0);

	star_value = synccom_port_get_register(port, 0, STAR_OFFSET);

	return (unsigned)((star_value & 0x00040000) >> 18);
}


int synccom_port_purge_rx(struct synccom_port *port)
{
	int error_code = 0;
	return_val_if_untrue(port, 0);

	dev_dbg(port->device, "purge_rx\n");

    spin_lock(&port->queued_iframes_spinlock);
    spin_lock(&port->istream_spinlock);
	
		error_code = synccom_port_execute_RRES(port);
		memset(port->masterbuf, 0, port->mbsize);
		port->mbsize = 0;
		memset(port->bc_buffer, 0, port->running_frame_count);
		port->running_frame_count = 0;
		
	spin_unlock(&port->queued_iframes_spinlock);
    spin_unlock(&port->istream_spinlock);
	
	return 1;
}

int synccom_port_purge_tx(struct synccom_port *port)
{
	int error_code = 0;
	
	return_val_if_untrue(port, 0);

	dev_dbg(port->device, "purge_tx\n");

	spin_lock(&port->board_tx_spinlock);
	error_code = synccom_port_execute_TRES(port);
	spin_unlock(&port->board_tx_spinlock);

	if (error_code < 0)
		return error_code;
/*
	spin_lock(&port->queued_oframes_spinlock);
	synccom_flist_clear(&port->queued_oframes);
	spin_unlock(&port->queued_oframes_spinlock);

	spin_lock(&port->sent_oframes_spinlock);
	synccom_flist_clear(&port->sent_oframes);
	spin_unlock(&port->sent_oframes_spinlock);

	spin_lock(&port->pending_oframe_spinlock);
	if (port->pending_oframe) {
		synccom_frame_delete(port->pending_oframe);
		port->pending_oframe = 0;
	}
	spin_unlock(&port->pending_oframe_spinlock);
*/
	wake_up_interruptible(&port->output_queue);

	return 1;
}

unsigned synccom_port_get_input_memory_usage(struct synccom_port *port)
{
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

unsigned synccom_port_get_output_memory_usage(struct synccom_port *port)
{
	
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


unsigned synccom_port_get_input_memory_cap(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->memory_cap.input;
}

unsigned synccom_port_get_output_memory_cap(struct synccom_port *port)
{
	  
	return_val_if_untrue(port, 0);

	return port->memory_cap.output;
}

void synccom_port_set_memory_cap(struct synccom_port *port,
							  struct synccom_memory_cap *value)
{
	return_if_untrue(port);
	return_if_untrue(value);

	if (value->input >= 0) {
		if (port->memory_cap.input != value->input) {
			dev_dbg(port->device, "memory cap (input) %i => %i\n",
					port->memory_cap.input, value->input);
		}
		else {
			dev_dbg(port->device, "memory cap (input) %i\n",
					value->input);
		}

		port->memory_cap.input = value->input;
	}

	if (value->output >= 0) {
		if (port->memory_cap.output != value->output) {
			dev_dbg(port->device, "memory cap (output) %i => %i\n",
					port->memory_cap.output, value->output);
		}
		else {
			dev_dbg(port->device, "memory cap (output) %i\n",
					value->output);
		}

		port->memory_cap.output = value->output;
	}
}

#define STRB_BASE 0x00000008
#define DTA_BASE 0x00000001
#define CLK_BASE 0x00000002

void synccom_port_set_clock_bits(struct synccom_port *port,
							  unsigned char *clock_data)
{
	
	__u32 orig_fcr_value = 0;
	__u32 new_fcr_value = 0;
	int j = 0; // Must be signed because we are going backwards through the array
	int i = 0; // Must be signed because we are going backwards through the array
	unsigned strb_value = STRB_BASE;
	unsigned dta_value = DTA_BASE;
	unsigned clk_value = CLK_BASE;
	unsigned long flags;
    char buf_data[4];
	__u32 *data = 0;
	unsigned data_index = 0;

	return_if_untrue(port);

     clock_data[15] |= 0x04;
   

   
	data = kmalloc(sizeof(__u32) * 323, GFP_KERNEL);

	if (data == NULL) {
		printk(KERN_ERR DEVICE_NAME "kmalloc failed\n");
		return;
	}

	if (port->channel == 1) {
		strb_value <<= 0x08;
		dta_value <<= 0x08;
		clk_value <<= 0x08;
	}

	orig_fcr_value = synccom_port_get_register(port, 2, FCR_OFFSET);
	spin_lock_irqsave(&port->board_settings_spinlock, flags);

	
	
	data[data_index++] = new_fcr_value = orig_fcr_value & 0xfffff0f0;

	for (i = 19; i >= 0; i--) {
		for (j = 7; j >= 0; j--) {
			int bit = ((clock_data[i] >> j) & 1);

            /* This is required for 4-port cards. I'm not sure why at the
               moment */
			//data[data_index++] = new_fcr_value;

			if (bit)
				new_fcr_value |= dta_value; /* Set data bit */
			else
				new_fcr_value &= ~dta_value; /* Clear data bit */

			data[data_index++] = new_fcr_value |= clk_value; /* Set clock bit */
			data[data_index++] = new_fcr_value &= ~clk_value; /* Clear clock bit */

			new_fcr_value = orig_fcr_value & 0xfffff0f0;
		}
	}

	new_fcr_value = orig_fcr_value & 0xfffff0f0;

	new_fcr_value |= strb_value; /* Set strobe bit */
	new_fcr_value &= ~clk_value; /* Clear clock bit	*/

	data[data_index++] = new_fcr_value;
	data[data_index++] = orig_fcr_value;

for(i = 0; i < 323; i++)
{
        buf_data[0] = data[i] >> 24; 
		buf_data[1] = data[i] >> 16; 
		buf_data[2] = data[i] >> 8;
		buf_data[3] = data[i] >> 0;
		
	synccom_port_set_clock(port, 0, FCR_OFFSET, &buf_data[0], data_index);
}
	spin_unlock_irqrestore(&port->board_settings_spinlock, flags);

	kfree(data);
}

int synccom_port_set_append_status(struct synccom_port *port, unsigned value)
{
	return_val_if_untrue(port, 0);

	if (value && synccom_port_is_streaming(port))
	    return -EOPNOTSUPP;

    if (port->append_status != value) {
		dev_dbg(port->device, "append status %i => %i", port->append_status,
		        value);
    }
    else {
		dev_dbg(port->device, "append status = %i", value);
    }

	port->append_status = (value) ? 1 : 0;

	return 1;
}

int synccom_port_set_append_timestamp(struct synccom_port *port, unsigned value)
{
	
	    return -EOPNOTSUPP;

}

void synccom_port_set_ignore_timeout(struct synccom_port *port,
								  unsigned value)
{
	return_if_untrue(port);

    if (port->ignore_timeout != value) {
		dev_dbg(port->device, "ignore timeout %i => %i",
		        port->ignore_timeout, value);
    }
    else {
		dev_dbg(port->device, "ignore timeout = %i", value);
    }

	port->ignore_timeout = (value) ? 1 : 0;
}

void synccom_port_set_rx_multiple(struct synccom_port *port,
							   unsigned value)
{
	return_if_untrue(port);

    if (port->rx_multiple != value) {
		dev_dbg(port->device, "receive multiple %i => %i",
		        port->rx_multiple, value);
    }
    else {
		dev_dbg(port->device, "receive multiple = %i", value);
    }

	port->rx_multiple = (value) ? 1 : 0;
}

unsigned synccom_port_get_append_status(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return !synccom_port_is_streaming(port) && port->append_status;
}

unsigned synccom_port_get_append_timestamp(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return !synccom_port_is_streaming(port) && port->append_timestamp;
}

unsigned synccom_port_get_ignore_timeout(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->ignore_timeout;
}

unsigned synccom_port_get_rx_multiple(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->rx_multiple;
}

int synccom_port_execute_TRES(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return synccom_port_set_register(port, 0, CMDR_OFFSET, 0x08000000);
}

int synccom_port_execute_RRES(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);
	
	return synccom_port_set_register(port, 0, CMDR_OFFSET, 0x00020000);
}


unsigned synccom_port_is_streaming(struct synccom_port *port)
{
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

	return ((transparent_mode || xsync_mode) && !(rlc_mode || fsc_mode || ntb)) ? 1 : 0;
}


/* Returns -EINVAL if you set an incorrect transmit modifier */
int synccom_port_set_tx_modifiers(struct synccom_port *port, int value)
{
	return_val_if_untrue(port, 0);

	switch (value) {
		case XF:
		case XF|TXT:
		case XF|TXEXT:
		case XREP:
		case XREP|TXT:
			if (port->tx_modifiers != value) {
				dev_dbg(port->device, "transmit modifiers 0x%x => 0x%x\n",
						port->tx_modifiers, value);
			}
			else {
				dev_dbg(port->device, "transmit modifiers 0x%x\n",
						value);
			}

			port->tx_modifiers = value;

			break;

		default:
			dev_warn(port->device, "tx modifiers (invalid value 0x%x)\n",
					 value);

			return -EINVAL;
	}

	return 1;
}

unsigned synccom_port_get_tx_modifiers(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->tx_modifiers;
}

void synccom_port_execute_transmit(struct synccom_port *port, unsigned dma)
{
	
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
	
	synccom_port_set_register(port, command_bar, command_register, command_value);
}

#define TX_FIFO_SIZE 4096

int prepare_frame_for_fifo(struct synccom_port *port, struct synccom_frame *frame,
                           unsigned *length)
{
	
	unsigned current_length = 0;
	unsigned buffer_size = 0;
	unsigned fifo_space = 0;
	unsigned size_in_fifo = 0;
	unsigned transmit_length = 0;
	unsigned data_transmit_length = 0;

	current_length = synccom_frame_get_length(frame);
	
	buffer_size = synccom_frame_get_buffer_size(frame);
	if((current_length % 4) != 0 )
	size_in_fifo = current_length + (4 - current_length % 4);
	
	else
	size_in_fifo = current_length;
	
	
	/* Subtracts 1 so a TDO overflow doesn't happen on the 4096th byte. */
	fifo_space = TX_FIFO_SIZE - synccom_port_get_TXCNT(port) - 1;
	fifo_space -= fifo_space % 4;
	

	/* Determine the maximum amount of data we can send this time around. */
	data_transmit_length = (size_in_fifo > fifo_space) ? fifo_space : current_length;
	transmit_length = (size_in_fifo > fifo_space) ? fifo_space : size_in_fifo;
	
	//frame->fifo_initialized = 1; what was this for???

	if (transmit_length == 0)
		return 1;
		
		
	synccom_port_send_data(port, 0, FIFO_OFFSET,
							   frame->buffer,
							   transmit_length);

	synccom_frame_remove_data(frame, NULL, data_transmit_length);

	*length = transmit_length;

	/* If this is the first time we add data to the FIFO for this frame we
	   tell the port how much data is in this frame. */
	 if (current_length == buffer_size)
		synccom_port_set_register(port, 0, BC_FIFO_L_OFFSET, buffer_size);

	/* We still have more data to send. */
	if (!synccom_frame_is_empty(frame))
		return 1;

	return 2;
}

unsigned synccom_port_transmit_frame(struct synccom_port *port, struct synccom_frame *frame)
{
	
	
	unsigned transmit_length = 0;
	int result;
	
		result = prepare_frame_for_fifo(port, frame, &transmit_length);

	if (result)
		synccom_port_execute_transmit(port, 0);

	while(result == 1){
		result = prepare_frame_for_fifo(port, frame, &transmit_length);

	}
	dev_dbg(port->device, "F#%i => %i byte%s%s\n",
			frame->number, transmit_length,
			(transmit_length == 1) ? "" : "s",
			(result != 2) ? " (starting)" : "");

	return result;
}

void program_synccom(struct synccom_port *port, char *line)
{
	
	 int count;
	 char msg[50];
	 int i;
	 msg[0] = 0x06;
	 
	 for(i = 0; line[i] != 13; i++)
	 {
		 msg[i+1] = line[i];
	 }
	  
	  usb_bulk_msg(port->udev, 
	        usb_sndbulkpipe(port->udev, 1), &msg, 
		    i + 1, &count, HZ*10);
	
}

void timer_handler(unsigned long data)
{
	struct synccom_port *port = (struct synccom_port *)data;
		
	
	synccom_port_cont_read(port, 0, CCR0_OFFSET);
	synccom_port_cont_read2(port);
	synccom_port_cont_read3(port);
	synccom_port_cont_read4(port);
	
}

/*
int update_buffer_size(struct synccom_port *port, unsigned size, int buffer_type)
{
	char *new_buffer;
	
	new_buffer = kmalloc(size, GFP_ATOMIC);
	
	if (new_buffer == NULL) {
		dev_err(port->device, "not enough memory to update frame buffer size\n");
		return 0;
	}
		
	switch(buffer_type){
		case 1:
			memcpy(new_buffer, port->masterbuf, port->mbsize);
	
			kfree(port->masterbuf);
	
			port->masterbuf = new_buffer;
			
			break;
			
		case 2:
			memcpy(new_buffer, port->bc_buffer, port->running_frame_count * 4);
	
			kfree(port->bc_buffer);
	
			port->bc_buffer = new_buffer;
			
			break;
			
		default:
			return 0;
			break;
	}
	
	return 1;
	
}

*/
