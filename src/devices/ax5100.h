//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
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
#ifndef _LCEC_AX5100_H_
#define _LCEC_AX5100_H_

#include "../lcec.h"
#include "../classes/class_ax5.h"

#define LCEC_AX5100_VID LCEC_BECKHOFF_VID
#define LCEC_AX5101_PID 0x13ed6012
#define LCEC_AX5103_PID 0x13ef6012
#define LCEC_AX5106_PID 0x13f26012
#define LCEC_AX5112_PID 0x13f86012
#define LCEC_AX5118_PID 0x13fe6012

int lcec_ax5100_preinit(struct lcec_slave *slave);
int lcec_ax5100_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

