#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

//#define BCM2708_PERI_BASE 0x20000000  // pi0-1
//#define BCM2708_PERI_BASE	0xFE000000  // pi4
#define BCM2708_PERI_BASE 0x3F000000 // pi3
#define BCM2708_PERI_SIZE 0x01000000

#define GPIO_ADDR 0x200000
#define GPCLK_ADDR 0x101000

#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define CTL_PASSWD (0x5a << 24)
#define CTL_BUSY (1 << 7)
#define CTL_KILL (1 << 5)
#define CTL_ENAB (1 << 4)

#define CTL_SRC_PLLC 5
#define CTL_SRC_PLLD 6

volatile unsigned int *gpio;
volatile unsigned int *gpclk;

static void create_dev_mem_mapping()
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        printf("Unable to open /dev/mem. Run as root using sudo?\n");
        exit(-1);
    }

    void *gpio_map = mmap(
        NULL,                   // Any adddress in our space will do
        BCM2708_PERI_SIZE,      // Map length
        PROT_READ | PROT_WRITE, // Enable reading & writting to mapped memory
        MAP_SHARED,             // Shared with other processes
        fd,                     // File to map
        BCM2708_PERI_BASE       // Offset to GPIO peripheral
    );

    close(fd);

    if (gpio_map == MAP_FAILED)
    {
        printf("mmap failed, errno = %d\n", errno);
        exit(-1);
    }

    gpio = ((volatile unsigned *)gpio_map) + GPIO_ADDR / 4;
    gpclk = ((volatile unsigned *)gpio_map) + GPCLK_ADDR / 4;
}

static void start_gpclk(int divisor)
{
    volatile unsigned int *ctl = gpclk + (CLK_GP0_CTL / 4);
    volatile unsigned int *div = gpclk + (CLK_GP0_DIV / 4);

    *ctl = CTL_PASSWD | CTL_KILL;
    usleep(10);
    while (*ctl & CTL_BUSY)
        ;
    usleep(100);
    *div = CTL_PASSWD | (divisor << 12);
    usleep(10);
    *ctl = CTL_PASSWD | CTL_ENAB | CTL_SRC_PLLC;
    usleep(10);
    while (!(*ctl & CTL_BUSY))
        ;
    usleep(100);

    // GPIO pin 4 takes alternate function 0 (GPCLK0).
    *(gpio + 0) = (*(gpio + 0) & ~0x00007000) | 0x00004000;
}

int main(int argc, char **argv)
{
    create_dev_mem_mapping();

    int requested_mhz = 90;

    if (argc > 1)
    {
        requested_mhz = atoi(argv[1]);
        if (requested_mhz < 10 || requested_mhz > 200)
        {
            printf("Requested clock frequency \"%d\" (MHz) is outside of range 10..200\n", requested_mhz);
            return 0;
        }
    }

    int divisor = (1200 + requested_mhz - 1) / requested_mhz;
    int actual_mhz = 1200 / divisor;
    if (actual_mhz == requested_mhz)
        printf("Starting GPCLK0 with frequency=%d, divisor=%d\n", actual_mhz, divisor);
    else
        printf("Starting GPCLK0 with frequency=%d (requested=%d), divisor=%d\n", actual_mhz, requested_mhz, divisor);

    start_gpclk(divisor);

    return 0;
}
