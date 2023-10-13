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
#ifndef _LCEC_CLASS_ENC_H_
#define _LCEC_CLASS_ENC_H_

#include "lcec.h"

typedef struct {
  hal_s32_t raw_home;
  hal_u32_t raw_bits;
  hal_float_t pprev_scale;

  hal_s32_t *raw;
  hal_u32_t *ext_lo;
  hal_u32_t *ext_hi;
  hal_u32_t *ref_lo;
  hal_u32_t *ref_hi;

  hal_bit_t *index_ena;
  hal_bit_t *pos_reset;

  hal_float_t *pos_enc;
  hal_float_t *pos_abs;
  hal_float_t *pos;

  hal_bit_t *on_home_neg;
  hal_bit_t *on_home_pos;

  int do_init;

  int raw_shift;
  uint32_t raw_mask;

  uint64_t pprev_last;

  int index_sign;

} lcec_class_enc_data_t;

int class_enc_init(struct lcec_slave *slave, lcec_class_enc_data_t *hal_data, int raw_bits, const char *pfx);
void class_enc_update(lcec_class_enc_data_t *hal_data, uint64_t pprev, double scale, uint32_t raw, uint32_t ext_latch_raw, int ext_latch_ena);

#endif
