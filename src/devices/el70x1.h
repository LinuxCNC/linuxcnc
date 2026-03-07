/**
 * @file el70x1.h
 * @brief Driver header for the Beckhoff EL7031 and EL7041 stepper motor terminals.
 *
 * The EL70x1 family are single-channel stepper motor terminals with an
 * integrated incremental encoder interface.  The EL7031 supports up to
 * 1.5 A coil current; the EL7041 supports up to 5 A.  Both use a 32-bit
 * position setpoint PDO (position mode via 0x8012:01 = 3).
 *
 * @copyright Copyright (C) 2015-2026 Jakob Flierl  <jakob.flierl@gmail.com>
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

#ifndef _LCEC_EL70x1_H_
#define _LCEC_EL70x1_H_

#include "../lcec.h"

#define LCEC_EL70x1_VID         LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL7031_PID         0x1B773052         /**< EL7031 product ID */
#define LCEC_EL7041_0052_PID    0x1B813052         /**< EL7041 (revision 0052) product ID */

#define LCEC_EL70x1_PDOS        15 /**< Number of PDO entries used by this driver */

#define LCEC_EL70x1_PARAM_MAX_CURR  1 /**< Module param: maximum coil current (mA, SDO 0x8010:01) */
#define LCEC_EL70x1_PARAM_RED_CURR  2 /**< Module param: reduced coil current (mA, SDO 0x8010:02) */
#define LCEC_EL70x1_PARAM_NOM_VOLT  3 /**< Module param: nominal supply voltage (mV, SDO 0x8010:03) */
#define LCEC_EL70x1_PARAM_COIL_RES  4 /**< Module param: coil resistance (mΩ, SDO 0x8010:04) */
#define LCEC_EL70x1_PARAM_MOTOR_EMF 5 /**< Module param: motor back-EMF constant (SDO 0x8010:05) */

/**
 * @brief Initialise an EL7031 stepper motor terminal.
 *
 * Sets the EL7031-specific sync configuration and delegates to the shared
 * lcec_el70x1_init() implementation.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7031_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

/**
 * @brief Initialise an EL7041 (revision 0052) stepper motor terminal.
 *
 * Sets the EL7041-0052-specific sync configuration (no encoder PDOs) and
 * delegates to the shared lcec_el70x1_init() implementation.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7041_0052_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
