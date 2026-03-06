//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file ax5805.h
 * @brief Driver interface for the Beckhoff AX5805 TwinSAFE drive option card.
 *
 * The AX5805 is a safety option card that plugs into an AX5100 or AX5200
 * servo drive and provides a Functional Safety over EtherCAT (FSoE) channel.
 * It must be configured after the host drive (AX5100 / AX5200) because it
 * borrows the FSoE configuration from it via the @c fsoeConf pointer.
 *
 * HAL pins expose the raw FSoE master and slave command/CRC/connection-ID
 * words and the Safe Torque Off (STO) status bits for each data channel
 * (one channel for AX5100, two channels for AX5200).
 */
#ifndef _LCEC_AX5805_H_
#define _LCEC_AX5805_H_

#include "../lcec.h"

/** @brief Beckhoff vendor ID for the AX5805. */
#define LCEC_AX5805_VID LCEC_BECKHOFF_VID

/** @brief EtherCAT product ID for the AX5805 TwinSAFE option card. */
#define LCEC_AX5805_PID 0x16AD6012

/**
 * @brief Pre-initialisation hook for the AX5805 safety card.
 *
 * Locates the associated AX5100 or AX5200 drive at index - 1, triggers its
 * own pre-init if necessary, and inherits the FSoE configuration from it.
 * Also calculates the required PDO entry count based on the number of FSoE
 * data channels.
 *
 * @param slave Pointer to the EtherCAT slave descriptor.
 * @return 0 on success, -EINVAL if the drive cannot be found or is not an
 *         AX5x00, -ENOMEM on allocation failure.
 */
int lcec_ax5805_preinit(struct lcec_slave *slave);

/**
 * @brief Initialise the AX5805 HAL component and PDO mappings.
 *
 * Registers PDO entries for the FSoE master/slave command, connection ID, and
 * CRC words, as well as the STO input bits.  Exports HAL pins whose names
 * depend on whether one or two FSoE data channels are configured.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_ax5805_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

