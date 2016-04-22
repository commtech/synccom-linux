# Ignore Timeout

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Get
### IOCTL
```c
SYNCCOM_GET_IGNORE_TIMEOUT
```

###### Examples
```c
#include <synccom.h>
...

unsigned status;

ioctl(fd, SYNCCOM_GET_IGNORE_TIMEOUT, &status);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/ignore_timeout
```

###### Examples
```
cat /sys/class/synccom/synccom0/settings/ignore_timeout
```


## Enable
### IOCTL
```c
SYNCCOM_ENABLE_IGNORE_TIMEOUT
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_ENABLE_IGNORE_TIMEOUT);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/ignore_timeout
```

###### Examples
```
echo 1 > /sys/class/synccom/synccom0/settings/ignore_timeout
```


## Disable
### IOCTL
```c
SYNCCOM_DISABLE_IGNORE_TIMEOUT
```

###### Examples
```c
#include <synccom.h>
...

ioctl(fd, SYNCCOM_DISABLE_IGNORE_TIMEOUT);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/ignore_timeout
```

###### Examples
```
echo 0 > /sys/class/synccom/synccom0/settings/ignore_timeout
```


### Additional Resources
- Complete example: [`examples/ignore-timeout.c`](../examples/ignore-timeout.c)
