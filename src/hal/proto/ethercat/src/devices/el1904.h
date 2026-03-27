/**
 * @file el1904.h
 * @brief Driver for Beckhoff EL1904 4-channel TwinSAFE digital input terminal.
 *
 * Vendor ID: LCEC_BECKHOFF_VID (0x00000002)
 * Product ID: 0x07703052
 *
 * HAL pins exposed:
 *   - `<pfx>.fsoe-master-cmd`    (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE master command word.
 *   - `<pfx>.fsoe-master-crc`    (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE master CRC.
 *   - `<pfx>.fsoe-master-connid` (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE master connection ID.
 *   - `<pfx>.fsoe-slave-cmd`     (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE slave command word.
 *   - `<pfx>.fsoe-slave-crc`     (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE slave CRC.
 *   - `<pfx>.fsoe-slave-connid`  (GOMC_HAL_U32, GOMC_HAL_OUT): FSoE slave connection ID.
 *   - `<pfx>.fsoe-in-<n>`        (GOMC_HAL_BIT, GOMC_HAL_OUT): Safe digital input state (n=0..3).
 *   - `<pfx>.fsoe-in-<n>-not`    (GOMC_HAL_BIT, GOMC_HAL_OUT): Inverted safe digital input state.
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

#ifndef _LCEC_EL1904_H_
#define _LCEC_EL1904_H_

#include "../lcec.h"

/** @brief EtherCAT vendor ID for EL1904 (Beckhoff). */
#define LCEC_EL1904_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for EL1904. */
#define LCEC_EL1904_PID 0x07703052

/** @brief Number of TwinSAFE digital input channels on the EL1904. */
#define LCEC_EL1904_INPUT_COUNT 4

/** @brief Total number of PDO entries (6 FSoE + 1 per input channel). */
#define LCEC_EL1904_PDOS (6 + LCEC_EL1904_INPUT_COUNT * 1)

/**
 * @brief Pre-initialise the EL1904 slave (sets FSoE configuration).
 *
 * Must be called before lcec_el1904_init(). Attaches the static FSoE
 * configuration descriptor to the slave.
 *
 * @param slave Pointer to the lcec slave structure.
 * @return 0 on success.
 */
int lcec_el1904_preinit(struct lcec_slave *slave);

/**
 * @brief Initialize the EL1904 TwinSAFE digital input slave.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1904_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
