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

#ifndef SYNCCOM_FRAME_H
#define SYNCCOM_FRAME_H

#include <linux/list.h> /* struct list_head */
#include "descriptor.h" /* struct synccom_descriptor */


#ifdef RELEASE_PREVIEW
typedef struct timespec synccom_timestamp;
#else
typedef struct timeval synccom_timestamp;
#endif


struct synccom_frame {
	struct list_head list;
	char *buffer;
	unsigned data_length;
	unsigned buffer_size;
	unsigned number;
	unsigned fifo_initialized;
	synccom_timestamp timestamp;
	struct synccom_descriptor *d1;
	struct synccom_descriptor *d2;
	struct synccom_port *port;
};

struct synccom_frame *synccom_frame_new(struct synccom_port *port);
void synccom_frame_delete(struct synccom_frame *frame);

unsigned synccom_frame_get_length(struct synccom_frame *frame);
unsigned synccom_frame_get_buffer_size(struct synccom_frame *frame);

int synccom_frame_add_data(struct synccom_frame *frame, const char *data,
						 unsigned length);

int synccom_frame_add_data_from_user(struct synccom_frame *frame, const char *data,
						 unsigned length);
int synccom_frame_remove_data(struct synccom_frame *frame, char *destination,
						   unsigned length);
unsigned synccom_frame_is_empty(struct synccom_frame *frame);

void synccom_frame_clear(struct synccom_frame *frame);

unsigned synccom_frame_is_fifo(struct synccom_frame *frame);
int get_frame_size(struct synccom_port *port);
int get_frame_count(struct synccom_port *port);
void update_bc_buffer(struct synccom_port *dev);
#endif
