
//
//  epp.[ch] - a userspace library that provides EPP access by direct i/o 
//
//  Copyright (C) 2007-2008 Sebastian Kuzminsky
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//


#include "epp.h"


void epp_addr8(struct epp *epp, uint8_t addr) {
    outb(addr, epp->io_addr + EPP_ADDRESS_OFFSET);
}

void epp_addr16(struct epp *epp, uint16_t addr) {
    outb((addr & 0x00FF), epp->io_addr + EPP_ADDRESS_OFFSET);
    outb((addr >> 8),     epp->io_addr + EPP_ADDRESS_OFFSET);
}

void epp_write(struct epp *epp, uint8_t val) {
    outb(val, epp->io_addr + EPP_DATA_OFFSET);
}

int epp_read(struct epp *epp) {
    return inb(epp->io_addr + EPP_DATA_OFFSET);
}

uint8_t epp_read_status(struct epp *epp) {
    return inb(epp->io_addr + EPP_STATUS_OFFSET);
}

void epp_write_status(struct epp *epp, uint8_t status_byte) {
    outb(status_byte, epp->io_addr + EPP_STATUS_OFFSET);
}

void epp_write_control(struct epp *epp, uint8_t control_byte) {
    outb(control_byte, epp->io_addr + EPP_CONTROL_OFFSET);
}

int epp_check_for_timeout(struct epp *epp) {
    return (epp_read_status(epp) & 0x01);
}

int epp_clear_timeout(struct epp *epp) {
    uint8_t status;

    if (!epp_check_for_timeout(epp)) {
        return 1;
    }

    /* To clear timeout some chips require double read */
    (void)epp_read_status(epp);

    // read in the actual status register
    status = epp_read_status(epp);

    epp_write_status(epp, status | 0x01);  // Some reset by writing 1
    epp_write_status(epp, status & 0xFE);  // Others by writing 0

    return !epp_check_for_timeout(epp);
}


void epp_init(struct epp *epp) {
    outb(0x80, epp->io_addr_hi + ECP_CONTROL_HIGH_OFFSET);  // select EPP mode in ECR
    epp_write_control(epp, 0x04);                           // set control lines and input mode
    epp_clear_timeout(epp);
}

