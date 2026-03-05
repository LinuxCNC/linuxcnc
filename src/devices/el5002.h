//
//    Copyright (C) 2023 Sascha Ittner <sascha.ittner@modusoft.de>
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
#ifndef _LCEC_EL5002_H_
#define _LCEC_EL5002_H_

#include "../lcec.h"

#define LCEC_EL5002_VID LCEC_BECKHOFF_VID
#define LCEC_EL5002_PID 0x138a3052

#define LCEC_EL5002_CHANS 2
#define LCEC_EL5002_PDOS  (7 * LCEC_EL5002_CHANS)

#define LCEC_EL5002_PARAM_CH_MASK          0x000f
#define LCEC_EL5002_PARAM_FNK_MASK         0xfff0

#define LCEC_EL5002_PARAM_CH_0             0x0000
#define LCEC_EL5002_PARAM_CH_1             0x0001

#define LCEC_EL5002_PARAM_DIS_FRAME_ERR    0x0010
#define LCEC_EL5002_PARAM_EN_PWR_FAIL_CHK  0x0020
#define LCEC_EL5002_PARAM_EN_INHIBIT_TIME  0x0030
#define LCEC_EL5002_PARAM_CODING           0x0040
#define LCEC_EL5002_PARAM_BAUDRATE         0x0050
#define LCEC_EL5002_PARAM_CLK_JIT_COMP     0x0060
#define LCEC_EL5002_PARAM_FRAME_TYPE       0x0070
#define LCEC_EL5002_PARAM_FRAME_SIZE       0x0080
#define LCEC_EL5002_PARAM_DATA_LEN         0x0090
#define LCEC_EL5002_PARAM_MIN_INHIBIT_TIME 0x00a0
#define LCEC_EL5002_PARAM_NO_CLK_BURSTS    0x00b0

int lcec_el5002_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

