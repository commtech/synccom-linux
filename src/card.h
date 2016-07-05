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

#ifndef SYNCCOM_CARD
#define SYNCCOM_CARD

#include <linux/pci.h> /* struct pci_dev */
#include <linux/fs.h> /* struct file_operations */
#include <linux/8250_pci.h> /* struct serial_private */

#define FCR_OFFSET 0x40
#define DSTAR_OFFSET 0x30

struct synccom_card {
	struct list_head list;
	struct list_head ports;
	struct pci_dev *pci_dev;

	void __iomem *bar[3];

	unsigned dma;
};



void synccom_card_delete(struct synccom_card *card);
void synccom_card_suspend(struct synccom_card *card);
void synccom_card_resume(struct synccom_card *card);

struct synccom_card *synccom_card_find(struct pci_dev *pdev,
								 struct list_head *card_list);

void __iomem *synccom_card_get_BAR(struct synccom_card *card, unsigned number);

__u32 synccom_card_get_register(struct synccom_card *card, unsigned bar,
							 unsigned offset);

void synccom_card_set_register(struct synccom_card *card, unsigned bar,
							unsigned offset, __u32 value);

void synccom_card_get_register_rep(struct synccom_card *card, unsigned bar,
								unsigned offset, char *buf,
								unsigned byte_count);

void synccom_card_set_register_rep(struct synccom_card *card, unsigned bar,
								unsigned offset, const char *data,
								unsigned byte_count);

struct list_head *synccom_card_get_ports(struct synccom_card *card);
unsigned synccom_card_get_irq(struct synccom_card *card);
struct device *synccom_card_get_device(struct synccom_card *card);
char *synccom_card_get_name(struct synccom_card *card);

#endif
