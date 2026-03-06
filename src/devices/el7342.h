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
/**
 * @file el7342.h
 * @brief Driver header for the Beckhoff EL7342 2-channel DC motor terminal.
 *
 * The EL7342 provides two independent DC motor drive channels, each with
 * an integrated incremental encoder interface and a 16-bit signed velocity
 * setpoint PDO.  Each channel also supports two synchronous information
 * words that can be selected to report quantities such as motor voltage,
 * current, duty cycle, velocity, or temperature.
 */
#ifndef _LCEC_EL7342_H_
#define _LCEC_EL7342_H_

#include "../lcec.h"

#define LCEC_EL7342_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL7342_PID 0x1cae3052         /**< EL7342 product ID */

#define LCEC_EL7342_CHANS 2                                  /**< Number of independent motor channels */
#define LCEC_EL7342_PDOS  (33 * LCEC_EL7342_CHANS)          /**< Total PDO entry count (33 per channel) */

/**
 * @brief Initialise the EL7342 2-channel DC motor terminal.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7342_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

