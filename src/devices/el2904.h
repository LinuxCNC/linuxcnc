/**
 * @file el2904.h
 * @brief Header for the Beckhoff EL2904 4-channel TwinSAFE digital output
 *        terminal.
 *
 * The EL2904 is a safety-rated (SIL 3 / PLe) EtherCAT output terminal that
 * implements the Fail-Safe over EtherCAT (FsoE) protocol.  Safety output
 * states are governed by an FsoE master; this driver exposes both the FsoE
 * frame fields and the standard digital output commands as HAL pins.
 *
 * EtherCAT identifiers:
 *  - Vendor ID  : 0x00000002 (Beckhoff) — use ::LCEC_EL2904_VID
 *  - Product code: 0x0B583052            — use ::LCEC_EL2904_PID
 *
 * @copyright Copyright (C) 2018-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EL2904_H_
#define _LCEC_EL2904_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID for the EL2904. */
#define LCEC_EL2904_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product code for the EL2904. */
#define LCEC_EL2904_PID 0x0B583052

/** @brief Total number of PDO entries used by the EL2904 (FsoE + standard outputs). */
#define LCEC_EL2904_PDOS 14

/**
 * @brief Pre-initialization hook — configure FsoE parameters before PDO setup.
 * @param slave  Pointer to the lcec slave descriptor.
 * @return 0 on success.
 */
int lcec_el2904_preinit(struct lcec_slave *slave);

/**
 * @brief Initialize the EL2904 slave.
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the slave descriptor.
 * @param pdo_entry_regs PDO entry registration array (updated in place).
 * @return 0 on success, negative errno on failure.
 */
int lcec_el2904_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
