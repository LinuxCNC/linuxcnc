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
 * @file el5151.h
 * @brief Driver for the Beckhoff EL5151 1-channel incremental encoder interface terminal (extended).
 *
 * The EL5151 is a single-channel incremental encoder terminal with extended
 * feature set compared to the EL5101. It provides a 32-bit position counter,
 * latch inputs (C index and external positive/negative edge), counter preset,
 * direct readback of all input lines (A, B, C, EXT), extrapolation-stall
 * detection, sync-error reporting, TxPDO toggle, and a 32-bit period
 * measurement. The period scale is 100 ns per tick (1e-7 s/tick).
 */
#ifndef _LCEC_EL5151_H_
#define _LCEC_EL5151_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5151. */
#define LCEC_EL5151_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5151. */
#define LCEC_EL5151_PID 0x141f3052

/** @brief Total number of PDO entries for the EL5151. */
#define LCEC_EL5151_PDOS 18

/** @brief Scale factor converting raw period ticks to seconds (100 ns/tick). */
#define LCEC_EL5151_PERIOD_SCALE 1e-7

/**
 * @brief Initialise the EL5151 slave driver.
 *
 * Allocates HAL memory, registers all PDO entries (status, counter, latch,
 * period, and control words), and exports the full set of HAL pins including
 * period measurement and latch controls.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5151_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

