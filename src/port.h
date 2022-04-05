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

#ifndef SYNCCOM_PORT
#define SYNCCOM_PORT

#include <linux/fs.h> /* Needed to build on older kernel version */
#include <linux/cdev.h> /* struct cdev */
#include <linux/interrupt.h> /* struct tasklet_struct */
#include <linux/version.h> /* LINUX_VERSION_CODE, KERNEL_VERSION */
#include <linux/completion.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#include <linux/semaphore.h> /* struct semaphore */
#endif
#include <linux/usb.h>
#include "synccom.h" /* struct synccom_registers */
#include "descriptor.h" /* struct synccom_descriptor */
#include "debug.h" /* stuct debug_interrupt_tracker */
#include "flist.h" /* struct synccom_registers */


#define FCR_OFFSET 0x40
#define DSTAR_OFFSET 0x30

#define FIFO_OFFSET 0x00
#define BC_FIFO_L_OFFSET 0x04
#define FIFOT_OFFSET 0x08
#define FIFO_BC_OFFSET 0x0C
#define FIFO_FC_OFFSET 0x10
#define CMDR_OFFSET 0x14
#define STAR_OFFSET 0x18
#define CCR0_OFFSET 0x1C
#define CCR1_OFFSET 0x20
#define CCR2_OFFSET 0x24
#define BGR_OFFSET 0x28
#define SSR_OFFSET 0x2C
#define SMR_OFFSET 0x30
#define TSR_OFFSET 0x34
#define TMR_OFFSET 0x38
#define RAR_OFFSET 0x3C
#define RAMR_OFFSET 0x40
#define PPR_OFFSET 0x44
#define TCR_OFFSET 0x48
#define VSTR_OFFSET 0x4C
#define ISR_OFFSET 0x50
#define IMR_OFFSET 0x54
#define DPLLR_OFFSET 0x58
#define MAX_OFFSET 0x58 //must equal the highest FCore register address

#define DMACCR_OFFSET 0x04
#define DMA_RX_BASE_OFFSET 0x0c
#define DMA_TX_BASE_OFFSET 0x10
#define DMA_CURRENT_RX_BASE_OFFSET 0x20
#define DMA_CURRENT_TX_BASE_OFFSET 0x24

#define RFE 0x00000004
#define RFT 0x00000002
#define RFS 0x00000001
#define RFO 0x00000008
#define RDO 0x00000010
#define RFL 0x00000020
#define TIN 0x00000100
#define TDU 0x00040000
#define TFT 0x00010000
#define ALLS 0x00020000
#define CTSS 0x01000000
#define DSRC 0x02000000
#define CDC 0x04000000
#define CTSA 0x08000000
#define DR_STOP 0x00004000
#define DT_STOP 0x00008000
#define DT_FE 0x00002000
#define DR_FE 0x00001000
#define DT_HI 0x00000800
#define DR_HI 0x00000400

#define CE_BIT 0x00040000


#define NUMBER_OF_URBS 8
#define URB_BUFFER_SIZE 512

struct synccom_port {
	struct list_head list;
	dev_t dev_t;
	struct class *class;
	struct cdev cdev;
	struct synccom_card *card;
	struct device *device;
	unsigned channel;
	char *name;

	/* Prevents simultaneous read(), write() and poll() calls. */
	struct semaphore read_semaphore;
	struct semaphore write_semaphore;

	wait_queue_head_t input_queue;
	wait_queue_head_t output_queue;

	struct synccom_flist queued_iframes; /* Frames already retrieved from the FIFO */
	struct synccom_flist pending_iframes;
	struct synccom_flist queued_oframes; /* Frames not yet in the FIFO yet */

	struct synccom_frame *pending_iframe; /* Frame retrieving from the FIFO */
	struct synccom_frame *pending_oframe; /* Frame being put in the FIFO */
	struct synccom_frame *istream; /* Transparent stream */

	struct synccom_registers register_storage; /* Only valid on suspend/resume */
	struct synccom_memory_cap memory_cap;



	unsigned last_isr_value;
	unsigned append_status;
	unsigned append_timestamp;
	unsigned ignore_timeout;
	unsigned rx_multiple;
	int tx_modifiers;

	spinlock_t board_rx_spinlock; /* Anything that will alter the state of rx at a board level */
	spinlock_t board_tx_spinlock; /* Anything that will alter the state of rx at a board level */

	spinlock_t istream_spinlock;
	spinlock_t pending_iframe_spinlock;
	spinlock_t pending_oframe_spinlock;
	spinlock_t queued_oframes_spinlock;
	spinlock_t queued_iframes_spinlock;
	spinlock_t pending_iframes_spinlock;
	struct mutex io_mutex;		/* synchronize I/O with disconnect */
	struct mutex running_bc_mutex;
	struct mutex register_access_mutex;

	struct tasklet_struct send_oframe_tasklet;
	struct timer_list timer;
	struct work_struct bclist_worker;

	/***************************usb structure***********************/
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct semaphore	limit_sem;		/* limiting the number of writes in progress */ // TODO is this necessary?
	struct usb_anchor	submitted;		/* in case we need to retract our submissions */
	struct urb							**bulk_in_urbs;
	unsigned char						**bulk_in_buffers;

		/* the buffer to receive data */
	size_t			bulk_in_size;		/* the size of the receive buffer */
	size_t			bulk_in_filled;		/* number of bytes in the buffer */
	size_t			bulk_in_copied;		/* already copied to user space */
	__u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
	__u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
  __u8            bulk_out_endpointAddr2;
	int			    errors;			/* the last request tanked */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	wait_queue_head_t	bulk_in_wait;		/* to wait for an ongoing read */

#ifdef DEBUG
	struct debug_interrupt_tracker *interrupt_tracker;
	struct tasklet_struct print_tasklet;
#endif
};


int initialize(struct synccom_port *port);
void program_synccom(struct synccom_port *port, char *line);

int synccom_port_write(struct synccom_port *port, const char *data, unsigned length);
ssize_t synccom_port_read(struct synccom_port *port, char *buf, size_t count);

unsigned synccom_port_has_iframes(struct synccom_port *port, unsigned lock);
unsigned synccom_port_has_oframes(struct synccom_port *port, unsigned lock);

__u32 synccom_port_get_register(struct synccom_port *port, unsigned bar, unsigned register_offset, int need_lock);

__u32 syncom_update_frames(struct synccom_port *port);
int synccom_port_set_register(struct synccom_port *port, unsigned bar, unsigned register_offset, __u32 value, int need_lock);

int synccom_port_send_data(struct synccom_port *port, char *data, unsigned byte_count);

int synccom_port_purge_tx(struct synccom_port *port);
int synccom_port_purge_rx(struct synccom_port *port);

__u32 synccom_port_get_TXCNT(struct synccom_port *port);
unsigned synccom_port_get_RXCNT(struct synccom_port *port);

__u8 synccom_port_get_FREV(struct synccom_port *port);
__u8 synccom_port_get_PREV(struct synccom_port *port);

int synccom_port_execute_TRES(struct synccom_port *port, int need_lock);
int synccom_port_execute_RRES(struct synccom_port *port, int need_lock);

void synccom_port_suspend(struct synccom_port *port);
void synccom_port_resume(struct synccom_port *port);

unsigned synccom_port_get_output_memory_usage(struct synccom_port *port);
unsigned synccom_port_get_input_memory_usage(struct synccom_port *port);

unsigned synccom_port_get_input_memory_cap(struct synccom_port *port);
unsigned synccom_port_get_output_memory_cap(struct synccom_port *port);
void synccom_port_set_memory_cap(struct synccom_port *port, struct synccom_memory_cap *memory_cap);

void synccom_port_set_clock_bits(struct synccom_port *port, unsigned char *clock_data);

void synccom_port_set_ignore_timeout(struct synccom_port *port, unsigned ignore_timeout);
unsigned synccom_port_get_ignore_timeout(struct synccom_port *port);

void synccom_port_set_rx_multiple(struct synccom_port *port, unsigned rx_multiple);
unsigned synccom_port_get_rx_multiple(struct synccom_port *port);

int synccom_port_set_append_status(struct synccom_port *port, unsigned value);
unsigned synccom_port_get_append_status(struct synccom_port *port);

int synccom_port_set_append_timestamp(struct synccom_port *port, unsigned value);
unsigned synccom_port_get_append_timestamp(struct synccom_port *port);

int synccom_port_set_registers(struct synccom_port *port, const struct synccom_registers *regs);
void synccom_port_get_registers(struct synccom_port *port, struct synccom_registers *regs);

unsigned synccom_port_is_streaming(struct synccom_port *port);
unsigned synccom_port_has_incoming_data(struct synccom_port *port);

#ifdef DEBUG
unsigned synccom_port_get_interrupt_count(struct synccom_port *port, __u32 isr_bit);
void synccom_port_increment_interrupt_counts(struct synccom_port *port, __u32 isr_value);
#endif /* DEBUG */

int synccom_port_set_tx_modifiers(struct synccom_port *port, int tx_modifiers);
unsigned synccom_port_get_tx_modifiers(struct synccom_port *port);
void synccom_port_execute_transmit(struct synccom_port *port, unsigned dma);

void synccom_port_reset_timer(struct synccom_port *port);
unsigned synccom_port_transmit_frame(struct synccom_port *port, struct synccom_frame *frame);
__u32 synccom_port_cont_read(struct synccom_port *port, struct urb *read_urb, unsigned char *data_buffer, u32 size);
void oframe_worker(unsigned long data);
int synccom_port_create_urbs(struct synccom_port *port);
int synccom_port_destroy_urbs(struct synccom_port *port);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
void timer_handler(unsigned long data);
#else
void timer_handler(struct timer_list *t);
#endif
#endif
