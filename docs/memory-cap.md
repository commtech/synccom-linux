# Memory Cap

If your system has limited memory available, there are safety checks in place to prevent spurious incoming data from overrunning your system. Each port has an option for setting it's input and output memory cap.


###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Structure
```c
struct synccom_memory_cap {
    int input;
    int output;
};
```


## Macros
```c
SYNCCOM_MEMORY_CAP_INIT(memcap)
```

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| `memcap` | `struct synccom_memory_cap *` | The memory cap structure to initialize |

The `SYNCCOM_MEMORY_CAP_INIT` macro should be called each time you use the `struct synccom_memory_cap` structure. An initialized structure will allow you to only set/receive the memory cap you need.


## Get
### IOCTL
```c
SYNCCOM_GET_MEMORY_CAP
```

###### Examples
```
#include <synccom.h>
...

struct synccom_memory_cap memcap;

SYNCCOM_MEMORY_CAP_INIT(memcap);

ioctl(fd, SYNCCOM_GET_MEMORY_CAP, &memcap);
```

At this point `memcap.input` and `memcap.output` would be set to their respective values.

### Sysfs
```
/sys/class/synccom/synccom*/settings/input_memory_cap
/sys/class/synccom/synccom*/settings/output_memory_cap
```

###### Examples
```
cat /sys/class/synccom/synccom0/settings/input_memory_cap
cat /sys/class/synccom/synccom0/settings/output_memory_cap
```


## Set
### IOCTL
```c
SYNCCOM_SET_MEMORY_CAP
```

###### Examples
```
#include <synccom.h>
...

struct synccom_memory_cap memcap;

SYNCCOM_MEMORY_CAP_INIT(memcap);

memcap.input = 1000000; /* 1 MB */
memcap.output = 2000000; /* 2 MB */

ioctl(fd, SYNCCOM_SET_MEMORY_CAP, &memcap);
```

### Sysfs
```
/sys/class/synccom/synccom*/settings/input_memory_cap
/sys/class/synccom/synccom*/settings/output_memory_cap
```

###### Examples
```
echo 1000000 > /sys/class/synccom/synccom0/settings/input_memory_cap
echo 2000000 > /sys/class/synccom/synccom0/settings/output_memory_cap
```


### Additional Resources
- Complete example: [`examples/memory-cap.c`](../examples/memory-cap.c)
