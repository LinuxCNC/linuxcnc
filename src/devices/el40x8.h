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
 * @file el40x8.h
 * @brief Driver for Beckhoff EL40x8 8-channel analog output terminals.
 *
 * Supports the following terminals:
 *  - EL4008: 8-channel 0–10 V analog output, 12-bit
 *  - EL4018: 8-channel 0–20 mA analog output, 12-bit
 *  - EL4028: 8-channel 4–20 mA analog output, 12-bit
 *  - EL4038: 8-channel ±10 V analog output, 12-bit
 *
 * Each of the 8 channels independently scales a HAL float value to a
 * 16-bit signed DAC word written to the EtherCAT process image each cycle.
 */
#ifndef _LCEC_EL40X8_H_
#define _LCEC_EL40X8_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL40x8 terminals. */
#define LCEC_EL40x8_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the EL4008 (0–10 V, 8 ch). */
#define LCEC_EL4008_PID 0x0fa83052
/** @brief EtherCAT product ID for the EL4018 (0–20 mA, 8 ch). */
#define LCEC_EL4018_PID 0x0fb23052
/** @brief EtherCAT product ID for the EL4028 (4–20 mA, 8 ch). */
#define LCEC_EL4028_PID 0x0fbc3052
/** @brief EtherCAT product ID for the EL4038 (±10 V, 8 ch). */
#define LCEC_EL4038_PID 0x0fc63052

/** @brief Number of PDOs used by the EL40x8 (one per channel). */
#define LCEC_EL40x8_PDOS  8

/** @brief Number of analog output channels on an EL40x8 terminal. */
#define LCEC_EL40x8_CHANS 8

/**
 * @brief Initialize an EL40x8 slave device.
 *
 * Allocates HAL memory, registers PDO entries for all 8 channels, exports
 * HAL pins, and installs the per-cycle write callback for the terminal.
 *
 * @param comp_id       LinuxCNC HAL component ID.
 * @param slave         Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el40x8_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

