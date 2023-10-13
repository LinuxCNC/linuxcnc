//
//    Copyright (C) 2014 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_em7004.h"

typedef struct {
  hal_bit_t *in;
  hal_bit_t *in_not;
  unsigned int pdo_os;
  unsigned int pdo_bp;
} lcec_em7004_din_t;

typedef struct {
  hal_bit_t *out;
  hal_bit_t invert;
  unsigned int pdo_os;
  unsigned int pdo_bp;
} lcec_em7004_dout_t;

typedef struct {
  hal_bit_t *pos;
  hal_bit_t *neg;
  hal_bit_t *enable;
  hal_bit_t *absmode;
  hal_float_t *value;
  hal_float_t *scale;
  hal_float_t *offset;
  double old_scale;
  double scale_recip;
  hal_float_t *min_dc;
  hal_float_t *max_dc;
  hal_float_t *curr_dc;
  hal_s32_t *raw_val;
  unsigned int val_pdo_os;
} lcec_em7004_aout_t;

typedef struct {
  hal_bit_t *ena_latch_ext_pos;
  hal_bit_t *ena_latch_ext_neg;
  hal_bit_t *reset;
  hal_bit_t *ina;
  hal_bit_t *inb;
  hal_bit_t *ingate;
  hal_bit_t *inext;
  hal_bit_t *latch_ext_valid;
  hal_bit_t *set_raw_count;
  hal_s32_t *set_raw_count_val;
  hal_s32_t *raw_count;
  hal_s32_t *raw_latch;
  hal_s32_t *count;
  hal_float_t *pos_scale;
  hal_float_t *pos;

  unsigned int ena_latch_ext_pos_pdo_os;
  unsigned int ena_latch_ext_pos_pdo_bp;
  unsigned int ena_latch_ext_neg_pdo_os;
  unsigned int ena_latch_ext_neg_pdo_bp;
  unsigned int set_count_pdo_os;
  unsigned int set_count_pdo_bp;
  unsigned int set_count_val_pdo_os;
  unsigned int set_count_done_pdo_os;
  unsigned int set_count_done_pdo_bp;
  unsigned int latch_ext_valid_pdo_os;
  unsigned int latch_ext_valid_pdo_bp;
  unsigned int ina_pdo_os;
  unsigned int ina_pdo_bp;
  unsigned int inb_pdo_os;
  unsigned int inb_pdo_bp;
  unsigned int ingate_pdo_os;
  unsigned int ingate_pdo_bp;
  unsigned int inext_pdo_os;
  unsigned int inext_pdo_bp;
  unsigned int count_pdo_os;
  unsigned int latch_pdo_os;

  int do_init;
  int16_t last_count;
  double old_scale;
  double scale;
} lcec_em7004_enc_t;

typedef struct {
  lcec_em7004_din_t dins[LCEC_EM7004_DIN_COUNT];
  lcec_em7004_dout_t douts[LCEC_EM7004_DOUT_COUNT];
  lcec_em7004_aout_t aouts[LCEC_EM7004_AOUT_COUNT];
  lcec_em7004_enc_t encs[LCEC_EM7004_ENC_COUNT];
  int last_operational;
} lcec_em7004_data_t;

static const lcec_pindesc_t slave_din_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_din_t, in), "%s.%s.%s.din-%d" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_din_t, in_not), "%s.%s.%s.din-%d-not" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_dout_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_em7004_dout_t, out), "%s.%s.%s.dout-%d" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_dout_params[] = {
  { HAL_BIT, HAL_RW, offsetof(lcec_em7004_dout_t, invert), "%s.%s.%s.dout-%d-invert" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_aout_pins[] = {
  { HAL_FLOAT, HAL_IO, offsetof(lcec_em7004_aout_t, scale), "%s.%s.%s.aout-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_em7004_aout_t, offset), "%s.%s.%s.aout-%d-offset" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_em7004_aout_t, min_dc), "%s.%s.%s.aout-%d-min-dc" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_em7004_aout_t, max_dc), "%s.%s.%s.aout-%d-max-dc" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_em7004_aout_t, curr_dc), "%s.%s.%s.aout-%d-curr-dc" },
  { HAL_BIT, HAL_IN, offsetof(lcec_em7004_aout_t, enable), "%s.%s.%s.aout-%d-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_em7004_aout_t, absmode), "%s.%s.%s.aout-%d-absmode" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_em7004_aout_t, value), "%s.%s.%s.aout-%d-value" },
  { HAL_S32, HAL_OUT, offsetof(lcec_em7004_aout_t, raw_val), "%s.%s.%s.aout-%d-raw" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_aout_t, pos), "%s.%s.%s.aout-%d-pos" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_aout_t, neg), "%s.%s.%s.aout-%d-neg" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_enc_pins[] = {
  { HAL_BIT, HAL_IO, offsetof(lcec_em7004_enc_t, ena_latch_ext_pos), "%s.%s.%s.enc-%d-index-ext-pos-enable" },
  { HAL_BIT, HAL_IO, offsetof(lcec_em7004_enc_t, ena_latch_ext_neg), "%s.%s.%s.enc-%d-index-ext-neg-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_em7004_enc_t, reset), "%s.%s.%s.enc-%d-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_enc_t, ina), "%s.%s.%s.enc-%d-ina" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_enc_t, inb), "%s.%s.%s.enc-%d-inb" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_enc_t, ingate), "%s.%s.%s.enc-%d-ingate" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_enc_t, inext), "%s.%s.%s.enc-%d-inext" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_em7004_enc_t, latch_ext_valid), "%s.%s.%s.enc-%d-latch-ext-valid" },
  { HAL_BIT, HAL_IO, offsetof(lcec_em7004_enc_t, set_raw_count), "%s.%s.%s.enc-%d-set-raw-count" },
  { HAL_S32, HAL_IN, offsetof(lcec_em7004_enc_t, set_raw_count_val), "%s.%s.%s.enc-%d-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_em7004_enc_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_em7004_enc_t, count), "%s.%s.%s.enc-%d-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_em7004_enc_t, raw_latch), "%s.%s.%s.enc-%d-raw-latch" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_em7004_enc_t, pos), "%s.%s.%s.enc-%d-pos" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_em7004_enc_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

void lcec_em7004_read(struct lcec_slave *slave, long period);
void lcec_em7004_write(struct lcec_slave *slave, long period);

int lcec_em7004_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_em7004_data_t *hal_data;
  lcec_em7004_din_t *din;
  lcec_em7004_dout_t *dout;
  lcec_em7004_aout_t *aout;
  lcec_em7004_enc_t *enc;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_em7004_read;
  slave->proc_write = lcec_em7004_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_em7004_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_em7004_data_t));
  slave->hal_data = hal_data;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize digital input pins
  for (i=0, din=hal_data->dins; i<LCEC_EM7004_DIN_COUNT; i++, din++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, i + 1, &din->pdo_os, &din->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf_list(din, slave_din_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  // initialize digital output pins
  for (i=0, dout=hal_data->douts; i<LCEC_EM7004_DOUT_COUNT; i++, dout++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, i + 1, &dout->pdo_os, &dout->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf_list(dout, slave_dout_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
    if ((err = lcec_param_newf_list(dout, slave_dout_params, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  // initialize analog output pins
  for (i=0, aout=hal_data->aouts; i<LCEC_EM7004_AOUT_COUNT; i++, aout++) {
    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x11, &aout->val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(aout, slave_aout_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // set default pin values
    *(aout->scale) = 1.0;
    *(aout->min_dc) = -1.0;
    *(aout->max_dc) = 1.0;

    // init other fields
    aout->old_scale = *(aout->scale) + 1.0;
    aout->scale_recip = 1.0;
  }

  // initialize encoder pins
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x02, &enc->latch_ext_valid_pdo_os, &enc->latch_ext_valid_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x03, &enc->set_count_done_pdo_os, &enc->set_count_done_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x09, &enc->ina_pdo_os, &enc->ina_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0a, &enc->inb_pdo_os, &enc->inb_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0c, &enc->ingate_pdo_os, &enc->ingate_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0d, &enc->inext_pdo_os, &enc->inext_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x11, &enc->count_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x12, &enc->latch_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x02, &enc->ena_latch_ext_pos_pdo_os, &enc->ena_latch_ext_pos_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x03, &enc->set_count_pdo_os, &enc->set_count_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x04, &enc->ena_latch_ext_neg_pdo_os, &enc->ena_latch_ext_neg_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x11, &enc->set_count_val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(enc, slave_enc_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(enc->pos_scale) = 1.0;

    // initialize variables
    enc->do_init = 1;
    enc->last_count = 0;
    enc->old_scale = *(enc->pos_scale) + 1.0;
    enc->scale = 1.0;
  }

  return 0;
}

void lcec_em7004_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_em7004_data_t *hal_data = (lcec_em7004_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_em7004_din_t *din;
  lcec_em7004_enc_t *enc;
  int i, s;
  int16_t raw_count, raw_latch, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check digital inputs
  for (i=0, din=hal_data->dins; i<LCEC_EM7004_DIN_COUNT; i++, din++) {
    s = EC_READ_BIT(&pd[din->pdo_os], din->pdo_bp);
    *(din->in) = s;
    *(din->in_not) = !s;
  }

  // read encoder data
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    // check for change in scale value
    if (*(enc->pos_scale) != enc->old_scale) {
      // scale value has changed, test and update it
      if ((*(enc->pos_scale) < 1e-20) && (*(enc->pos_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(enc->pos_scale) = 1.0;
      }
      // save new scale to detect future changes
      enc->old_scale = *(enc->pos_scale);
      // we actually want the reciprocal
      enc->scale = 1.0 / *(enc->pos_scale);
    }

    // get bit states
    *(enc->ina) = EC_READ_BIT(&pd[enc->ina_pdo_os], enc->ina_pdo_bp);
    *(enc->inb) = EC_READ_BIT(&pd[enc->inb_pdo_os], enc->inb_pdo_bp);
    *(enc->ingate) = EC_READ_BIT(&pd[enc->ingate_pdo_os], enc->ingate_pdo_bp);
    *(enc->inext) = EC_READ_BIT(&pd[enc->inext_pdo_os], enc->inext_pdo_bp);
    *(enc->latch_ext_valid) = EC_READ_BIT(&pd[enc->latch_ext_valid_pdo_os], enc->latch_ext_valid_pdo_bp);

    // read raw values
    raw_count = EC_READ_S16(&pd[enc->count_pdo_os]);
    raw_latch = EC_READ_S16(&pd[enc->latch_pdo_os]);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      enc->last_count = raw_count;
    }

    // check for counter set done
    if (EC_READ_BIT(&pd[enc->set_count_done_pdo_os], enc->set_count_done_pdo_bp)) {
      enc->last_count = raw_count;
      *(enc->set_raw_count) = 0;
    }

    // update raw values
    if (! *(enc->set_raw_count)) {
      *(enc->raw_count) = raw_count;
    }

    // handle initialization
    if (enc->do_init || *(enc->reset)) {
      enc->do_init = 0;
      enc->last_count = raw_count;
      *(enc->count) = 0;
    }

    // handle index
    if (*(enc->latch_ext_valid)) {
      *(enc->raw_latch) = raw_latch;
      enc->last_count = raw_latch;
      *(enc->count) = 0;
      *(enc->ena_latch_ext_pos) = 0;
      *(enc->ena_latch_ext_neg) = 0;
    }

    // compute net counts
    raw_delta = raw_count - enc->last_count;
    enc->last_count = raw_count;
    *(enc->count) += raw_delta;

    // scale count to make floating point position
    *(enc->pos) = *(enc->count) * enc->scale;
  }

  hal_data->last_operational = 1;
}

void lcec_em7004_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_em7004_data_t *hal_data = (lcec_em7004_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_em7004_dout_t *dout;
  lcec_em7004_aout_t *aout;
  lcec_em7004_enc_t *enc;
  int i, s;
  double tmpval, tmpdc, raw_val;

  // set digital outputs
  for (i=0, dout=hal_data->douts; i<LCEC_EM7004_DOUT_COUNT; i++, dout++) {
    s = *(dout->out);
    if (dout->invert) {
      s = !s;
    }
    EC_WRITE_BIT(&pd[dout->pdo_os], dout->pdo_bp, s);
  }

  // set analog outputs
  for (i=0, aout=hal_data->aouts; i<LCEC_EM7004_AOUT_COUNT; i++, aout++) {
    // validate duty cycle limits, both limits must be between
    // 0.0 and 1.0 (inclusive) and max must be greater then min
    if (*(aout->max_dc) > 1.0) {
      *(aout->max_dc) = 1.0;
    }
    if (*(aout->min_dc) > *(aout->max_dc)) {
      *(aout->min_dc) = *(aout->max_dc);
    }
    if (*(aout->min_dc) < -1.0) {
      *(aout->min_dc) = -1.0;
    }
    if (*(aout->max_dc) < *(aout->min_dc)) {
      *(aout->max_dc) = *(aout->min_dc);
    }

    // do scale calcs only when scale changes
    if (*(aout->scale) != aout->old_scale) {
      // validate the new scale value
      if ((*(aout->scale) < 1e-20) && (*(aout->scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(aout->scale) = 1.0;
      }
      // get ready to detect future scale changes
      aout->old_scale = *(aout->scale);
      // we will need the reciprocal
      aout->scale_recip = 1.0 / *(aout->scale);
    }

    // get command
    tmpval = *(aout->value);
    if (*(aout->absmode) && (tmpval < 0)) {
      tmpval = -tmpval;
    }

    // convert value command to duty cycle
    tmpdc = tmpval * aout->scale_recip + *(aout->offset);
    if (tmpdc < *(aout->min_dc)) {
      tmpdc = *(aout->min_dc);
    }
    if (tmpdc > *(aout->max_dc)) {
      tmpdc = *(aout->max_dc);
    }

    // set output values
    if (*(aout->enable) == 0) {
      raw_val = 0;
      *(aout->pos) = 0;
      *(aout->neg) = 0;
      *(aout->curr_dc) = 0;
    } else {
      raw_val = (double)0x7fff * tmpdc;
      if (raw_val > (double)0x7fff) {
        raw_val = (double)0x7fff;
      }
      if (raw_val < (double)-0x7fff) {
        raw_val = (double)-0x7fff;
      }
      *(aout->pos) = (*(aout->value) > 0);
      *(aout->neg) = (*(aout->value) < 0);
      *(aout->curr_dc) = tmpdc;
    }

    // update value
    EC_WRITE_S16(&pd[aout->val_pdo_os], (int16_t)raw_val);
    *(aout->raw_val) = (int32_t)raw_val;
  }

  // write encoder data
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    EC_WRITE_BIT(&pd[enc->set_count_pdo_os], enc->set_count_pdo_bp, *(enc->set_raw_count));
    EC_WRITE_BIT(&pd[enc->ena_latch_ext_pos_pdo_os], enc->ena_latch_ext_pos_pdo_bp, *(enc->ena_latch_ext_pos));
    EC_WRITE_BIT(&pd[enc->ena_latch_ext_neg_pdo_os], enc->ena_latch_ext_neg_pdo_bp, *(enc->ena_latch_ext_neg));
    EC_WRITE_S16(&pd[enc->set_count_val_pdo_os], *(enc->set_raw_count_val));
  }
}

