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
#ifndef _LCEC_EL2XXX_H_
#define _LCEC_EL2XXX_H_

#include "lcec.h"

#define LCEC_EL2xxx_VID LCEC_BECKHOFF_VID

#define LCEC_EL2002_PID 0x07D23052
#define LCEC_EL2004_PID 0x07D43052
#define LCEC_EL2008_PID 0x07D83052
#define LCEC_EL2022_PID 0x07E63052
#define LCEC_EL2024_PID 0x07E83052
#define LCEC_EL2032_PID 0x07F03052
#define LCEC_EL2034_PID 0x07F23052
#define LCEC_EL2042_PID 0x07FA3052
#define LCEC_EL2084_PID 0x08243052
#define LCEC_EL2088_PID 0x08283052
#define LCEC_EL2124_PID 0x084C3052
#define LCEC_EL2612_PID 0x0A343052
#define LCEC_EL2622_PID 0x0A3E3052
#define LCEC_EL2634_PID 0x0A4A3052
#define LCEC_EL2652_PID 0x0A5C3052
#define LCEC_EL2808_PID 0x0AF83052
#define LCEC_EL2798_PID 0x0AEE3052
#define LCEC_EL2809_PID 0x0AF93052
#define LCEC_EP2008_PID 0x07D84052
#define LCEC_EP2028_PID 0x07EC4052
#define LCEC_EP2809_PID 0x0AF94052

#define LCEC_EL2002_PDOS 2
#define LCEC_EL2004_PDOS 4
#define LCEC_EL2008_PDOS 8
#define LCEC_EL2022_PDOS 2
#define LCEC_EL2024_PDOS 4
#define LCEC_EL2032_PDOS 2
#define LCEC_EL2034_PDOS 4
#define LCEC_EL2042_PDOS 2
#define LCEC_EL2084_PDOS 4
#define LCEC_EL2088_PDOS 8
#define LCEC_EL2124_PDOS 4
#define LCEC_EL2612_PDOS 2
#define LCEC_EL2622_PDOS 2
#define LCEC_EL2634_PDOS 4
#define LCEC_EL2652_PDOS 2
#define LCEC_EL2808_PDOS 8
#define LCEC_EL2798_PDOS 8
#define LCEC_EL2809_PDOS 16
#define LCEC_EP2008_PDOS 8
#define LCEC_EP2028_PDOS 8
#define LCEC_EP2809_PDOS 16

int lcec_el2xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

