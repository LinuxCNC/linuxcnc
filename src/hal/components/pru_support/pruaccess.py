# basic Python access to PRU using mmap
# this requires /dev/uio* to be group readable for the users group


import os
import mmap
import ctypes


PAGE_SIZE                   = 4096
PRUSS_V1                    =  1
PRUSS_V2                    =  2

AM33XX_PRUSS_INTC_REV      = 0x4E82A900
AM18XX_PRUSS_INTC_REV      = 0x4E825900

PRUSS_MAX_IRAM_SIZE        = 8192

AM33XX_PRUSS_IRAM_SIZE     = 8192
AM33XX_PRUSS_MMAP_SIZE     = 0x40000

# this is now all WORD offsets relative to dataram0_phys_base (0x4a300000)
_base = 4

AM33XX_DATARAM0            = 0x00000000/_base
AM33XX_DATARAM1            = 0x00002000/_base
AM33XX_INTC                = 0x00020000/_base
AM33XX_PRU0CONTROL         = 0x00022000/_base
AM33XX_PRU0DEBUG           = 0x00022400/_base
AM33XX_PRU1CONTROL         = 0x00024000/_base
AM33XX_PRU1DEBUG           = 0x00024400/_base
AM33XX_PRU0IRAM            = 0x00034000/_base
AM33XX_PRU1IRAM            = 0x00038000/_base
AM33XX_PRUSS_SHAREDRAM_BASE  = 0x00010000/_base

AM33XX_PRUSS_CFG_BASE      = 0x00026000/_base
AM33XX_PRUSS_UART_BASE     = 0x00028000/_base
AM33XX_PRUSS_IEP_BASE      = 0x0002e000/_base
AM33XX_PRUSS_ECAP_BASE     = 0x00030000/_base
AM33XX_PRUSS_MIIRT_BASE    = 0x00032000/_base
AM33XX_PRUSS_MDIO_BASE     = 0x00032400/_base

AM18XX_PRUSS_IRAM_SIZE     = 4096
AM18XX_PRUSS_MMAP_SIZE     = 0x7C00
AM18XX_DATARAM0            = 0x00000000/_base
AM18XX_DATARAM1            = 0x00002000/_base
AM18XX_INTC                = 0x00004000/_base
AM18XX_PRU0CONTROL         = 0x00007000/_base
AM18XX_PRU0DEBUG           = 0x00007400/_base
AM18XX_PRU1CONTROL         = 0x00007800/_base
AM18XX_PRU1DEBUG           = 0x00007C00/_base
AM18XX_PRU0IRAM            = 0x00008000/_base
AM18XX_PRU1IRAM            = 0x0000C000/_base

# PRUSS INTC register offsets
PRU_INTC_REVID_REG   = 0x000/_base
PRU_INTC_CR_REG      = 0x004/_base
PRU_INTC_HCR_REG     = 0x00C/_base
PRU_INTC_GER_REG     = 0x010/_base
PRU_INTC_GNLR_REG    = 0x01C/_base
PRU_INTC_SISR_REG    = 0x020/_base
PRU_INTC_SICR_REG    = 0x024/_base
PRU_INTC_EISR_REG    = 0x028/_base
PRU_INTC_EICR_REG    = 0x02C/_base
PRU_INTC_HIEISR_REG  = 0x034/_base
PRU_INTC_HIDISR_REG  = 0x038/_base
PRU_INTC_GPIR_REG    = 0x080/_base

PRU_INTC_SRSR1_REG   = 0x200/_base
PRU_INTC_SRSR2_REG   = 0x204/_base

PRU_INTC_SECR1_REG   = 0x280/_base
PRU_INTC_SECR2_REG   = 0x284/_base

PRU_INTC_ESR1_REG    = 0x300/_base
PRU_INTC_ESR2_REG    = 0x304/_base

PRU_INTC_ECR1_REG    = 0x380/_base
PRU_INTC_ECR2_REG    = 0x384/_base

PRU_INTC_CMR1_REG    = 0x400/_base
PRU_INTC_CMR2_REG    = 0x404/_base
PRU_INTC_CMR3_REG    = 0x408/_base
PRU_INTC_CMR4_REG    = 0x40C/_base
PRU_INTC_CMR5_REG    = 0x410/_base
PRU_INTC_CMR6_REG    = 0x414/_base
PRU_INTC_CMR7_REG    = 0x418/_base
PRU_INTC_CMR8_REG    = 0x41C/_base
PRU_INTC_CMR9_REG    = 0x420/_base
PRU_INTC_CMR10_REG   = 0x424/_base
PRU_INTC_CMR11_REG   = 0x428/_base
PRU_INTC_CMR12_REG   = 0x42C/_base
PRU_INTC_CMR13_REG   = 0x430/_base
PRU_INTC_CMR14_REG   = 0x434/_base
PRU_INTC_CMR15_REG   = 0x438/_base
PRU_INTC_CMR16_REG   = 0x43C/_base

PRU_INTC_HMR1_REG    = 0x800/_base
PRU_INTC_HMR2_REG    = 0x804/_base
PRU_INTC_HMR3_REG    = 0x808/_base

PRU_INTC_SIPR1_REG   = 0xD00/_base
PRU_INTC_SIPR2_REG   = 0xD04/_base

PRU_INTC_SITR1_REG   = 0xD80/_base
PRU_INTC_SITR2_REG   = 0xD84/_base

PRU_INTC_HIER_REG    = 0x1500/_base

#define PRUSS_UIO_MAP_OFFSET_PRUSS 0*PAGE_SIZE
pruss_base = "/sys/class/uio/uio0/maps/map0/addr"
pruss_size =  "/sys/class/uio/uio0/maps/map0/size"

# #ifndef DISABLE_L3RAM_SUPPORT
# #define PRUSS_UIO_MAP_OFFSET_L3RAM 1*PAGE_SIZE
# l3ram_base =  "/sys/class/uio/uio0/maps/map1/addr"
# l3ram_size =  "/sys/class/uio/uio0/maps/map1/size"
# #define PRUSS_UIO_MAP_OFFSET_EXTRAM 2*PAGE_SIZE
# extram_base = "/sys/class/uio/uio0/maps/map2/addr"
# extram_size = "/sys/class/uio/uio0/maps/map2/size"
# #else

#define PRUSS_UIO_MAP_OFFSET_EXTRAM 1*PAGE_SIZE
extram_base = "/sys/class/uio/uio0/maps/map1/addr"
extram_size = "/sys/class/uio/uio0/maps/map1/size"
#endif

# pruss.h
PRU_EVTOUT_0        =     0
PRU_EVTOUT_1        =     1
PRU_EVTOUT_2        =     2
PRU_EVTOUT_3        =     3
PRU_EVTOUT_4        =     4
PRU_EVTOUT_5        =     5
PRU_EVTOUT_6        =     6
PRU_EVTOUT_7        =     7

def readhex(filename):
    f = open(filename, "r")
    n = int(f.read(),16)
    f.close()
    return n

class Pru:
    def __init__(self, num):
        self.num = num

class Pruss:

    def detect_hw_version(self):
        version = self.dataram_w[AM18XX_INTC]
        if version == AM18XX_PRUSS_INTC_REV:
            print "V1"
            return PRUSS_V1
        else:
            version = self.dataram_w[AM33XX_INTC]
            if version == AM33XX_PRUSS_INTC_REV:
                print "V2"
                return PRUSS_V2
            else:
                return -1

    def __init__(self, evtout):
        path = format("/dev/uio%d" % (evtout))
        self.uio = os.open(path, os.O_RDWR | os.O_SYNC, 0)
        self.pruss_phys_base = readhex(pruss_base)
        self.pruss_map_size = readhex(pruss_size)
        self.dataram_base = mmap.mmap(self.uio, self.pruss_map_size,
                                      mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)

        # make this accessible as an array of uint32 words
        self.dataram_w = (ctypes.c_uint32 * (self.pruss_map_size/4)).from_buffer(self.dataram_base)

        # and as byte array for convenience
        self.dataram_b = (ctypes.c_uint8 * self.pruss_map_size).from_buffer(self.dataram_base)

        self.version =  self.detect_hw_version()
        if self.version < 0:
            raise Exception, "cannot detect hardware version"

        self.extram_phys_base = readhex(extram_base)
        self.extram_map_size = readhex(extram_size)
        self.extram = mmap.mmap(self.uio, self.extram_map_size,
                                mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
        self.extram_w = (ctypes.c_uint32 * (self.extram_map_size/4)).from_buffer(self.extram)
        self.extram_b = (ctypes.c_uint8 * self.extram_map_size).from_buffer(self.extram)

    def __delete__(self):
        self.pruss_io.close()
        self.extram.close()



def main():
    p = Pruss(PRU_EVTOUT_0)

    # reset the IEP counter
    p.dataram_w[AM33XX_PRUSS_IEP_BASE+3] = 0

    # enable it
    p.dataram_w[AM33XX_PRUSS_IEP_BASE+3]  |= 1;

    # PRU 0 processor status register
    control =  p.dataram_w[AM33XX_PRU0CONTROL]
    print "pr0 control %8.8x" % control

    # PRU 0 program counter
    print "pr0 PC %d" % p.dataram_w[AM33XX_PRU0CONTROL+1]

    print "pr0 IEP lap count %d (*5nS)" % p.dataram_w[AM33XX_PRUSS_IEP_BASE+3]

    if control & 0x8000:
        print "disabling pr0"
        p.dataram_w[AM33XX_PRU0CONTROL] = 0
        # dump few words from PRU0 iram
        # this works only if PRU disabled
        for i in range(AM33XX_PRU0IRAM,AM33XX_PRU0IRAM+4):
            print "%8.8x " % (p.dataram_w[i]),
    else:
        print "enabling pr0"
        p.dataram_w[AM33XX_PRU0CONTROL] = 2 # |= 0x8001



if __name__ == '__main__':
    main()
