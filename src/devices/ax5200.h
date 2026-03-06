//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file ax5200.h
 * @brief Driver interface for the Beckhoff AX5200 dual-axis EtherCAT servo drive.
 *
 * The AX5200 series are dual-axis EtherCAT servo drives.  Each of the two
 * independent axes is implemented using the class_ax5 channel helper and
 * exposes the same HAL pins as a single-axis AX5100 drive, but prefixed with
 * @c ch0. and @c ch1.  The FSoE configuration carries two data channels,
 * matching the two axes exposed to an AX5805 safety card.
 */
#ifndef _LCEC_AX5200_H_
#define _LCEC_AX5200_H_

#include "../lcec.h"
#include "../classes/class_ax5.h"

/** @brief Beckhoff vendor ID shared by all AX52xx variants. */
#define LCEC_AX5200_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the AX5203 (2 × 3 A) dual-axis drive. */
#define LCEC_AX5203_PID 0x14536012
/** @brief EtherCAT product ID for the AX5206 (2 × 6 A) dual-axis drive. */
#define LCEC_AX5206_PID 0x14566012

/** @brief Number of servo axes provided by the AX5200. */
#define LCEC_AX5200_CHANS 2
/** @brief Total PDO entry count for both axes combined. */
#define LCEC_AX5200_PDOS  (LCEC_AX5200_CHANS * LCEC_CLASS_AX5_PDOS)

/**
 * @brief Pre-initialisation hook for the AX5200 slave.
 *
 * Sets up the FSoE configuration (two data channels) and the combined PDO
 * entry count for both axes.  Required before the associated AX5805 safety
 * card can resolve its dependency on this drive.  Idempotent.
 *
 * @param slave Pointer to the EtherCAT slave descriptor.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ax5200_preinit(struct lcec_slave *slave);

/**
 * @brief Initialise the AX5200 HAL component and PDO mappings for both axes.
 *
 * Allocates HAL memory, registers read/write callbacks, initialises both
 * class_ax5 channels (prefixed @c ch0. and @c ch1.), and configures the
 * EtherCAT sync-manager PDO assignment covering control words, velocity
 * commands, status words, position feedback, torque feedback, and optional
 * second-feedback / diagnostic entries for each axis.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ax5200_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

