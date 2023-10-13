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

#include "lcec.h"
#include "lcec_class_enc.h"

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

static const lcec_pindesc_t slave_params[] = {
  { HAL_U32, HAL_RW, offsetof(lcec_class_enc_data_t, raw_home), "%s.%s.%s.%s-raw-home" },
  { HAL_U32, HAL_RO, offsetof(lcec_class_enc_data_t, raw_bits), "%s.%s.%s.%s-raw-bits" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_class_enc_data_t, pprev_scale), "%s.%s.%s.%s-pprev-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static int32_t raw_diff(int shift, uint32_t raw_a, uint32_t raw_b);
static void set_ref(lcec_class_enc_data_t *hal_data, long long ref);
static long long signed_mod_64(long long val, unsigned long div);

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

static int32_t raw_diff(int shift, uint32_t a, uint32_t b) {
  return ((int32_t) (a << shift) - (int32_t) (b << shift)) >> shift;
}

static void set_ref(lcec_class_enc_data_t *hal_data, long long ref) {
  *(hal_data->ref_hi) = (uint32_t) (ref >> 32);
  *(hal_data->ref_lo) = (uint32_t) ref;
}

static long long signed_mod_64(long long val, unsigned long div) {
  long long rem = lcec_mod_64(val, div);

  if (rem < 0) {
    rem += div;
  }

  if (rem > (div >> 1)) {
    rem -= div;
  }

  return rem;
}


