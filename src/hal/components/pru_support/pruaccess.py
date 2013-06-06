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

AM33XX_DATARAM0            = 0x00000000
AM33XX_DATARAM1            = 0x00002000
AM33XX_INTC                = 0x00020000
AM33XX_PRU0CONTROL         = 0x00022000
AM33XX_PRU0DEBUG           = 0x00022400
AM33XX_PRU1CONTROL         = 0x00024000
AM33XX_PRU1DEBUG           = 0x00024400
AM33XX_PRU0IRAM            = 0x00034000
AM33XX_PRU1IRAM            = 0x00038000
AM33XX_PRUSS_SHAREDRAM_BASE  = 0x00010000

AM33XX_PRUSS_CFG_BASE      = 0x00026000
AM33XX_PRUSS_UART_BASE     = 0x00028000
AM33XX_PRUSS_IEP_BASE      = 0x0002e000
AM33XX_PRUSS_ECAP_BASE     = 0x00030000
AM33XX_PRUSS_MIIRT_BASE    = 0x00032000
AM33XX_PRUSS_MDIO_BASE     = 0x00032400

AM18XX_PRUSS_IRAM_SIZE     = 4096
AM18XX_PRUSS_MMAP_SIZE     = 0x7C00
AM18XX_DATARAM0            = 0x00000000
AM18XX_DATARAM1            = 0x00002000
AM18XX_INTC                = 0x00004000
AM18XX_PRU0CONTROL         = 0x00007000
AM18XX_PRU0DEBUG           = 0x00007400
AM18XX_PRU1CONTROL         = 0x00007800
AM18XX_PRU1DEBUG           = 0x00007C00
AM18XX_PRU0IRAM            = 0x00008000
AM18XX_PRU1IRAM            = 0x0000C000

# PRUSS INTC register offsets
PRU_INTC_REVID_REG   = 0x000
PRU_INTC_CR_REG      = 0x004
PRU_INTC_HCR_REG     = 0x00C
PRU_INTC_GER_REG     = 0x010
PRU_INTC_GNLR_REG    = 0x01C
PRU_INTC_SISR_REG    = 0x020
PRU_INTC_SICR_REG    = 0x024
PRU_INTC_EISR_REG    = 0x028
PRU_INTC_EICR_REG    = 0x02C
PRU_INTC_HIEISR_REG  = 0x034
PRU_INTC_HIDISR_REG  = 0x038
PRU_INTC_GPIR_REG    = 0x080

PRU_INTC_SRSR1_REG   = 0x200
PRU_INTC_SRSR2_REG   = 0x204

PRU_INTC_SECR1_REG   = 0x280
PRU_INTC_SECR2_REG   = 0x284

PRU_INTC_ESR1_REG    = 0x300
PRU_INTC_ESR2_REG    = 0x304

PRU_INTC_ECR1_REG    = 0x380
PRU_INTC_ECR2_REG    = 0x384

PRU_INTC_CMR1_REG    = 0x400
PRU_INTC_CMR2_REG    = 0x404
PRU_INTC_CMR3_REG    = 0x408
PRU_INTC_CMR4_REG    = 0x40C
PRU_INTC_CMR5_REG    = 0x410
PRU_INTC_CMR6_REG    = 0x414
PRU_INTC_CMR7_REG    = 0x418
PRU_INTC_CMR8_REG    = 0x41C
PRU_INTC_CMR9_REG    = 0x420
PRU_INTC_CMR10_REG   = 0x424
PRU_INTC_CMR11_REG   = 0x428
PRU_INTC_CMR12_REG   = 0x42C
PRU_INTC_CMR13_REG   = 0x430
PRU_INTC_CMR14_REG   = 0x434
PRU_INTC_CMR15_REG   = 0x438
PRU_INTC_CMR16_REG   = 0x43C

PRU_INTC_HMR1_REG    = 0x800
PRU_INTC_HMR2_REG    = 0x804
PRU_INTC_HMR3_REG    = 0x808

PRU_INTC_SIPR1_REG   = 0xD00
PRU_INTC_SIPR2_REG   = 0xD04

PRU_INTC_SITR1_REG   = 0xD80
PRU_INTC_SITR2_REG   = 0xD84

PRU_INTC_HIER_REG    = 0x1500

# offset relative to PRUxCONTROL
PRU_CONTROL = 0
PRU_STATUS = 4
PRU_WAKEUP_EN = 8
PRU_CYCLE = 0xC
PRU_STALL = 0x10
PRU_CTBIR0 = 0x20
PPU_CTBIR1 = 0x24
PRU_CTPPR0 = 0x28
PRU_CTPPR1 = 0x2c


# offset relative to IEP_BASE
IEP_GLOBAL_CFG = 0
IEP_CNT_ENABLE = 1

IEP_GLOBAL_STATUS  = 0x04
IEP_COMPEN = 0x08
IEP_COUNT = 0x0C

IEP_CMP_CFG = 0x40

IEP_CMP0_RST_CNT_EN = 1 # Enable bit for each of the compare registers, where CMP_EN0 maps to CMP0

IEP_CMP_EN0 = 2 # Enable the reset of the counter if a CMP0 event occurs
IEP_CMP_EN1 = 4
IEP_CMP_EN2 = 8
#...
IEP_CMP_EN7 = 256

# Status bit for each of the compare registers, where CMP_HIT0 is mapped to CMP0.
IEP_CMP_STATUS = 0x44
IEP_CMP_HIT0  = 1
IEP_CMP_HIT1  = 2
# etc etc

IEP_CMP0 = 0x48
IEP_CMP1 = 0x4c
IEP_CMP2 = 0x50
IEP_CMP3 = 0x54
IEP_CMP4 = 0x58
IEP_CMP5 = 0x5c
IEP_CMP6 = 0x60
IEP_CMP7 = 0x64

# ecap doc starts p1736 spruh73f.pdf
# ecap regs
# p1763ff

#define ECAP_INT_NUM 15
#define ECAP_INT_BIT 15

#//ecap registers
ECAP0_TSCTR 	= 0x0
ECAP0_CTRPHS	= 0x4
ECAP0_CAP1	= 0x8
ECAP0_CAP2	= 0xC
ECAP0_CAP3	= 0x10
ECAP0_CAP4	= 0x14
ECAP0_ECCTL1	= 0x28

#TSCTRSTOP = 16  # 1=run !!

ECAP0_ECCTL2	= 0x2A
ECAP0_ECEINT	= 0x2C
ECAP0_ECFLG	= 0x2E
ECAP0_ECCLR	= 0x30
ECAP0_ECFRC	= 0x32
ECAP0_REVID	= 0x5C

ECAP_REVID = 0x44D22100

#//ecap bit fields
#define ECCTL2_APWMPOL_BIT		10
#define ECCTL2_CAP_APWM_BIT		9
#define ECCTL2_TSCTRSTOP_BIT	4

#define ECEINT_CTRPRD_BIT		6

#//ecap cfg values
#define ECCTL2_VAL 1<<ECCTL2_APWMPOL_BIT |(1<<ECCTL2_CAP_APWM_BIT)
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

class Mem:
    ''' primitive method of accessing memory by a an address (*not* an array index)
    the key is always a byte address, regardless of byt/short/word access
    TBD: alignment check
    '''
    def __init__(self, wordsize, memaddr, size):
        self.wordsize = wordsize
        self.memaddr = memaddr
        self.size = size

        if wordsize == 1:
            PTRTYPE = ctypes.POINTER(ctypes.c_uint8)
            self.shift = 0
        elif wordsize == 2:
            PTRTYPE = ctypes.POINTER(ctypes.c_uint16)
            self.shift = 1
        elif wordsize == 4:
            self.shift = 2
            PTRTYPE = ctypes.POINTER(ctypes.c_uint32)
        self.memory = ctypes.cast(memaddr, PTRTYPE)

    def __setitem__(self, key, value):
        if key < 0 or key > self.size-1:
            raise ValueError, "key out of range: " + str(key)
        self.memory[key >> self.shift] = value

    def __getitem__(self, key):
        if key < 0 or key > self.size-1:
            raise ValueError, "key out of range: " + str(key)
        return self.memory[key >> self.shift]

class Pru:

    def __init__(self, num, pruss):
        self.num = num
        self.pruss = pruss

    def iram(self):
	if self.pruss.version == PRUSS_V1:
		return AM18XX_PRU0IRAM if self.num == 0 else AM18XX_PRU1IRAM
	if self.pruss.version == PRUSS_V2:
		return AM33XX_PRU0IRAM if self.num == 0 else AM33XX_PRU1IRAM

    def control(self):
	if self.pruss.version == PRUSS_V1:
		return AM18XX_PRU0CONTROL if self.num == 0 else AM18XX_PRU1CONTROL
	if self.pruss.version == PRUSS_V2:
		return AM33XX_PRU0CONTROL if self.num == 0 else AM33XX_PRU1CONTROL

    def iep(self):
	if self.pruss.version == PRUSS_V1:
		raise ValueError,"iep not supported on V1"
	if self.pruss.version == PRUSS_V2:
		return AM33XX_PRUSS_IEP_BASE

    def ecap(self):
	if self.pruss.version == PRUSS_V1:
		raise ValueError,"ecap not supported on V1"
	if self.pruss.version == PRUSS_V2:
		return AM33XX_PRUSS_ECAP_BASE

    def config(self):
        if self.pruss.version == PRUSS_V1:
		raise ValueError,"CFG not supported on V1"
	if self.pruss.version == PRUSS_V2:
		return AM33XX_PRUSS_CFG_BASE

    def load(self,pru, code):
	status = self.halt(pru)
	bytes_read = open(code, "r").read()
	if len(bytes_read) & 3:
		raise Exception, "file size not a multiple of 4 : " + code
	loc = self.iram(pru) * 4
	for b in bytes_read:
		self.dataram_b[b]
		b += 1

class Pruss:


    def detect_hw_version(self):
        version = self.drw[AM18XX_INTC]
        if version == AM18XX_PRUSS_INTC_REV:
            print "V1"
            return PRUSS_V1
        else:
            version = self.drw[AM33XX_INTC]
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

        # hokey way to get at address of mmap region
        i = ctypes.c_uint8.from_buffer(self.dataram_base)
        ba = ctypes.addressof(i)

        self.drw = Mem(4,ba, self.pruss_map_size)
        self.drs = Mem(2,ba, self.pruss_map_size)
        self.drb = Mem(1,ba, self.pruss_map_size)

        self.version =  self.detect_hw_version()
        if self.version < 0:
            raise Exception, "cannot detect hardware version"

        self.extram_phys_base = readhex(extram_base)
        self.extram_map_size = readhex(extram_size)
        self.extram = mmap.mmap(self.uio, self.extram_map_size,
                                mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)

        e = ctypes.c_uint8.from_buffer(self.extram)
        ea = ctypes.addressof(e)

        self.erw = Mem(4,ea, self.extram_map_size)
        self.ers = Mem(2,ea, self.extram_map_size)
        self.erb = Mem(1,ea, self.extram_map_size)

    def __delete__(self):
        self.pruss_io.close()
        self.extram.close()



def main():
    p = Pruss(PRU_EVTOUT_0)
    p.pru = Pru(0,p)

    iep = p.pru.iep()
    pructrl = p.pru.control()
    iram = p.pru.iram()

    # enable the IEP counter
    p.drw[iep + IEP_GLOBAL_CFG]  |= IEP_CNT_ENABLE

    # reset the IEP counter
    p.drw[iep + IEP_COUNT] = 0

    # PRU 0 processor status register
    control =  p.drw[pructrl + PRU_CONTROL]
    print "pr0 control %8.8x" % control

    # PRU 0 program counter
    print "pr0 PC %d" % p.drw[pructrl + PRU_STATUS]

    print "pr0 IEP lap count %d (*5nS)" % p.drw[AM33XX_PRUSS_IEP_BASE + IEP_COUNT]

    if control & 0x8000:
        print "disabling pr0"
        p.drw[pructrl  + PRU_CONTROL] = 0
        # dump few words from PRU0 iram
        # this works only if PRU disabled
        for i in range(4):
            print "%8.8x " % (p.drw[iram + (i << 2)]),
        print
    else:
        print "enabling pr0"
        p.drw[pructrl + PRU_CONTROL] = 2



if __name__ == '__main__':
    main()
