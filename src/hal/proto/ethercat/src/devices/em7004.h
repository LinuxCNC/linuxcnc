/**
 * @file em7004.h
 * @brief Driver header for the Beckhoff EM7004 4-channel stepper motor module.
 *
 * The EM7004 is a multi-function stepper module with:
 * - 16 digital inputs (0x6000)
 * - 16 digital outputs (0x7010)
 * - 4 analog outputs with 16-bit resolution (0x7020–0x7050)
 * - 4 incremental encoder interfaces with latch support (0x6060–0x6090 / 0x7060–0x7090)
 *
 * Each encoder supports preset, external latch on rising/falling edge, and
 * exposes position as a scaled floating-point value.
 *
 * @copyright Copyright (C) 2014-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EM7004_H_
#define _LCEC_EM7004_H_

#include "../lcec.h"

#define LCEC_EM7004_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EM7004_PID 0x1B5C3452         /**< EM7004 product ID */

#define LCEC_EM7004_DIN_COUNT  16  /**< Number of digital input channels */
#define LCEC_EM7004_DOUT_COUNT 16  /**< Number of digital output channels */
#define LCEC_EM7004_AOUT_COUNT 4   /**< Number of analog output channels */
#define LCEC_EM7004_ENC_COUNT  4   /**< Number of encoder channels */

/** Total PDO entry count (DIN + DOUT + AOUT + 12 per encoder channel). */
#define LCEC_EM7004_PDOS (LCEC_EM7004_DIN_COUNT + LCEC_EM7004_DOUT_COUNT + LCEC_EM7004_AOUT_COUNT + (LCEC_EM7004_ENC_COUNT * 12))

/**
 * @brief Initialise the EM7004 stepper motor module.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_em7004_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
