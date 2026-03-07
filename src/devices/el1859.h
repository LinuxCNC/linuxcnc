/**
 * @file el1859.h
 * @brief Driver for Beckhoff EL1859 8-channel digital input + 8-channel digital output combo terminal.
 *
 * Vendor ID: LCEC_BECKHOFF_VID (0x00000002)
 * Product ID: 0x07433052
 *
 * HAL pins exposed per channel (0–7):
 *   - `<pfx>.din-<n>`        (HAL_BIT, HAL_OUT): Digital input state.
 *   - `<pfx>.din-<n>-not`    (HAL_BIT, HAL_OUT): Inverted digital input state.
 *   - `<pfx>.dout-<n>`       (HAL_BIT, HAL_IN):  Digital output command.
 *   - `<pfx>.dout-<n>-invert`(HAL_BIT, HAL_RW param): Invert output polarity when set.
 *
 * @copyright Copyright (C) 2011-2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _LCEC_EL1859_H_
#define _LCEC_EL1859_H_

#include "../lcec.h"

/** @brief EtherCAT vendor ID for EL1859 (Beckhoff). */
#define LCEC_EL1859_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for EL1859. */
#define LCEC_EL1859_PID 0x07433052

/** @brief Number of input/output channel pairs on the EL1859. */
#define LCEC_EL1859_PINS 8
/** @brief Total number of PDO entries (input + output per channel). */
#define LCEC_EL1859_PDOS (2 * LCEC_EL1859_PINS)

/**
 * @brief Initialize the EL1859 digital I/O combo slave.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1859_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
