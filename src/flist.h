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
	along with synccom-linux.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef SYNCCOM_FLIST_H
#define SYNCCOM_FLIST_H

#include <linux/fs.h> /* Needed to build on older kernel version */
#include <linux/cdev.h> /* struct cdev */
#include <linux/interrupt.h> /* struct tasklet_struct */
#include <linux/version.h> /* LINUX_VERSION_CODE, KERNEL_VERSION */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#include <linux/semaphore.h> /* struct semaphore */
#endif

#include "frame.h"

struct synccom_flist {
	struct list_head frames;
	unsigned estimated_memory_usage;
};

void synccom_flist_init(struct synccom_flist *flist);
void synccom_flist_delete(struct synccom_flist *flist);
void synccom_flist_add_frame(struct synccom_flist *flist, struct synccom_frame *frame);
struct synccom_frame *synccom_flist_remove_frame(struct synccom_flist *flist);
struct synccom_frame *synccom_flist_remove_frame_if_lte(struct synccom_flist *flist, unsigned size);
struct synccom_frame *synccom_flist_peek_front(struct synccom_flist *flist);
struct synccom_frame *synccom_flist_peek_back(struct synccom_flist *flist);
void synccom_flist_clear(struct synccom_flist *flist);
unsigned synccom_flist_is_empty(struct synccom_flist *flist);
unsigned synccom_flist_calculate_memory_usage(struct synccom_flist *flist);
unsigned synccom_flist_length(struct synccom_flist *flist);

#endif
