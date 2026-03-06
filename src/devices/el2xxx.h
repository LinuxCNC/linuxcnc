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
 * @file el2xxx.h
 * @brief Header for Beckhoff EL2xxx digital output terminals.
 *
 * Covers EL2002, EL2004, EL2008, EL2022, EL2024, EL2032, EL2034,
 * EL2042, EL2084, EL2088, EL2124, EL2612, EL2622, EL2634, EL2652,
 * EL2798, EL2808, EL2809, EP2008, EP2028, and EP2809 — providing
 * 2–16 channel digital outputs over EtherCAT.
 */
#ifndef _LCEC_EL2XXX_H_
#define _LCEC_EL2XXX_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID shared by all EL2xxx / EP2xxx output terminals. */
#define LCEC_EL2xxx_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product code for the EL2002 (2-channel digital output). */
#define LCEC_EL2002_PID 0x07D23052
/** @brief EtherCAT product code for the EL2004 (4-channel digital output). */
#define LCEC_EL2004_PID 0x07D43052
/** @brief EtherCAT product code for the EL2008 (8-channel digital output). */
#define LCEC_EL2008_PID 0x07D83052
/** @brief EtherCAT product code for the EL2022 (2-channel relay output). */
#define LCEC_EL2022_PID 0x07E63052
/** @brief EtherCAT product code for the EL2024 (4-channel relay output). */
#define LCEC_EL2024_PID 0x07E83052
/** @brief EtherCAT product code for the EL2032 (2-channel digital output with diagnostic). */
#define LCEC_EL2032_PID 0x07F03052
/** @brief EtherCAT product code for the EL2034 (4-channel digital output with diagnostic). */
#define LCEC_EL2034_PID 0x07F23052
/** @brief EtherCAT product code for the EL2042 (2-channel digital output, 24 V, 2 A). */
#define LCEC_EL2042_PID 0x07FA3052
/** @brief EtherCAT product code for the EL2084 (4-channel digital output, 24 V, switching). */
#define LCEC_EL2084_PID 0x08243052
/** @brief EtherCAT product code for the EL2088 (8-channel digital output, 24 V). */
#define LCEC_EL2088_PID 0x08283052
/** @brief EtherCAT product code for the EL2124 (4-channel digital output, 5 V). */
#define LCEC_EL2124_PID 0x084C3052
/** @brief EtherCAT product code for the EL2612 (2-channel relay output). */
#define LCEC_EL2612_PID 0x0A343052
/** @brief EtherCAT product code for the EL2622 (2-channel relay output). */
#define LCEC_EL2622_PID 0x0A3E3052
/** @brief EtherCAT product code for the EL2634 (4-channel relay output). */
#define LCEC_EL2634_PID 0x0A4A3052
/** @brief EtherCAT product code for the EL2652 (2-channel relay output). */
#define LCEC_EL2652_PID 0x0A5C3052
/** @brief EtherCAT product code for the EL2808 (8-channel digital output, 24 V, 0.5 A). */
#define LCEC_EL2808_PID 0x0AF83052
/** @brief EtherCAT product code for the EL2798 (8-channel digital output, 24 V, PWM). */
#define LCEC_EL2798_PID 0x0AEE3052
/** @brief EtherCAT product code for the EL2809 (16-channel digital output, 24 V). */
#define LCEC_EL2809_PID 0x0AF93052
/** @brief EtherCAT product code for the EP2008 (8-channel digital output, IP67, 24 V). */
#define LCEC_EP2008_PID 0x07D84052
/** @brief EtherCAT product code for the EP2028 (8-channel digital output, IP67). */
#define LCEC_EP2028_PID 0x07EC4052
/** @brief EtherCAT product code for the EP2809 (16-channel digital output, IP67). */
#define LCEC_EP2809_PID 0x0AF94052

/** @brief Number of PDO entries for the EL2002 (2 output channels). */
#define LCEC_EL2002_PDOS 2
/** @brief Number of PDO entries for the EL2004 (4 output channels). */
#define LCEC_EL2004_PDOS 4
/** @brief Number of PDO entries for the EL2008 (8 output channels). */
#define LCEC_EL2008_PDOS 8
/** @brief Number of PDO entries for the EL2022 (2 output channels). */
#define LCEC_EL2022_PDOS 2
/** @brief Number of PDO entries for the EL2024 (4 output channels). */
#define LCEC_EL2024_PDOS 4
/** @brief Number of PDO entries for the EL2032 (2 output channels). */
#define LCEC_EL2032_PDOS 2
/** @brief Number of PDO entries for the EL2034 (4 output channels). */
#define LCEC_EL2034_PDOS 4
/** @brief Number of PDO entries for the EL2042 (2 output channels). */
#define LCEC_EL2042_PDOS 2
/** @brief Number of PDO entries for the EL2084 (4 output channels). */
#define LCEC_EL2084_PDOS 4
/** @brief Number of PDO entries for the EL2088 (8 output channels). */
#define LCEC_EL2088_PDOS 8
/** @brief Number of PDO entries for the EL2124 (4 output channels). */
#define LCEC_EL2124_PDOS 4
/** @brief Number of PDO entries for the EL2612 (2 output channels). */
#define LCEC_EL2612_PDOS 2
/** @brief Number of PDO entries for the EL2622 (2 output channels). */
#define LCEC_EL2622_PDOS 2
/** @brief Number of PDO entries for the EL2634 (4 output channels). */
#define LCEC_EL2634_PDOS 4
/** @brief Number of PDO entries for the EL2652 (2 output channels). */
#define LCEC_EL2652_PDOS 2
/** @brief Number of PDO entries for the EL2808 (8 output channels). */
#define LCEC_EL2808_PDOS 8
/** @brief Number of PDO entries for the EL2798 (8 output channels). */
#define LCEC_EL2798_PDOS 8
/** @brief Number of PDO entries for the EL2809 (16 output channels). */
#define LCEC_EL2809_PDOS 16
/** @brief Number of PDO entries for the EP2008 (8 output channels). */
#define LCEC_EP2008_PDOS 8
/** @brief Number of PDO entries for the EP2028 (8 output channels). */
#define LCEC_EP2028_PDOS 8
/** @brief Number of PDO entries for the EP2809 (16 output channels). */
#define LCEC_EP2809_PDOS 16

/**
 * @brief Initialize an EL2xxx digital output slave.
 * @param comp_id   HAL component ID.
 * @param slave     Pointer to the slave structure.
 * @param pdo_entry_regs  PDO entry registration array (updated in place).
 * @return 0 on success, negative errno on failure.
 */
int lcec_el2xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

