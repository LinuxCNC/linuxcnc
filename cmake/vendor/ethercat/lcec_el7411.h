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
#ifndef _LCEC_EL7411_H_
#define _LCEC_EL7411_H_

#include "lcec.h"
#include "lcec_el7211.h"

#define LCEC_EL7411_VID   LCEC_BECKHOFF_VID
#define LCEC_EL7411_PID   0x1Cf33052

#define LCEC_EL7411_PDOS  LCEC_EL7211_PDOS

#define LCEC_EL7411_PARAM_DCLINK_NOM     1
#define LCEC_EL7411_PARAM_DCLINK_MIN     2
#define LCEC_EL7411_PARAM_DCLINK_MAX     3
#define LCEC_EL7411_PARAM_MAX_CURR       4
#define LCEC_EL7411_PARAM_RATED_CURR     5
#define LCEC_EL7411_PARAM_RATED_VOLT     6
#define LCEC_EL7411_PARAM_POLE_PAIRS     7
#define LCEC_EL7411_PARAM_RESISTANCE     8
#define LCEC_EL7411_PARAM_INDUCTANCE     9
#define LCEC_EL7411_PARAM_TOURQUE_CONST  10
#define LCEC_EL7411_PARAM_VOLTAGE_CONST  11
#define LCEC_EL7411_PARAM_ROTOR_INERTIA  12
#define LCEC_EL7411_PARAM_MAX_SPEED      13
#define LCEC_EL7411_PARAM_RATED_SPEED    14
#define LCEC_EL7411_PARAM_TH_TIME_CONST  15
#define LCEC_EL7411_PARAM_HALL_VOLT      16
#define LCEC_EL7411_PARAM_HALL_ADJUST    17

int lcec_el7411_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

