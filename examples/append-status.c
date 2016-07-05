#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;
    unsigned status = 0;

    fd = open("/dev/SYNCCOM0", O_RDWR);

    ioctl(fd, SYNCCOM_GET_APPEND_STATUS, &status);

    ioctl(fd, SYNCCOM_ENABLE_APPEND_STATUS);
    ioctl(fd, SYNCCOM_DISABLE_APPEND_STATUS);

    close(fd);

    return 0;
}
