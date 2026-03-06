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
 * @file el31x4.h
 * @brief Driver header for Beckhoff EL31x4 4-channel analog input terminals.
 *
 * Supports the following EtherCAT analog input terminals:
 *   - EL3104: 4-channel differential ±10 V input
 *   - EL3114: 4-channel differential 0..20 mA input
 *   - EL3124: 4-channel differential 4..20 mA input
 *   - EL3144: 4-channel single-ended 0..20 mA input
 *   - EL3154: 4-channel single-ended 4..20 mA input
 *   - EL3164: 4-channel single-ended 0..10 V input
 *
 * Similar to EL31x2 but provides four channels.  Each channel exposes
 * overrange, underrange, sync-error, and error HAL pins alongside a
 * scaled floating-point output value.  PDO status bits are mapped via
 * individual byte-offset/bit-position pairs to allow correct extraction
 * from the packed 16-bit status word.
 */
#ifndef _LCEC_EL31X4_H_
#define _LCEC_EL31X4_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL31x4 variants. */
#define LCEC_EL31x4_VID LCEC_BECKHOFF_VID

#define LCEC_EL3164_PID 0x0C5C3052  /**< Product ID for EL3164: 4-ch single-ended 0..10 V. */

/** @brief Number of analog input channels on each EL31x4 terminal. */
#define LCEC_EL31x4_CHANS 4

/** @brief Total number of EtherCAT PDO entries registered per slave (5 entries per channel). */
#define LCEC_EL31x4_PDOS  (5 * LCEC_EL31x4_CHANS)

/**
 * @brief Initialise an EL31x4 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el31x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

