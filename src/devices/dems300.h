//
//    Copyright (C) 2021 Dominik Braun <dominik.braun@eventor.de>
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

/*#############################################################################

Description of Pins/Parameters:

Pins:
  FLOAT OUT vel-fb-rpm
    Actual velocity feddback in revolutions per minute.

  FLOAT OUT vel-fb-rpm-abs
    Actual absolute value of velocity feddback in
    revolutions per minute.

  FLOAT IN vel-rpm-cmd
    Current velocity command send to motor driver in
    revolutions per minute.

  BIT OUT stat-switch-on-ready
    ready statusword flag from motor driver

  BIT OUT stat-switched-on
    switched-on statusword flag from motor driver

  BIT OUT stat-op-enabled
    operation-enabled statusword flag from motor driver

  BIT OUT stat-fault
    fault statusword flag from motor driver

  BIT OUT stat-volt-enabled
    voltage-enabled statusword flag from motor driver

  BIT OUT stat-quick-stoped
    quick-stoped statusword flag from motor driver

  BIT OUT stat-switch-on-disabled
    switch-on-disabled statusword flag from motor driver

  BIT OUT stat-warning
    warning statusword flag from motor driver

  BIT OUT stat-remote
    motor driver is remote controlled statusword flag

  BIT OUT stat-at-speed
    motor has reached requested velocity

  
  BIT IN enable
    Switch motor driver on

  BIT IN quick-stop
    Enable controlword motor quick stop

  BIT IN fault-reset
    Reset motor driver error

  BIT IN halt
    Set controlword driver halt flag

  
Parameters:
  BIT IN auto-fault-reset
    If set to 1 errors get automatically reset if drive is enabled again (enable = 0 -> 1)

  
#############################################################################*/

#ifndef _LCEC_DEMS300_H_
#define _LCEC_DEMS300_H_

#include "../lcec.h"

#define LCEC_DEMS300_VID LCEC_DELTA_VID
#define LCEC_DEMS300_PID 0x10400200

#define LCEC_DEMS300_PDOS 11

int lcec_dems300_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

