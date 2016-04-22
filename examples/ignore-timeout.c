#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;
    unsigned status = 0;

    fd = open("/dev/SYNCCOM0", O_RDWR);

    ioctl(fd, SYNCCOM_GET_IGNORE_TIMEOUT, &status);

    ioctl(fd, SYNCCOM_ENABLE_IGNORE_TIMEOUT);
    ioctl(fd, SYNCCOM_DISABLE_IGNORE_TIMEOUT);

    close(fd);

    return 0;
}
