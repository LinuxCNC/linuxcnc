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
#ifndef _LCEC_STMDS5K_H_
#define _LCEC_STMDS5K_H_

#include "lcec.h"

#define LCEC_STMDS5K_VID LCEC_STOEBER_VID
#define LCEC_STMDS5K_PID 0x00001388

#define LCEC_STMDS5K_PDOS 8
#define LCEC_STMDS5K_EXTINC_PDOS 1

#define LCEC_STMDS5K_PARAM_MULTITURN 1
#define LCEC_STMDS5K_PARAM_EXTENC 2

int lcec_stmds5k_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

