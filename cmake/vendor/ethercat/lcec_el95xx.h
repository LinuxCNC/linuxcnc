//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
#ifndef _LCEC_EL95XX_H_
#define _LCEC_EL95XX_H_

#include "lcec.h"

#define LCEC_EL95xx_VID LCEC_BECKHOFF_VID

#define LCEC_EL9505_PID 0x25213052
#define LCEC_EL9508_PID 0x25243052
#define LCEC_EL9510_PID 0x25263052
#define LCEC_EL9512_PID 0x25283052
#define LCEC_EL9515_PID 0x252b3052
#define LCEC_EL9576_PID 0x25683052

#define LCEC_EL95xx_PDOS 2

int lcec_el95xx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

