/**
 * @file el3314.h
 * @brief Driver header for the Beckhoff EL3314 4-channel thermocouple input terminal.
 *
 * The EL3314 reads temperature from up to four type-K (or other type, depending
 * on firmware configuration) thermocouples via EtherCAT.  Each channel delivers
 * a 16-bit signed value with a resolution of 0.1 °C per LSB.
 *
 * The floating-point HAL output is calculated as:
 * @code
 *   val = bias + scale * (double)raw * 0.1
 * @endcode
 * so with default scale=1.0 and bias=0.0 the pin value is the temperature in °C.
 *
 * @copyright Copyright (C) 2016-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EL3314_H_
#define _LCEC_EL3314_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID for the EL3314. */
#define LCEC_EL3314_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL3314 4-channel thermocouple input terminal. */
#define LCEC_EL3314_PID 0x0CF23052

/** @brief Number of thermocouple input channels on the EL3314. */
#define LCEC_EL3314_CHANS 4
/** @brief Total number of EtherCAT PDO entries registered per slave (4 entries per channel). */
#define LCEC_EL3314_PDOS  (LCEC_EL3314_CHANS * 4)

/**
 * @brief Initialise an EL3314 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el3314_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
