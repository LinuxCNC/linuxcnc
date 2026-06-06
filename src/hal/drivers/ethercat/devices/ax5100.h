/**
 * @file ax5100.h
 * @brief Driver interface for the Beckhoff AX5100 single-axis EtherCAT servo drive.
 *
 * The AX5100 series are compact, single-axis EtherCAT servo drives.  They use
 * the class_ax5 implementation for axis control and expose HAL pins for
 * velocity command, encoder position feedback, torque feedback, drive enable
 * and fault status.  The matching AX5805 TwinSAFE card (if present) provides
 * the FSoE safety channel for this drive.
 *
 * @copyright Copyright (C) 2018-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_AX5100_H_
#define _LCEC_AX5100_H_

#include "../lcec.h"
#include "../classes/class_ax5.h"

/** @brief Beckhoff vendor ID shared by all AX51xx variants. */
#define LCEC_AX5100_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the AX5101 (1 A) drive. */
#define LCEC_AX5101_PID 0x13ed6012
/** @brief EtherCAT product ID for the AX5103 (3 A) drive. */
#define LCEC_AX5103_PID 0x13ef6012
/** @brief EtherCAT product ID for the AX5106 (6 A) drive. */
#define LCEC_AX5106_PID 0x13f26012
/** @brief EtherCAT product ID for the AX5112 (12 A) drive. */
#define LCEC_AX5112_PID 0x13f86012
/** @brief EtherCAT product ID for the AX5118 (18 A) drive. */
#define LCEC_AX5118_PID 0x13fe6012

/**
 * @brief Pre-initialisation hook for the AX5100 slave.
 *
 * Sets up the FSoE configuration and PDO entry count so that the associated
 * AX5805 safety card can locate and validate this drive during its own
 * pre-initialisation.  Safe to call multiple times; subsequent calls are
 * no-ops once the FSoE configuration has been set.
 *
 * @param slave Pointer to the EtherCAT slave descriptor.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ax5100_preinit(struct lcec_slave *slave);

/**
 * @brief Initialise the AX5100 HAL component and PDO mappings.
 *
 * Allocates HAL memory, registers the read/write callbacks, initialises the
 * class_ax5 single-axis channel, and configures the EtherCAT sync-manager
 * PDO assignment (control word + velocity command output; status word +
 * position feedback + torque feedback input, with optional second position
 * feedback and diagnostic entries).
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array;
 *                  advanced by LCEC_PDO_INIT for each registered entry.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ax5100_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
