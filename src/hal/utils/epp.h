
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


#include <stdint.h>
#include <sys/io.h>


#define EPP_SPP_DATA_OFFSET (0)
#define EPP_STATUS_OFFSET   (1)
#define EPP_CONTROL_OFFSET  (2)
#define EPP_ADDRESS_OFFSET  (3)
#define EPP_DATA_OFFSET     (4)

#define ECP_CONFIG_A_HIGH_OFFSET  (0)
#define ECP_CONFIG_B_HIGH_OFFSET  (1)
#define ECP_ECR_OFFSET            (2)


struct epp {
    uint16_t io_addr;
    uint16_t io_addr_hi;
};


void epp_init(struct epp *epp);
void epp_addr8(struct epp *epp, uint8_t addr);
void epp_addr16(struct epp *epp, uint16_t addr);
void epp_write(struct epp *epp, uint8_t val);
int epp_read(struct epp *epp);
uint8_t epp_read_status(struct epp *epp);
void epp_write_status(struct epp *epp, uint8_t status_byte);
void epp_write_control(struct epp *epp, uint8_t control_byte);
int epp_check_for_timeout(struct epp *epp);
int epp_clear_timeout(struct epp *epp);

