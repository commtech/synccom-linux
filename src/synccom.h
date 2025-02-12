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

#ifndef SYNCCOM_H
#define SYNCCOM_H

#include <linux/fs.h>    /* struct indode on <= 2.6.19 */
#include <linux/sched.h> /* wait_queue_head_t */

#define SYNCCOM_REGISTERS_INIT(registers)                                      \
  memset(&registers, -1, sizeof(registers))
#define SYNCCOM_MEMORY_CAP_INIT(memory_cap)                                    \
  memset(&memory_cap, -1, sizeof(memory_cap))
#define SYNCCOM_UPDATE_VALUE -2

#define SYNCCOM_IOCTL_MAGIC 0x18

#define TEST _IO(SYNCCOM_IOCTL_MAGIC, 22)

#define SYNCCOM_REPROGRAM _IOW(SYNCCOM_IOCTL_MAGIC, 23, char *)

#define SYNCCOM_GET_REGISTERS                                                  \
  _IOR(SYNCCOM_IOCTL_MAGIC, 0, struct synccom_registers *)
#define SYNCCOM_SET_REGISTERS                                                  \
  _IOW(SYNCCOM_IOCTL_MAGIC, 1, const struct synccom_registers *)

#define SYNCCOM_PURGE_TX _IO(SYNCCOM_IOCTL_MAGIC, 2)
#define SYNCCOM_PURGE_RX _IO(SYNCCOM_IOCTL_MAGIC, 3)

#define SYNCCOM_ENABLE_APPEND_STATUS _IO(SYNCCOM_IOCTL_MAGIC, 4)
#define SYNCCOM_DISABLE_APPEND_STATUS _IO(SYNCCOM_IOCTL_MAGIC, 5)
#define SYNCCOM_GET_APPEND_STATUS _IOR(SYNCCOM_IOCTL_MAGIC, 13, unsigned *)

#define SYNCCOM_SET_MEMORY_CAP                                                 \
  _IOW(SYNCCOM_IOCTL_MAGIC, 6, struct synccom_memory_cap *)
#define SYNCCOM_GET_MEMORY_CAP                                                 \
  _IOR(SYNCCOM_IOCTL_MAGIC, 7, struct synccom_memory_cap *)

#define SYNCCOM_SET_CLOCK_BITS                                                 \
  _IOW(SYNCCOM_IOCTL_MAGIC, 8, const unsigned char[20])

#define SYNCCOM_ENABLE_IGNORE_TIMEOUT _IO(SYNCCOM_IOCTL_MAGIC, 10)
#define SYNCCOM_DISABLE_IGNORE_TIMEOUT _IO(SYNCCOM_IOCTL_MAGIC, 11)
#define SYNCCOM_GET_IGNORE_TIMEOUT _IOR(SYNCCOM_IOCTL_MAGIC, 15, unsigned *)

#define SYNCCOM_SET_TX_MODIFIERS _IOW(SYNCCOM_IOCTL_MAGIC, 12, const unsigned)
#define SYNCCOM_GET_TX_MODIFIERS _IOR(SYNCCOM_IOCTL_MAGIC, 14, unsigned *)

#define SYNCCOM_ENABLE_RX_MULTIPLE _IO(SYNCCOM_IOCTL_MAGIC, 16)
#define SYNCCOM_DISABLE_RX_MULTIPLE _IO(SYNCCOM_IOCTL_MAGIC, 17)
#define SYNCCOM_GET_RX_MULTIPLE _IOR(SYNCCOM_IOCTL_MAGIC, 18, unsigned *)

#define SYNCCOM_ENABLE_APPEND_TIMESTAMP _IO(SYNCCOM_IOCTL_MAGIC, 19)
#define SYNCCOM_DISABLE_APPEND_TIMESTAMP _IO(SYNCCOM_IOCTL_MAGIC, 20)
#define SYNCCOM_GET_APPEND_TIMESTAMP _IOR(SYNCCOM_IOCTL_MAGIC, 21, unsigned *)

#define SYNCCOM_SET_NONVOLATILE _IOW(SYNCCOM_IOCTL_MAGIC, 29, const unsigned)
#define SYNCCOM_GET_NONVOLATILE _IOR(SYNCCOM_IOCTL_MAGIC, 30, unsigned *)

#define SYNCCOM_GET_FX2_FIRMWARE _IOR(SYNCCOM_IOCTL_MAGIC, 31, unsigned *)

enum transmit_modifiers { XF = 0, XREP = 1, TXT = 2, TXEXT = 4 };
typedef __s64 synccom_register;

struct synccom_registers {
  /* BAR 0 */
  synccom_register reserved1[2];

  synccom_register FIFOT;

  synccom_register reserved2[2];

  synccom_register CMDR;
  synccom_register STAR; /* Read-only */
  synccom_register CCR0;
  synccom_register CCR1;
  synccom_register CCR2;
  synccom_register BGR;
  synccom_register SSR;
  synccom_register SMR;
  synccom_register TSR;
  synccom_register TMR;
  synccom_register RAR;
  synccom_register RAMR;
  synccom_register PPR;
  synccom_register TCR;
  synccom_register VSTR; /* Read-only */

  synccom_register reserved3[1];

  synccom_register IMR;
  synccom_register DPLLR;

  /* BAR 2 */
  synccom_register FCR;
};

struct synccom_memory_cap {
  int input;
  int output;
};

extern struct list_head synccom_cards;

#define COMMTECH_VENDOR_ID 0x18f7

#define SYNCCOM_ID 0x000f
#define SSYNCCOM_ID 0x0014
#define SSYNCCOM_104_LVDS_ID 0x0015
#define SYNCCOM_232_ID 0x0016
#define SSYNCCOM_104_UA_ID 0x0017
#define SSYNCCOM_4_UA_ID 0x0018
#define SSYNCCOM_UA_ID 0x0019
#define SSYNCCOM_LVDS_ID 0x001a
#define SYNCCOM_4_UA_ID 0x001b
#define SSYNCCOM_4_LVDS_ID 0x001c
#define SYNCCOM_UA_ID 0x001d
#define SFSCCe_4_ID 0x001e
#define SSYNCCOM_4_CPCI_ID 0x001f
#define SSYNCCOM_4_UA_CPCI_ID 0x0023
#define SSYNCCOM_4_UA_LVDS_ID 0x0025
#define SSYNCCOM_UA_LVDS_ID 0x0026
#define FSCCe_4_UA_ID 0x0027

#define STATUS_LENGTH 2

#endif
