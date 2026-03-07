/**
 * @file stmds5k.h
 * @brief Driver interface for the Stöber MDS5000 EtherCAT servo drive.
 *
 * The Stöber MDS5000 (MDS5k) is a velocity-controlled servo drive using a
 * vendor-specific PDO layout.  It provides a proprietary device-control byte
 * and relative speed setpoint in the output direction, and returns a device
 * status byte, actual motor speed, motor torque and rotor position in the
 * input direction.
 *
 * An optional external encoder (EnDat/SSI, resolver or incremental) can be
 * mapped by setting the @c extenc module parameter to one of the type indices
 * defined in the driver.
 *
 * HAL pins cover velocity command/feedback (in both scale units/s and RPM),
 * torque feedback (raw, absolute and percentage), status flags (stopped,
 * at-speed, overload, ready, error, toggle, loc-ena) and control inputs
 * (enable, error-reset, fast-ramp, brake).
 *
 * @copyright Copyright (C) 2011-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_STMDS5K_H_
#define _LCEC_STMDS5K_H_

#include "../lcec.h"

/** @brief Stöber vendor ID. */
#define LCEC_STMDS5K_VID LCEC_STOEBER_VID
/** @brief EtherCAT product ID for the MDS5000 drive. */
#define LCEC_STMDS5K_PID 0x00001388

/** @brief Number of PDO entries used without an external encoder. */
#define LCEC_STMDS5K_PDOS 8
/** @brief Additional PDO entries required when an external encoder is configured. */
#define LCEC_STMDS5K_EXTINC_PDOS 1

/** @brief Module parameter index: select multiturn (32-bit) encoder mode. */
#define LCEC_STMDS5K_PARAM_MULTITURN 1
/** @brief Module parameter index: select external encoder type (0–8). */
#define LCEC_STMDS5K_PARAM_EXTENC 2

/**
 * @brief Pre-initialisation hook for the MDS5000 slave.
 *
 * Sets the PDO entry count, increasing it by @c LCEC_STMDS5K_EXTINC_PDOS when
 * the @c extenc module parameter is present and valid.
 *
 * @param slave Pointer to the EtherCAT slave descriptor.
 * @return 0 on success, -EINVAL if the extenc type index is out of range.
 */
int lcec_stmds5k_preinit(struct lcec_slave *slave);

/**
 * @brief Initialise the MDS5000 HAL component and PDO mappings.
 *
 * Reads SDO parameters (torque reference, max RPM, setpoint max RPM), builds
 * the EtherCAT sync-info from a compile-time template, registers PDO entries,
 * exports HAL pins and parameters, and initialises the rotor and (optionally)
 * external encoder subclasses.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_stmds5k_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
