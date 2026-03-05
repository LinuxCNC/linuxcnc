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

#include "../lcec.h"
#include "el5122.h"

typedef struct {
  hal_bit_t *ena_latch_ext_pos;
  hal_bit_t *ena_latch_ext_neg;
  hal_bit_t *index;
  hal_bit_t *index_ena;
  hal_bit_t *reset;
  hal_bit_t *underflow;
  hal_bit_t *overflow;
  hal_bit_t *ina;
  hal_bit_t *inb;
  hal_bit_t *gate_state;
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
  unsigned int undeflow_pdo_os;
  unsigned int undeflow_pdo_bp;
  unsigned int overflow_pdo_os;
  unsigned int overflow_pdo_bp;
  unsigned int ina_pdo_os;
  unsigned int ina_pdo_bp;
  unsigned int inb_pdo_os;
  unsigned int inb_pdo_bp;
  unsigned int gate_state_pdo_os;
  unsigned int gate_state_pdo_bp;
  unsigned int count_pdo_os;
  unsigned int latch_pdo_os;

  int do_init;
  int32_t last_count;
  int last_index;
  double old_scale;
  double scale;
} lcec_el5122_chan_t;

typedef struct {
  lcec_el5122_chan_t chans[LCEC_EL5122_CHANS];
  int last_operational;
} lcec_el5122_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IO, offsetof(lcec_el5122_chan_t, ena_latch_ext_pos), "%s.%s.%s.enc-%d-index-ext-pos-enable" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5122_chan_t, ena_latch_ext_neg), "%s.%s.%s.enc-%d-index-ext-neg-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5122_chan_t, index), "%s.%s.%s.enc-%d-index" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5122_chan_t, index_ena), "%s.%s.%s.enc-%d-index-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5122_chan_t, reset), "%s.%s.%s.enc-%d-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, underflow), "%s.%s.%s.enc-%d-underflow" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, overflow), "%s.%s.%s.enc-%d-overflow" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, ina), "%s.%s.%s.enc-%d-ina" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, inb), "%s.%s.%s.enc-%d-inb" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, gate_state), "%s.%s.%s.enc-%d-gate-state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5122_chan_t, latch_ext_valid), "%s.%s.%s.enc-%d-latch-ext-valid" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5122_chan_t, set_raw_count), "%s.%s.%s.enc-%d-set-raw-count" },
  { HAL_S32, HAL_IN, offsetof(lcec_el5122_chan_t, set_raw_count_val), "%s.%s.%s.enc-%d-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5122_chan_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5122_chan_t, raw_latch), "%s.%s.%s.enc-%d-raw-latch" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5122_chan_t, count), "%s.%s.%s.enc-%d-count" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5122_chan_t, pos), "%s.%s.%s.enc-%d-pos" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el5122_chan_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5122_channel1_in[] = {
   {0x0000, 0x01, 1},
   {0x6000, 0x02, 1},  // Latch extern valid
   {0x6000, 0x03, 1},  // Set counter done
   {0x6000, 0x04, 1},  // Counter underflow
   {0x6000, 0x05, 1},  // Counter overflow
   {0x0000, 0x06, 1},
   {0x0000, 0x07, 1},
   {0x0000, 0x08, 1},
   {0x6000, 0x09, 1},  // Status of input A
   {0x6000, 0x0a, 1},  // Status of input B
   {0x0000, 0x0b, 1},
   {0x6000, 0x0c, 1},  // Status of input gate
   {0x6002, 0x0d, 1},  // Diag
   {0x6002, 0x0e, 1},  // TxPDO State
   {0x6002, 0x0f, 2},  // Input cycle counter
   {0x6002, 0x11, 1},  // Software gate valid
   {0x0000, 0x12, 1},
   {0x0000, 0x13, 1},
   {0x0000, 0x14, 1},
   {0x6002, 0x15, 1},  // Counter value out of range
   {0x0000, 0x00, 3},  // Gap
   {0x0000, 0x00, 8},  // Gap
   {0x6000, 0x11, 32}, // Counter value
   {0x6000, 0x12, 32}, // Latch value
};

static ec_pdo_entry_info_t lcec_el5122_channel2_in[] = {
   {0x0000, 0x01, 1},
   {0x6010, 0x02, 1},  // Latch extern valid
   {0x6010, 0x03, 1},  // Set counter done
   {0x6010, 0x04, 1},  // Counter underflow
   {0x6010, 0x05, 1},  // Counter overflow
   {0x0000, 0x06, 1},
   {0x0000, 0x07, 1},
   {0x0000, 0x08, 1},
   {0x6010, 0x09, 1},  // Status of input A
   {0x6010, 0x0a, 1},  // Status of input B
   {0x0000, 0x0b, 1},
   {0x6010, 0x0c, 1},  // Status of input gate
   {0x6002, 0x0d, 1},  // Diag
   {0x6002, 0x0e, 1},  // TxPDO State
   {0x6002, 0x0f, 2},  // Input cycle counter
   {0x6002, 0x11, 1},  // Software gate valid
   {0x0000, 0x12, 1},
   {0x0000, 0x13, 1},
   {0x0000, 0x14, 1},
   {0x6002, 0x15, 1},  // Counter value out of range
   {0x0000, 0x00, 3},  // Gap
   {0x0000, 0x00, 8},  // Gap
   {0x6010, 0x11, 32}, // Counter value
   {0x6010, 0x12, 32}  // Latch value
};

static ec_pdo_entry_info_t lcec_el5122_channel1_out[] = {
   {0x0000, 0x00, 1}, // Gap
   {0x7000, 0x02, 1}, // Enable latch extern on positive edge
   {0x7000, 0x03, 1}, // Set counter
   {0x7000, 0x04, 1}, // Enable latch extern on negative edge
   {0x0000, 0x00, 3}, // Gap
   {0x0000, 0x08, 1},
   {0x7000, 0x09, 1}, // Set software gate
   {0x7000, 0x0a, 1}, // Set counter on latch extern on positive edge
   {0x7000, 0x0b, 1}, // Set counter on latch extern on negative edge
   {0x0000, 0x0c, 1},
   {0x0000, 0x0d, 1},
   {0x0000, 0x00, 3}, // Gap
   {0x7000, 0x11, 32} // Set counter value
};

static ec_pdo_entry_info_t lcec_el5122_channel2_out[] = {
   {0x0000, 0x00, 1}, // Gap
   {0x7010, 0x02, 1}, // Enable latch extern on positive edge
   {0x7010, 0x03, 1}, // Set counter
   {0x7010, 0x04, 1}, // Enable latch extern on negative edge
   {0x0000, 0x00, 3}, // Gap
   {0x0000, 0x08, 1},
   {0x7010, 0x09, 1}, // Set software gate
   {0x7010, 0x0a, 1}, // Set counter on latch extern on positive edge
   {0x7010, 0x0b, 1}, // Set counter on latch extern on negative edge
   {0x0000, 0x0c, 1},
   {0x0000, 0x0d, 1},
   {0x0000, 0x00, 3}, // Gap
   {0x7010, 0x11, 32} // Set counter value
};

static ec_pdo_info_t lcec_el5122_pdos_out[] = {
    {0x1600, 13, lcec_el5122_channel1_out},
    {0x1604, 13, lcec_el5122_channel2_out}
};

static ec_pdo_info_t lcec_el5122_pdos_in[] = {
    {0x1A00, 24, lcec_el5122_channel1_in},
    {0x1A08, 24, lcec_el5122_channel2_in},
};

static ec_sync_info_t lcec_el5122_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_el5122_pdos_out},
    {3, EC_DIR_INPUT,  2, lcec_el5122_pdos_in},
    {0xff}
};


void lcec_el5122_read(struct lcec_slave *slave, long period);
void lcec_el5122_write(struct lcec_slave *slave, long period);

int lcec_el5122_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el5122_data_t *hal_data;
  int i;
  lcec_el5122_chan_t *chan;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el5122_read;
  slave->proc_write = lcec_el5122_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el5122_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5122_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el5122_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize pins
  for (i=0; i<LCEC_EL5122_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->latch_ext_valid_pdo_os, &chan->latch_ext_valid_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x03, &chan->set_count_done_pdo_os, &chan->set_count_done_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x04, &chan->undeflow_pdo_os, &chan->overflow_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x05, &chan->overflow_pdo_os, &chan->overflow_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x09, &chan->ina_pdo_os, &chan->ina_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0a, &chan->inb_pdo_os, &chan->inb_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0c, &chan->gate_state_pdo_os, &chan->gate_state_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->count_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x12, &chan->latch_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x02, &chan->ena_latch_ext_pos_pdo_os, &chan->ena_latch_ext_pos_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x03, &chan->set_count_pdo_os, &chan->set_count_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x04, &chan->ena_latch_ext_neg_pdo_os, &chan->ena_latch_ext_neg_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x11, &chan->set_count_val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(chan->pos_scale) = 1.0;

    // initialize variables
    chan->do_init = 1;
    chan->last_count = 0;
    chan->last_index = 0;
    chan->old_scale = *(chan->pos_scale) + 1.0;
    chan->scale = 1.0;
  }

  return 0;
}

void lcec_el5122_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5122_data_t *hal_data = (lcec_el5122_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i, idx_flag;
  lcec_el5122_chan_t *chan;
  int32_t idx_count, raw_count, raw_latch, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL5122_CHANS; i++) {
    chan = &hal_data->chans[i];

    // check for change in scale value
    if (*(chan->pos_scale) != chan->old_scale) {
      // scale value has changed, test and update it
      if ((*(chan->pos_scale) < 1e-20) && (*(chan->pos_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(chan->pos_scale) = 1.0;
      }
      // save new scale to detect future changes
      chan->old_scale = *(chan->pos_scale);
      // we actually want the reciprocal
      chan->scale = 1.0 / *(chan->pos_scale);
    }

    // get bit states
    *(chan->latch_ext_valid) = EC_READ_BIT(&pd[chan->latch_ext_valid_pdo_os], chan->latch_ext_valid_pdo_bp);
    *(chan->underflow) = EC_READ_BIT(&pd[chan->undeflow_pdo_os], chan->undeflow_pdo_bp);
    *(chan->overflow) = EC_READ_BIT(&pd[chan->overflow_pdo_os], chan->overflow_pdo_bp);
    *(chan->ina) = EC_READ_BIT(&pd[chan->ina_pdo_os], chan->ina_pdo_bp);
    *(chan->inb) = EC_READ_BIT(&pd[chan->inb_pdo_os], chan->inb_pdo_bp);
    *(chan->gate_state) = EC_READ_BIT(&pd[chan->gate_state_pdo_os], chan->gate_state_pdo_bp);

    // read raw values
    raw_count = EC_READ_S32(&pd[chan->count_pdo_os]);
    raw_latch = EC_READ_S32(&pd[chan->latch_pdo_os]);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      chan->last_count = raw_count;
    }

    // check for counter set done
    if (EC_READ_BIT(&pd[chan->set_count_done_pdo_os], chan->set_count_done_pdo_bp)) {
      chan->last_count = raw_count;
      *(chan->set_raw_count) = 0;
    }

    // update raw values
    if (! *(chan->set_raw_count)) {
      *(chan->raw_count) = raw_count;
    }

    // check for index edge
    idx_flag = 0;
    idx_count = 0;
    if (*(chan->index) && !chan->last_index) {
      idx_count = raw_count;
      idx_flag = 1;
    }
    chan->last_index = *(chan->index);

    // handle initialization
    if (chan->do_init || *(chan->reset)) {
      chan->do_init = 0;
      chan->last_count = raw_count;
      *(chan->count) = 0;
      idx_flag = 0;
    }

    // handle index
    if (*(chan->latch_ext_valid)) {
      *(chan->raw_latch) = raw_latch;
      chan->last_count = raw_latch;
      *(chan->count) = 0;
      *(chan->ena_latch_ext_pos) = 0;
      *(chan->ena_latch_ext_neg) = 0;
    }
    if (idx_flag && *(chan->index_ena)) {
      chan->last_count = idx_count;
      *(chan->count) = 0;
      *(chan->index_ena) = 0;
    }

    // compute net counts
    raw_delta = raw_count - chan->last_count;
    chan->last_count = raw_count;
    *(chan->count) += raw_delta;

    // scale count to make floating point position
    *(chan->pos) = *(chan->count) * chan->scale;
  }

  hal_data->last_operational = 1;
}

void lcec_el5122_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5122_data_t *hal_data = (lcec_el5122_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el5122_chan_t *chan;

  // set outputs
  for (i=0; i<LCEC_EL5122_CHANS; i++) {
    chan = &hal_data->chans[i];

    // set output data
    EC_WRITE_BIT(&pd[chan->set_count_pdo_os], chan->set_count_pdo_bp, *(chan->set_raw_count));
    EC_WRITE_S32(&pd[chan->set_count_val_pdo_os], *(chan->set_raw_count_val));
    EC_WRITE_BIT(&pd[chan->ena_latch_ext_pos_pdo_os], chan->ena_latch_ext_pos_pdo_bp, *(chan->ena_latch_ext_pos));
    EC_WRITE_BIT(&pd[chan->ena_latch_ext_neg_pdo_os], chan->ena_latch_ext_neg_pdo_bp, *(chan->ena_latch_ext_neg));
  }
}

