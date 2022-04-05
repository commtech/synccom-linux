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
	frame->frame_size = 0;
	frame->lost_bytes = 0;
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

unsigned synccom_frame_get_frame_size(struct synccom_frame *frame)
{
	return_val_if_untrue(frame, 0);

	return frame->frame_size;
}

unsigned synccom_frame_is_empty(struct synccom_frame *frame)
{
	return_val_if_untrue(frame, 0);

	return frame->data_length == 0;
}

int synccom_frame_add_data(struct synccom_frame *frame, const char *data, unsigned length)
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

int synccom_frame_add_data_from_user(struct synccom_frame *frame, const char *data, unsigned length)
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

int synccom_frame_transfer_data(struct synccom_frame *destination, struct synccom_frame *source, unsigned length)
{
	unsigned char *new_buffer = 0;

	return_val_if_untrue(destination, 0);
	return_val_if_untrue(source, 0);

	if(length == 0)
		return 1;

	if (source->data_length == 0) {
			dev_warn(source->port->device, "%s - attempting data removal from empty frame\n", __func__);
			return 1;
	}

	if (length > source->data_length) {
		dev_warn(source->port->device, "%s - attempting removal of more data than available\n", __func__);
		return 0;
	}

	new_buffer = kmalloc(length, GFP_ATOMIC);
	if(!new_buffer) {
		dev_warn(source->port->device, "%s - failed to create new_buffer\n", __func__);
		return 0;
	}

	memcpy(new_buffer, source->buffer, length);
	source->data_length -= length;
	memmove(source->buffer, source->buffer + length, source->data_length);
	synccom_frame_update_buffer_size(destination, destination->frame_size);
	synccom_frame_add_data(destination, new_buffer, length);

	kfree(new_buffer);
	return 1;
}

int synccom_frame_remove_data(struct synccom_frame *frame, char *destination, unsigned length)
{
	unsigned untransferred_bytes = 0;

	return_val_if_untrue(frame, 0);

	if (length == 0)
		return 1;

	if (frame->data_length == 0) {
		dev_warn(frame->port->device, "%s - attempting data removal from empty frame\n", __func__);
		return 1;
	}

	/* Make sure we don't remove more data than we have */
	if (length > frame->data_length) {
		dev_warn(frame->port->device, "%s - attempting removal of more data than available\n", __func__);
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
		dev_err(frame->port->device, "%s - not enough memory to update frame buffer size\n", __func__);
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

void update_bc_buffer(struct synccom_port *port)
{
	int i = 0, do_once = 0;
	int frame_count, byte_count;
	unsigned long queued_flags = 0, stream_flags = 0, pending_iframes = 0;
	struct synccom_frame *frame;

	mutex_lock(&port->register_access_mutex);
	frame_count = synccom_port_get_register(port, 0, FIFO_FC_OFFSET, 0) & 0x3ff;
	// This loop may never run, and that's actually okay.
	for(i=0;i<frame_count;i++) {
		byte_count = synccom_port_get_register(port, 0, BC_FIFO_L_OFFSET, 0);
		dev_dbg(port->device, "New frame size: %d", byte_count);
		frame = synccom_frame_new(port);
		frame->frame_size = byte_count;

		spin_lock_irqsave(&port->pending_iframes_spinlock, pending_iframes);
		synccom_flist_add_frame(&port->pending_iframes, frame);
		spin_unlock_irqrestore(&port->pending_iframes_spinlock, pending_iframes);
	}
	mutex_unlock(&port->register_access_mutex);

	spin_lock_irqsave(&port->pending_iframes_spinlock, pending_iframes);
	spin_lock_irqsave(&port->istream_spinlock, stream_flags);
	do {
		if(!port->istream) break;
		if(synccom_frame_get_length(port->istream) < 1) break;
		if(synccom_flist_is_empty(&port->pending_iframes)) break;
		frame = synccom_flist_peek_front(&port->pending_iframes);
		if(!frame) break;
		if(frame->frame_size < 1) {
			frame = synccom_flist_remove_frame(&port->pending_iframes);
			synccom_frame_delete(frame);
			break;
		}
		if(synccom_frame_get_length(port->istream) < (frame->frame_size - frame->lost_bytes)) break;
		frame = synccom_flist_remove_frame(&port->pending_iframes);
		synccom_frame_transfer_data(frame, port->istream, (frame->frame_size - frame->lost_bytes));
		//append_timestamp
		dev_dbg(port->device, "F#%d <=: %d, data: %d", frame->number, frame->frame_size, frame->data_length);
		spin_lock_irqsave(&port->queued_iframes_spinlock, queued_flags);
		synccom_flist_add_frame(&port->queued_iframes, frame);
		spin_unlock_irqrestore(&port->queued_iframes_spinlock, queued_flags);
		frame = 0;
	} while(do_once);
	spin_unlock_irqrestore(&port->istream_spinlock, stream_flags);
	spin_unlock_irqrestore(&port->pending_iframes_spinlock, pending_iframes);

	wake_up_interruptible(&port->input_queue);
}
