#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;
    unsigned modifiers;

    fd = open("/dev/synccom", O_RDWR);

    ioctl(fd, SYNCCOM_GET_TX_MODIFIERS, &modifiers);

    /* Enable transmit repeat & transmit on timer */
    ioctl(fd, SYNCCOM_SET_TX_MODIFIERS, TXT | XREP);

    /* Revert to normal transmission */
    ioctl(fd, SYNCCOM_SET_TX_MODIFIERS, XF);

    close(fd);

    return 0;
}

