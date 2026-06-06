/**
 * @file el1918_logic.h
 * @brief Driver for Beckhoff EL1918 TwinSAFE logic terminal.
 *
 * The EL1918 is a programmable TwinSAFE logic unit that bridges multiple FSoE
 * slaves to a safety PLC. It exposes FSoE command/CRC/connection-ID fields for
 * each connected FSoE slave, as well as standard I/O bits configurable through
 * module parameters.
 *
 * Vendor ID: LCEC_BECKHOFF_VID (0x00000002)
 * Product ID: 0x077e3052
 *
 * Module parameters (lcec_slave_modparam_t):
 *   - LCEC_EL1918_LOGIC_PARAM_SLAVEID     (1): EtherCAT index of a connected FSoE slave.
 *   - LCEC_EL1918_LOGIC_PARAM_STDIN_NAME  (2): HAL pin name for a standard input bit.
 *   - LCEC_EL1918_LOGIC_PARAM_STDOUT_NAME (3): HAL pin name for a standard output bit.
 *
 * @copyright Copyright (C) 2021-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EL1918_LOGIC_H_
#define _LCEC_EL1918_LOGIC_H_

#include "../lcec.h"

/** @brief EtherCAT vendor ID for EL1918 (Beckhoff). */
#define LCEC_EL1918_LOGIC_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for EL1918 TwinSAFE logic terminal. */
#define LCEC_EL1918_LOGIC_PID 0x077e3052

/** @brief Base number of PDO entries (state + cycle counter). */
#define LCEC_EL1918_LOGIC_PDOS 2
/** @brief PDO entries added when standard inputs are configured. */
#define LCEC_EL1918_LOGIC_STDIN_PDOS 1
/** @brief PDO entries added when standard outputs are configured. */
#define LCEC_EL1918_LOGIC_STDOUT_PDOS 1

/** @brief Module parameter ID: EtherCAT slave index of a connected FSoE slave. */
#define LCEC_EL1918_LOGIC_PARAM_SLAVEID      1
/** @brief Module parameter ID: HAL pin name for a standard input bit. */
#define LCEC_EL1918_LOGIC_PARAM_STDIN_NAME   2
/** @brief Module parameter ID: HAL pin name for a standard output bit. */
#define LCEC_EL1918_LOGIC_PARAM_STDOUT_NAME  3

/** @brief PDO entries added per FSoE slave connection (cmd + connid). */
#define LCEC_EL1918_LOGIC_PARAM_SLAVE_PDOS    4
/** @brief PDO entries added per FSoE data channel (slave-crc + master-crc). */
#define LCEC_EL1918_LOGIC_PARAM_SLAVE_CH_PDOS 2

/** @brief Maximum number of configurable standard I/O pins. */
#define LCEC_EL1918_LOGIC_DIO_MAX_COUNT 8

/**
 * @brief Pre-initialise the EL1918 logic terminal slave.
 *
 * Counts module parameters to determine the total PDO entry count before
 * the EtherCAT master is started.
 *
 * @param slave Pointer to the lcec slave structure.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1918_logic_preinit(struct lcec_slave *slave);

/**
 * @brief Initialize the EL1918 TwinSAFE logic terminal slave.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1918_logic_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
