/**
 * @file el5032.h
 * @brief Driver for the Beckhoff EL5032 2-channel EnDat 2.2 encoder interface terminal.
 *
 * The EL5032 communicates with Heidenhain EnDat 2.2 absolute encoders over a
 * differential serial link. Each channel delivers a 64-bit absolute position
 * value along with status bits (warning, error, ready, diagnostic) and a
 * 2-bit cycle counter. Both relative (incremental) and absolute position
 * modes are supported through the abs-mode HAL pin.
 *
 * @copyright Copyright (C) 2023-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EL5032_H_
#define _LCEC_EL5032_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5032. */
#define LCEC_EL5032_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5032. */
#define LCEC_EL5032_PID 0x13a83052

/** @brief Number of EnDat encoder channels on the EL5032. */
#define LCEC_EL5032_CHANS 2
/** @brief Total number of PDO entries (7 per channel). */
#define LCEC_EL5032_PDOS  (7 * LCEC_EL5032_CHANS)

/**
 * @brief Initialise the EL5032 slave driver.
 *
 * Allocates HAL memory, registers PDO entries for both EnDat channels,
 * and exports HAL pins including the 64-bit position split into lo/hi words.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5032_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
