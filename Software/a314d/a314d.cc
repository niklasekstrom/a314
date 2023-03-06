/*
 * Copyright (c) 2018 Niklas Ekström
 */

#include <arpa/inet.h>

#include <linux/spi/spidev.h>
#include <linux/types.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#if !(defined(MODEL_TD) ^ defined(MODEL_CP))
#error Need to define MODEL_TD xor MODEL_CP
#endif

#define LOGGER_TRACE 0
#define logger_trace(...) do { if (LOGGER_TRACE) fprintf(stdout, __VA_ARGS__); } while (0)

#define LOGGER_DEBUG 0
#define logger_debug(...) do { if (LOGGER_DEBUG) fprintf(stdout, __VA_ARGS__); } while (0)

#define LOGGER_INFO 1
#define logger_info(...) do { if (LOGGER_INFO) fprintf(stdout, __VA_ARGS__); } while (0)

#define LOGGER_WARN 1
#define logger_warn(...) do { if (LOGGER_WARN) fprintf(stdout, __VA_ARGS__); } while (0)

#define LOGGER_ERROR 1
#define logger_error(...) do { if (LOGGER_ERROR) fprintf(stderr, __VA_ARGS__); } while (0)

// SPI commands.
#define READ_SRAM_CMD           0
#define WRITE_SRAM_CMD          1
#define READ_CMEM_CMD           2
#define WRITE_CMEM_CMD          3
#define SPI_PROTO_VER_CMD       255

#define READ_SRAM_HDR_LEN       4

// Addresses to variables in CMEM.
#define BASE_ADDRESS_LEN        6
#define R_EVENTS_ADDRESS        12
#define R_ENABLE_ADDRESS        13
#define A_EVENTS_ADDRESS        14
#define A_ENABLE_ADDRESS        15

// Events that are communicated via IRQ from Amiga to Raspberry.
#define R_EVENT_A2R_TAIL        1
#define R_EVENT_R2A_HEAD        2
#define R_EVENT_BASE_ADDRESS    4

// Events that are communicated from Raspberry to Amiga.
#define A_EVENT_R2A_TAIL        1
#define A_EVENT_A2R_HEAD        2

// TODO: These constants should be the same for both TR and CP.
// Need to update a314.device in order to change these.
#if defined(MODEL_TD)

// Offset relative to communication area for queue pointers.
#define A2R_TAIL_OFFSET         0
#define R2A_HEAD_OFFSET         1
#define R2A_TAIL_OFFSET         2
#define A2R_HEAD_OFFSET         3

// Addresses of fixed data structures in shared memory.
#define CAP_BASE                0
#define A2R_BASE                4
#define R2A_BASE                260

#elif defined(MODEL_CP)

// Offset relative to communication area for queue pointers.
#define R2A_TAIL_OFFSET         0
#define A2R_HEAD_OFFSET         1
#define A2R_TAIL_OFFSET         2
#define R2A_HEAD_OFFSET         3

// Addresses of fixed data structures in shared memory.
#define A2R_BASE                0
#define R2A_BASE                256
#define CAP_BASE                512

#endif

// Packets that are communicated across physical channels (A2R and R2A).
#define PKT_CONNECT             4
#define PKT_CONNECT_RESPONSE    5
#define PKT_DATA                6
#define PKT_EOS                 7
#define PKT_RESET               8

// Valid responses for PKT_CONNECT_RESPONSE.
#define CONNECT_OK              0
#define CONNECT_UNKNOWN_SERVICE 3

// Messages that are communicated between driver and client.
#define MSG_REGISTER_REQ        1
#define MSG_REGISTER_RES        2
#define MSG_DEREGISTER_REQ      3
#define MSG_DEREGISTER_RES      4
#define MSG_READ_MEM_REQ        5
#define MSG_READ_MEM_RES        6
#define MSG_WRITE_MEM_REQ       7
#define MSG_WRITE_MEM_RES       8
#define MSG_CONNECT             9
#define MSG_CONNECT_RESPONSE    10
#define MSG_DATA                11
#define MSG_EOS                 12
#define MSG_RESET               13

#define MSG_SUCCESS             1
#define MSG_FAIL                0

#if defined(MODEL_TD)
#define IRQ_GPIO                "25"
#define IRQ_GPIO_EDGE           "both"
#elif defined(MODEL_CP)
#define IRQ_GPIO                "2"
#define IRQ_GPIO_EDGE           "rising"
#endif

#define PIN_IRQ                 2
#define PIN_CLK                 4
#define PIN_REQ                 14
#define PIN_ACK                 15
#define PIN_D(x)                (16 + x)
#define PIN_A(x)                (24 + x)
#define PIN_WR                  27

#define PINS_OUT            ((0xff << PIN_D(0)) | (3 << PIN_A(0)) | (1 << PIN_WR) | (1 << PIN_REQ))
#define PINS_OUT_NOT_WR     ((0xff << PIN_D(0)) | (3 << PIN_A(0)) | (1 << PIN_REQ))

#define GPIO_DIR_0              0x08004000
#define GPIO_DIR_1_DIN          0x00001009
#define GPIO_DIR_1_DOUT         0x09241009
#define GPIO_DIR_2_DIN          0x08209000
#define GPIO_DIR_2_DOUT         0x08209249

#define GPIO_PULL_NONE          0
#define GPIO_PULL_DOWN          1
#define GPIO_PULL_UP            2

#define REG_SRAM                0
#define REG_IRQ                 1
#define REG_ADDR_LO             2
#define REG_ADDR_HI             3

#define REG_IRQ_SET             0x80
#define REG_IRQ_CLR             0x00
#define REG_IRQ_PI              0x02
#define REG_IRQ_CP              0x01

// Global variables.
static sigset_t original_sigset;

static uint8_t mode = SPI_CS_HIGH;
static uint8_t bits = 8;
static uint32_t speed = 67000000;

static int spi_fd = -1;
static int spi_proto_ver = 0;

static uint8_t tx_buf[65536];
static uint8_t rx_buf[65536];

static volatile unsigned int *gpio;

static unsigned short current_address;
static int current_dir = 0; // 0 = input, 1 = output.

static bool read_a314_magic = false;
static int restart_counter;

static bool gpio_irq_exported = false;
static bool gpio_irq_edge_set = false;
static int gpio_irq_fd = -1;

static bool auto_clear_cp_irq = false;
static time_t auto_clear_cp_irq_after;

static int server_socket = -1;

static int epfd = -1;

static bool have_base_address = false;
static unsigned int base_address = 0;

#if defined(MODEL_TD)
#define BASE_ADDRESS base_address
#elif defined(MODEL_CP)
#define BASE_ADDRESS 0
#endif

static uint8_t channel_status[4];
static uint8_t channel_status_updated = 0;

static uint8_t recv_buf[256];
static uint8_t send_buf[256];

struct LogicalChannel;
struct ClientConnection;

#pragma pack(push, 1)
struct MessageHeader
{
    uint32_t length;
    uint32_t stream_id;
    uint8_t type;
}; //} __attribute__((packed));
#pragma pack(pop)

struct MessageBuffer
{
    int pos;
    std::vector<uint8_t> data;
};

struct RegisteredService
{
    std::string name;
    ClientConnection *cc;
};

struct PacketBuffer
{
    int type;
    std::vector<uint8_t> data;
};

struct ClientConnection
{
    int fd;

    int next_stream_id;

    int bytes_read;
    MessageHeader header;
    std::vector<uint8_t> payload;

    std::list<MessageBuffer> message_queue;

    std::list<LogicalChannel*> associations;
};

struct LogicalChannel
{
    int channel_id;

    ClientConnection *association;
    int stream_id;

    bool got_eos_from_ami;
    bool got_eos_from_client;

    std::list<PacketBuffer> packet_queue;
};

static void remove_association(LogicalChannel *ch);
static void clear_packet_queue(LogicalChannel *ch);
static void create_and_enqueue_packet(LogicalChannel *ch, uint8_t type, uint8_t *data, uint8_t length);

static std::list<ClientConnection> connections;
static std::list<RegisteredService> services;
static std::list<LogicalChannel> channels;
static std::list<LogicalChannel*> send_queue;

struct OnDemandStart
{
    std::string service_name;
    std::string program;
    std::vector<std::string> arguments;
};

std::vector<OnDemandStart> on_demand_services;

static void load_config_file(const char *filename)
{
    FILE *f = fopen(filename, "rt");
    if (f == nullptr)
        return;

    char line[256];
    std::vector<char *> parts;

    while (fgets(line, 256, f) != nullptr)
    {
        char org_line[256];
        strcpy(org_line, line);

        bool in_quotes = false;

        int start = 0;
        for (int i = 0; i < 256; i++)
        {
            if (line[i] == 0)
            {
                if (start < i)
                    parts.push_back(&line[start]);
                break;
            }
            else if (line[i] == '"')
            {
                line[i] = 0;
                if (in_quotes)
                    parts.push_back(&line[start]);
                in_quotes = !in_quotes;
                start = i + 1;
            }
            else if (isspace(line[i]) && !in_quotes)
            {
                line[i] = 0;
                if (start < i)
                    parts.push_back(&line[start]);
                start = i + 1;
            }
        }

        if (parts.size() >= 2)
        {
            on_demand_services.emplace_back();
            auto &e = on_demand_services.back();
            e.service_name = parts[0];
            e.program = parts[1];
            for (int i = 1; i < parts.size(); i++)
                e.arguments.push_back(std::string(parts[i]));
        }
        else if (parts.size() != 0)
            logger_warn("Invalid number of columns in configuration file line: %s\n", org_line);

        parts.clear();
    }

    fclose(f);

    if (on_demand_services.empty())
        logger_warn("No registered services\n");
}

static int init_spi()
{
    spi_fd = open("/dev/spidev0.0", O_RDWR | O_CLOEXEC);
    if (spi_fd < 0)
        return -1;

    int ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ret |= ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ret |= ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret != 0)
        return ret;

    return 0;
}

static void shutdown_spi()
{
    if (spi_fd != -1)
        close(spi_fd);
}

static int spi_transfer(int len)
{
    struct spi_ioc_transfer tr =
    {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = (uintptr_t)rx_buf,
        .len = (uint32_t)len,
        .speed_hz = speed,
        .delay_usecs = 0,
        .bits_per_word = bits,
        .cs_change = 0,
    };

    return ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static int spi_protocol_version()
{
    tx_buf[0] = (uint8_t)SPI_PROTO_VER_CMD;
    tx_buf[1] = 0;
    spi_transfer(2);
    logger_trace("SPI protocol version = %d\n", rx_buf[1]);
    return (int)rx_buf[1];
}

static void spi_read_shm_rxbuf(unsigned int address, unsigned int length)
{
    logger_trace("SPI read mem address = %d length = %d\n", address, length);

    unsigned int header;
    if (spi_proto_ver == 1)
        header = (READ_SRAM_CMD << 21) | (address & 0x1fffff);
    else
        header = (READ_SRAM_CMD << 20) | (address & 0xfffff);

    tx_buf[0] = (uint8_t)((header >> 16) & 0xff);
    tx_buf[1] = (uint8_t)((header >> 8) & 0xff);
    tx_buf[2] = (uint8_t)(header & 0xff);
	tx_buf[3] = 0;

    spi_transfer(length + 4);
}

static void spi_read_shm(unsigned char *data, unsigned int address, unsigned int length)
{
    spi_read_shm_rxbuf(address, length);
    memcpy(data, &rx_buf[READ_SRAM_HDR_LEN], length);
}

static void spi_write_shm(unsigned int address, uint8_t *buf, unsigned int length)
{
    logger_trace("SPI write mem address = %d length = %d\n", address, length);

    unsigned int header;
    if (spi_proto_ver == 1)
        header = (WRITE_SRAM_CMD << 21) | (address & 0x1fffff);
    else
        header = (WRITE_SRAM_CMD << 20) | (address & 0xfffff);

    tx_buf[0] = (uint8_t)((header >> 16) & 0xff);
    tx_buf[1] = (uint8_t)((header >> 8) & 0xff);
    tx_buf[2] = (uint8_t)(header & 0xff);

    memcpy(&tx_buf[3], buf, length);
    spi_transfer(length + 3);
}

static uint8_t spi_read_cmem(unsigned int address)
{
    if (spi_proto_ver == 1)
        tx_buf[0] = (uint8_t)((READ_CMEM_CMD << 5) | (address & 0xf));
    else
        tx_buf[0] = (uint8_t)((READ_CMEM_CMD << 4) | (address & 0xf));
    tx_buf[1] = 0;
    spi_transfer(2);
    logger_trace("SPI read cmem, address = %d, returned = %d\n", address, rx_buf[1]);
    return rx_buf[1];
}

static void spi_write_cmem(unsigned int address, unsigned int data)
{
    logger_trace("SPI write cmem, address = %d, data = %d\n", address, data);

    if (spi_proto_ver == 1)
        tx_buf[0] = (uint8_t)((WRITE_CMEM_CMD << 5) | (address & 0xf));
    else
        tx_buf[0] = (uint8_t)((WRITE_CMEM_CMD << 4) | (address & 0xf));
    tx_buf[1] = (uint8_t)(data & 0xf);
    spi_transfer(2);
}

static uint8_t spi_ack_irq()
{
    logger_trace("SPI ack_irq\n");
    return spi_read_cmem(R_EVENTS_ADDRESS);
}

static int create_dev_gpiomem_mapping()
{
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        logger_error("Unable to open /dev/gpiomem\n");
        return -1;
    }

    void *gpio_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    if (gpio_map == MAP_FAILED)
    {
        logger_error("mmap failed, errno = %d\n", errno);
        return -1;
    }

    gpio = (volatile unsigned int *)gpio_map;
    return 0;
}

static void gpio_write_reg(unsigned int reg, unsigned int value)
{
    if (current_dir == 0)
    {
        *(gpio + 7) = (1 << PIN_WR);
        *(gpio + 1) = GPIO_DIR_1_DOUT;
        *(gpio + 2) = GPIO_DIR_2_DOUT;
        current_dir = 1;
    }

    while ((*(gpio + 13) & (1 << PIN_ACK)))
        ;

    *(gpio + 7) = ((value & 0xff) << PIN_D(0)) | (reg << PIN_A(0)) | (1 << PIN_REQ);

    while (!(*(gpio + 13) & (1 << PIN_ACK)))
        ;

    *(gpio + 10) = PINS_OUT_NOT_WR;
}

static unsigned int gpio_read_reg(unsigned int reg)
{
    if (current_dir == 1)
    {
        *(gpio + 1) = GPIO_DIR_1_DIN;
        *(gpio + 2) = GPIO_DIR_2_DIN;
        *(gpio + 10) = (1 << PIN_WR);
        current_dir = 0;
    }

    while ((*(gpio + 13) & (1 << PIN_ACK)))
        ;

    *(gpio + 7) = (reg << PIN_A(0)) | (1 << PIN_REQ);

    unsigned int value;
    while (!((value = *(gpio + 13)) & (1 << PIN_ACK)))
        ;

    value = (value >> PIN_D(0)) & 0xff;

    *(gpio + 10) = PINS_OUT_NOT_WR;

    return value;
}

static void clear_pi_irq()
{
    gpio_write_reg(REG_IRQ, REG_IRQ_CLR | REG_IRQ_PI);
}

static void set_cp_irq()
{
    gpio_write_reg(REG_IRQ, REG_IRQ_SET | REG_IRQ_CP);
}

static void clear_cp_irq()
{
    gpio_write_reg(REG_IRQ, REG_IRQ_CLR | REG_IRQ_CP);
}

static void gpio_set_address(unsigned short address)
{
    if ((address & 0xff) != (current_address & 0xff))
        gpio_write_reg(REG_ADDR_LO, address & 0xff);

    if (((address >> 8) & 0xff) != ((current_address >> 8) & 0xff))
        gpio_write_reg(REG_ADDR_HI, (address >> 8) & 0xff);

    current_address = address;
}

static void gpio_write_shm(unsigned short address, unsigned char *data, unsigned short length)
{
    gpio_set_address(address);

    for (int i = 0; i < length; i++)
        gpio_write_reg(REG_SRAM, *data++);

    current_address += length;
}

static void gpio_read_shm(unsigned char *data, unsigned short address, unsigned short length)
{
    gpio_set_address(address);

    for (int i = 0; i < length; i++)
        *data++ = gpio_read_reg(REG_SRAM);

    current_address += length;
}

static void set_gpio_pull_mode(int mask, int mode)
{
    *(gpio + 37) = mode;
    usleep(50);
    *(gpio + 38) = mask;
    usleep(50);
    *(gpio + 38) = 0;
    *(gpio + 37) = 0;
    usleep(50);
}

static int init_gpio()
{
    if (create_dev_gpiomem_mapping())
        return -1;

    // Set pin directions.
    // Inputs: PIN_IRQ, PIN_ACK, PIN_D(x), unused pins.
    // Outputs: PIN_REQ, PIN_A(x), PIN_WR, pin 29 (connects to LED on Pi3).
    // Alt0: PIN_CLK.
    *(gpio + 0) = GPIO_DIR_0;
    *(gpio + 1) = GPIO_DIR_1_DIN;
    *(gpio + 2) = GPIO_DIR_2_DIN;

    set_gpio_pull_mode((1 << PIN_ACK) | (1 << PIN_IRQ), GPIO_PULL_DOWN);
    set_gpio_pull_mode(PINS_OUT | (1 << PIN_CLK), GPIO_PULL_NONE);

    current_dir = 0;

    // Set address pointer.
    gpio_write_reg(REG_ADDR_LO, 0);
    gpio_write_reg(REG_ADDR_HI, 0);

    current_address = 0;

    return 0;
}

static void shutdown_gpio()
{
    // Let Linux take care of this.
}

#if defined(MODEL_TD)
#define read_shm spi_read_shm
#define write_shm spi_write_shm
#elif defined(MODEL_CP)
#define write_shm gpio_write_shm
#define read_shm gpio_read_shm
#endif

static int open_write_close(const char *filename, const char *text)
{
    int fd = open(filename, O_WRONLY);
    if (fd == -1)
        return -1;

    write(fd, text, strlen(text));
    close(fd);
    return 0;
}

static void sleep_100ms()
{
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 100000000L;
    nanosleep(&delay, NULL);
}

static void set_direction()
{
    for (int retry = 0; retry < 100; retry++)
    {
        int fd = open("/sys/class/gpio/gpio" IRQ_GPIO "/direction", O_WRONLY);
        if (fd != -1)
        {
            write(fd, "in", 2);
            close(fd);
            break;
        }
        sleep_100ms();
    }
}

static int init_gpio_irq()
{
    if (open_write_close("/sys/class/gpio/export", IRQ_GPIO) != 0)
        return -1;

    gpio_irq_exported = true;

    set_direction();

    if (open_write_close("/sys/class/gpio/gpio" IRQ_GPIO "/edge", IRQ_GPIO_EDGE) == -1)
        return -1;

    gpio_irq_edge_set = true;

    gpio_irq_fd = open("/sys/class/gpio/gpio" IRQ_GPIO "/value", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (gpio_irq_fd == -1)
        return -1;

    return 0;
}

static void shutdown_gpio_irq()
{
    if (gpio_irq_fd != -1)
        close(gpio_irq_fd);

    if (gpio_irq_edge_set)
        open_write_close("/sys/class/gpio/gpio" IRQ_GPIO "/edge", "none");

    if (gpio_irq_exported)
        open_write_close("/sys/class/gpio/unexport", IRQ_GPIO);
}

static int init_server_socket()
{
    server_socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_socket == -1)
    {
        logger_error("Failed to create server socket\n");
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(7110);

    int res = bind(server_socket, (struct sockaddr *)&address, sizeof(address));
    if (res < 0)
    {
        logger_error("Bind to localhost:7110 failed\n");
        return -1;
    }

    listen(server_socket, 16);

    return 0;
}

static void shutdown_server_socket()
{
    if (server_socket != -1)
        close(server_socket);
    server_socket = -1;
}

static void sigterm_handler(int signo)
{
}

static void init_sigterm()
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGTERM);
    sigprocmask(SIG_BLOCK, &ss, &original_sigset);

    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
}

static int init_driver()
{
    init_sigterm();

    if (init_server_socket() != 0)
        return -1;

#if defined(MODEL_TD)
    if (init_spi() != 0)
        return -1;

    spi_proto_ver = spi_protocol_version();
#elif defined(MODEL_CP)
    if (init_gpio() != 0)
        return -1;
#endif

    if (init_gpio_irq() != 0)
        return -1;

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1)
        return -1;

    struct epoll_event ev;
    ev.events = EPOLLPRI | EPOLLERR;
    ev.data.fd = gpio_irq_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, gpio_irq_fd, &ev) != 0)
        return -1;

    ev.events = EPOLLIN;
    ev.data.fd = server_socket;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev) != 0)
        return -1;

    return 0;
}

static void shutdown_driver()
{
    if (epfd != -1)
        close(epfd);

    shutdown_gpio_irq();
#if defined(MODEL_TD)
    shutdown_spi();
#elif defined(MODEL_CP)
    shutdown_gpio();
#endif
    shutdown_server_socket();
}

void create_and_send_msg(ClientConnection *cc, int type, int stream_id, uint8_t *data, int length)
{
    MessageBuffer mb;
    mb.pos = 0;
    mb.data.resize(sizeof(MessageHeader) + length);

    MessageHeader *mh = (MessageHeader *)&mb.data[0];
    mh->length = length;
    mh->stream_id = stream_id;
    mh->type = type;
    if (length && data)
        memcpy(&mb.data[sizeof(MessageHeader)], data, length);

    if (!cc->message_queue.empty())
    {
        cc->message_queue.push_back(std::move(mb));
        return;
    }

    while (1)
    {
        int left = mb.data.size() - mb.pos;
        uint8_t *src = &mb.data[mb.pos];
        ssize_t r = write(cc->fd, src, left);
        if (r == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                cc->message_queue.push_back(std::move(mb));
                return;
            }
            else if (errno == ECONNRESET)
            {
                // Do not close connection here; it will get done at some other place.
                return;
            }
            else
            {
                logger_error("Write failed unexpectedly with errno = %d\n", errno);
                exit(-1);
            }
        }

        mb.pos += r;
        if (r == left)
        {
            return;
        }
    }
}

static void handle_msg_register_req(ClientConnection *cc)
{
    uint8_t result = MSG_FAIL;

    std::string service_name((char *)&cc->payload[0], cc->payload.size());

    auto it = services.begin();
    for (; it != services.end(); it++)
        if (it->name == service_name)
            break;

    if (it == services.end())
    {
        services.emplace_back();

        RegisteredService &srv = services.back();
        srv.cc = cc;
        srv.name = std::move(service_name);

        result = MSG_SUCCESS;
    }

    create_and_send_msg(cc, MSG_REGISTER_RES, 0, &result, 1);
}

static void handle_msg_deregister_req(ClientConnection *cc)
{
    uint8_t result = MSG_FAIL;

    std::string service_name((char *)&cc->payload[0], cc->payload.size());

    for (auto it = services.begin(); it != services.end(); it++)
    {
        if (it->name == service_name && it->cc == cc)
        {
            services.erase(it);
            result = MSG_SUCCESS;
            break;
        }
    }

    create_and_send_msg(cc, MSG_DEREGISTER_RES, 0, &result, 1);
}

static void handle_msg_read_mem_req(ClientConnection *cc)
{
    uint32_t address = *(uint32_t *)&(cc->payload[0]);
    uint32_t length = *(uint32_t *)&(cc->payload[4]);

#if defined(MODEL_TD)
    // This is an optimization to save a memcpy.
    spi_read_shm_rxbuf(address, length);
    create_and_send_msg(cc, MSG_READ_MEM_RES, 0, &rx_buf[READ_SRAM_HDR_LEN], length);
#else
    read_shm(rx_buf, address, length);
    create_and_send_msg(cc, MSG_READ_MEM_RES, 0, rx_buf, length);
#endif
}

static void handle_msg_write_mem_req(ClientConnection *cc)
{
    uint32_t address = *(uint32_t *)&(cc->payload[0]);
    uint32_t length = cc->payload.size() - 4;

    write_shm(address, &(cc->payload[4]), length);

    create_and_send_msg(cc, MSG_WRITE_MEM_RES, 0, nullptr, 0);
}

static LogicalChannel *get_associated_channel_by_stream_id(ClientConnection *cc, int stream_id)
{
    for (auto ch : cc->associations)
    {
        if (ch->stream_id == stream_id)
            return ch;
    }
    return nullptr;
}

static void handle_msg_connect(ClientConnection *cc)
{
    // We currently don't handle that a client tries to connect to a service on the Amiga.
}

static void handle_msg_connect_response(ClientConnection *cc)
{
    LogicalChannel *ch = get_associated_channel_by_stream_id(cc, cc->header.stream_id);
    if (!ch)
        return;

    create_and_enqueue_packet(ch, PKT_CONNECT_RESPONSE, &cc->payload[0], cc->payload.size());

    if (cc->payload[0] != CONNECT_OK)
        remove_association(ch);
}

static void handle_msg_data(ClientConnection *cc)
{
    LogicalChannel *ch = get_associated_channel_by_stream_id(cc, cc->header.stream_id);
    if (!ch)
        return;

    create_and_enqueue_packet(ch, PKT_DATA, &cc->payload[0], cc->header.length);
}

static void handle_msg_eos(ClientConnection *cc)
{
    LogicalChannel *ch = get_associated_channel_by_stream_id(cc, cc->header.stream_id);
    if (!ch || ch->got_eos_from_client)
        return;

    ch->got_eos_from_client = true;

    create_and_enqueue_packet(ch, PKT_EOS, nullptr, 0);

    if (ch->got_eos_from_ami)
        remove_association(ch);
}

static void handle_msg_reset(ClientConnection *cc)
{
    LogicalChannel *ch = get_associated_channel_by_stream_id(cc, cc->header.stream_id);
    if (!ch)
        return;

    remove_association(ch);

    clear_packet_queue(ch);
    create_and_enqueue_packet(ch, PKT_RESET, nullptr, 0);
}

static void handle_received_message(ClientConnection *cc)
{
    switch (cc->header.type)
    {
    case MSG_REGISTER_REQ:
        handle_msg_register_req(cc);
        break;
    case MSG_DEREGISTER_REQ:
        handle_msg_deregister_req(cc);
        break;
    case MSG_READ_MEM_REQ:
        handle_msg_read_mem_req(cc);
        break;
    case MSG_WRITE_MEM_REQ:
        handle_msg_write_mem_req(cc);
        break;
    case MSG_CONNECT:
        handle_msg_connect(cc);
        break;
    case MSG_CONNECT_RESPONSE:
        handle_msg_connect_response(cc);
        break;
    case MSG_DATA:
        handle_msg_data(cc);
        break;
    case MSG_EOS:
        handle_msg_eos(cc);
        break;
    case MSG_RESET:
        handle_msg_reset(cc);
        break;
    default:
        // This is bad, probably should disconnect from client.
        logger_warn("Received a message of unknown type from client\n");
        break;
    }
}

static void close_and_remove_connection(ClientConnection *cc)
{
    shutdown(cc->fd, SHUT_WR);
    close(cc->fd);

    {
        auto it = services.begin();
        while (it != services.end())
        {
            if (it->cc == cc)
                it = services.erase(it);
            else
                it++;
        }
    }

    {
        auto it = cc->associations.begin();
        while (it != cc->associations.end())
        {
            auto ch = *it;

            clear_packet_queue(ch);
            create_and_enqueue_packet(ch, PKT_RESET, nullptr, 0);

            ch->association = nullptr;
            ch->stream_id = 0;

            it = cc->associations.erase(it);
        }
    }

    for (auto it = connections.begin(); it != connections.end(); it++)
    {
        if (&(*it) == cc)
        {
            connections.erase(it);
            break;
        }
    }
}

static void remove_association(LogicalChannel *ch)
{
    auto &ass = ch->association->associations;
    ass.erase(std::find(ass.begin(), ass.end(), ch));

    ch->association = nullptr;
    ch->stream_id = 0;
}

static void clear_packet_queue(LogicalChannel *ch)
{
    if (!ch->packet_queue.empty())
    {
        ch->packet_queue.clear();
        send_queue.erase(std::find(send_queue.begin(), send_queue.end(), ch));
    }
}

static void create_and_enqueue_packet(LogicalChannel *ch, uint8_t type, uint8_t *data, uint8_t length)
{
    if (ch->packet_queue.empty())
        send_queue.push_back(ch);

    ch->packet_queue.emplace_back();

    PacketBuffer &pb = ch->packet_queue.back();
    pb.type = type;
    pb.data.resize(length);
    if (data && length)
        memcpy(&pb.data[0], data, length);
}

static void handle_pkt_connect(int channel_id, uint8_t *data, int plen)
{
    for (auto &ch : channels)
    {
        if (ch.channel_id == channel_id)
        {
            // We should handle this in some constructive way.
            // This signals that should reset all logical channels.
            logger_error("Received a CONNECT packet on a channel that was believed to be previously allocated\n");
            exit(-1);
        }
    }

    channels.emplace_back();

    auto &ch = channels.back();

    ch.channel_id = channel_id;
    ch.association = nullptr;
    ch.stream_id = 0;
    ch.got_eos_from_ami = false;
    ch.got_eos_from_client = false;

    std::string service_name((char *)data, plen);

    for (auto &srv : services)
    {
        if (srv.name == service_name)
        {
            ClientConnection *cc = srv.cc;

            ch.association = cc;
            ch.stream_id = cc->next_stream_id;

            cc->next_stream_id += 2;
            cc->associations.push_back(&ch);

            create_and_send_msg(ch.association, MSG_CONNECT, ch.stream_id, data, plen);
            return;
        }
    }

    for (auto &on_demand : on_demand_services)
    {
        if (on_demand.service_name == service_name)
        {
            int fds[2];
            int status = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
            if (status != 0)
            {
                logger_error("Unexpectedly not able to create socket pair.\n");
                exit(-1);
            }

            pid_t child = fork();
            if (child == -1)
            {
                logger_error("Unexpectedly was not able to fork.\n");
                exit(-1);
            }
            else if (child == 0)
            {
                close(fds[0]);
                int fd = fds[1];

                std::vector<std::string> args(on_demand.arguments);
                args.push_back("-ondemand");
                args.push_back(std::to_string(fd));
                std::vector<const char *> args_arr;
                for (auto &arg : args)
                    args_arr.push_back(arg.c_str());
                args_arr.push_back(nullptr);

                execvp(on_demand.program.c_str(), (char* const*) &args_arr[0]);
            }
            else
            {
                close(fds[1]);
                int fd = fds[0];

                int status = fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
                if (status == -1)
                {
                    logger_error("Unexpectedly unable to set close-on-exec flag on client socket descriptor; errno = %d\n", errno);
                    exit(-1);
                }

                status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
                if (status == -1)
                {
                    logger_error("Unexpectedly unable to set client socket to non blocking; errno = %d\n", errno);
                    exit(-1);
                }

                int flag = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

                connections.emplace_back();

                ClientConnection &cc = connections.back();
                cc.fd = fd;
                cc.next_stream_id = 1;
                cc.bytes_read = 0;

                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
                {
                    logger_error("epoll_ctl() failed unexpectedly with errno = %d\n", errno);
                    exit(-1);
                }

                services.emplace_back();

                RegisteredService &srv = services.back();
                srv.cc = &cc;
                srv.name = std::move(service_name);

                ch.association = &cc;
                ch.stream_id = cc.next_stream_id;

                cc.next_stream_id += 2;
                cc.associations.push_back(&ch);

                create_and_send_msg(ch.association, MSG_CONNECT, ch.stream_id, data, plen);
                return;
            }
        }
    }

    uint8_t response = CONNECT_UNKNOWN_SERVICE;
    create_and_enqueue_packet(&ch, PKT_CONNECT_RESPONSE, &response, 1);
}

static void handle_pkt_data(int channel_id, uint8_t *data, int plen)
{
    for (auto &ch : channels)
    {
        if (ch.channel_id == channel_id)
        {
            if (ch.association != nullptr && !ch.got_eos_from_ami)
                create_and_send_msg(ch.association, MSG_DATA, ch.stream_id, data, plen);

            break;
        }
    }
}

static void handle_pkt_eos(int channel_id)
{
    for (auto &ch : channels)
    {
        if (ch.channel_id == channel_id)
        {
            if (ch.association != nullptr && !ch.got_eos_from_ami)
            {
                ch.got_eos_from_ami = true;

                create_and_send_msg(ch.association, MSG_EOS, ch.stream_id, nullptr, 0);

                if (ch.got_eos_from_client)
                    remove_association(&ch);
            }
            break;
        }
    }
}

static void handle_pkt_reset(int channel_id)
{
    for (auto &ch : channels)
    {
        if (ch.channel_id == channel_id)
        {
            clear_packet_queue(&ch);

            if (ch.association != nullptr)
            {
                create_and_send_msg(ch.association, MSG_RESET, ch.stream_id, nullptr, 0);
                remove_association(&ch);
            }

            break;
        }
    }
}

static void remove_channel_if_not_associated_and_empty_pq(int channel_id)
{
    for (auto it = channels.begin(); it != channels.end(); it++)
    {
        if (it->channel_id == channel_id)
        {
            if (it->association == nullptr && it->packet_queue.empty())
                channels.erase(it);

            break;
        }
    }
}

static void handle_received_pkt(int ptype, int channel_id, uint8_t *data, int plen)
{
    if (ptype == PKT_CONNECT)
        handle_pkt_connect(channel_id, data, plen);
    else if (ptype == PKT_DATA)
        handle_pkt_data(channel_id, data, plen);
    else if (ptype == PKT_EOS)
        handle_pkt_eos(channel_id);
    else if (ptype == PKT_RESET)
        handle_pkt_reset(channel_id);

    remove_channel_if_not_associated_and_empty_pq(channel_id);
}

static void receive_from_a2r()
{
    int head = channel_status[A2R_HEAD_OFFSET];
    int tail = channel_status[A2R_TAIL_OFFSET];
    int len = (tail - head) & 255;
    if (len == 0)
        return;

    if (head < tail)
        read_shm(recv_buf, BASE_ADDRESS + A2R_BASE + head, len);
    else
    {
        read_shm(recv_buf, BASE_ADDRESS + A2R_BASE + head, 256 - head);

        if (tail != 0)
            read_shm(&recv_buf[len - tail], BASE_ADDRESS + A2R_BASE, tail);
    }

    uint8_t *p = recv_buf;
    while (p < recv_buf + len)
    {
        uint8_t plen = *p++;
        uint8_t ptype = *p++;
        uint8_t channel_id = *p++;
        handle_received_pkt(ptype, channel_id, p, plen);
        p += plen;
    }

    channel_status[A2R_HEAD_OFFSET] = channel_status[A2R_TAIL_OFFSET];
    channel_status_updated |= A_EVENT_A2R_HEAD;
}

static void flush_send_queue()
{
    int tail = channel_status[R2A_TAIL_OFFSET];
    int head = channel_status[R2A_HEAD_OFFSET];
    int len = (tail - head) & 255;
    int left = 255 - len;

    int pos = 0;

    while (!send_queue.empty())
    {
        LogicalChannel *ch = send_queue.front();
        PacketBuffer &pb = ch->packet_queue.front();

        int ptype = pb.type;
        int plen = 3 + pb.data.size();

        if (left < plen)
            break;

        send_buf[pos++] = pb.data.size();
        send_buf[pos++] = ptype;
        send_buf[pos++] = ch->channel_id;
        memcpy(&send_buf[pos], &pb.data[0], pb.data.size());
        pos += pb.data.size();

        ch->packet_queue.pop_front();

        send_queue.pop_front();

        if (!ch->packet_queue.empty())
            send_queue.push_back(ch);
        else
            remove_channel_if_not_associated_and_empty_pq(ch->channel_id);

        left -= plen;
    }

    int to_write = pos;
    if (!to_write)
        return;

    uint8_t *p = send_buf;
    int at_end = 256 - tail;
    if (at_end < to_write)
    {
        write_shm(BASE_ADDRESS + R2A_BASE + tail, p, at_end);
        p += at_end;
        to_write -= at_end;
        tail = 0;
    }

    write_shm(BASE_ADDRESS + R2A_BASE + tail, p, to_write);
    tail = (tail + to_write) & 255;

    channel_status[R2A_TAIL_OFFSET] = tail;
    channel_status_updated |= A_EVENT_R2A_TAIL;
}

static void read_base_address()
{
    have_base_address = false;

    unsigned int ba1 = 0;
    for (int i = 0; i < BASE_ADDRESS_LEN; i++)
        ba1 |= spi_read_cmem(i) << (i * 4);

    if ((ba1 & 1) == 1)
    {
        unsigned int ba2 = 0;
        for (int i = 0; i < BASE_ADDRESS_LEN; i++)
            ba2 |= spi_read_cmem(i) << (i * 4);

        if (ba1 == ba2)
        {
            have_base_address = true;
            base_address = ba1 & ~1;
        }
    }
}

static void read_channel_status()
{
    read_shm(channel_status, BASE_ADDRESS + CAP_BASE, 4);
    channel_status_updated = 0;
}

static void write_channel_status()
{
    if (channel_status_updated != 0)
    {
        write_shm(BASE_ADDRESS + CAP_BASE + R2A_TAIL_OFFSET, &channel_status[R2A_TAIL_OFFSET], 2);

#if defined(MODEL_TD)
        spi_write_cmem(A_EVENTS_ADDRESS, channel_status_updated);
#elif defined(MODEL_CP)
        set_cp_irq();

        auto_clear_cp_irq = true;
        auto_clear_cp_irq_after = time(NULL) + 3;
#endif
        channel_status_updated = 0;
    }
}

static void close_all_logical_channels()
{
    send_queue.clear();

    auto it = channels.begin();
    while (it != channels.end())
    {
        LogicalChannel &ch = *it;

        if (ch.association != nullptr)
        {
            create_and_send_msg(ch.association, MSG_RESET, ch.stream_id, nullptr, 0);
            remove_association(&ch);
        }

        it = channels.erase(it);
    }
}

static void handle_a314_td_irq()
{
    uint8_t events = spi_ack_irq();
    if (events == 0)
        return;

    if ((events & R_EVENT_BASE_ADDRESS) || !have_base_address)
    {
        if (have_base_address && !channels.empty())
            logger_info("Base address was updated while logical channels are open -- closing channels\n");

        close_all_logical_channels();
        read_base_address();
    }

    if (!have_base_address)
        return;

    read_channel_status();

    receive_from_a2r();
    flush_send_queue();

    write_channel_status();
}

static void handle_a314_cp_irq()
{
    // TODO: Currently there's a single notification event in either direction.
    // Could possibly have two events, one to signal that R2A_TAIL is updated
    // and one for A2R_HEAD.
    // Also in that case should add enable bits so that the other side can
    // mute notifications.

    clear_pi_irq();

    if (!read_a314_magic)
    {
        uint8_t values[2];
        read_shm(values, BASE_ADDRESS + CAP_BASE + 5, 2);
        if (values[0] != 0xa3 || values[1] != 0x14)
            return;

        read_shm(values, BASE_ADDRESS + CAP_BASE + 4, 1);
        restart_counter = values[0];

        read_a314_magic = true;
    }

    read_channel_status();

    uint8_t current_restart_counter;
    read_shm(&current_restart_counter, BASE_ADDRESS + CAP_BASE + 4, 1);

    if (current_restart_counter != restart_counter)
    {
        if (!channels.empty())
            logger_info("Restart counter was updated while logical channels are open -- closing channels\n");
        else
            logger_info("Restart counter was updated\n");

        close_all_logical_channels();
        restart_counter = current_restart_counter;

        read_channel_status();
    }

    receive_from_a2r();
    flush_send_queue();

    write_channel_status();
}

#if defined(MODEL_TD)
#define handle_a314_irq handle_a314_td_irq
#elif defined(MODEL_CP)
#define handle_a314_irq handle_a314_cp_irq
#endif

static void handle_client_connection_event(ClientConnection *cc, struct epoll_event *ev)
{
    if (ev->events & EPOLLERR)
    {
        logger_warn("Received EPOLLERR for client connection\n");
        close_and_remove_connection(cc);
        return;
    }

    if (ev->events & EPOLLIN)
    {
        while (1)
        {
            int left;
            uint8_t *dst;

            if (cc->payload.empty())
            {
                left = sizeof(MessageHeader) - cc->bytes_read;
                dst = (uint8_t *)&(cc->header) + cc->bytes_read;
            }
            else
            {
                left = cc->header.length - cc->bytes_read;
                dst = &cc->payload[cc->bytes_read];
            }

            ssize_t r = read(cc->fd, dst, left);
            if (r == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                logger_error("Read failed unexpectedly with errno = %d\n", errno);
                exit(-1);
            }

            if (r == 0)
            {
                logger_info("Received End-of-File on client connection\n");
                close_and_remove_connection(cc);
                return;
            }
            else
            {
                cc->bytes_read += r;
                left -= r;
                if (!left)
                {
                    if (cc->payload.empty())
                    {
                        if (cc->header.length == 0)
                        {
                            logger_trace("header: length=%d, stream_id=%d, type=%d\n", cc->header.length, cc->header.stream_id, cc->header.type);
                            handle_received_message(cc);
                        }
                        else
                        {
                            cc->payload.resize(cc->header.length);
                        }
                    }
                    else
                    {
                        logger_trace("header: length=%d, stream_id=%d, type=%d\n", cc->header.length, cc->header.stream_id, cc->header.type);
                        handle_received_message(cc);
                        cc->payload.clear();
                    }
                    cc->bytes_read = 0;
                }
            }
        }
    }

    if (ev->events & EPOLLOUT)
    {
        while (!cc->message_queue.empty())
        {
            MessageBuffer &mb = cc->message_queue.front();

            int left = mb.data.size() - mb.pos;
            uint8_t *src = &mb.data[mb.pos];
            ssize_t r = write(cc->fd, src, left);
            if (r == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else if (errno == ECONNRESET)
                {
                    close_and_remove_connection(cc);
                    return;
                }
                else
                {
                    logger_error("Write failed unexpectedly with errno = %d\n", errno);
                    exit(-1);
                }
            }

            mb.pos += r;
            if (r == left)
                cc->message_queue.pop_front();
        }
    }
}

static void handle_server_socket_ready()
{
    struct sockaddr_in address;
    int alen = sizeof(struct sockaddr_in);

    int fd = accept(server_socket, (struct sockaddr *)&address, (socklen_t *)&alen);
    if (fd < 0)
    {
        logger_error("Accept failed unexpectedly with errno = %d\n", errno);
        exit(-1);
    }

    int status = fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
    if (status == -1)
    {
        logger_error("Unexpectedly unable to set close-on-exec flag on client socket descriptor; errno = %d\n", errno);
        exit(-1);
    }

    status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (status == -1)
    {
        logger_error("Unexpectedly unable to set client socket to non blocking; errno = %d\n", errno);
        exit(-1);
    }

    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    connections.emplace_back();

    ClientConnection &cc = connections.back();
    cc.fd = fd;
    cc.next_stream_id = 1;
    cc.bytes_read = 0;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        logger_error("epoll_ctl() failed unexpectedly with errno = %d\n", errno);
        exit(-1);
    }
}

static void main_loop()
{
    handle_a314_irq();

    bool first_gpio_event = true;
    bool shutting_down = false;
    time_t force_shutdown_after;
    bool done = false;

#if defined(MODEL_CP)
    auto_clear_cp_irq = true;
    auto_clear_cp_irq_after = time(NULL) + 3;
#endif

    while (!done)
    {
        struct epoll_event ev;
        int n = epoll_pwait(epfd, &ev, 1, 1000, &original_sigset);
        if (n == -1)
        {
            if (errno == EINTR)
            {
                logger_info("Received SIGTERM\n");

                shutdown_server_socket();

                while (!connections.empty())
                    close_and_remove_connection(&connections.front());

                flush_send_queue();
                write_channel_status();

                if (!shutting_down)
                    force_shutdown_after = time(NULL) + 10;

                shutting_down = true;
            }
            else
            {
                logger_error("epoll_pwait failed with unexpected errno = %d\n", errno);
                exit(-1);
            }
        }
        else if (n == 0)
        {
            // Timeout. Handle below.
        }
        else
        {
            if (ev.data.fd == gpio_irq_fd)
            {
                logger_trace("Epoll event: gpio is ready, events = %d\n", ev.events);

                lseek(gpio_irq_fd, 0, SEEK_SET);

                char buf;
                if (read(gpio_irq_fd, &buf, 1) != 1)
                {
                    logger_error("Read from GPIO value file, and unexpectedly didn't return 1 byte\n");
                    exit(-1);
                }

                if (first_gpio_event)
                {
                    logger_debug("Received first GPIO event, which is ignored\n");
                    first_gpio_event = false;
                }
                else
                {
                    logger_trace("GPIO interupted\n");
                    handle_a314_irq();
                }
            }
            else if (ev.data.fd == server_socket)
            {
                logger_trace("Epoll event: server socket is ready, events = %d\n", ev.events);
                handle_server_socket_ready();
            }
            else
            {
                logger_trace("Epoll event: client socket is ready, events = %d\n", ev.events);

                auto it = connections.begin();
                for (; it != connections.end(); it++)
                {
                    if (it->fd == ev.data.fd)
                        break;
                }

                if (it == connections.end())
                {
                    logger_error("Got notified about an event on a client connection that supposedly isn't currently open\n");
                    exit(-1);
                }

                ClientConnection *cc = &(*it);
                handle_client_connection_event(cc, &ev);

                flush_send_queue();
                write_channel_status();
            }
        }

        time_t now = time(NULL);

        if (auto_clear_cp_irq && now > auto_clear_cp_irq_after)
        {
            clear_cp_irq();
            auto_clear_cp_irq = false;
        }

        if (shutting_down && (channels.empty() || now > force_shutdown_after))
            done = true;
    }
}

int main(int argc, char **argv)
{
    std::string conf_filename("/etc/opt/a314/a314d.conf");

    if (argc >= 2)
        conf_filename = argv[1];

    load_config_file(conf_filename.c_str());

    if (init_driver() == 0)
        main_loop();
    shutdown_driver();
    return 0;
}
