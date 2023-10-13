//
//    Copyright (C) 2019 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_OMRG5_H_
#define _LCEC_OMRG5_H_

#include "lcec.h"

#define LCEC_OMRG5_VID LCEC_OMRON_VID
#define LCEC_OMRG5_R88D_KNA5L_ECT_PID 0x00000001
#define LCEC_OMRG5_R88D_KN01L_ECT_PID 0x00000002
#define LCEC_OMRG5_R88D_KN02L_ECT_PID 0x00000003
#define LCEC_OMRG5_R88D_KN04L_ECT_PID 0x00000004
#define LCEC_OMRG5_R88D_KN01H_ECT_PID 0x00000005
#define LCEC_OMRG5_R88D_KN02H_ECT_PID 0x00000006
#define LCEC_OMRG5_R88D_KN04H_ECT_PID 0x00000007
#define LCEC_OMRG5_R88D_KN08H_ECT_PID 0x00000008
#define LCEC_OMRG5_R88D_KN10H_ECT_PID 0x00000009
#define LCEC_OMRG5_R88D_KN15H_ECT_PID 0x0000000A
#define LCEC_OMRG5_R88D_KN20H_ECT_PID 0x00000056
#define LCEC_OMRG5_R88D_KN30H_ECT_PID 0x00000057
#define LCEC_OMRG5_R88D_KN50H_ECT_PID 0x00000058
#define LCEC_OMRG5_R88D_KN75H_ECT_PID 0x00000059
#define LCEC_OMRG5_R88D_KN150H_ECT_PID 0x0000005A
#define LCEC_OMRG5_R88D_KN06F_ECT_PID 0x0000000B
#define LCEC_OMRG5_R88D_KN10F_ECT_PID 0x0000000C
#define LCEC_OMRG5_R88D_KN15F_ECT_PID 0x0000000D
#define LCEC_OMRG5_R88D_KN20F_ECT_PID 0x0000005B
#define LCEC_OMRG5_R88D_KN30F_ECT_PID 0x0000005C
#define LCEC_OMRG5_R88D_KN50F_ECT_PID 0x0000005D
#define LCEC_OMRG5_R88D_KN75F_ECT_PID 0x0000005E
#define LCEC_OMRG5_R88D_KN150F_ECT_PID 0x0000005F

#define LCEC_OMRG5_PDOS 13

int lcec_omrg5_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

