/**
 * @file el40x1.h
 * @brief Driver for Beckhoff EL40x1 1-channel analog output terminals.
 *
 * Supports the following terminals:
 *  - EL4001: 1-channel 0–10 V analog output, 12-bit
 *  - EL4011: 1-channel 0–20 mA analog output, 12-bit
 *  - EL4021: 1-channel 4–20 mA analog output, 12-bit
 *  - EL4031: 1-channel ±10 V analog output, 12-bit
 *
 * A HAL float value is scaled and offset to produce a 16-bit signed
 * DAC word that is written to the EtherCAT process image each cycle.
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

#ifndef _LCEC_EL40X1_H_
#define _LCEC_EL40X1_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL40x1 terminals. */
#define LCEC_EL40x1_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the EL4001 (0–10 V, 1 ch). */
#define LCEC_EL4001_PID 0x0fa13052
/** @brief EtherCAT product ID for the EL4011 (0–20 mA, 1 ch). */
#define LCEC_EL4011_PID 0x0fab3052
/** @brief EtherCAT product ID for the EL4021 (4–20 mA, 1 ch). */
#define LCEC_EL4021_PID 0x0fb53052
/** @brief EtherCAT product ID for the EL4031 (±10 V, 1 ch). */
#define LCEC_EL4031_PID 0x0fbf3052

/** @brief Number of PDOs used by the EL40x1 (one output PDO). */
#define LCEC_EL40x1_PDOS  1

/**
 * @brief Initialize an EL40x1 slave device.
 *
 * Allocates HAL memory, registers PDO entries, exports HAL pins, and
 * installs the per-cycle write callback for the terminal.
 *
 * @param comp_id       LinuxCNC HAL component ID.
 * @param slave         Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el40x1_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
