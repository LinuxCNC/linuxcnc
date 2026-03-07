/**
 * @file el5021.h
 * @brief Driver for the Beckhoff EL5021 1-channel SinCos encoder interface terminal.
 *
 * The EL5021 accepts analog sine/cosine signals from an incremental SinCos
 * encoder and converts them into a 32-bit position counter value. It supports
 * a reference-mark latch (input C), counter preset, frequency/amplitude error
 * detection, and a TxPDO toggle bit for data-freshness monitoring.
 *
 * @copyright Copyright (C) 2024-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EL5021_H_
#define _LCEC_EL5021_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5021. */
#define LCEC_EL5021_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5021. */
#define LCEC_EL5021_PID 0x139d3052

/** @brief Total number of PDO entries for the EL5021 (input + output). */
#define LCEC_EL5021_PDOS 12

/**
 * @brief Initialise the EL5021 slave driver.
 *
 * Allocates HAL memory, registers PDO entries, exports HAL pins, and
 * installs the read/write callbacks for the SinCos encoder terminal.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5021_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
