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

#include "utils.h"
#include "port.h" /* *_OFFSET */


__u32 chars_to_u32(const char *data)
{
	return *((__u32*)data);
}

int str_to_register_offset(const char *str)
{
	if (strcmp(str, "fifo") == 0)
		return FIFO_OFFSET;
	else if (strcmp(str, "bcfl") == 0)
		return BC_FIFO_L_OFFSET;
	else if (strcmp(str, "fifot") == 0)
		return FIFOT_OFFSET;
	else if (strcmp(str, "fifobc") == 0)
		return FIFO_BC_OFFSET;
	else if (strcmp(str, "fifofc") == 0)
		return FIFO_FC_OFFSET;
	else if (strcmp(str, "cmdr") == 0)
		return CMDR_OFFSET;
	else if (strcmp(str, "star") == 0)
		return STAR_OFFSET;
	else if (strcmp(str, "ccr0") == 0)
		return CCR0_OFFSET;
	else if (strcmp(str, "ccr1") == 0)
		return CCR1_OFFSET;
	else if (strcmp(str, "ccr2") == 0)
		return CCR2_OFFSET;
	else if (strcmp(str, "bgr") == 0)
		return BGR_OFFSET;
	else if (strcmp(str, "ssr") == 0)
		return SSR_OFFSET;
	else if (strcmp(str, "smr") == 0)
		return SMR_OFFSET;
	else if (strcmp(str, "tsr") == 0)
		return TSR_OFFSET;
	else if (strcmp(str, "tmr") == 0)
		return TMR_OFFSET;
	else if (strcmp(str, "rar") == 0)
		return RAR_OFFSET;
	else if (strcmp(str, "ramr") == 0)
		return RAMR_OFFSET;
	else if (strcmp(str, "ppr") == 0)
		return PPR_OFFSET;
	else if (strcmp(str, "tcr") == 0)
		return TCR_OFFSET;
	else if (strcmp(str, "vstr") == 0)
		return VSTR_OFFSET;
	else if (strcmp(str, "isr") == 0)
		return ISR_OFFSET;
	else if (strcmp(str, "imr") == 0)
		return IMR_OFFSET;
	else if (strcmp(str, "dpllr") == 0)
		return DPLLR_OFFSET;
	else if (strcmp(str, "fstel") == 0)
		return 0x5c;
	else if (strcmp(str, "fstew") == 0)
		return 0x60;
	else if (strcmp(str, "fcr") == 0)
		return FCR_OFFSET;
	else
		printk(KERN_NOTICE DEVICE_NAME " invalid str passed into str_to_register_offset\n");

	return -1;
}


unsigned is_read_only_register(unsigned offset)
{
	switch (offset) {
	case STAR_OFFSET:
	case VSTR_OFFSET:
			return 1;
	}

	return 0;
}

unsigned port_offset(struct synccom_port *port, unsigned bar, unsigned offset)
{
	switch (bar) {
	case 0:
		//return (port->channel == 0) ? offset : offset + 0x80;
		//add the 0x80 to the front of the address and shift the offset 1 to the left
    return (0x80<<8) | (offset<<1); 
	case 2:
		switch (offset) {
			case DMACCR_OFFSET:
				return (port->channel == 0) ? offset : offset + 0x04;

			case DMA_RX_BASE_OFFSET:
			case DMA_TX_BASE_OFFSET:
			case DMA_CURRENT_RX_BASE_OFFSET:
			case DMA_CURRENT_TX_BASE_OFFSET:
				return (port->channel == 0) ? offset : offset + 0x08;

			default:
				break;
		}

		break;

	default:
		break;
	}

	return offset;
}
