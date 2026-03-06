//
//    Copyright (C) 2024 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

/**
 * @file el32x4.h
 * @brief Driver header for Beckhoff EL32x4 4-channel analog input terminals.
 *
 * Supports EL3204 and compatible 4-channel analog input terminals.
 * Each channel provides a 16-bit signed measurement value together with
 * overrange, underrange, error, TxPDO-state, and TxPDO-toggle status HAL pins.
 *
 * The raw PDO value is converted to floating-point using a factor of 0.1:
 * @code
 *   val = bias + scale * (double)raw * 0.1
 * @endcode
 * allowing the output to represent, for example, a temperature in tenths
 * of a degree Celsius depending on the connected sensor type.
 */
#ifndef _LCEC_EL32X4_H_
#define _LCEC_EL32X4_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL32x4 variants. */
#define LCEC_EL32x4_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the EL3204 4-channel analog input terminal. */
#define LCEC_EL3204_PID 0x0C843052

/** @brief Number of analog input channels on each EL32x4 terminal. */
#define LCEC_EL32x4_CHANS 4

/** @brief Total number of EtherCAT PDO entries registered per slave (6 entries per channel). */
#define LCEC_EL32x4_PDOS  (6 * LCEC_EL32x4_CHANS)

/**
 * @brief Initialise an EL32x4 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el32x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

