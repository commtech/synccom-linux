#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;

    fd = open("/dev/synccom0", O_RDWR);

    ioctl(fd, SYNCCOM_PURGE_TX);
    ioctl(fd, SYNCCOM_PURGE_RX);

    close(fd);

    return 0;
}
