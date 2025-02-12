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



#ifndef SYNCCOM_H
#define SYNCCOM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

#define SYNCCOM_REGISTERS_INIT(regs) memset(&regs, -1, sizeof(regs))
#define SYNCCOM_MEMORY_CAP_INIT(memcap) memset(&memcap, -1, sizeof(memcap))
#define SYNCCOM_UPDATE_VALUE -2

enum transmit_type { XF=0, XREP=1, TXT=2, TXEXT=4 };

typedef int64_t synccom_register;

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


#define SYNCCOM_IOCTL_MAGIC 0x18
#define TEST _IO(SYNCCOM_IOCTL_MAGIC, 22)
#define SYNCCOM_REPROGRAM _IOW(SYNCCOM_IOCTL_MAGIC, 23, char *)
#define SYNCCOM_GET_REGISTERS _IOR(SYNCCOM_IOCTL_MAGIC, 0, struct synccom_registers *)
#define SYNCCOM_SET_REGISTERS _IOW(SYNCCOM_IOCTL_MAGIC, 1, const struct synccom_registers *)

#define SYNCCOM_PURGE_TX _IO(SYNCCOM_IOCTL_MAGIC, 2)
#define SYNCCOM_PURGE_RX _IO(SYNCCOM_IOCTL_MAGIC, 3)

#define SYNCCOM_ENABLE_APPEND_STATUS _IO(SYNCCOM_IOCTL_MAGIC, 4)
#define SYNCCOM_DISABLE_APPEND_STATUS _IO(SYNCCOM_IOCTL_MAGIC, 5)
#define SYNCCOM_GET_APPEND_STATUS _IOR(SYNCCOM_IOCTL_MAGIC, 13, unsigned *)

#define SYNCCOM_SET_MEMORY_CAP _IOW(SYNCCOM_IOCTL_MAGIC, 6, struct synccom_memory_cap *)
#define SYNCCOM_GET_MEMORY_CAP _IOR(SYNCCOM_IOCTL_MAGIC, 7, struct synccom_memory_cap *)

#define SYNCCOM_SET_CLOCK_BITS _IOW(SYNCCOM_IOCTL_MAGIC, 8, const unsigned char[20])

#define SYNCCOM_ENABLE_IGNORE_TIMEOUT _IO(SYNCCOM_IOCTL_MAGIC, 10)
#define SYNCCOM_DISABLE_IGNORE_TIMEOUT _IO(SYNCCOM_IOCTL_MAGIC, 11)
#define SYNCCOM_GET_IGNORE_TIMEOUT _IOR(SYNCCOM_IOCTL_MAGIC, 15, unsigned *)

#define SYNCCOM_SET_TX_MODIFIERS _IOW(SYNCCOM_IOCTL_MAGIC, 12, const int)
#define SYNCCOM_GET_TX_MODIFIERS _IOR(SYNCCOM_IOCTL_MAGIC, 14, unsigned *)

#define SYNCCOM_ENABLE_RX_MULTIPLE _IO(SYNCCOM_IOCTL_MAGIC, 16)
#define SYNCCOM_DISABLE_RX_MULTIPLE _IO(SYNCCOM_IOCTL_MAGIC, 17)
#define SYNCCOM_GET_RX_MULTIPLE _IOR(SYNCCOM_IOCTL_MAGIC, 18, unsigned *)

#define SYNCCOM_ENABLE_APPEND_TIMESTAMP _IO(SYNCCOM_IOCTL_MAGIC, 19)
#define SYNCCOM_DISABLE_APPEND_TIMESTAMP _IO(SYNCCOM_IOCTL_MAGIC, 20)
#define SYNCCOM_GET_APPEND_TIMESTAMP _IOR(SYNCCOM_IOCTL_MAGIC, 21, unsigned *)

#define SYNCCOM_SET_NONVOLATILE _IOW(SYNCCOM_IOCTL_MAGIC, 29, const unsigned)
#define SYNCCOM_GET_NONVOLATILE _IOR(SYNCCOM_IOCTL_MAGIC, 30, unsigned *)

#ifdef __cplusplus
}
#endif

#endif
