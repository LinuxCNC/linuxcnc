//
//    Copyright (C) 2015 Claudio lorini <claudio.lorini@iit.it>
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
 * @file el1252.h
 * @brief Driver for Beckhoff EL1252 2-channel fast digital input with timestamp.
 *
 * Vendor ID: LCEC_BECKHOFF_VID (0x00000002)
 * Product ID: 0x04E43052
 *
 * HAL pins exposed per channel:
 *   - `<pfx>.din-<n>` (HAL_BIT, HAL_OUT): Digital input state.
 *
 * @note Timestamp/latch data is read internally but not yet exposed to HAL.
 */
#ifndef _LCEC_EL1252_H_
#define _LCEC_EL1252_H_

/** \brief Product Code */
#define LCEC_EL1252_PID 0x04E43052

/** \brief Number of channels */
#define LCEC_EL1252_CHANS 2

/** \brief Number of PDO entries (4 per channel: input bit, status, latch-pos, latch-neg) */
#define LCEC_EL1252_PDOS (4 * LCEC_EL1252_CHANS )

/** \brief Vendor ID */
#define LCEC_EL1252_VID LCEC_BECKHOFF_VID

/**
 * @brief Initialize the EL1252 fast digital input slave.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1252_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

