/**
 * @file el41x2.h
 * @brief Driver for Beckhoff EL41x2 2-channel analog output terminals (extended range).
 *
 * Supports the following terminals:
 *  - EL4102: 2-channel 0–10 V analog output, 16-bit
 *  - EL4112: 2-channel 0–20 mA analog output, 16-bit
 *  - EL4122: 2-channel 4–20 mA analog output, 16-bit
 *  - EL4132: 2-channel ±10 V analog output, 16-bit
 *
 * Unlike the EL40x2, the EL41x2 uses CoE object indices 0x3001/0x3002 for
 * its PDO channel mappings instead of the 0x70xx scheme.  Each channel
 * independently scales a HAL float value to a 16-bit signed DAC word
 * written to the EtherCAT process image each cycle.
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

#ifndef _LCEC_EL41X2_H_
#define _LCEC_EL41X2_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL41x2 terminals. */
#define LCEC_EL41x2_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the EL4102 (0–10 V, 2 ch, 16-bit). */
#define LCEC_EL4102_PID 0x10063052
/** @brief EtherCAT product ID for the EL4112 (0–20 mA, 2 ch, 16-bit). */
#define LCEC_EL4112_PID 0x10103052
/** @brief EtherCAT product ID for the EL4122 (4–20 mA, 2 ch, 16-bit). */
#define LCEC_EL4122_PID 0x101A3052
/** @brief EtherCAT product ID for the EL4132 (±10 V, 2 ch, 16-bit). */
#define LCEC_EL4132_PID 0x10243052

/** @brief Number of PDOs used by the EL41x2 (one per channel). */
#define LCEC_EL41x2_PDOS  2

/** @brief Number of analog output channels on an EL41x2 terminal. */
#define LCEC_EL41x2_CHANS 2

/**
 * @brief Initialize an EL41x2 slave device.
 *
 * Allocates HAL memory, registers PDO entries for both channels, exports
 * HAL pins, and installs the per-cycle write callback for the terminal.
 *
 * @param comp_id       LinuxCNC HAL component ID.
 * @param slave         Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el41x2_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
