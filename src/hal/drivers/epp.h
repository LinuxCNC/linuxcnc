
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

#ifndef __EPP_H
#define __EPP_H


#include "hal_parport.h"

#define PRINT(fmt, args...)  rtapi_print("epp: " fmt, ## args);
#define DEBUG(fmt, args...)  if (debug_epp) { rtapi_print("epp: " fmt, ## args); }

#define EPP_STATUS_OFFSET   (1)
#define EPP_CONTROL_OFFSET  (2)
#define EPP_ADDRESS_OFFSET  (3)
#define EPP_DATA_OFFSET     (4)

#define ECP_CONFIG_A_HIGH_OFFSET  (0)
#define ECP_CONFIG_B_HIGH_OFFSET  (1)
#define ECP_CONTROL_HIGH_OFFSET   (2)


typedef struct {
    hal_parport_t port;
    int wide;
    int debug;
} epp_t;


int epp_get(epp_t *epp_port, int ioaddr, int ioaddr_hi, int wide);
void epp_release(epp_t *epp_port);

void epp_addr8(u8 addr, epp_t *epp_port);
void epp_addr16(u16 addr, epp_t *epp_port);
void epp_write(int w, epp_t *epp_port);
int epp_read(epp_t *epp_port);
u32 epp_read32(epp_t *epp_port);
void epp_write32(uint32_t w, epp_t *epp_port);
uint8_t epp_read_status(epp_t *epp_port);
void epp_write_status(uint8_t status_byte, epp_t *epp_port);
void epp_write_control(uint8_t control_byte, epp_t *epp_port);
int epp_check_for_timeout(epp_t *epp_port);
int epp_clear_timeout(epp_t *epp_port);


#endif

