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

#include <linux/slab.h> /* kmalloc */
#include <linux/uaccess.h>

#include "frame.h"
#include "utils.h" /* return_{val_}if_true */
#include "port.h" /* struct synccom_port */
#include "card.h" /* struct synccom_card */

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
	frame->dma_initialized = 0;
	frame->port = port;

	frame->number = frame_counter;
	frame_counter += 1;

	return frame;
}

void synccom_frame_delete(struct synccom_frame *frame)
{
	return_if_untrue(frame);

	if (frame->dma_initialized) {
		pci_unmap_single(frame->port->card->pci_dev, frame->data_handle,
						 frame->data_length, DMA_TO_DEVICE);

		pci_unmap_single(frame->port->card->pci_dev, frame->d1_handle,
						 sizeof(*frame->d1), DMA_TO_DEVICE);

		kfree(frame->d1);
	}

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

int synccom_frame_add_data_from_port(struct synccom_frame *frame, struct synccom_port *port,
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
	synccom_port_get_register_rep(port, 0, FIFO_OFFSET, frame->buffer + frame->data_length, length);

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

int synccom_frame_setup_descriptors(struct synccom_frame *frame)
{
	if (frame->fifo_initialized)
		return 0;

	frame->d1 = kmalloc(sizeof(*frame->d1), GFP_ATOMIC | GFP_DMA);

	if (!frame->d1)
		return 0;

	memset(frame->d1, 0, sizeof(*frame->d1));

	frame->d1_handle = pci_map_single(frame->port->card->pci_dev, frame->d1,
	                                  sizeof(*frame->d1), DMA_TO_DEVICE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
	if (dma_mapping_error(&frame->port->card->pci_dev->dev, frame->d1_handle)) {
#else
	if (dma_mapping_error(frame->d1_handle)) {
#endif
		dev_err(frame->port->device, "dma_mapping_error failed\n");

		kfree(frame->d1);
		frame->d1 = 0;

		return 0;
	}


	frame->data_handle = pci_map_single(frame->port->card->pci_dev,
								        frame->buffer, frame->data_length,
								        DMA_TO_DEVICE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
	if (dma_mapping_error(&frame->port->card->pci_dev->dev, frame->data_handle)) {
#else
	if (dma_mapping_error(frame->data_handle)) {
#endif
		dev_err(frame->port->device, "dma_mapping_error failed\n");

		pci_unmap_single(frame->port->card->pci_dev, frame->d1_handle,
						 sizeof(*frame->d1), DMA_TO_DEVICE);

		kfree(frame->d1);
		frame->d1 = 0;

		return 0;
	}

	frame->d1->control = 0xA0000000 | frame->data_length;
	frame->d1->data_address = cpu_to_le32(frame->data_handle);
	frame->d1->data_count = frame->data_length;
	frame->d1->next_descriptor = cpu_to_le32(frame->port->null_handle);

	frame->dma_initialized = 1;

	return 1;
}

unsigned synccom_frame_is_dma(struct synccom_frame *frame)
{
	return (frame->dma_initialized);
}

unsigned synccom_frame_is_fifo(struct synccom_frame *frame)
{
	return (frame->fifo_initialized);
}

int get_frame_size(struct synccom_port *port)
{
	int frame_length;
	char msg[3];
	int count;
	int value;
	msg[0] = 0x6b;
	msg[1] = 0x80;
	msg[2] = 0x08;
	
	 usb_bulk_msg(port->udev, 
	 usb_sndbulkpipe(port->udev, 1), &msg, 
     sizeof(msg), &count, HZ*10);
	
     usb_bulk_msg(port->udev, 
	 usb_rcvbulkpipe(port->udev, 1), &value, 
     4, &count, HZ*10);	
	frame_length = ((value>>24)&0xff) | ((value<<8)&0xff0000) | ((value>>8)&0xff00) | ((value<<24)&0xff000000);
	
	return frame_length;	
	
}

int get_frame_count(struct synccom_port *port)
{
	int frame_count;
	char msg[3];
	int count;
	int value;
	msg[0] = 0x6b;
	msg[1] = 0x80;
	msg[2] = 0x20;
	
	 usb_bulk_msg(port->udev, 
	 usb_sndbulkpipe(port->udev, 1), &msg, 
     sizeof(msg), &count, HZ*10);
	
     usb_bulk_msg(port->udev, 
	 usb_rcvbulkpipe(port->udev, 1), &value, 
     4, &count, HZ*10);	
	frame_count = ((value>>24)&0xff) | ((value<<8)&0x000000) | ((value>>8)&0xff00) | ((value<<24)&0x00000000);
	
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
	
	j = port->running_frame_count;
	frame_count = get_frame_count(port);
	while(i < frame_count){
		byte_count = get_frame_size(port);
	  	
		memcpy((port->bc_buffer + j + i), &byte_count, 4);
		i++;
	  }	
   
	port->running_frame_count += i;
	
}
