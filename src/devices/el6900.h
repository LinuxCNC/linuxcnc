/**
 * @file el6900.h
 * @brief Driver header for the Beckhoff EL6900 TwinSAFE logic controller.
 *
 * The EL6900 is a TwinSAFE logic terminal that acts as a FsoE (Fail-safe
 * over EtherCAT) safety master. It processes safety-relevant digital I/O PDOs
 * from connected TwinSAFE slaves and exposes control/state information to HAL.
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

#ifndef _LCEC_EL6900_H_
#define _LCEC_EL6900_H_

#include "../lcec.h"

#define LCEC_EL6900_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL6900_PID 0x1AF43052         /**< EL6900 product ID */

#define LCEC_EL6900_PDOS 5 /**< Base number of PDO entries for the EL6900 itself */

#define LCEC_EL6900_PARAM_SLAVEID      1 /**< Module param ID: EtherCAT slave index of a FsoE slave */
#define LCEC_EL6900_PARAM_STDIN_NAME   2 /**< Module param ID: name for a standard safety input bit */
#define LCEC_EL6900_PARAM_STDOUT_NAME  3 /**< Module param ID: name for a standard safety output bit */

#define LCEC_EL6900_PARAM_SLAVE_PDOS    4 /**< Number of base PDO entries added per FsoE slave (cmd + connid) */
#define LCEC_EL6900_PARAM_SLAVE_CH_PDOS 2 /**< Number of PDO entries added per data channel of a FsoE slave (CRC pair) */

#define LCEC_EL6900_DIO_MAX_COUNT 32 /**< Maximum number of standard safety digital I/O bits */

/**
 * @brief Pre-initialisation hook to count and register PDO entries.
 *
 * Called before the main init to determine the total number of PDO entries
 * needed, based on the configured FsoE slaves and standard I/O channels.
 *
 * @param slave Pointer to the EtherCAT slave structure.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el6900_preinit(struct lcec_slave *slave);

/**
 * @brief Initialise the EL6900 HAL component and map PDOs.
 *
 * Allocates HAL memory, maps all PDO entries for the EL6900 safety controller
 * (control/state, standard I/O, and per-FsoE-slave command/CRC channels),
 * and exports HAL pins.
 *
 * @param comp_id  HAL component ID.
 * @param slave    Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs Pointer to the PDO entry registration array (advanced in place).
 * @return 0 on success, negative errno on failure.
 */
int lcec_el6900_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
