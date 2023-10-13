//
//    Copyright (C) 2016 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_el3255.h"

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
} lcec_el3255_chan_t;

typedef struct {
  lcec_el3255_chan_t chans[LCEC_EL3255_CHANS];
} lcec_el3255_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el3255_chan_t, overrange), "%s.%s.%s.pot-%d-overrange" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el3255_chan_t, underrange), "%s.%s.%s.pot-%d-underrange" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el3255_chan_t, error), "%s.%s.%s.pot-%d-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el3255_chan_t, sync_err), "%s.%s.%s.pot-%d-sync-err" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el3255_chan_t, raw_val), "%s.%s.%s.pot-%d-raw" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el3255_chan_t, val), "%s.%s.%s.pot-%d-val" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el3255_chan_t, scale), "%s.%s.%s.pot-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el3255_chan_t, bias), "%s.%s.%s.pot-%d-bias" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};


static ec_pdo_entry_info_t lcec_el3255_channel1[] = {
    {0x6000, 0x01,  1}, // Underrange
    {0x6000, 0x02,  1}, // Overrange
    {0x6000, 0x03,  2}, // Limit 1
    {0x6000, 0x05,  2}, // Limit 2
    {0x6000, 0x07,  1}, // Error
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  5}, // Gap
    {0x6000, 0x0e,  1}, // Sync error
    {0x6000, 0x0f,  1}, // TxPDO State
    {0x6000, 0x10,  1}, // TxPDO Toggle
    {0x6000, 0x11, 16}  // Value
};

static ec_pdo_entry_info_t lcec_el3255_channel2[] = {
    {0x6010, 0x01,  1}, // Underrange
    {0x6010, 0x02,  1}, // Overrange
    {0x6010, 0x03,  2}, // Limit 1
    {0x6010, 0x05,  2}, // Limit 2
    {0x6010, 0x07,  1}, // Error
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  5}, // Gap
    {0x6010, 0x0e,  1}, // Sync error
    {0x6010, 0x0f,  1}, // TxPDO State
    {0x6010, 0x10,  1}, // TxPDO Toggle
    {0x6010, 0x11, 16}  // Value
};

static ec_pdo_entry_info_t lcec_el3255_channel3[] = {
    {0x6020, 0x01,  1}, // Underrange
    {0x6020, 0x02,  1}, // Overrange
    {0x6020, 0x03,  2}, // Limit 1
    {0x6020, 0x05,  2}, // Limit 2
    {0x6020, 0x07,  1}, // Error
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  5}, // Gap
    {0x6020, 0x0e,  1}, // Sync error
    {0x6020, 0x0f,  1}, // TxPDO State
    {0x6020, 0x10,  1}, // TxPDO Toggle
    {0x6020, 0x11, 16}  // Value
};

static ec_pdo_entry_info_t lcec_el3255_channel4[] = {
    {0x6030, 0x01,  1}, // Underrange
    {0x6030, 0x02,  1}, // Overrange
    {0x6030, 0x03,  2}, // Limit 1
    {0x6030, 0x05,  2}, // Limit 2
    {0x6030, 0x07,  1}, // Error
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  5}, // Gap
    {0x6030, 0x0e,  1}, // Sync error
    {0x6030, 0x0f,  1}, // TxPDO State
    {0x6030, 0x10,  1}, // TxPDO Toggle
    {0x6030, 0x11, 16}  // Value
};

static ec_pdo_entry_info_t lcec_el3255_channel5[] = {
    {0x6040, 0x01,  1}, // Underrange
    {0x6040, 0x02,  1}, // Overrange
    {0x6040, 0x03,  2}, // Limit 1
    {0x6040, 0x05,  2}, // Limit 2
    {0x6040, 0x07,  1}, // Error
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  5}, // Gap
    {0x6040, 0x0e,  1}, // Sync error
    {0x6040, 0x0f,  1}, // TxPDO State
    {0x6040, 0x10,  1}, // TxPDO Toggle
    {0x6040, 0x11, 16}  // Value
};

static ec_pdo_info_t lcec_el3255_pdos_in[] = {
    {0x1A00, 11, lcec_el3255_channel1},
    {0x1A02, 11, lcec_el3255_channel2},
    {0x1A04, 11, lcec_el3255_channel3},
    {0x1A06, 11, lcec_el3255_channel4},
    {0x1A08, 11, lcec_el3255_channel5}
};

static ec_sync_info_t lcec_el3255_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 0, NULL},
    {3, EC_DIR_INPUT,  5, lcec_el3255_pdos_in},
    {0xff}
};

void lcec_el3255_read(struct lcec_slave *slave, long period);

int lcec_el3255_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el3255_data_t *hal_data;
  lcec_el3255_chan_t *chan;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el3255_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el3255_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el3255_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_el3255_syncs;

  // initialize pins
  for (i=0; i<LCEC_EL3255_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->ovr_pdo_os, &chan->ovr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->udr_pdo_os, &chan->udr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x07, &chan->error_pdo_os, &chan->error_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0e, &chan->sync_err_pdo_os, &chan->sync_err_pdo_bp);
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

void lcec_el3255_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el3255_data_t *hal_data = (lcec_el3255_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el3255_chan_t *chan;
  int16_t value;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL3255_CHANS; i++) {
    chan = &hal_data->chans[i];

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

