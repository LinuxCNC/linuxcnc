/**
 * @file el2202.h
 * @brief Header for the Beckhoff EL2202 2-channel fast digital output terminal
 *        with tri-state.
 *
 * EtherCAT identifiers:
 *  - Vendor ID  : 0x00000002 (Beckhoff) — use ::LCEC_EL2202_VID
 *  - Product code: 0x089A3052            — use ::LCEC_EL2202_PID
 *
 * HAL pins exported per channel (N = 0 … LCEC_EL2202_CHANS-1):
 *  - @c dout-N   (GOMC_HAL_BIT, IN) — output logic level
 *  - @c tristate-N (GOMC_HAL_BIT, IN) — enable tri-state / high-Z on the output
 *
 * @copyright Copyright (C) 2015-2026 Claudio lorini <claudio.lorini@iit.it>
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

#ifndef _LCEC_EL2202_H_
#define _LCEC_EL2202_H_

/** @brief EtherCAT product code for the EL2202. */
#define LCEC_EL2202_PID 0x089A3052

/** @brief Number of output channels on the EL2202. */
#define LCEC_EL2202_CHANS 2

/** @brief Total number of PDO entries (2 per channel: Output + TriState). */
#define LCEC_EL2202_PDOS (2 * LCEC_EL2202_CHANS)

/** @brief Beckhoff vendor ID for the EL2202. */
#define LCEC_EL2202_VID LCEC_BECKHOFF_VID

/**
 * @brief Initialize the EL2202 slave.
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the slave descriptor.
 * @param pdo_entry_regs PDO entry registration array (updated in place).
 * @return 0 on success, negative errno on failure.
 */
int lcec_el2202_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
