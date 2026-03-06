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
 * @file el7211.h
 * @brief Driver header for the Beckhoff EL7211, EL7221, and EL7201-9014 servo motor terminals.
 *
 * These terminals drive BLDC (brushless DC) servo motors using the CiA-402
 * motion profile over EtherCAT.  The EL7211/EL7221 provide a 6-PDO interface
 * (position, status word, velocity feedback, torque feedback, control word,
 * velocity command).  The EL7201-9014 adds two additional info PDOs for
 * digital inputs and extended error diagnostics.
 */
#ifndef _LCEC_EL7211_H_
#define _LCEC_EL7211_H_

#include "../lcec.h"

#define LCEC_EL7211_VID      LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL7211_PID      0x1C2B3052         /**< EL7211 product ID */
#define LCEC_EL7221_PID      0x1C353052         /**< EL7221 product ID */
#define LCEC_EL7201_9014_PID 0x1C213052         /**< EL7201-9014 product ID */

#define LCEC_EL7211_PDOS      6 /**< PDO entry count for EL7211/EL7221 */
#define LCEC_EL7201_9014_PDOS 8 /**< PDO entry count for EL7201-9014 (includes info1/info2) */

/**
 * @brief Initialise the EL7211 (or EL7221) servo terminal.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7211_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

/**
 * @brief Initialise the EL7201-9014 servo terminal (extended diagnostics variant).
 *
 * Configures additional info PDOs for digital inputs and error word, then
 * calls the shared EL7211 init path.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7201_9014_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

