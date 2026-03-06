//
//    Copyright (C) 2014 Sascha Ittner <sascha.ittner@modusoft.de>
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
  FLOAT OUT srv-vel-fb
    Actual velocity feddback in scale units per second.

  FLOAT OUT srv-vel-fb-rpm
    Actual velocity feddback in revolutions per minute.

  FLOAT OUT srv-vel-fb-rpm-abs
    Actual absolute value of velocity feddback in
    revolutions per minute.

  FLOAT OUT srv-vel-rpm
    Current velocity command send to motor driver in
    revolutions per minute.

  BIT OUT srv-ready
    ready status flag from motor driver

  BIT OUT srv-switched-on
    switched-on status flag from motor driver

  BIT OUT srv-oper-enabled
    operation-enabled status flag from motor driver

  BIT OUT srv-fault
    fault status flag from motor driver

  BIT OUT srv-volt-enabled
    voltage-enabled status flag from motor driver

  BIT OUT srv-quick-stoped
    quick-stoped status flag from motor driver

  BIT OUT srv-on-disabled
    on-disabled status flag from motor driver

  BIT OUT srv-warning
    warning status flag from motor driver

  BIT OUT srv-remote
    motor driver is remote controlled

  BIT OUT srv-at-speed
    motor has reached requested velocity

  BIT OUT srv-limit-active
    motor driver has reached some limit.

  BIT OUT srv-zero-speed
    motor has stopped

  S32 OUT srv-enc-raw
    raw value from motor encoder without offset calculation. This
    value is the unmodified one ad repoted by the motor driver.

  U32 OUT srv-pos-raw-hi
    Upper 32 bits of range extended 64 bit encoder value. This is a relative
    value wich is reset on restart/reset pin trigger.

  U32 OUT srv-pos-raw-lo
    Lower 32 bits of range extended 64 bit encoder value. This is a relative
    value wich is reset on restart/reset pin trigger.

  FLOAT OUT srv-pos-fb
    Actual encoder position

  BIT OUT srv-on-home-neg
    Motor encoder is below virtual home switch position. (See parameter
    srv-home-raw)

  BIT OUT srv-on-home-pos
    Motor encoder is above virtual home switch position. (See parameter
    srv-home-raw)

  BIT IN srv-pos-reset
    Reset relative position counter (srv-pos-raw-hi/srv-pos-raw-lo)

  BIT IN srv-switch-on
    Switch motor driver on

  BIT IN srv-enable-volt
    Enable motor driver voltage

  BIT IN srv-quick-stop
    Enable motor quick stop

  BIT IN srv-enable
    Enable motor driver operation. This signal is internaly delayed
    until driver reports switched-on.

  BIT IN srv-fault-reset
    Reset motor driver error

  BIT IN srv-fault-autoreset
    If set to 1 errors get automatically reset if drive is disabled (enable = 0)

  BIT IN srv-halt
    Set motor driver halt flag

  FLOAT IN srv-vel-cmd
    Velocity command input in scale units per second.

Parameters:
  RW FLOAT srv-pos-scale
    Scale for position/velocity values (1.0 -> 1 revolution per second)

  RW S32 srv-home-raw
    Absolute position of virtual home switch. This could be used to implement
    a home switch on motors with abolute encoders without a hardware switch.

#############################################################################*/

/**
 * @file deasda.h
 * @brief Driver interface for the Delta ASDA-A2 EtherCAT servo drive.
 *
 * The Delta ASDA-A2 is a velocity-controlled servo drive implementing the
 * CiA-402 profile via a vendor-specific PDO set.  The driver configures the
 * drive for cyclic synchronous velocity (CSV) mode at startup and maps:
 *   - Output PDO: CiA-402 control word (0x6040) and target velocity (0x60FF).
 *   - Input PDO:  CiA-402 status word (0x6041), actual velocity (0x606C),
 *                 actual position (0x6064), and external encoder (0x2511).
 *
 * HAL pins expose velocity command/feedback (in scale units/s and RPM), all
 * CiA-402 status bits, and control inputs including a built-in automatic
 * fault-reset feature with configurable retry count and inter-reset delay.
 */
#ifndef _LCEC_DEASDA_H_
#define _LCEC_DEASDA_H_

#include "../lcec.h"

/** @brief Delta vendor ID. */
#define LCEC_DEASDA_VID LCEC_DELTA_VID
/** @brief EtherCAT product ID for the ASDA-A2 servo drive. */
#define LCEC_DEASDA_PID 0x10305070

/** @brief Number of PDO entries used by this driver. */
#define LCEC_DEASDA_PDOS 6

/**
 * @brief Initialise the ASDA-A2 HAL component and PDO mappings.
 *
 * Configures the drive for CSV mode via SDO writes, sets the interpolation
 * time period, registers PDO entries, exports HAL pins and parameters, and
 * initialises the main and external encoder subclasses.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_deasda_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

