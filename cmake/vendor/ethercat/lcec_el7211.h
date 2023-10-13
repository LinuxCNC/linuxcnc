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
#ifndef _LCEC_EL7211_H_
#define _LCEC_EL7211_H_

#include "lcec.h"

#define LCEC_EL7211_VID      LCEC_BECKHOFF_VID
#define LCEC_EL7211_PID      0x1C2B3052
#define LCEC_EL7221_PID      0x1C353052
#define LCEC_EL7201_9014_PID 0x1C213052

#define LCEC_EL7211_PDOS      5
#define LCEC_EL7201_9014_PDOS 6

int lcec_el7211_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);
int lcec_el7201_9014_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

