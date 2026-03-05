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
#ifndef _LCEC_EL2202_H_
#define _LCEC_EL2202_H_

/** \brief Product Code */
#define LCEC_EL2202_PID 0x089A3052

/** \brief Number of channels */
#define LCEC_EL2202_CHANS 2

/** \brief Number of PDO */
#define LCEC_EL2202_PDOS (2 * LCEC_EL2202_CHANS)

/** \brief Vendor ID */
#define LCEC_EL2202_VID LCEC_BECKHOFF_VID

int lcec_el2202_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

