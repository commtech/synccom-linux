# TX Modifiers

| Modifiers | Value | Description |
| --------- | -----:| ----------- |
| `XF` | 0 | Normal transmit (disable modifiers) |
| `XREP` | 1 | Transmit frame repeatedly |
| `TXT` | 2 | Transmit frame on timer |
| `TXEXT` | 4 | Transmit frame on external signal |

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |

## Get
### IOCTL
```c
SYNCCOM_GET_TX_MODIFIERS
```

###### Examples
```
#include <synccom.h>
...

unsigned modifiers;

ioctl(fd, SYNCCOM_GET_TX_MODIFIERS, &modifiers);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/tx_modifiers
```

###### Examples
```
cat /sys/class/synccom/synccom0/settings/tx_modifiers
```


## Set
### IOCTL
```c
SYNCCOM_SET_TX_MODIFIERS
```

###### Examples
```
#include <synccom.h>
...

ioctl(fd, SYNCCOM_SET_TX_MODIFIERS, XF);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/tx_modifiers
```

###### Examples
```
echo 0 > /sys/class/synccom/synccom0/settings/tx_modifiers
```


### Additional Resources
- Complete example: [`examples/tx-modifiers.c`](../examples/tx-modifiers.c)
