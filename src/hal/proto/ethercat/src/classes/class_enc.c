/**
 * @file class_enc.c
 * @brief Generic EtherCAT encoder device-class implementation.
 *
 * Implements position tracking for incremental and absolute encoders connected
 * via EtherCAT.  Key features:
 *
 * - **64-bit position accumulator** – extrapolates a 32-bit (or narrower) raw
 *   encoder count into a monotonically increasing 64-bit integer, correctly
 *   handling counter overflow and underflow at the boundaries of the raw word.
 *
 * - **Index-pulse homing** – when `index_ena` is asserted the driver watches
 *   for the position modulo pulses-per-revolution to change sign, indicating
 *   that the encoder's index mark has been crossed, and snaps the reference
 *   point to the nearest full revolution boundary.
 *
 * - **External latch** – an optional hardware latch event can capture the
 *   exact position at which an external trigger occurred.
 *
 * - **Position reset** – pulsing `pos_reset` from the HAL resets the
 *   reference point to the current position, making `pos` read zero.
 *
 * The `ext_lo` / `ext_hi` HAL pins are exposed as IO so that a motion
 * controller (or HAL component) can seed or persist the 64-bit accumulator
 * across restarts for multi-turn tracking.
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

#include "lcec.h"
#include "class_enc.h"

/** @brief HAL pin descriptors for encoder output and control pins. */
static const lcec_pindesc_t slave_pins[] = {
  { HAL_S32, HAL_OUT, offsetof(lcec_class_enc_data_t, raw), "%s.%s.%s.%s-raw" },
  { HAL_U32, HAL_IO, offsetof(lcec_class_enc_data_t, ext_lo), "%s.%s.%s.%s-ext-lo" },
  { HAL_U32, HAL_IO, offsetof(lcec_class_enc_data_t, ext_hi), "%s.%s.%s.%s-ext-hi" },
  { HAL_U32, HAL_OUT, offsetof(lcec_class_enc_data_t, ref_lo), "%s.%s.%s.%s-ref-lo" },
  { HAL_U32, HAL_OUT, offsetof(lcec_class_enc_data_t, ref_hi), "%s.%s.%s.%s-ref-hi" },
  { HAL_BIT, HAL_IO, offsetof(lcec_class_enc_data_t, index_ena), "%s.%s.%s.%s-index-ena" },
  { HAL_BIT, HAL_IN, offsetof(lcec_class_enc_data_t, pos_reset), "%s.%s.%s.%s-pos-reset" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_class_enc_data_t, pos_enc), "%s.%s.%s.%s-pos-enc" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_class_enc_data_t, pos_abs), "%s.%s.%s.%s-pos-abs" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_class_enc_data_t, pos), "%s.%s.%s.%s-pos" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_class_enc_data_t, on_home_neg), "%s.%s.%s.%s-on-home-neg" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_class_enc_data_t, on_home_pos), "%s.%s.%s.%s-on-home-pos" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/** @brief HAL parameter descriptors for encoder configuration (home position, bit width, scale). */
static const lcec_pindesc_t slave_params[] = {
  { HAL_U32, HAL_RW, offsetof(lcec_class_enc_data_t, raw_home), "%s.%s.%s.%s-raw-home" },
  { HAL_U32, HAL_RO, offsetof(lcec_class_enc_data_t, raw_bits), "%s.%s.%s.%s-raw-bits" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_class_enc_data_t, pprev_scale), "%s.%s.%s.%s-pprev-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

// Forward declarations for internal helper functions
static int32_t raw_diff(int shift, uint32_t raw_a, uint32_t raw_b);
static void set_ref(lcec_class_enc_data_t *hal_data, long long ref);
static long long signed_mod_64(long long val, unsigned long div);

/**
 * @brief Initialise an encoder channel and register its HAL objects.
 *
 * Creates HAL pins (raw, ext_lo/hi, ref_lo/hi, index_ena, pos_reset,
 * pos_enc, pos_abs, pos, on_home_neg, on_home_pos) and parameters
 * (raw_home, raw_bits, pprev_scale) under the
 * `<module>.<master>.<slave>.<pfx>` namespace.
 *
 * Also pre-computes the bit-shift and bit-mask values derived from
 * @p raw_bits that are needed by `class_enc_update` for sign-extension
 * and overflow detection.
 *
 * @param slave     EtherCAT slave descriptor (supplies master and slave names).
 * @param hal_data  Encoder state structure to initialise.
 * @param raw_bits  Width in bits of the encoder's raw position word (1–32).
 * @param pfx       HAL name suffix appended after the slave name.
 * @return          0 on success, negative errno on failure.
 */
int class_enc_init(struct lcec_slave *slave, lcec_class_enc_data_t *hal_data, int raw_bits, const char *pfx) {
  lcec_master_t *master = slave->master;
  int err;

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  hal_data->do_init = 1;
  hal_data->index_sign = 0;

  hal_data->pprev_last = 0;
  hal_data->pprev_scale = 1.0;

  hal_data->raw_bits = raw_bits;
  hal_data->raw_shift = 32 - hal_data->raw_bits;
  hal_data->raw_mask = (1LL << hal_data->raw_bits) - 1;

  return 0;
}

/**
 * @brief Process one encoder sample and update all HAL output pins.
 *
 * This is the hot path called every EtherCAT cycle.  The algorithm:
 *
 *  **Scale computation** – if @p pprev is non-zero and has changed since the
 *  last call (or this is the first call), recompute `pprev_scale = 1/pprev`
 *  and reset the index tracking.  The effective per-count scale applied to
 *  all position outputs is `pprev_scale * scale` (or just `scale` when
 *  pprev == 0).
 *
 *  **64-bit extrapolation** – the raw count is only @p raw_bits wide, so
 *  overflow/underflow must be tracked.  `raw_diff` sign-extends both the new
 *  and previous raw values and returns their signed difference.  This delta
 *  is added to the 64-bit accumulator stored in `ext_hi:ext_lo`.
 *
 *  **Index-pulse detection** – with @p pprev set, the modulo of the 64-bit
 *  position with respect to the period is watched.  A sign change in that
 *  remainder (within an overflow-safe window of pprev/4) indicates that the
 *  encoder's physical index mark was just crossed, and the reference is
 *  snapped to the nearest revolution boundary.
 *
 *  **External latch** – when @p ext_latch_ena is non-zero the reference is
 *  set to the position that corresponds to @p ext_latch_raw.
 *
 *  **Position reset** – when `pos_reset` is asserted (or on first call), the
 *  reference is set to the current 64-bit position, making `pos` read zero.
 *
 * @param hal_data        Encoder state (must be initialised by `class_enc_init`).
 * @param pprev           Pulses per revolution; 0 if not applicable.
 * @param scale           Additional scaling factor (reciprocal of user-units
 *                        per count, so multiply counts × scale → user-units).
 * @param raw             Current raw encoder value from the PDO.
 * @param ext_latch_raw   Raw encoder value at the external latch event.
 * @param ext_latch_ena   Non-zero if @p ext_latch_raw is valid this cycle.
 */
void class_enc_update(lcec_class_enc_data_t *hal_data, uint64_t pprev, double scale, uint32_t raw, uint32_t ext_latch_raw, int ext_latch_ena) {
  long long pos, mod;
  uint32_t ovfl_win;
  int sign;
  double pos_scale;

  // calculate pos scale
  if (pprev > 0) {
    if (hal_data->do_init || hal_data->pprev_last != pprev) {
      hal_data->pprev_scale = 1.0 / ((double) pprev);
      hal_data->index_sign = 0;
    }
    pos_scale = hal_data->pprev_scale * scale;
  } else {
    pos_scale = scale;
  }
  hal_data->pprev_last = pprev;

  // init last encoder value to last known position
  // this could be used to retain extrapolated multiturn tracking
  // IMPORTANT: hi/lo values need to be stored atomic
  if (hal_data->do_init) {
    *(hal_data->raw) = *(hal_data->ext_lo) & hal_data->raw_mask;
  }

  // extrapolate to 64 bits
  pos = ((long long) *(hal_data->ext_hi) << 32) | *(hal_data->ext_lo);
  pos += raw_diff(hal_data->raw_shift, raw, *(hal_data->raw));
  *(hal_data->raw) = raw;
  *(hal_data->ext_hi) = (uint32_t) (pos >> 32);
  *(hal_data->ext_lo) = (uint32_t) pos;

  // set raw encoder pos
  *(hal_data->pos_enc) = ((double) pos) * pos_scale;

  // calculate home based abs pos
  pos += raw_diff(hal_data->raw_shift, 0, hal_data->raw_home);
  *(hal_data->pos_abs) = ((double) pos) * pos_scale;
  *(hal_data->on_home_neg) = (pos <= 0);
  *(hal_data->on_home_pos) = (pos >= 0);

  // handle index
  if (*(hal_data->index_ena)) {
    // get overflow detection window (pprev / 4)
    ovfl_win = pprev >> 2;
    if (ovfl_win == 0 || pprev > 0xffffffff) {
      // no useable singleturn bits -> just reset the position
      *(hal_data->index_ena) = 0;
      set_ref(hal_data, pos);
    } else {
      mod = signed_mod_64(pos, pprev);
      sign = (mod >= 0) ? 1 : -1;
      if (hal_data->index_sign != 0 && sign != hal_data->index_sign && mod <= ovfl_win) {
        *(hal_data->index_ena) = 0;
        set_ref(hal_data, pos - mod);
      }
      hal_data->index_sign = sign;
    }
  } else {
    hal_data->index_sign = 0;
  }

  // handle external latch
  if (ext_latch_ena) {
    set_ref(hal_data, pos + raw_diff(hal_data->raw_shift, ext_latch_raw, raw));
  }

  // handle rel position init
  if (hal_data->do_init || *(hal_data->pos_reset)) {
    set_ref(hal_data, pos);
  }

  // calculate rel pos
  pos -= ((long long) *(hal_data->ref_hi) << 32) | *(hal_data->ref_lo);
  *(hal_data->pos) = ((double) pos) * pos_scale;

  hal_data->do_init = 0;
}

/**
 * @brief Compute the signed difference between two raw encoder counts.
 *
 * Both values are left-shifted by @p shift bits to fill a 32-bit signed
 * integer, then subtracted.  The arithmetic right-shift on the result
 * sign-extends back to the original bit-width, giving the shortest
 * signed path between @p a and @p b (i.e. the difference is in the range
 * [-(2^(raw_bits-1)), +(2^(raw_bits-1))]).
 *
 * This technique correctly handles counter wrap-around at the raw word
 * boundary without any branch.
 *
 * @param shift  32 minus the number of significant encoder bits.
 * @param a      New (current) raw encoder value.
 * @param b      Old (previous) raw encoder value.
 * @return       Signed delta from @p b to @p a.
 */
static int32_t raw_diff(int shift, uint32_t a, uint32_t b) {
  return ((int32_t) (a << shift) - (int32_t) (b << shift)) >> shift;
}

/**
 * @brief Atomically store a 64-bit reference position into the HAL ref pins.
 *
 * Splits @p ref into a high 32-bit word written to `ref_hi` and a low
 * 32-bit word written to `ref_lo`.  These two HAL pins together form the
 * 64-bit reference (zero-point) used when computing the relative position
 * output `pos`.
 *
 * @param hal_data  Encoder state structure whose ref_hi / ref_lo pins to update.
 * @param ref       New 64-bit reference position value.
 */
static void set_ref(lcec_class_enc_data_t *hal_data, long long ref) {
  *(hal_data->ref_hi) = (uint32_t) (ref >> 32);
  *(hal_data->ref_lo) = (uint32_t) ref;
}

/**
 * @brief Compute a signed modulo in the range [-(div/2), +(div/2)].
 *
 * First obtains the non-negative remainder, then folds it
 * into the symmetric range so that values just below @p div are reported as
 * small negative numbers rather than large positives.  This is used by the
 * index-pulse detection code to determine which "side" of the revolution
 * boundary the current position is on.
 *
 * @param val  Value to reduce.
 * @param div  Period (e.g. pulses per revolution).
 * @return     Remainder in the range [-(div/2), +(div/2)].
 */
static long long signed_mod_64(long long val, unsigned long div) {
  long long rem = val % div;

  if (rem < 0) {
    rem += div;
  }

  if (rem > (div >> 1)) {
    rem -= div;
  }

  return rem;
}
