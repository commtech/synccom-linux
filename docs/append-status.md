# Append Status

It is a good idea to pay attention to the status of each frame. For example, you may want to see if the frame's CRC check succeeded or failed.

The Sync Com reports this data to you by appending two additional bytes to each frame you read from the card, if you opt-in to see this data. There are a few methods of enabling this additional data.

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Get
### IOCTL
```c
SYNCCOM_GET_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

unsigned status;

ioctl(fd, SYNCCOM_GET_APPEND_STATUS, &status);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/append_status
```

###### Examples
```
cat /sys/class/synccom/synccom0/settings/append_status
```


## Enable
### IOCTL
```c
SYNCCOM_ENABLE_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_ENABLE_APPEND_STATUS);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/append_status
```

###### Examples
```
echo 1 > /sys/class/synccom/synccom0/settings/append_status
```


## Disable
### IOCTL
```c
SYNCCOM_DISABLE_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_DISABLE_APPEND_STATUS);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/append_status
```

###### Examples
```
echo 0 > /sys/class/synccom/synccom0/settings/append_status
```


### Additional Resources
- Complete example: [`examples/append-status.c`](../examples/append-status.c)
