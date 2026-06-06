/**
 * @file el41x4.h
 * @brief Driver for Beckhoff EL41x4 4-channel analog output terminals (extended range).
 *
 * Supports the following terminals:
 *  - EL4104: 4-channel 0–10 V analog output, 16-bit
 *  - EL4134: 4-channel ±10 V analog output, 16-bit
 *
 * Each of the 4 channels independently scales a HAL float value to a
 * 16-bit signed DAC word written to the EtherCAT process image each cycle.
 * PDO channel mapping uses the 0x70xx CoE object scheme (same as EL40x8).
 *
 * @copyright Copyright (C) 2016-2026 Frank Brossette<frank.brossette@gmail.com>
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

#ifndef _LCEC_EL41X4_H_
#define _LCEC_EL41X4_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL41x4 terminals. */
#define LCEC_EL41x4_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the EL4104 (0–10 V, 4 ch, 16-bit). */
#define LCEC_EL4104_PID 0x10083052
/** @brief EtherCAT product ID for the EL4134 (±10 V, 4 ch, 16-bit). */
#define LCEC_EL4134_PID 0x10263052

/** @brief Number of PDOs used by the EL41x4 (one per channel). */
#define LCEC_EL41x4_PDOS  4

/** @brief Number of analog output channels on an EL41x4 terminal. */
#define LCEC_EL41x4_CHANS 4

/**
 * @brief Initialize an EL41x4 slave device.
 *
 * Allocates HAL memory, registers PDO entries for all 4 channels, exports
 * HAL pins, and installs the per-cycle write callback for the terminal.
 *
 * @param comp_id       LinuxCNC HAL component ID.
 * @param slave         Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el41x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
