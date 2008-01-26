//    Copyright (C) 2007 Jeff Epler
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#ifdef SIM
#include <sys/io.h>
#include <errno.h>
#else
#include <asm/io.h>
#endif

int ioaddr = 0x378;
int ioaddr_hi = 0;
int epp_wide = 1;
int watchdog = 1;

RTAPI_MP_INT(ioaddr, "Address of parallel port where pluto-p is attached");
RTAPI_MP_INT(ioaddr_hi,
	"Secondary address of parallel port (0 to use ioaddr+0x400)");
RTAPI_MP_INT(epp_wide, "Use 16- and 32-bit EPP transfers with hardware EPP");
RTAPI_MP_INT(watchdog,
	"Enable hardware watchdog to tristate outputs if EMC crashes");

#ifndef labs // linux/kernel.h may provide labs for realtime systems
static long labs(long l) { if(l < 0) return -l; return l; }
#endif

static inline long long extend(long long old, int newlow, int nbits) {
    long long mask = (1<<nbits) - 1;
    long long maxdelta = mask / 2;
    long long oldhigh = old & ~mask;
    long long oldlow = old & mask;
    long long candidate1, candidate2;

    candidate1 = oldhigh | newlow;
    if(oldlow < newlow) candidate2 = candidate1 - (1<<nbits);
    else                candidate2 = candidate1 + (1<<nbits);

    if (labs(old-candidate1) > maxdelta)
	return candidate2;
    else
	return candidate1;
}

static inline void EPP_DIR_WRITE(void) { }
static inline void EPP_DIR_READ(void) { }
static inline void ADDR(int w) {
    outb(w, ioaddr+3);
}

static inline void WRITE(int w) {
    outb(w, ioaddr+4);
}

static inline int READ(void) {
    return inb(ioaddr+4);
}


static inline __u32 read32(void) {
    unsigned char a, b, c, d;

    if(epp_wide)
	return inl(ioaddr+4);

    a = READ();
    b = READ();
    c = READ();
    d = READ();

    return a + (b<<8) + (c<<16) + (d<<24);
}

static inline void write32(long w) {
    if(epp_wide) {
	outl(w, ioaddr+4);
	return;
    }

    WRITE(w);
    WRITE(w >> 8);
    WRITE(w >> 16);
    WRITE(w >> 24);
}

static inline void write16(int w) {
    if(epp_wide) {
	outw(w, ioaddr+4);
	return;
    }

    WRITE(w & 0xff);
    WRITE(w >> 8);
}


#define FIRMWARE_SIZE 19895
static void pluto_program(unsigned char firmware[FIRMWARE_SIZE]) {
    int byte, bit;
    int i;
    rtapi_print_msg(RTAPI_MSG_INFO, "uploading firmware\n");

    // pull the reset low -- bit 2 of status register
    // keep it low 2 microseconds
    for(i=0; i<4; i++) outb(0, ioaddr+2);

    // let it go high again
    // delay 10 microseconds to guarantee nStatus high
    for(i=0; i<20; i++) outb(4, ioaddr+2);

    // Now program the device...
    for(byte = 0; byte < FIRMWARE_SIZE; byte++) {
	for(bit = 0; bit < 8; bit++) {
	    int v = firmware[byte] & (1<<bit);
	    if(v) outb(0xff, ioaddr); else outb(0, ioaddr);
	    outb(0|4, ioaddr+2);
	    outb(1|4, ioaddr+2);
	    outb(0|4, ioaddr+2);
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "done\n");
}

static void *region1=0, *region2=0;
static void pluto_clear_error_register(void) {
    /* To clear timeout some chips require double read */
    int r = inb(ioaddr+1);
    outb (r | 0x01, ioaddr+1); /* Some reset by writing 1 */
    outb (r & 0xfe, ioaddr+1); /* Others by writing 0 */
}

static void pluto_cleanup(void) {
    if(region1)
	outb(0, ioaddr+2); // set nInitialize low and reset the FPGA
    if(region1)
	    rtapi_release_region(ioaddr, 8);
    if(region2)
	rtapi_release_region(ioaddr_hi, 4);
}

static int pluto_setup(unsigned char *firmware) {

    int status;
    if(ioaddr_hi == 0) ioaddr_hi = ioaddr + 0x400;

    region1 = rtapi_request_region(ioaddr, 8, "pluto");
    if(!region1) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	     "PLUTO: ERROR: request_region(%x) failed\n", ioaddr);
	rtapi_print_msg(RTAPI_MSG_ERR,
	     "(make sure the kernel module 'parport' is unloaded)\n");
	hal_exit(comp_id);
	return -EBUSY;
    }

    if(ioaddr_hi != -1) {
	region2 = rtapi_request_region(ioaddr_hi, 4, "pluto");
	if(!region2) {
	    rtapi_release_region(ioaddr, 8);
	    rtapi_print_msg(RTAPI_MSG_ERR,
		 "PLUTO: ERROR: request_region(%x) failed\n", ioaddr);
	    rtapi_print_msg(RTAPI_MSG_ERR,
		 "(make sure the kernel module 'parport' is unloaded)\n");
	    hal_exit(comp_id);
	    return -EBUSY;
	}
    }

    outb(4, ioaddr + 2);        // set control lines and input mode
    if(ioaddr_hi != -1)
        outb(0, ioaddr_hi + 0x2);    // select SPP mode in ECR
    pluto_program(firmware);

    if(ioaddr_hi != -1) {
        outb(0x80, ioaddr_hi + 0x2); // select EPP mode in ECR
    }

    // Check for presence of working EPP hardware
    pluto_clear_error_register();
    ADDR(0);
    EPP_DIR_READ();
    READ();
    status = inb(ioaddr+1) & 1;
    if(status) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Failed to communicate with pluto-servo board after programming firmware.\n");
        return -EIO;
    }
    return 0;
}
