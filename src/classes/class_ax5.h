/**
 * @file class_ax5.h
 * @brief Public interface for the Beckhoff AX5xxx servo-drive device class.
 *
 * Declares the per-channel HAL data structure and the four entry points used
 * by AX5100 / AX5200 slave drivers.  Each physical drive channel maps to one
 * `lcec_class_ax5_chan_t` instance.
 *
 * @copyright Copyright (C) 2018-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_CLASS_AX5_H_
#define _LCEC_CLASS_AX5_H_

#include "lcec.h"
#include "class_enc.h"

/**
 * @brief Module-parameter ID: enable secondary feedback (FB2) PDO.
 *
 * When this parameter is set in the slave XML configuration the driver
 * registers an extra position-feedback encoder (`enc_fb2`) sourced from
 * PDO object 0x0035.
 */
#define LCEC_AX5_PARAM_ENABLE_FB2  1

/**
 * @brief Module-parameter ID: enable diagnostics PDO.
 *
 * When set, the driver maps PDO object 0x0186 and exposes a `srv-diag`
 * HAL pin containing the raw diagnostic word from the drive.
 */
#define LCEC_AX5_PARAM_ENABLE_DIAG 2

/**
 * @brief Per-channel runtime state for an AX5xxx servo drive.
 *
 * Holds all HAL pins, HAL parameters, PDO byte-offsets, encoder objects,
 * and internal bookkeeping for one servo axis.  For a dual-axis AX5200 two
 * instances are allocated side-by-side (index 0 and 1).
 */
typedef struct {
  hal_bit_t *drive_on;        /**< HAL IN: assert to allow the power stage to be energised. */

  hal_bit_t *enable;          /**< HAL IN: assert to enable closed-loop velocity control. */
  hal_bit_t *enabled;         /**< HAL OUT: drive reports ready-to-operate level 3 (fully enabled). */
  hal_bit_t *halted;          /**< HAL OUT: drive velocity is zero / axis is in halt state. */
  hal_bit_t *fault;           /**< HAL OUT: drive fault or error shut-off is active. */

  hal_bit_t *halt;            /**< HAL IN: assert to command an immediate halt (velocity → 0). */

  hal_float_t *velo_cmd;      /**< HAL IN: velocity command in user units per second. */

  int fb2_enabled;            /**< Non-zero when secondary feedback PDO was activated via module param. */
  int diag_enabled;           /**< Non-zero when diagnostics PDO was activated via module param. */

  hal_u32_t *status;          /**< HAL IN: raw 16-bit drive status word (IDN S-0-0135). */
  hal_float_t *torque_fb_pct; /**< HAL OUT: actual torque feedback as percentage of rated torque. */
  hal_u32_t *diag;            /**< HAL OUT: raw 32-bit diagnostics word (only valid if diag_enabled). */

  unsigned int status_pdo_os;     /**< Process-data byte offset of the status word PDO (0x0087). */
  unsigned int pos_fb_pdo_os;     /**< Process-data byte offset of the primary position feedback PDO (0x0033). */
  unsigned int pos_fb2_pdo_os;    /**< Process-data byte offset of the secondary position feedback PDO (0x0035). */
  unsigned int torque_fb_pdo_os;  /**< Process-data byte offset of the torque feedback PDO (0x0054). */
  unsigned int diag_pdo_os;       /**< Process-data byte offset of the diagnostics PDO (0x0186). */
  unsigned int ctrl_pdo_os;       /**< Process-data byte offset of the control word PDO (0x0086). */
  unsigned int vel_cmd_pdo_os;    /**< Process-data byte offset of the velocity command PDO (0x0018). */

  hal_float_t scale;          /**< HAL RW param: user-units per encoder count for primary feedback. */
  hal_float_t scale_fb2;      /**< HAL RW param: user-units per encoder count for secondary feedback. */
  hal_float_t vel_scale;      /**< HAL RO param: drive velocity scaling factor read from IDN S-0-0045/46. */
  hal_u32_t pos_resolution;   /**< HAL RO param: encoder counts per revolution read from IDN S-0-0079. */

  lcec_class_enc_data_t enc;      /**< Primary encoder state (position tracking, index, etc.). */
  lcec_class_enc_data_t enc_fb2;  /**< Secondary encoder state (only meaningful when fb2_enabled). */

  double scale_old;       /**< Shadow copy of @p scale used to detect HAL parameter changes. */
  double scale_rcpt;      /**< Reciprocal of @p scale (1/scale), recomputed on change. */
  double scale_fb2_old;   /**< Shadow copy of @p scale_fb2 used to detect HAL parameter changes. */
  double scale_fb2_rcpt;  /**< Reciprocal of @p scale_fb2, recomputed on change. */

  double vel_output_scale; /**< Pre-computed factor converting user-units/s to drive velocity counts (60/vel_scale). */

  int toggle;             /**< Sync-bit toggled every cycle to signal life to the drive. */

} lcec_class_ax5_chan_t;

/**
 * @brief Return the number of PDO entries required by this slave configuration.
 *
 * Starts from the mandatory five PDO entries and adds one for optional secondary
 * feedback and one for optional diagnostics depending on module parameters.
 *
 * @param slave  Pointer to the EtherCAT slave descriptor.
 * @return       Total PDO entry count to allocate.
 */
int lcec_class_ax5_pdos(struct lcec_slave *slave);

/**
 * @brief Initialise a single AX5xxx channel and register its HAL objects.
 *
 * Reads velocity and position scaling IDNs from the drive, maps all required
 * PDO entries, exports HAL pins and parameters, and initialises the encoder
 * sub-object.
 *
 * @param slave           Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array (advanced in place).
 * @param chan            Per-channel data structure to initialise.
 * @param index           Zero-based axis index within the drive (0 for single-axis AX5100,
 *                        0 or 1 for dual-axis AX5200).
 * @param pfx             HAL name prefix string for this channel's pins/params.
 * @return                0 on success, negative errno on failure.
 */
int lcec_class_ax5_init(struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs, lcec_class_ax5_chan_t *chan, int index, const char *pfx);

/**
 * @brief Read process data from the drive and update HAL output pins.
 *
 * Called from the real-time read function of the owning slave driver.
 * Decodes the status word, updates fault/enabled/halted flags, reads
 * position feedback and torque feedback, and advances the encoder state.
 *
 * @param slave  Pointer to the EtherCAT slave descriptor.
 * @param chan   Per-channel data structure for this axis.
 */
void lcec_class_ax5_read(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan);

/**
 * @brief Write HAL input pin values to the drive process data.
 *
 * Called from the real-time write function of the owning slave driver.
 * Builds the 16-bit control word from the drive-on/enable/halt pins,
 * scales the velocity command, and writes both to the EtherCAT process
 * data image.
 *
 * @param slave  Pointer to the EtherCAT slave descriptor.
 * @param chan   Per-channel data structure for this axis.
 */
void lcec_class_ax5_write(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan);

#endif
