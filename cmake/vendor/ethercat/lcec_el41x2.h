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
#ifndef _LCEC_EL41X2_H_
#define _LCEC_EL41X2_H_

#include "lcec.h"

#define LCEC_EL41x2_VID LCEC_BECKHOFF_VID

#define LCEC_EL4102_PID 0x10063052
#define LCEC_EL4112_PID 0x10103052
#define LCEC_EL4122_PID 0x101A3052
#define LCEC_EL4132_PID 0x10243052

#define LCEC_EL41x2_PDOS  2

#define LCEC_EL41x2_CHANS 2

int lcec_el41x2_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

