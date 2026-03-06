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
 * @file ek1100.h
 * @brief Driver definitions for Beckhoff EK1100-series EtherCAT bus couplers.
 *
 * Covers the following coupler variants:
 *  - EK1100: Standard EtherCAT bus coupler (VID 0x00000002, PID 0x044C2C52)
 *  - EK1101: EtherCAT bus coupler with ID switch (VID 0x00000002, PID 0x044D2C52)
 *  - EK1110: EtherCAT extension (VID 0x00000002, PID 0x04562C52)
 *  - EK1122: 2-port EtherCAT junction (VID 0x00000002, PID 0x04622C52)
 *
 * These couplers carry no process data themselves (PDOS = 0); they couple
 * EtherCAT segments and power the local E-bus terminals.
 */
#ifndef _LCEC_EK1100_H_
#define _LCEC_EK1100_H_

/** @brief Beckhoff vendor ID for EK1100. */
#define LCEC_EK1100_VID  LCEC_BECKHOFF_VID
/** @brief EtherCAT product code for the EK1100 bus coupler. */
#define LCEC_EK1100_PID  0x044C2C52
/** @brief Number of PDO entries for EK1100 (no process data). */
#define LCEC_EK1100_PDOS 0

/** @brief Beckhoff vendor ID for EK1101. */
#define LCEC_EK1101_VID  LCEC_BECKHOFF_VID
/** @brief EtherCAT product code for the EK1101 bus coupler with ID switch. */
#define LCEC_EK1101_PID  0x044D2C52
/** @brief Number of PDO entries for EK1101 (no process data). */
#define LCEC_EK1101_PDOS 0

/** @brief Beckhoff vendor ID for EK1110. */
#define LCEC_EK1110_VID  LCEC_BECKHOFF_VID
/** @brief EtherCAT product code for the EK1110 EtherCAT extension. */
#define LCEC_EK1110_PID  0x04562C52
/** @brief Number of PDO entries for EK1110 (no process data). */
#define LCEC_EK1110_PDOS 0

/** @brief Beckhoff vendor ID for EK1122. */
#define LCEC_EK1122_VID  LCEC_BECKHOFF_VID
/** @brief EtherCAT product code for the EK1122 2-port EtherCAT junction. */
#define LCEC_EK1122_PID  0x04622C52
/** @brief Number of PDO entries for EK1122 (no process data). */
#define LCEC_EK1122_PDOS 0

#endif

