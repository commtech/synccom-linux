#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */
#include "calculate-clock-bits.h"

int main(void)
{
    int fd = 0;
    unsigned char clock_bits[20];

    fd = open("/dev/synccom0", O_RDWR);

    /* 18.432 MHz */
    calculate_clock_bits(18432000, 10, clock_bits);

    ioctl(fd, SYNCCOM_SET_CLOCK_BITS, &clock_bits);

    close(fd);

    return 0;
}
