# Connect

###### Support
| Code | Version |
| ---- | ------- |
| synccom-linux | 1.0.0 |


## Connect
The Linux [`open`](http://linux.die.net/man/3/open) is used to connect to the port.

| Return Value | Value | Cause |
| ------------ | -----:| ----- |
| `ENOENT` | 2 (0x02) | Port not found |
| `EACCES` | 13 (0x0D) | Insufficient permissions |

###### Examples
Connect to port 0.
```c
#include <fcntl.h>
...

int fd;

fd = open("/dev/synccom0", O_RDWR);
```


### Additional Resources
- Complete example: [`examples/tutorial.c`](../examples/tutorial.c)
