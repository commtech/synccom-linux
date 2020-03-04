/*
Copyright 2020 Commtech, Inc.

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

#include <linux/slab.h> /* kmalloc */
#include <linux/uaccess.h>

#include "frame.h"
#include "utils.h" /* return_{val_}if_true */
#include "port.h" /* struct synccom_port */

static unsigned frame_counter = 1;

int synccom_frame_update_buffer_size(struct synccom_frame *frame, unsigned length);

struct synccom_frame *synccom_frame_new(struct synccom_port *port)
{
	
	struct synccom_frame *frame = 0;

	frame = kmalloc(sizeof(*frame), GFP_ATOMIC);

	return_val_if_untrue(frame, 0);

	memset(frame, 0, sizeof(*frame));

	INIT_LIST_HEAD(&frame->list);

	frame->data_length = 0;
	frame->buffer_size = 0;
	frame->buffer = 0;
	frame->fifo_initialized = 0;
	frame->port = port;

	frame->number = frame_counter;
	frame_counter += 1;

	return frame;
}

void synccom_frame_delete(struct synccom_frame *frame)
{
	return_if_untrue(frame);

	
	synccom_frame_update_buffer_size(frame, 0);

	kfree(frame);
}

unsigned synccom_frame_get_length(struct synccom_frame *frame)
{

	return_val_if_untrue(frame, 0);
   
	return frame->data_length;
}

unsigned synccom_frame_get_buffer_size(struct synccom_frame *frame)
{
	return_val_if_untrue(frame, 0);

	return frame->buffer_size;
}

unsigned synccom_frame_is_empty(struct synccom_frame *frame)
{
	return_val_if_untrue(frame, 0);

	return frame->data_length == 0;
}

int synccom_frame_add_data(struct synccom_frame *frame, const char *data,
						 unsigned length)
{
	return_val_if_untrue(frame, 0);
	return_val_if_untrue(length > 0, 0);

	/* Only update buffer size if there isn't enough space already */
	if (frame->data_length + length > frame->buffer_size) {
		if (synccom_frame_update_buffer_size(frame, frame->data_length + length) == 0) {
			return 0;
		}
	}

	/* Copy the new data to the end of the frame */
	memmove(frame->buffer + frame->data_length, data, length);

	frame->data_length += length;

	return 1;
}


int synccom_frame_add_data_from_user(struct synccom_frame *frame, const char *data,
						 unsigned length)
{
	
	
	unsigned uncopied_bytes = 0;

	return_val_if_untrue(frame, 0);
	return_val_if_untrue(length > 0, 0);

	/* Only update buffer size if there isn't enough space already */
	if (frame->data_length + length > frame->buffer_size) {
		if (synccom_frame_update_buffer_size(frame, frame->data_length + length) == 0) {
			return 0;
		}
	}

	/* Copy the new data to the end of the frame */
	uncopied_bytes = copy_from_user(frame->buffer + frame->data_length, data, length);
	return_val_if_untrue(!uncopied_bytes, 0);

	frame->data_length += length;

	return 1;
}

int synccom_frame_remove_data(struct synccom_frame *frame, char *destination,
							unsigned length)
{
	
	
    unsigned untransferred_bytes = 0;

	return_val_if_untrue(frame, 0);

	if (length == 0)
		return 1;

	if (frame->data_length == 0) {
		dev_warn(frame->port->device, "attempting data removal from empty frame\n");
		return 1;
	}

	/* Make sure we don't remove more data than we have */
	if (length > frame->data_length) {
		dev_warn(frame->port->device, "attempting removal of more data than available\n"); 
		return 0;
	}

	/* Copy the data into the outside buffer */
	if (destination)
		untransferred_bytes = copy_to_user(destination, frame->buffer, length);
   
    if (untransferred_bytes > 0)
        return 0;
  
	frame->data_length -= length;
	

	/* Move the data up in the buffer (essentially removing the old data) */
	memmove(frame->buffer, frame->buffer + length, frame->data_length);

	return 1;
}

void synccom_frame_clear(struct synccom_frame *frame)
{
    synccom_frame_update_buffer_size(frame, 0);
}

int synccom_frame_update_buffer_size(struct synccom_frame *frame, unsigned size)
{
	char *new_buffer = 0;
	int malloc_flags = 0;

	return_val_if_untrue(frame, 0);

	if (size == 0) {
		if (frame->buffer) {
			kfree(frame->buffer);
			frame->buffer = 0;
		}

		frame->buffer_size = 0;
		frame->data_length = 0;

		return 1;
	}

	malloc_flags |= GFP_ATOMIC;

	new_buffer = kmalloc(size, malloc_flags);

	if (new_buffer == NULL) {
		dev_err(frame->port->device, "not enough memory to update frame buffer size\n");
		return 0;
	}

	memset(new_buffer, 0, size);

	if (frame->buffer) {
		if (frame->data_length) {
			/* Truncate data length if the new buffer size is less than the data length */
			frame->data_length = min(frame->data_length, size);

			/* Copy over the old buffer data to the new buffer */
			memmove(new_buffer, frame->buffer, frame->data_length);
		}

		kfree(frame->buffer);
	}

	frame->buffer = new_buffer;
	frame->buffer_size = size;

	return 1;
}



unsigned synccom_frame_is_fifo(struct synccom_frame *frame)
{
	return (frame->fifo_initialized);
}

int get_frame_size(struct synccom_port *port)
{
	int frame_length;
	char *msg = NULL;
	int count;
	__u32 *value = NULL;

	msg = kmalloc(3, GFP_KERNEL);
	value = kmalloc(sizeof(*value) * 1, GFP_KERNEL);

	msg[0] = 0x6b;
	msg[1] = 0x80;
	msg[2] = 0x08;

	usb_bulk_msg(port->udev,
	usb_sndbulkpipe(port->udev, 1), msg,
	3, &count, HZ*10);
	
	usb_bulk_msg(port->udev,
	usb_rcvbulkpipe(port->udev, 1), value,
	sizeof(*value), &count, HZ*10);

	frame_length = ((*value>>24)&0xff) | ((*value<<8)&0xff0000) | ((*value>>8)&0xff00) | ((*value<<24)&0xff000000);
	kfree(msg);
	kfree(value);

	return frame_length;
}

int get_frame_count(struct synccom_port *port, int need_lock)
{
	int frame_count;
	char *msg = NULL;
	int count;
	__u32 *value = NULL;
	
	
	msg = kmalloc(3, GFP_KERNEL);
	value = kmalloc(sizeof(*value) * 1, GFP_KERNEL);
	
	msg[0] = 0x6b;
	msg[1] = 0x80;
	msg[2] = 0x20;
	
	if (need_lock) {
		mutex_lock(&port->register_access_mutex);
	}

	usb_bulk_msg(port->udev, 
	usb_sndbulkpipe(port->udev, 1), msg, 
	3, &count, HZ*10);
	
	usb_bulk_msg(port->udev, 
	usb_rcvbulkpipe(port->udev, 1), value, 
	sizeof(*value), &count, HZ*10);

	if (need_lock) {
		mutex_unlock(&port->register_access_mutex);
	}
	frame_count = ((*value>>24)&0xff) | ((*value<<8)&0x000000) | ((*value>>8)&0xff00) | ((*value<<24)&0x00000000);
	//frame_count = 1;	
	kfree(msg);
	kfree(value);

	return frame_count;	
}

void update_bc_buffer(struct synccom_port *dev)
{


	int i = 0;
	int j = 0;
	int frame_count;
	int byte_count;
	struct synccom_port *port;
	port = dev;
	
	mutex_lock(&port->running_bc_mutex);
	mutex_lock(&port->register_access_mutex);
	j = port->running_frame_count;
	frame_count = get_frame_count(port, 0);

	if((frame_count + j) > 1000){
		printk("bc buffer full\n");
		mutex_unlock(&port->register_access_mutex);
		mutex_unlock(&port->running_bc_mutex);
		return;
	}
	
	while(i < frame_count){
		byte_count = synccom_port_get_register(port, 0, BC_FIFO_L_OFFSET, 0);
	  	
		memcpy((port->bc_buffer + j + i), &byte_count, 4);
		i++;
	  }	
    
	port->running_frame_count += i;
	mutex_unlock(&port->register_access_mutex);
	mutex_unlock(&port->running_bc_mutex);
	wake_up_interruptible(&port->input_queue);
	
}
