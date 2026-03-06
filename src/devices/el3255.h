//
//    Copyright (C) 2016 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file el3255.h
 * @brief Driver header for the Beckhoff EL3255 5-channel potentiometer input terminal.
 *
 * The EL3255 measures the position of up to five resistive potentiometers.
 * Each channel returns a 16-bit signed value proportional to the wiper
 * position.  The driver exposes per-channel overrange, underrange, sync-error,
 * and error HAL pins together with a user-scalable floating-point output value.
 *
 * The raw PDO value is scaled as:
 * @code
 *   val = bias + scale * (double)raw * (1.0 / 0x7fff)
 * @endcode
 */
#ifndef _LCEC_EL3255_H_
#define _LCEC_EL3255_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID for the EL3255. */
#define LCEC_EL3255_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL3255 5-channel potentiometer input terminal. */
#define LCEC_EL3255_PID 0x0CB73052

/** @brief Number of potentiometer input channels on the EL3255. */
#define LCEC_EL3255_CHANS 5
/** @brief Total number of EtherCAT PDO entries registered per slave (5 entries per channel). */
#define LCEC_EL3255_PDOS  (LCEC_EL3255_CHANS * 5)

/**
 * @brief Initialise an EL3255 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el3255_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

