//
//    Copyright (C) 2023 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file el5002.h
 * @brief Driver for the Beckhoff EL5002 2-channel SSI encoder interface terminal.
 *
 * The EL5002 reads absolute position data from up to two encoders using the
 * Synchronous Serial Interface (SSI) protocol. Each channel provides a 32-bit
 * position counter, error flags (data error, frame error, power fail, sync
 * error), and TxPDO status/toggle bits. Channel behaviour (baud rate, frame
 * size, data length, coding, etc.) is configurable via CoE SDO module
 * parameters.
 */
#ifndef _LCEC_EL5002_H_
#define _LCEC_EL5002_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID used for the EL5002. */
#define LCEC_EL5002_VID LCEC_BECKHOFF_VID
/** @brief EtherCAT product ID for the EL5002. */
#define LCEC_EL5002_PID 0x138a3052

/** @brief Number of SSI encoder channels on the EL5002. */
#define LCEC_EL5002_CHANS 2
/** @brief Total number of PDO entries (7 per channel). */
#define LCEC_EL5002_PDOS  (7 * LCEC_EL5002_CHANS)

/** @brief Mask to extract the channel index from a modparam ID. */
#define LCEC_EL5002_PARAM_CH_MASK          0x000f
/** @brief Mask to extract the function code from a modparam ID. */
#define LCEC_EL5002_PARAM_FNK_MASK         0xfff0

/** @brief Modparam channel index 0. */
#define LCEC_EL5002_PARAM_CH_0             0x0000
/** @brief Modparam channel index 1. */
#define LCEC_EL5002_PARAM_CH_1             0x0001

/** @brief Modparam: disable frame-error detection (SDO 0x8000/0x01). */
#define LCEC_EL5002_PARAM_DIS_FRAME_ERR    0x0010
/** @brief Modparam: enable power-fail check (SDO 0x8000/0x02). */
#define LCEC_EL5002_PARAM_EN_PWR_FAIL_CHK  0x0020
/** @brief Modparam: enable inhibit-time feature (SDO 0x8000/0x03). */
#define LCEC_EL5002_PARAM_EN_INHIBIT_TIME  0x0030
/** @brief Modparam: SSI data coding (Gray or binary) (SDO 0x8000/0x06). */
#define LCEC_EL5002_PARAM_CODING           0x0040
/** @brief Modparam: SSI baud rate selection (SDO 0x8000/0x09). */
#define LCEC_EL5002_PARAM_BAUDRATE         0x0050
/** @brief Modparam: clock jitter compensation setting (SDO 0x8000/0x0c). */
#define LCEC_EL5002_PARAM_CLK_JIT_COMP     0x0060
/** @brief Modparam: SSI frame type (SDO 0x8000/0x0f). */
#define LCEC_EL5002_PARAM_FRAME_TYPE       0x0070
/** @brief Modparam: SSI frame size in bits (SDO 0x8000/0x11). */
#define LCEC_EL5002_PARAM_FRAME_SIZE       0x0080
/** @brief Modparam: position data length within the SSI frame (SDO 0x8000/0x12). */
#define LCEC_EL5002_PARAM_DATA_LEN         0x0090
/** @brief Modparam: minimum inhibit time between SSI transmissions (SDO 0x8000/0x13). */
#define LCEC_EL5002_PARAM_MIN_INHIBIT_TIME 0x00a0
/** @brief Modparam: number of SSI clock bursts per cycle (SDO 0x8000/0x14). */
#define LCEC_EL5002_PARAM_NO_CLK_BURSTS    0x00b0

/**
 * @brief Initialise the EL5002 slave driver.
 *
 * Applies CoE SDO configuration from module parameters, allocates HAL memory,
 * registers PDO entries, and exports HAL pins for both encoder channels.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced
 *                       by this function for each registered entry.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5002_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

