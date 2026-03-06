//
//    Copyright (C) 2019 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file omrg5.h
 * @brief Driver interface for the Omron G5 series EtherCAT servo drives.
 *
 * The Omron G5 (R88D-KN*-ECT) drives implement the CiA-402 cyclic synchronous
 * position (CSP) profile.  The driver configures opmode 8 (CSP) at startup
 * and maps:
 *   - Output PDO 0x1701: control word (0x6040), target position (0x607A),
 *                        latch function (0x60B8), physical digital outputs (0x60FE:01).
 *   - Input PDO 0x1B01:  error code (0x603F), status word (0x6041), current
 *                        position (0x6064), current torque (0x6077), current
 *                        following error (0x60F4), latch status/positions
 *                        (0x60B9, 0x60BA, 0x60BC), digital inputs (0x60FD).
 *
 * HAL pins expose position command/feedback (in scaled units and raw counts),
 * following error, torque feedback, CiA-402 status bits, a rich set of digital
 * inputs (limit switches, monitor bits, safety inputs, EDM) and the drive
 * error code.  An @c auto-fault-reset parameter enables automatic fault
 * clearing on the rising edge of the enable input.
 */
#ifndef _LCEC_OMRG5_H_
#define _LCEC_OMRG5_H_

#include "../lcec.h"

/** @brief Omron vendor ID. */
#define LCEC_OMRG5_VID LCEC_OMRON_VID
/** @brief Product ID: R88D-KNA5L-ECT (50 W, single-phase). */
#define LCEC_OMRG5_R88D_KNA5L_ECT_PID 0x00000001
/** @brief Product ID: R88D-KN01L-ECT (100 W, single-phase). */
#define LCEC_OMRG5_R88D_KN01L_ECT_PID 0x00000002
/** @brief Product ID: R88D-KN02L-ECT (200 W, single-phase). */
#define LCEC_OMRG5_R88D_KN02L_ECT_PID 0x00000003
/** @brief Product ID: R88D-KN04L-ECT (400 W, single-phase). */
#define LCEC_OMRG5_R88D_KN04L_ECT_PID 0x00000004
/** @brief Product ID: R88D-KN01H-ECT (100 W, three-phase). */
#define LCEC_OMRG5_R88D_KN01H_ECT_PID 0x00000005
/** @brief Product ID: R88D-KN02H-ECT (200 W, three-phase). */
#define LCEC_OMRG5_R88D_KN02H_ECT_PID 0x00000006
/** @brief Product ID: R88D-KN04H-ECT (400 W, three-phase). */
#define LCEC_OMRG5_R88D_KN04H_ECT_PID 0x00000007
/** @brief Product ID: R88D-KN08H-ECT (750 W, three-phase). */
#define LCEC_OMRG5_R88D_KN08H_ECT_PID 0x00000008
/** @brief Product ID: R88D-KN10H-ECT (1 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN10H_ECT_PID 0x00000009
/** @brief Product ID: R88D-KN15H-ECT (1.5 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN15H_ECT_PID 0x0000000A
/** @brief Product ID: R88D-KN20H-ECT (2 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN20H_ECT_PID 0x00000056
/** @brief Product ID: R88D-KN30H-ECT (3 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN30H_ECT_PID 0x00000057
/** @brief Product ID: R88D-KN50H-ECT (5 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN50H_ECT_PID 0x00000058
/** @brief Product ID: R88D-KN75H-ECT (7.5 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN75H_ECT_PID 0x00000059
/** @brief Product ID: R88D-KN150H-ECT (15 kW, three-phase). */
#define LCEC_OMRG5_R88D_KN150H_ECT_PID 0x0000005A
/** @brief Product ID: R88D-KN06F-ECT (600 W, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN06F_ECT_PID 0x0000000B
/** @brief Product ID: R88D-KN10F-ECT (1 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN10F_ECT_PID 0x0000000C
/** @brief Product ID: R88D-KN15F-ECT (1.5 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN15F_ECT_PID 0x0000000D
/** @brief Product ID: R88D-KN20F-ECT (2 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN20F_ECT_PID 0x0000005B
/** @brief Product ID: R88D-KN30F-ECT (3 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN30F_ECT_PID 0x0000005C
/** @brief Product ID: R88D-KN50F-ECT (5 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN50F_ECT_PID 0x0000005D
/** @brief Product ID: R88D-KN75F-ECT (7.5 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN75F_ECT_PID 0x0000005E
/** @brief Product ID: R88D-KN150F-ECT (15 kW, three-phase, 400 V). */
#define LCEC_OMRG5_R88D_KN150F_ECT_PID 0x0000005F

/** @brief Total number of PDO entries used by this driver. */
#define LCEC_OMRG5_PDOS 13

/**
 * @brief Initialise the Omron G5 HAL component and PDO mappings.
 *
 * Configures the drive for CSP mode via an SDO write, registers all PDO
 * entries, exports HAL pins and parameters, and initialises internal state.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_omrg5_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

