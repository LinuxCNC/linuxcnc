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
#ifndef _LCEC_EL2521_H_
#define _LCEC_EL2521_H_

/**
 * @file el2521.h
 * @brief Header for the Beckhoff EL2521 1-channel pulse train output terminal.
 *
 * The EL2521 generates a frequency-controlled pulse train suitable for driving
 * stepper motor controllers.  The output frequency and direction are set via
 * EtherCAT PDO; the internal ramp function limits acceleration.
 *
 * EtherCAT identifiers:
 *  - Vendor ID  : 0x00000002 (Beckhoff) — use ::LCEC_EL2521_VID
 *  - Product code: 0x09D93052            — use ::LCEC_EL2521_PID
 *
 * @note The following SDO (CoE) settings must remain at their factory defaults
 *       for this driver to operate correctly:
 *       - Sign/amount representation (0x8000:04) = FALSE (0x00)
 *       - Ramp function (0x8000:06) = active (0x01) — can be disabled via HAL
 *       - Direct input mode (0x8000:08) = FALSE (0x00)
 *       - Travel distance control (0x8000:0A) = FALSE (0x00)
 */

#include "../lcec.h"

/** @brief Beckhoff vendor ID for the EL2521. */
#define LCEC_EL2521_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product code for the EL2521. */
#define LCEC_EL2521_PID 0x09d93052

/** @brief Total number of PDO entries used by the EL2521 (2 input + 2 output). */
#define LCEC_EL2521_PDOS  4

/**
 * @brief Initialize the EL2521 slave.
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the slave descriptor.
 * @param pdo_entry_regs PDO entry registration array (updated in place).
 * @return 0 on success, negative errno on failure.
 */
int lcec_el2521_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

