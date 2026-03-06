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
 * @file el5122.h
 * @brief Driver for the Beckhoff EL5122 2-channel incremental encoder interface terminal.
 *
 * The EL5122 provides two independent A/B quadrature encoder channels, each
 * with a 32-bit position counter, external latch (positive and negative edge),
 * software-controlled gate, counter preset, overflow/underflow detection, and
 * direct readback of the A and B input states. Both channels share a common
 * HAL pin descriptor table parameterised by channel index.
 */
#ifndef _LCEC_EL5122_H_
#define _LCEC_EL5122_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5122. */
#define LCEC_EL5122_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5122. */
#define LCEC_EL5122_PID 0x14023052

/** @brief Number of encoder channels on the EL5122. */
#define LCEC_EL5122_CHANS 2
/** @brief Total number of PDO entries (13 per channel, both directions). */
#define LCEC_EL5122_PDOS  (13 * LCEC_EL5122_CHANS)

/**
 * @brief Initialise the EL5122 slave driver.
 *
 * Allocates HAL memory, registers PDO entries for both encoder channels
 * (input and output), and exports HAL pins for each channel including latch
 * control, gate, counter preset, and position output.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5122_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

