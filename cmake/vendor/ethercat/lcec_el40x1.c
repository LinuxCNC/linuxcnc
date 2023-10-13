//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_el40x1.h"

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
} lcec_el40x1_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el40x1_data_t, scale), "%s.%s.%s.aout-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el40x1_data_t, offset), "%s.%s.%s.aout-offset" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el40x1_data_t, min_dc), "%s.%s.%s.aout-min-dc" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el40x1_data_t, max_dc), "%s.%s.%s.aout-max-dc" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el40x1_data_t, curr_dc), "%s.%s.%s.aout-curr-dc" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el40x1_data_t, enable), "%s.%s.%s.aout-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el40x1_data_t, absmode), "%s.%s.%s.aout-absmode" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_el40x1_data_t, value), "%s.%s.%s.aout-value" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el40x1_data_t, raw_val), "%s.%s.%s.aout-raw" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el40x1_data_t, pos), "%s.%s.%s.aout-pos" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el40x1_data_t, neg), "%s.%s.%s.aout-neg" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};


static ec_pdo_entry_info_t lcec_el40x1_channel[] = {
    {0x7000, 1, 16}  // output
};

static ec_pdo_info_t lcec_el40x1_pdos_in[] = {
    {0x1600, 1, lcec_el40x1_channel},
};

static ec_sync_info_t lcec_el40x1_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_el40x1_pdos_in},
    {0xff}
};

void lcec_el40x1_write(struct lcec_slave *slave, long period);

int lcec_el40x1_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el40x1_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el40x1_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el40x1_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el40x1_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el40x1_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->val_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // set default pin values
  *(hal_data->scale) = 1.0;
  *(hal_data->min_dc) = -1.0;
  *(hal_data->max_dc) = 1.0;

  // init other fields
  hal_data->old_scale = *(hal_data->scale) + 1.0;
  hal_data->scale_recip = 1.0;

  return 0;
}

void lcec_el40x1_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el40x1_data_t *hal_data = (lcec_el40x1_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  double tmpval, tmpdc, raw_val;

  // validate duty cycle limits, both limits must be between
  // 0.0 and 1.0 (inclusive) and max must be greater then min
  if (*(hal_data->max_dc) > 1.0) {
    *(hal_data->max_dc) = 1.0;
  }
  if (*(hal_data->min_dc) > *(hal_data->max_dc)) {
    *(hal_data->min_dc) = *(hal_data->max_dc);
  }
  if (*(hal_data->min_dc) < -1.0) {
    *(hal_data->min_dc) = -1.0;
  }
  if (*(hal_data->max_dc) < *(hal_data->min_dc)) {
    *(hal_data->max_dc) = *(hal_data->min_dc);
  }

  // do scale calcs only when scale changes
  if (*(hal_data->scale) != hal_data->old_scale) {
    // validate the new scale value
    if ((*(hal_data->scale) < 1e-20) && (*(hal_data->scale) > -1e-20)) {
      // value too small, divide by zero is a bad thing
      *(hal_data->scale) = 1.0;
    }
    // get ready to detect future scale changes
    hal_data->old_scale = *(hal_data->scale);
    // we will need the reciprocal
    hal_data->scale_recip = 1.0 / *(hal_data->scale);
  }

  // get command
  tmpval = *(hal_data->value);
  if (*(hal_data->absmode) && (tmpval < 0)) {
    tmpval = -tmpval;
  }

  // convert value command to duty cycle
  tmpdc = tmpval * hal_data->scale_recip + *(hal_data->offset);
  if (tmpdc < *(hal_data->min_dc)) {
    tmpdc = *(hal_data->min_dc);
  }
  if (tmpdc > *(hal_data->max_dc)) {
    tmpdc = *(hal_data->max_dc);
  }

  // set output values
  if (*(hal_data->enable) == 0) {
    raw_val = 0;
    *(hal_data->pos) = 0;
    *(hal_data->neg) = 0;
    *(hal_data->curr_dc) = 0;
  } else {
    raw_val = (double)0x7fff * tmpdc;
    if (raw_val > (double)0x7fff) {
      raw_val = (double)0x7fff;
    }
    if (raw_val < (double)-0x7fff) {
      raw_val = (double)-0x7fff;
    }
    *(hal_data->pos) = (*(hal_data->value) > 0);
    *(hal_data->neg) = (*(hal_data->value) < 0);
    *(hal_data->curr_dc) = tmpdc;
  }

  // update value
  EC_WRITE_S16(&pd[hal_data->val_pdo_os], (int16_t)raw_val);
  *(hal_data->raw_val) = (int32_t)raw_val;
}

