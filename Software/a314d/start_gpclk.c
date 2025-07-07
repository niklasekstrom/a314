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

// The following three functions are borrowed from:
// https://github.com/raspberrypi/userland/blob/master/host_applications/linux/libs/bcm_host/bcm_host.c

static unsigned get_dt_ranges(const char *filename, unsigned offset)
{
    unsigned address = ~0;
    FILE *fp = fopen(filename, "rb");
    if (fp)
    {
        unsigned char buf[4];
        fseek(fp, offset, SEEK_SET);
        if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
            address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
        fclose(fp);
    }
    return address;
}

static unsigned bcm_host_get_peripheral_address(void)
{
    unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
    if (address == 0)
        address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
    return address == ~0 ? 0x20000000 : address;
}

static unsigned bcm_host_get_peripheral_size(void)
{
    unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
    address = get_dt_ranges("/proc/device-tree/soc/ranges", (address == 0) ? 12 : 8);
    return address == ~0 ? 0x01000000 : address;
}

static void create_dev_mem_mapping(size_t base, size_t size)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        printf("Unable to open /dev/mem. Run as root using sudo?\n");
        exit(-1);
    }

    void *gpio_map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);

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
    size_t base = bcm_host_get_peripheral_address();
    size_t size = bcm_host_get_peripheral_size();

    create_dev_mem_mapping(base, size);

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
