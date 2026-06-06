/**
 * @file el1xxx.h
 * @brief Driver for Beckhoff EL1xxx digital input terminals.
 *
 * Supports the following Beckhoff EtherCAT digital input terminals:
 *   - EL1002: 2-channel 24 V DC input, 3 ms filter
 *   - EL1004: 4-channel 24 V DC input, 3 ms filter
 *   - EL1008: 8-channel 24 V DC input, 3 ms filter
 *   - EL1012: 2-channel 24 V DC input, 10 µs filter
 *   - EL1014: 4-channel 24 V DC input, 10 µs filter
 *   - EL1018: 8-channel 24 V DC input, 10 µs filter
 *   - EL1024: 4-channel 24 V DC input, type 2 (P-switching)
 *   - EL1034: 4-channel 24 V DC input, no filter
 *   - EL1084: 4-channel 24 V DC input, 3 ms filter, N-switching
 *   - EL1088: 8-channel 24 V DC input, 3 ms filter, N-switching
 *   - EL1094: 4-channel 24 V DC input, N-switching
 *   - EL1098: 8-channel 24 V DC input, N-switching
 *   - EL1104: 4-channel 24 V DC input, 3 ms filter, sensor power
 *   - EL1114: 4-channel 24 V DC input, type 1/3 (IEC 61131-2)
 *   - EL1124: 4-channel 5 V DC input
 *   - EL1134: 4-channel NAMUR input
 *   - EL1144: 4-channel 12 V DC input
 *   - EL1808: 8-channel 24 V DC input (compact)
 *   - EL1809: 16-channel 24 V DC input (compact)
 *   - EL1819: 16-channel 24 V DC input (compact, fast)
 *
 * Vendor ID: LCEC_BECKHOFF_VID (0x00000002)
 *
 * HAL pins exposed per channel:
 *   - `<pfx>.din-<n>`     (GOMC_HAL_BIT, GOMC_HAL_OUT): Digital input state.
 *   - `<pfx>.din-<n>-not` (GOMC_HAL_BIT, GOMC_HAL_OUT): Inverted digital input state.
 *
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

#ifndef _LCEC_EL1XXX_H_
#define _LCEC_EL1XXX_H_

#include "../lcec.h"

/** @brief EtherCAT vendor ID for all EL1xxx devices (Beckhoff). */
#define LCEC_EL1xxx_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for EL1002 (2-channel digital input, 3 ms). */
#define LCEC_EL1002_PID 0x03EA3052
/** @brief EtherCAT product ID for EL1004 (4-channel digital input, 3 ms). */
#define LCEC_EL1004_PID 0x03EC3052
/** @brief EtherCAT product ID for EL1008 (8-channel digital input, 3 ms). */
#define LCEC_EL1008_PID 0x03F03052
/** @brief EtherCAT product ID for EL1012 (2-channel digital input, 10 µs). */
#define LCEC_EL1012_PID 0x03F43052
/** @brief EtherCAT product ID for EL1014 (4-channel digital input, 10 µs). */
#define LCEC_EL1014_PID 0x03F63052
/** @brief EtherCAT product ID for EL1018 (8-channel digital input, 10 µs). */
#define LCEC_EL1018_PID 0x03FA3052
/** @brief EtherCAT product ID for EL1024 (4-channel digital input, type 2). */
#define LCEC_EL1024_PID 0x04003052
/** @brief EtherCAT product ID for EL1034 (4-channel digital input, no filter). */
#define LCEC_EL1034_PID 0x040A3052
/** @brief EtherCAT product ID for EL1084 (4-channel digital input, N-switching). */
#define LCEC_EL1084_PID 0x043C3052
/** @brief EtherCAT product ID for EL1088 (8-channel digital input, N-switching). */
#define LCEC_EL1088_PID 0x04403052
/** @brief EtherCAT product ID for EL1094 (4-channel digital input, N-switching). */
#define LCEC_EL1094_PID 0x04463052
/** @brief EtherCAT product ID for EL1098 (8-channel digital input, N-switching). */
#define LCEC_EL1098_PID 0x044A3052
/** @brief EtherCAT product ID for EL1104 (4-channel digital input, sensor supply). */
#define LCEC_EL1104_PID 0x04503052
/** @brief EtherCAT product ID for EL1114 (4-channel digital input, IEC 61131-2 type 1/3). */
#define LCEC_EL1114_PID 0x045A3052
/** @brief EtherCAT product ID for EL1124 (4-channel digital input, 5 V DC). */
#define LCEC_EL1124_PID 0x04643052
/** @brief EtherCAT product ID for EL1134 (4-channel NAMUR digital input). */
#define LCEC_EL1134_PID 0x046E3052
/** @brief EtherCAT product ID for EL1144 (4-channel digital input, 12 V DC). */
#define LCEC_EL1144_PID 0x04783052
/** @brief EtherCAT product ID for EL1808 (8-channel digital input, compact). */
#define LCEC_EL1808_PID 0x07103052
/** @brief EtherCAT product ID for EL1809 (16-channel digital input, compact). */
#define LCEC_EL1809_PID 0x07113052
/** @brief EtherCAT product ID for EL1819 (16-channel digital input, compact, fast). */
#define LCEC_EL1819_PID 0x071B3052

/** @brief Number of PDO entries for EL1002. */
#define LCEC_EL1002_PDOS 2
/** @brief Number of PDO entries for EL1004. */
#define LCEC_EL1004_PDOS 4
/** @brief Number of PDO entries for EL1008. */
#define LCEC_EL1008_PDOS 8
/** @brief Number of PDO entries for EL1012. */
#define LCEC_EL1012_PDOS 2
/** @brief Number of PDO entries for EL1014. */
#define LCEC_EL1014_PDOS 4
/** @brief Number of PDO entries for EL1018. */
#define LCEC_EL1018_PDOS 8
/** @brief Number of PDO entries for EL1024. */
#define LCEC_EL1024_PDOS 4
/** @brief Number of PDO entries for EL1034. */
#define LCEC_EL1034_PDOS 4
/** @brief Number of PDO entries for EL1084. */
#define LCEC_EL1084_PDOS 4
/** @brief Number of PDO entries for EL1088. */
#define LCEC_EL1088_PDOS 8
/** @brief Number of PDO entries for EL1094. */
#define LCEC_EL1094_PDOS 4
/** @brief Number of PDO entries for EL1098. */
#define LCEC_EL1098_PDOS 8
/** @brief Number of PDO entries for EL1104. */
#define LCEC_EL1104_PDOS 4
/** @brief Number of PDO entries for EL1114. */
#define LCEC_EL1114_PDOS 4
/** @brief Number of PDO entries for EL1124. */
#define LCEC_EL1124_PDOS 4
/** @brief Number of PDO entries for EL1134. */
#define LCEC_EL1134_PDOS 4
/** @brief Number of PDO entries for EL1144. */
#define LCEC_EL1144_PDOS 4
/** @brief Number of PDO entries for EL1808. */
#define LCEC_EL1808_PDOS 8
/** @brief Number of PDO entries for EL1809. */
#define LCEC_EL1809_PDOS 16
/** @brief Number of PDO entries for EL1819. */
#define LCEC_EL1819_PDOS 16

/**
 * @brief Initialize an EL1xxx digital input slave.
 *
 * Allocates HAL memory, registers PDO entries (one per channel), and exports
 * HAL pins `din-<n>` and `din-<n>-not` for each input channel.
 *
 * @param comp_id        HAL component ID returned by hal_init().
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced
 *                       by this function for each registered entry.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
