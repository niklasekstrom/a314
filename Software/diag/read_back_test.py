import spidev

spi = spidev.SpiDev()
spi.open(0, 0)

spi.max_speed_hz = 70*1000*1000
spi.mode = 0
spi.cshigh = True

READ_SRAM_CMD = 0
WRITE_SRAM_CMD = 1
READ_CMEM_CMD = 2
WRITE_CMEM_CMD = 3

def read_protocol_version():
    return spi.xfer([255, 0])[1]

proto_ver = read_protocol_version()
if proto_ver not in [0, 1]:
    raise RuntimeError("Unknown protocol version: {}".format(proto_ver))

print("Uses protocol version: {}".format(proto_ver))

def read_sram(addr, n):
    if proto_ver == 0:
        return spi.xfer([(READ_SRAM_CMD << 4) | (addr >> 16) & 0xf, (addr >> 8) & 0xff, addr & 0xff, 0] + [0] * n)[4:]
    elif proto_ver == 1:
        return spi.xfer([(READ_SRAM_CMD << 5) | (addr >> 16) & 0x1f, (addr >> 8) & 0xff, addr & 0xff, 0] + [0] * n)[4:]

def write_sram(addr, data):
    if proto_ver == 0:
        spi.xfer([(WRITE_SRAM_CMD << 4) | ((addr >> 16) & 0xf), (addr >> 8) & 0xff, addr & 0xff] + data)
    elif proto_ver == 1:
        spi.xfer([(WRITE_SRAM_CMD << 5) | ((addr >> 16) & 0x1f), (addr >> 8) & 0xff, addr & 0xff] + data)

def g(n):
    w = 0
    for _ in range(n):
        yield (w >> 8)
        yield (w & 0xff)
        w += 1
        if w == 65536:
            w = 0

data = list(g(65536))
data = data * 4

for i in range(32):
    write_sram(i*2048, data[i*2048:(i+1)*2048])

print(read_sram(0, 2048))
