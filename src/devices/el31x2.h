//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file el31x2.h
 * @brief Driver header for Beckhoff EL31x2 2-channel analog input terminals.
 *
 * Supports the following EtherCAT analog input terminals:
 *   - EL3102: 2-channel differential ±10 V input
 *   - EL3112: 2-channel differential 0..20 mA input
 *   - EL3122: 2-channel differential 4..20 mA input
 *   - EL3142: 2-channel single-ended 0..20 mA input
 *   - EL3152: 2-channel single-ended 4..20 mA input
 *   - EL3162: 2-channel single-ended 0..10 V input
 *
 * Each channel exposes overrange, underrange, and error HAL pins
 * alongside a scaled floating-point output value derived from the
 * 16-bit signed ADC reading.
 */
#ifndef _LCEC_EL31X2_H_
#define _LCEC_EL31X2_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL31x2 variants. */
#define LCEC_EL31x2_VID LCEC_BECKHOFF_VID

#define LCEC_EL3102_PID 0x0C1E3052  /**< Product ID for EL3102: 2-ch differential ±10 V. */
#define LCEC_EL3112_PID 0x0C283052  /**< Product ID for EL3112: 2-ch differential 0..20 mA. */
#define LCEC_EL3122_PID 0x0C323052  /**< Product ID for EL3122: 2-ch differential 4..20 mA. */
#define LCEC_EL3142_PID 0x0C463052  /**< Product ID for EL3142: 2-ch single-ended 0..20 mA. */
#define LCEC_EL3152_PID 0x0C503052  /**< Product ID for EL3152: 2-ch single-ended 4..20 mA. */
#define LCEC_EL3162_PID 0x0C5A3052  /**< Product ID for EL3162: 2-ch single-ended 0..10 V. */

/** @brief Total number of EtherCAT PDO entries registered per slave (status + value per channel × 2 channels). */
#define LCEC_EL31x2_PDOS  4

/** @brief Number of analog input channels on each EL31x2 terminal. */
#define LCEC_EL31x2_CHANS 2

/**
 * @brief Initialise an EL31x2 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el31x2_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

