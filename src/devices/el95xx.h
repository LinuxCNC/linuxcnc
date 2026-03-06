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
 * @file el95xx.h
 * @brief Driver header for Beckhoff EL95xx power supply terminals.
 *
 * The EL95xx series are E-bus power supply terminals that provide regulated
 * output voltages (5 V for EL9505, 8 V for EL9508, 10 V for EL9510,
 * 12 V for EL9512, 15 V for EL9515) and power monitoring.  The EL9576 is
 * a 24 V power distribution terminal.  All variants expose two diagnostic
 * bits: power-ok and overload.
 */
#ifndef _LCEC_EL95XX_H_
#define _LCEC_EL95XX_H_

#include "../lcec.h"

#define LCEC_EL95xx_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */

#define LCEC_EL9505_PID 0x25213052  /**< EL9505 product ID (5 V output) */
#define LCEC_EL9508_PID 0x25243052  /**< EL9508 product ID (8 V output) */
#define LCEC_EL9510_PID 0x25263052  /**< EL9510 product ID (10 V output) */
#define LCEC_EL9512_PID 0x25283052  /**< EL9512 product ID (12 V output) */
#define LCEC_EL9515_PID 0x252b3052  /**< EL9515 product ID (15 V output) */
#define LCEC_EL9576_PID 0x25683052  /**< EL9576 product ID (24 V power distribution) */

#define LCEC_EL95xx_PDOS 2  /**< Number of PDO entries (power-ok + overload) */

/**
 * @brief Initialise an EL95xx power supply terminal.
 *
 * Allocates HAL memory, maps the two PDO entries (power-ok and overload),
 * and exports the corresponding HAL output pins.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el95xx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

