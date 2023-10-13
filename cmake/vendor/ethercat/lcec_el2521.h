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
#ifndef _LCEC_EL2521_H_
#define _LCEC_EL2521_H_

// ****************************************************************************
// CONFIG ISSUES:
// - sign/amount representation (8000:04) must be FALSE (0x00, default)
// - ramp function (8000:06) need to be active (0x01, default), can be disabled by hal pin
// - direct input mode (8000:08) must be FALSE (0x00, default)
// - travel distance control active (8000:0A) must be FALSE (0x00, default)
// ****************************************************************************

#include "lcec.h"

#define LCEC_EL2521_VID LCEC_BECKHOFF_VID
#define LCEC_EL2521_PID 0x09d93052

#define LCEC_EL2521_PDOS  4

int lcec_el2521_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

