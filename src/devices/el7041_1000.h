//
//    Copyright (C) 2015 Jakob Flierl  <jakob.flierl@gmail.com>
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
 * @file el7041_1000.h
 * @brief Driver header for the Beckhoff EL7041-1000 stepper motor terminal.
 *
 * The EL7041-1000 is a compact stepper motor terminal with an integrated
 * incremental encoder interface (up to 16-bit counter). It supports
 * velocity-mode motor control and provides encoder position feedback,
 * digital status inputs, and fault management.
 */
#ifndef _LCEC_EL7041_1000_H_
#define _LCEC_EL7041_1000_H_

#include "../lcec.h"

#define LCEC_EL7041_1000_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL7041_1000_PID 0x1B813052         /**< EL7041-1000 product ID */

#define LCEC_EL7041_1000_PDOS  34 /**< Total number of PDO entries for this terminal */

/**
 * @brief Initialise the EL7041-1000 HAL component and map PDOs.
 *
 * Allocates HAL memory, configures sync managers, maps encoder and stepper
 * motor PDO entries, and exports all HAL pins for both the encoder and
 * velocity-mode motor controller sub-devices.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7041_1000_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

