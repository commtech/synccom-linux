# Registers

The Sync Com driver is a Swiss army knife of sorts with communication. It can handle many different situations, if configured correctly. Typically to configure it to handle your specific situation you need to modify the card's register values.

_For a complete listing of all of the configuration options please see the manual._

In HDLC mode some settings are fixed at certain values. If you are in HDLC mode and after setting/getting your registers some bits don't look correct, then they are likely fixed. A complete list of the fixed values can be found in the `CCR0` section of the manual.

All of the registers, except `FCR`, are tied to a single port. `FCR` on the other hand is shared between two ports on a card. You can differentiate between which `FCR` settings affects what port by the A/B labels. A for port 0 and B for port 1.

_An [`SYNCCOM_PURGE_RX`](https://github.com/commtech/synccom-linux/blob/master/docs/purge.md) is required after changing the `MODE` bits in the `CCR0` register. If you need to change the `MODE` bits but don't have a clock present, change the `CM` bits to `0x7` temporarily. This will give you an internal clock to switch modes. You can then switch to your desired `CM` now that your `MODE` is locked in._

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Structure
```c
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
```


## Macros
```c
SYNCCOM_REGISTERS_INIT(regs)
```

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| `regs` | `struct synccom_registers *` | The registers structure to initialize |

The `SYNCCOM_REGISTERS_INIT` macro should be called each time you use the  `struct synccom_registers` structure. An initialized structure will allow you to only set/receive the registers you need.


## Set
### IOCTL
```c
SYNCCOM_SET_REGISTERS
```

###### Examples
```c
#include <synccom.h>
...

struct synccom_registers regs;

SYNCCOM_REGISTERS_INIT(regs);

regs.CCR0 = 0x0011201c;
regs.BGR = 10;

ioctl(fd, SYNCCOM_SET_REGISTERS, &regs);
```

### Sysfs
```
/sys/class/synccom/synccom*/registers/*
```

###### Examples
```
echo 0011201c > /sys/class/synccom/synccom0/registers/ccr0
```


## Get
### IOCTL
```c
SYNCCOM_GET_REGISTERS
```

###### Examples
```c
#include <synccom.h>
...

struct synccom_registers regs;

SYNCCOM_REGISTERS_INIT(regs);

regs.CCR0 = SYNCCOM_UPDATE_VALUE;
regs.BGR = SYNCCOM_UPDATE_VALUE;

ioctl(fd, SYNCCOM_GET_REGISTERS, &regs);
```

At this point `regs.CCR0` and `regs.BGR` would be set to their respective values.


### Sysfs
```
/sys/class/synccom/synccom*/registers/*
```

###### Examples
```
cat /sys/class/synccom/synccom0/registers/ccr0
```


### Additional Resources
- Complete example: [`examples/registers.c`](../examples/registers.c)
