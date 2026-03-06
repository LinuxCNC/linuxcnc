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
 * @file el5152.h
 * @brief Driver for the Beckhoff EL5152 2-channel incremental encoder interface terminal (extended).
 *
 * The EL5152 provides two independent incremental encoder channels, each with
 * a 32-bit position counter, counter preset, A/B input state readback,
 * extrapolation-stall flag, TxPDO toggle, and a 32-bit period measurement.
 * Each channel also supports a software index (Z) latch via the index/index-enable
 * HAL pins. The period scale is 100 ns per tick (1e-7 s/tick).
 */
#ifndef _LCEC_EL5152_H_
#define _LCEC_EL5152_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5152. */
#define LCEC_EL5152_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5152. */
#define LCEC_EL5152_PID 0x14203052

/** @brief Number of encoder channels on the EL5152. */
#define LCEC_EL5152_CHANS 2
/** @brief Total number of PDO entries (9 per channel, both directions). */
#define LCEC_EL5152_PDOS  (9 * LCEC_EL5152_CHANS)

/** @brief Scale factor converting raw period ticks to seconds (100 ns/tick). */
#define LCEC_EL5152_PERIOD_SCALE 1e-7

/**
 * @brief Initialise the EL5152 slave driver.
 *
 * Allocates HAL memory, registers PDO entries for both encoder channels
 * (including per-channel period PDOs), and exports all HAL pins including
 * period measurement and index latch support.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5152_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

