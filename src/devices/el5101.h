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
 * @file el5101.h
 * @brief Driver for the Beckhoff EL5101 1-channel incremental encoder interface terminal.
 *
 * The EL5101 processes standard A/B quadrature (and optional Z index) signals
 * from an incremental encoder. It exposes a 16-bit position counter, latch
 * inputs (index C, external positive/negative edge), counter preset, overflow
 * and underflow flags, and hardware frequency/period measurement. The
 * frequency measurement uses a 0.01 Hz scale and the period uses a 500 ns
 * scale.
 */
#ifndef _LCEC_EL5101_H_
#define _LCEC_EL5101_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5101. */
#define LCEC_EL5101_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5101. */
#define LCEC_EL5101_PID 0x13ed3052

/** @brief Total number of PDO entries for the EL5101. */
#define LCEC_EL5101_PDOS 8

/** @brief Scale factor converting raw period ticks to seconds (500 ns/tick). */
#define LCEC_EL5101_PERIOD_SCALE    500e-9
/** @brief Scale factor converting raw frequency ticks to Hz (0.01 Hz/tick). */
#define LCEC_EL5101_FREQUENCY_SCALE 0.01

/**
 * @brief Initialise the EL5101 slave driver.
 *
 * Allocates HAL memory, registers PDO entries (status, counter value, latch,
 * frequency, period, window, control, setval), and exports HAL pins including
 * frequency measurement and filtering.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5101_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

