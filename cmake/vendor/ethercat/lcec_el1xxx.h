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
#ifndef _LCEC_EL1XXX_H_
#define _LCEC_EL1XXX_H_

#include "lcec.h"

#define LCEC_EL1xxx_VID LCEC_BECKHOFF_VID

#define LCEC_EL1002_PID 0x03EA3052
#define LCEC_EL1004_PID 0x03EC3052
#define LCEC_EL1008_PID 0x03F03052
#define LCEC_EL1012_PID 0x03F43052
#define LCEC_EL1014_PID 0x03F63052
#define LCEC_EL1018_PID 0x03FA3052
#define LCEC_EL1024_PID 0x04003052
#define LCEC_EL1034_PID 0x040A3052
#define LCEC_EL1084_PID 0x043C3052
#define LCEC_EL1088_PID 0x04403052
#define LCEC_EL1094_PID 0x04463052
#define LCEC_EL1098_PID 0x044A3052
#define LCEC_EL1104_PID 0x04503052
#define LCEC_EL1114_PID 0x045A3052
#define LCEC_EL1124_PID 0x04643052
#define LCEC_EL1134_PID 0x046E3052
#define LCEC_EL1144_PID 0x04783052
#define LCEC_EL1808_PID 0x07103052
#define LCEC_EL1809_PID 0x07113052
#define LCEC_EL1819_PID 0x071B3052

#define LCEC_EL1002_PDOS 2
#define LCEC_EL1004_PDOS 4
#define LCEC_EL1008_PDOS 8
#define LCEC_EL1012_PDOS 2
#define LCEC_EL1014_PDOS 4
#define LCEC_EL1018_PDOS 8
#define LCEC_EL1024_PDOS 4
#define LCEC_EL1034_PDOS 4
#define LCEC_EL1084_PDOS 4
#define LCEC_EL1088_PDOS 8
#define LCEC_EL1094_PDOS 4
#define LCEC_EL1098_PDOS 8
#define LCEC_EL1104_PDOS 4
#define LCEC_EL1114_PDOS 4
#define LCEC_EL1124_PDOS 4
#define LCEC_EL1134_PDOS 4
#define LCEC_EL1144_PDOS 4
#define LCEC_EL1808_PDOS 8
#define LCEC_EL1809_PDOS 16
#define LCEC_EL1819_PDOS 16

int lcec_el1xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

