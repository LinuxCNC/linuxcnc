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
 * @file ph3lm2rm.h
 * @brief Driver interface for the Phytron phyMOTION stepper controller (3 LM + 2 RM).
 *
 * The Phytron phyMOTION PH3LM2RM is a modular stepper controller with three
 * linear measurement (LM) channels and two rotary measurement (RM) channels.
 * Each channel provides a 32-bit absolute encoder, hardware latch inputs, and
 * error/status flags.  LM channels additionally report optical signal quality,
 * while RM channels support index-pulse latch selection.
 *
 * @note The @c pdo_entry_regs pointer must NOT be incremented manually when
 * iterating over channels.  Each call to LCEC_PDO_INIT already advances the
 * pointer, so the per-channel init helpers pass the same pointer through
 * without any additional pointer arithmetic.
 */
#ifndef _LCEC_PH3LM2RM_H_
#define _LCEC_PH3LM2RM_H_

#include "../lcec.h"

/** @brief Modusoft vendor ID. */
#define LCEC_PH3LM2RM_VID LCEC_MODUSOFT_VID
/** @brief EtherCAT product ID for the PH3LM2RM module. */
#define LCEC_PH3LM2RM_PID 0x10000001

/** @brief Number of rotary measurement (RM) channels. */
#define LCEC_PH3LM2RM_RM_COUNT 2
/** @brief Number of linear measurement (LM) channels. */
#define LCEC_PH3LM2RM_LM_COUNT 3

/** @brief PDO entries per RM channel (latch select, latch-ena pos/neg, error,
 *         latch-valid, latch-state, counter, latch value). */
#define LCEC_PH3LM2RM_RM_PDOS 8
/** @brief PDO entries per LM channel (latch-ena pos/neg, error, latch-valid,
 *         latch-state, signal level, counter, latch value). */
#define LCEC_PH3LM2RM_LM_PDOS 8

/** @brief Total PDO entry count: 2 global + all channel entries. */
#define LCEC_PH3LM2RM_PDOS (2 + (LCEC_PH3LM2RM_RM_PDOS * LCEC_PH3LM2RM_RM_COUNT) + (LCEC_PH3LM2RM_LM_PDOS * LCEC_PH3LM2RM_LM_COUNT))

/**
 * @brief Initialise the PH3LM2RM HAL component and PDO mappings.
 *
 * Registers the global error-reset and sync-locked PDO entries, exports
 * global HAL pins, then iterates over all LM and RM channels calling their
 * respective init helpers.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array;
 *                  advanced automatically by each LCEC_PDO_INIT call.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ph3lm2rm_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

