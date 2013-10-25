
//
//    Copyright (C) 2013 Sebastian Kuzminsky
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
//


#include <asm/io.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "rtapi_string.h"

#include "hal.h"

#include "epp.h"


static int comp_id;


#ifdef MODULE_INFO
MODULE_INFO(linuxcnc, "component:epp:LinuxCNC driver for EPP communications.");
MODULE_INFO(linuxcnc, "license:GPL");
#endif // MODULE_INFO

MODULE_LICENSE("GPL");

int debug_epp = 0;
RTAPI_MP_INT(debug_epp, "Developer/debug use only!  Enable debug logging of most EPP\ntransfers.");




//
// EPP I/O code
//

EXPORT_SYMBOL_GPL(epp_get);
int epp_get(epp_t *epp_port, int ioaddr, int ioaddr_hi, int wide) {
    int r;

    epp_port->wide = wide;

    r = hal_parport_get(comp_id, &epp_port->port, ioaddr, ioaddr_hi, PARPORT_MODE_EPP);
    if (r < 0) {
        return r;
    }

    // set up the parport for EPP
    if (epp_port->port.base_hi) {
        outb(0x94, epp_port->port.base_hi + ECP_CONTROL_HIGH_OFFSET); // select EPP mode in ECR
    }

    return 0;
}

EXPORT_SYMBOL_GPL(epp_release);
void epp_release(epp_t *epp_port) {
    hal_parport_release(&epp_port->port);
}

EXPORT_SYMBOL_GPL(epp_addr8);
void epp_addr8(u8 addr, epp_t *epp_port) {
    outb(addr, epp_port->port.base + EPP_ADDRESS_OFFSET);
    DEBUG("selected address 0x%02X\n", addr);
}

EXPORT_SYMBOL_GPL(epp_addr16);
void epp_addr16(u16 addr, epp_t *epp_port) {
    outb((addr & 0x00FF), epp_port->port.base + EPP_ADDRESS_OFFSET);
    outb((addr >> 8),     epp_port->port.base + EPP_ADDRESS_OFFSET);
    DEBUG("selected address 0x%04X\n", addr);
}

EXPORT_SYMBOL_GPL(epp_write);
void epp_write(int w, epp_t *epp_port) {
    outb(w, epp_port->port.base + EPP_DATA_OFFSET);
    DEBUG("wrote data 0x%02X\n", w);
}

EXPORT_SYMBOL_GPL(epp_read);
int epp_read(epp_t *epp_port) {
    int val;
    val = inb(epp_port->port.base + EPP_DATA_OFFSET);
    DEBUG("read data 0x%02X\n", val);
    return val;
}

EXPORT_SYMBOL_GPL(epp_read32);
u32 epp_read32(epp_t *epp_port) {
    uint32_t data;

    if (epp_port->wide) {
	data = inl(epp_port->port.base + EPP_DATA_OFFSET);
        DEBUG("read data 0x%08X\n", data);
    } else {
        uint8_t a, b, c, d;
        a = epp_read(epp_port);
        b = epp_read(epp_port);
        c = epp_read(epp_port);
        d = epp_read(epp_port);
        data = a + (b<<8) + (c<<16) + (d<<24);
    }

    return data;
}

EXPORT_SYMBOL_GPL(epp_write32);
void epp_write32(uint32_t w, epp_t *epp_port) {
    if (epp_port->wide) {
	outl(w, epp_port->port.base + EPP_DATA_OFFSET);
        DEBUG("wrote data 0x%08X\n", w);
    } else {
        epp_write((w) & 0xFF, epp_port);
        epp_write((w >>  8) & 0xFF, epp_port);
        epp_write((w >> 16) & 0xFF, epp_port);
        epp_write((w >> 24) & 0xFF, epp_port);
    }
}

EXPORT_SYMBOL_GPL(epp_read_status);
uint8_t epp_read_status(epp_t *epp_port) {
    uint8_t val;
    val = inb(epp_port->port.base + EPP_STATUS_OFFSET);
    DEBUG("read status 0x%02X\n", val);
    return val;
}

EXPORT_SYMBOL_GPL(epp_write_status);
void epp_write_status(uint8_t status_byte, epp_t *epp_port) {
    outb(status_byte, epp_port->port.base + EPP_STATUS_OFFSET);
    DEBUG("wrote status 0x%02X\n", status_byte);
}

EXPORT_SYMBOL_GPL(epp_write_control);
void epp_write_control(uint8_t control_byte, epp_t *epp_port) {
    outb(control_byte, epp_port->port.base + EPP_CONTROL_OFFSET);
    DEBUG("wrote control 0x%02X\n", control_byte);
}

// returns TRUE if there's a timeout
EXPORT_SYMBOL_GPL(epp_check_for_timeout);
int epp_check_for_timeout(epp_t *epp_port) {
    return (epp_read_status(epp_port) & 0x01);
}

EXPORT_SYMBOL_GPL(epp_clear_timeout);
int epp_clear_timeout(epp_t *epp_port) {
    uint8_t status;

    if (!epp_check_for_timeout(epp_port)) {
        return 1;
    }

    /* To clear timeout some chips require double read */
    (void)epp_read_status(epp_port);

    // read in the actual status register
    status = epp_read_status(epp_port);

    epp_write_status(status | 0x01, epp_port);  // Some reset by writing 1
    epp_write_status(status & 0xFE, epp_port);  // Others by writing 0

    if (epp_check_for_timeout(epp_port)) {
        PRINT("failed to clear EPP Timeout!\n");
        return 0;  // fail
    }
    return 1;  // success
}




//
// setup and cleanup code
//


int rtapi_app_main(void) {
    comp_id = hal_init("epp");
    if (comp_id < 0) return comp_id;

    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

