#include <fcntl.h> /* open, O_RDWR */
#include <unistd.h> /* close */
#include <synccom.h> /* SYNCCOM_* */

int main(void)
{
    int fd = 0;
    struct synccom_registers regs;

    fd = open("/dev/SYNCCOM0", O_RDWR);

    SYNCCOM_REGISTERS_INIT(regs);

    /* Change the CCR0 and BGR elements to our desired values */
    regs.CCR0 = 0x0011201c;
    regs.BGR = 0;

    /* Set the CCR0 and BGR register values */
    ioctl(fd, SYNCCOM_SET_REGISTERS, &regs);

    /* Re-initialize our registers structure */
    SYNCCOM_REGISTERS_INIT(regs);

    /* Mark the CCR0 and CCR1 elements to retrieve values */
    regs.CCR1 = SYNCCOM_UPDATE_VALUE;
    regs.CCR2 = SYNCCOM_UPDATE_VALUE;

    /* Get the CCR1 and CCR2 register values */
    ioctl(fd, SYNCCOM_GET_REGISTERS, &regs);

    close(fd);

    return 0;
}
