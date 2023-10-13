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
#ifndef _LCEC_EL5101_H_
#define _LCEC_EL5101_H_

#include "lcec.h"

#define LCEC_EL5101_VID LCEC_BECKHOFF_VID
#define LCEC_EL5101_PID 0x13ed3052

#define LCEC_EL5101_PDOS 8

#define LCEC_EL5101_PERIOD_SCALE    500e-9
#define LCEC_EL5101_FREQUENCY_SCALE 5e-2

int lcec_el5101_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

