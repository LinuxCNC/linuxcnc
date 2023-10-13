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
#include "lcec_el31x4.h"

typedef struct {
  hal_bit_t *overrange;
  hal_bit_t *underrange;
  hal_bit_t *error;
  hal_bit_t *sync_err;
  hal_s32_t *raw_val;
  hal_float_t *scale;
  hal_float_t *bias;
  hal_float_t *val;
  unsigned int ovr_pdo_os;
  unsigned int ovr_pdo_bp;
  unsigned int udr_pdo_os;
  unsigned int udr_pdo_bp;
  unsigned int error_pdo_os;
  unsigned int error_pdo_bp;
  unsigned int sync_err_pdo_os;
  unsigned int sync_err_pdo_bp;
  unsigned int val_pdo_os;
} lcec_el31x4_chan_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el31x4_chan_t ,error), "%s.%s.%s.ain-%d-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el31x4_chan_t ,sync_err), "%s.%s.%s.ain-%d-sync-err" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el31x4_chan_t ,overrange), "%s.%s.%s.ain-%d-overrange" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el31x4_chan_t ,underrange), "%s.%s.%s.ain-%d-underrange" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el31x4_chan_t ,raw_val), "%s.%s.%s.ain-%d-raw" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el31x4_chan_t ,val), "%s.%s.%s.ain-%d-val" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el31x4_chan_t ,scale), "%s.%s.%s.ain-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el31x4_chan_t ,bias), "%s.%s.%s.ain-%d-bias" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

typedef struct {
  lcec_el31x4_chan_t chans[LCEC_EL31x4_CHANS];
} lcec_el31x4_data_t;

void lcec_el31x4_read(struct lcec_slave *slave, long period);

int lcec_el31x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el31x4_data_t *hal_data;
  lcec_el31x4_chan_t *chan;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el31x4_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el31x4_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el31x4_data_t));
  slave->hal_data = hal_data;

  // initialize pins
  for (i=0; i<LCEC_EL31x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->udr_pdo_os, &chan->udr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->ovr_pdo_os, &chan->ovr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x07, &chan->error_pdo_os, &chan->error_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0E, &chan->sync_err_pdo_os, &chan->sync_err_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(chan->scale) = 1.0;
  }

  return 0;
}

void lcec_el31x4_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el31x4_data_t *hal_data = (lcec_el31x4_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el31x4_chan_t *chan;
  int16_t value;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL31x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // update state
    // update state
    *(chan->overrange) = EC_READ_BIT(&pd[chan->ovr_pdo_os], chan->ovr_pdo_bp);
    *(chan->underrange) = EC_READ_BIT(&pd[chan->udr_pdo_os], chan->udr_pdo_bp);
    *(chan->error) = EC_READ_BIT(&pd[chan->error_pdo_os], chan->error_pdo_bp);
    *(chan->sync_err) = EC_READ_BIT(&pd[chan->sync_err_pdo_os], chan->sync_err_pdo_bp);

    // update value
    value = EC_READ_S16(&pd[chan->val_pdo_os]);
    *(chan->raw_val) = value;
    *(chan->val) = *(chan->bias) + *(chan->scale) * (double)value * ((double)1/(double)0x7fff);
  }
}

