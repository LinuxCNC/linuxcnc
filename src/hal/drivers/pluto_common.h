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

#include "config.h"

#if defined(BUILD_SYS_USER_DSO)

#include <sys/io.h>
#include <errno.h>
#else
#include <asm/io.h>
#endif

#include "hal_parport.h"

int ioaddr = 0x378;
int ioaddr_hi = 0;
int epp_wide = 1;
int watchdog = 1;
struct hal_parport_t portdata;

RTAPI_MP_INT(ioaddr, "Address of parallel port where pluto-p is attached");
RTAPI_MP_INT(ioaddr_hi,
	"Secondary address of parallel port (0 to use ioaddr+0x400)");
RTAPI_MP_INT(epp_wide, "Use 16- and 32-bit EPP transfers with hardware EPP");
RTAPI_MP_INT(watchdog,
	"Enable hardware watchdog to tristate outputs if EMC crashes");

#ifdef BUILD_SYS_KBUILD
#ifndef llabs // linux/kernel.h may provide labs for realtime systems
static int64_t llabs(int64_t l) { if(l < 0) return -l; return l; }
#endif
#endif

static inline int64_t extend(int64_t old, int newlow, int nbits) {
    int64_t mask = (1<<nbits) - 1;
    int64_t maxdelta = mask / 2;
    int64_t oldhigh = old & ~mask;
    int64_t oldlow = old & mask;
    int64_t candidate1, candidate2;

    candidate1 = oldhigh | newlow;
    if(oldlow < newlow) candidate2 = candidate1 - (1<<nbits);
    else                candidate2 = candidate1 + (1<<nbits);

    if (llabs(old-candidate1) > maxdelta)
	return candidate2;
    else
	return candidate1;
}

static inline void EPP_DIR_WRITE(void) { }
static inline void EPP_DIR_READ(void) { }
static inline void EPP_ADDR(int w) {
    outb(w, ioaddr+3);
}

static inline void EPP_WRITE(int w) {
    outb(w, ioaddr+4);
}

static inline int EPP_READ(void) {
    return inb(ioaddr+4);
}


static inline __u32 read32(void) {
    unsigned char a, b, c, d;

    if(epp_wide)
	return inl(ioaddr+4);

    a = EPP_READ();
    b = EPP_READ();
    c = EPP_READ();
    d = EPP_READ();

    return a + (b<<8) + (c<<16) + (d<<24);
}

static inline void write32(long w) {
    if(epp_wide) {
	outl(w, ioaddr+4);
	return;
    }

    EPP_WRITE(w);
    EPP_WRITE(w >> 8);
    EPP_WRITE(w >> 16);
    EPP_WRITE(w >> 24);
}

static inline void write16(int w) {
    if(epp_wide) {
	outw(w, ioaddr+4);
	return;
    }

    EPP_WRITE(w & 0xff);
    EPP_WRITE(w >> 8);
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

static void pluto_clear_error_register(void) {
    /* To clear timeout some chips require double read */
    int r = inb(ioaddr+1);
    outb (r | 0x01, ioaddr+1); /* Some reset by writing 1 */
    outb (r & 0xfe, ioaddr+1); /* Others by writing 0 */
}

static void pluto_cleanup(void) {
    hal_parport_release(&portdata);
}

static int pluto_setup(unsigned char *firmware) {
    int retval = hal_parport_get(comp_id, &portdata, ioaddr, ioaddr_hi,
        PARPORT_MODE_EPP);
    int status;

    if(retval < 0)
        return retval;

    ioaddr = portdata.base;
    ioaddr_hi = portdata.base_hi;

    outb(4, ioaddr + 2);        // set control lines and input mode
    if(ioaddr_hi != -1)
        outb(0, ioaddr_hi + 0x2);    // select SPP mode in ECR
    pluto_program(firmware);

    if(ioaddr_hi != -1) {
        outb(0x80, ioaddr_hi + 0x2); // select EPP mode in ECR
    }

    // Check for presence of working EPP hardware
    pluto_clear_error_register();
    EPP_ADDR(0);
    EPP_DIR_READ();
    EPP_READ();
    status = inb(ioaddr+1) & 1;
    if(status) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Failed to communicate with pluto-servo board after programming firmware.\n");
        return -EIO;
    }
    return 0;
}
