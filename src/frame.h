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

#ifndef SYNCCOM_FRAME_H
#define SYNCCOM_FRAME_H

#include "descriptor.h" /* struct synccom_descriptor */
#include <linux/list.h> /* struct list_head */
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
typedef struct timespec64 synccom_timestamp;
#define SET_TIMESTAMP(x) ktime_get_ts64(x)
#else
typedef struct timeval synccom_timestamp;
#define SET_TIMESTAMP(x) do_gettimeofday(x)
#endif

struct synccom_frame {
  struct list_head list;
  char *buffer;
  unsigned data_length;
  unsigned buffer_size;
  unsigned frame_size;
  unsigned lost_bytes;
  unsigned number;
  synccom_timestamp timestamp;
  struct synccom_port *port;
};

struct synccom_frame *synccom_frame_new(struct synccom_port *port);
void synccom_frame_delete(struct synccom_frame *frame);
unsigned synccom_frame_get_length(struct synccom_frame *frame);
unsigned synccom_frame_get_buffer_size(struct synccom_frame *frame);
unsigned synccom_frame_get_frame_size(struct synccom_frame *frame);
int synccom_frame_add_data(struct synccom_frame *frame, const char *data,
                           unsigned length);
int synccom_frame_add_data_from_user(struct synccom_frame *frame,
                                     const char *data, unsigned length);
int synccom_frame_remove_data(struct synccom_frame *frame, char *destination,
                              unsigned length);
int synccom_frame_transfer_data(struct synccom_frame *destination,
                                struct synccom_frame *source, unsigned length);
unsigned synccom_frame_is_empty(struct synccom_frame *frame);
void synccom_frame_clear(struct synccom_frame *frame);
void update_bc_buffer(struct synccom_port *dev);
#endif
