#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;
    unsigned status = 0;

    fd = open("/dev/SYNCCOM0", O_RDWR);

    ioctl(fd, SYNCCOM_GET_RX_MULTIPLE, &status);

    ioctl(fd, SYNCCOM_ENABLE_RX_MULTIPLE);
    ioctl(fd, SYNCCOM_DISABLE_RX_MULTIPLE);

    close(fd);

    return 0;
}
