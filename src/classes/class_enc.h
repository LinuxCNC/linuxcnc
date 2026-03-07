/**
 * @file class_enc.h
 * @brief Public interface for the generic EtherCAT encoder device class.
 *
 * Declares the HAL data structure and the two entry points that implement
 * position tracking, counter overflow / underflow detection, index-pulse
 * homing, and external-latch capture for a single incremental or absolute
 * encoder channel.
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

#ifndef _LCEC_CLASS_ENC_H_
#define _LCEC_CLASS_ENC_H_

#include "lcec.h"

/**
 * @brief HAL pins, parameters, and internal state for one encoder channel.
 *
 * The structure is allocated by the owning slave driver and passed to
 * `class_enc_init` for HAL registration and to `class_enc_update` on every
 * real-time cycle.
 *
 * **Position outputs** (all in user-units after applying @p scale):
 * - @p pos_enc  – encoder-relative position (rolls back to zero on re-init).
 * - @p pos_abs  – home-offset-corrected absolute position.
 * - @p pos      – reference-corrected relative position (resets on index or
 *                 pos_reset).
 */
typedef struct {
  hal_s32_t raw_home;       /**< HAL RW param: raw encoder count corresponding to the home position. */
  hal_u32_t raw_bits;       /**< HAL RO param: number of significant bits in the raw encoder word. */
  hal_float_t pprev_scale;  /**< HAL RO param: reciprocal of pulses-per-revolution (1/pprev), for display. */

  hal_s32_t *raw;           /**< HAL OUT: most-recent raw encoder count (sign-extended to 32 bits). */
  hal_u32_t *ext_lo;        /**< HAL IO:  low 32 bits of the 64-bit extrapolated position accumulator. */
  hal_u32_t *ext_hi;        /**< HAL IO:  high 32 bits of the 64-bit extrapolated position accumulator. */
  hal_u32_t *ref_lo;        /**< HAL OUT: low 32 bits of the 64-bit reference (zero-point) position. */
  hal_u32_t *ref_hi;        /**< HAL OUT: high 32 bits of the 64-bit reference (zero-point) position. */

  hal_bit_t *index_ena;     /**< HAL IO:  set by motion controller to arm index-pulse homing; cleared by driver when index is detected. */
  hal_bit_t *pos_reset;     /**< HAL IN:  pulse high to reset the relative position reference to the current position. */

  hal_float_t *pos_enc;     /**< HAL OUT: position in user-units relative to the encoder power-on zero. */
  hal_float_t *pos_abs;     /**< HAL OUT: absolute position in user-units corrected for the raw_home offset. */
  hal_float_t *pos;         /**< HAL OUT: relative position in user-units with respect to the current reference point. */

  hal_bit_t *on_home_neg;   /**< HAL OUT: true when the absolute position is at or below the home position. */
  hal_bit_t *on_home_pos;   /**< HAL OUT: true when the absolute position is at or above the home position. */

  int do_init;              /**< Internal: non-zero on first update cycle or after a slave fault; triggers state re-initialisation. */

  int raw_shift;            /**< Internal: left-shift amount (32 - raw_bits) used for sign-extension of raw counts. */
  uint32_t raw_mask;        /**< Internal: bitmask isolating the significant encoder bits (2^raw_bits - 1). */

  uint64_t pprev_last;      /**< Internal: last seen pulses-per-revolution value; used to detect pprev changes. */

  int index_sign;           /**< Internal: sign of the modulo remainder on the previous cycle, used for index-pulse edge detection. */

} lcec_class_enc_data_t;

/**
 * @brief Initialise an encoder channel and register its HAL objects.
 *
 * Creates all HAL pins and parameters listed in the encoder pin/param
 * descriptors, sets initial state flags, and pre-computes the raw-bit
 * shift and mask values used during `class_enc_update`.
 *
 * @param slave     Pointer to the EtherCAT slave descriptor (provides master
 *                  name and slave name for HAL object naming).
 * @param hal_data  Pointer to the encoder data structure to initialise.
 * @param raw_bits  Number of significant bits in the encoder's raw position
 *                  word (e.g. 32 for a 32-bit absolute encoder).
 * @param pfx       HAL name prefix appended after the master/slave path.
 * @return          0 on success, negative errno on failure.
 */
int class_enc_init(struct lcec_slave *slave, lcec_class_enc_data_t *hal_data, int raw_bits, const char *pfx);

/**
 * @brief Process one encoder sample and update all HAL output pins.
 *
 * Must be called from the owning slave's real-time read function every cycle.
 * The function:
 *  1. Recomputes the pos scale when @p pprev changes.
 *  2. Extrapolates the 32-bit raw count into a 64-bit position accumulator,
 *     correctly handling counter overflow and underflow.
 *  3. Detects index-pulse crossings and snaps the reference point.
 *  4. Optionally captures the reference point from an external latch signal.
 *  5. Handles position reset requests.
 *  6. Publishes `pos_enc`, `pos_abs`, and `pos` HAL pins.
 *
 * @param hal_data        Pointer to the encoder data structure (must have been
 *                        initialised by `class_enc_init`).
 * @param pprev           Pulses per revolution (encoder counts per full turn).
 *                        Pass 0 if the concept does not apply (e.g. linear scale).
 * @param scale           Scaling factor converting encoder counts to user units
 *                        (reciprocal of user-units per count).
 * @param raw             Current raw encoder count from the PDO process data.
 * @param ext_latch_raw   Raw encoder count captured at the external latch event.
 * @param ext_latch_ena   Non-zero if a valid external latch event occurred this cycle.
 */
void class_enc_update(lcec_class_enc_data_t *hal_data, uint64_t pprev, double scale, uint32_t raw, uint32_t ext_latch_raw, int ext_latch_ena);

#endif
