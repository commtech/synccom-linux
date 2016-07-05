# RX Multiple

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Get
### IOCTL
```c
SYNCCOM_GET_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

unsigned status;

ioctl(fd, SYNCCOM_GET_RX_MULTIPLE, &status);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/rx_multiple
```

###### Examples
```
cat /sys/class/synccom/synccom0/settings/rx_multiple
```


## Enable
### IOCTL
```c
SYNCCOM_ENABLE_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_ENABLE_RX_MULTIPLE);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/rx_multiple
```

###### Examples
```
echo 1 > /sys/class/synccom/synccom0/settings/rx_multiple
```


## Disable
### IOCTL
```c
SYNCCOM_DISABLE_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_DISABLE_RX_MULTIPLE);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/rx_multiple
```

###### Examples
```
echo 0 > /sys/class/synccom/synccom0/settings/rx_multiple
```


### Additional Resources
- Complete example: [`examples/rx-multiple.c`](../examples/rx-multiple.c)
