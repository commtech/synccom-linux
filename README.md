# Sync Com-linux
This README file is best viewed [online](http://github.com/commtech/synccom-linux/).

## Installing Driver


##### Build Source Code
Run the make command from within the source code directory to build the driver.

```
cd synccom-linux/
make
```

If you would like to enable debug prints within the driver you need to add
the DEBUG option while building the driver.

```
make DEBUG=1
```

Once debugging is enabled you will find extra kernel prints in the
/var/log/messages and /var/log/debug log files.

If the kernel header files you would like to build against are not in the
default location `/lib/modules/$(shell uname -r)/build` then you can specify
the location with the KDIR option while building the driver.

```
make KDIR="/location/to/kernel_headers/"
```

##### Loading Driver
Assuming the driver has been successfully built in the previous step you are
now ready to load the driver so you can begin using it. To do this you insert
the driver's kernel object file (synccom.ko) into the kernel.

```
cd synccom-linux/
insmod synccom.ko
```

_You will more than likely need administrator privileges for this and
the following commands._


_All driver load time options can be set in your modprobe.conf file for
using upon system boot_


##### Installing Driver
If you would like the driver to automatically load at boot use the included
installer.

```
cd synccom-linux/
make install
```
This will also install the header (.h) files.

To uninstall, use the included uninstaller.

```
cd synccom-linux/
make uninstall
```


## Quick Start Guide
There is documentation for each specific function listed below, but lets get started
with a quick programming example for fun.

_This tutorial has already been set up for you at_
[`synccom-linux/examples/tutorial.c`](examples/tutorial.c).

Create a new C file (named tutorial.c) with the following code.

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int fd = 0;
    char odata[] = "Hello world!";
    char idata[20];

    /* Open port 0 */
    fd = open("/dev/synccom0", O_RDWR);

    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* Send "Hello world!" text */
    write(fd, odata, sizeof(odata));

    /* Read the data back in (with our loopback connector) */
    read(fd, idata, sizeof(idata));

    fprintf(stdout, "%s\n", idata);

    close(fd);

    return EXIT_SUCCESS;
}
```

For this example I will use the gcc compiler, but you can use your compiler of
choice.

```
# gcc -I ..\lib\raw\ tutorial.c
```

Now attach the included loopback connector.

```
# ./a.out
Hello world!
```

You have now transmitted and received an HDLC frame!


## API Reference

There are likely other configuration options you will need to set up for your
own program. All of these options are described on their respective documentation page.

- [Connect](docs/connect.md)
- [Append Status](docs/append-status.md)
- [Clock Frequency](docs/clock-frequency.md)
- [Memory Cap](docs/memory-cap.md)
- [Purge](docs/purge.md)
- [Read](docs/read.md)
- [Registers](docs/registers.md)
- [RX Multiple](docs/rx-multiple.md)
- [TX Modifiers](docs/tx-modifiers.md)
- [Write](docs/write.md)
- [Disconnect](docs/disconnect.md)


### FAQ


##### Why are the /dev/synccom* ports not created even though the driver has loaded?
There are a couple of possibilities but you should first check
/var/log/messages for any helpful information. If that doesn't help you
out then continue reading.

One possibility is that there is another driver loaded that has claimed
our cards. For example if your kernel is patched to use our card for
asynchronous transmission the linux serial driver has more than likely
claimed the card and in turn we won't receive the appropriate 'probe'
notifications to load our card.

Another possibility is that you have accidentally tried insmod'ing with
the 'hot_plug' option enabled and your cards are not actually present.
Double check that your card shows up in the output of 'lspci' and make
sure to use hot_plug=0.

##### What does poll() and select() base their information on?
Whether or not you can read data will be based on if there is at least 1
byte of data available to be read in your current mode of operation. For
example, if there is streaming data it will not be considered when in
a frame based mode.

Whether or not you can write data will be based on if you have hit your
output memory cap.

##### Why does executing a purge without a clock put the card in a broken state?
When executing a purge on either the transmitter or receiver there is
a TRES or RRES (command from the CMDR register) happening behind the
scenes. If there is no clock available the command will stall until
a clock is available. This should work in theory but doesn't in
practice. So whenever you need to execute a purge without a clock, first
put it into clock mode 7, execute your purge then return to your other
clock mode.


## Build Dependencies
- Kernel Build Tools (GCC, make, kernel headers, etc)


## Run-time Dependencies
- OS: Linux
- Base Installation: >= 2.6.16 (might work with a lower version)
- Sysfs Support: >= 2.6.25


## API Compatibility
We follow [Semantic Versioning](http://semver.org/) when creating releases.


## License

Copyright (C) 2016 [Commtech, Inc.](http://commtech-fastcom.com)

Licensed under the [GNU General Public License v3](http://www.gnu.org/licenses/gpl.txt).
