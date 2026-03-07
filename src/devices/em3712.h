/**
 * @file em3712.h
 * @brief Driver header for the Beckhoff EM3712 2-channel analog input/output module.
 *
 * The EM3712 provides two independent analog input channels suitable for
 * temperature or voltage measurements.  Each channel returns a 16-bit signed
 * value plus overrange, underrange, and error flags.  The raw value is
 * normalised by the driver to the range [-1.0, 1.0] and scaled/biased by
 * configurable HAL parameters.
 *
 * @copyright Copyright (C) 2016-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_EM3712_H_
#define _LCEC_EM3712_H_

#include "../lcec.h"

#define LCEC_EM3712_VID LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EM3712_PID 0x0E803452         /**< EM3712 product ID */

#define LCEC_EM3712_CHANS 2                          /**< Number of analog input channels */
#define LCEC_EM3712_PDOS  (LCEC_EM3712_CHANS * 4)   /**< Total PDO entry count (4 per channel) */

/**
 * @brief Initialise the EM3712 analog input module.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_em3712_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
