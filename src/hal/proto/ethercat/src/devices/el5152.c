/**
 * @file el5152.c
 * @brief Driver implementation for the Beckhoff EL5152 2-channel incremental encoder terminal (extended).
 *
 * Provides HAL pins for two independent incremental encoder channels with an
 * extended feature set. Each channel offers a 32-bit position counter, counter
 * preset, direct readback of A and B inputs, extrapolation-stall detection,
 * TxPDO toggle, 32-bit period measurement, and a software Z-index latch via
 * the index/index-enable HAL pins. Each channel's period PDO is mapped
 * independently.
 *
 * @copyright Copyright (C) 2011-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include "../lcec.h"
#include "el5152.h"

/**
 * @brief Per-channel HAL pins and PDO mapping data for one EL5152 encoder channel.
 */
typedef struct {
  hal_bit_t *index;               /**< HAL IN: Z-index signal (active high); rising edge triggers latch if index_ena is set. */
  hal_bit_t *index_ena;           /**< HAL IO: enable index latch; cleared automatically when index event is processed. */
  hal_bit_t *reset;               /**< HAL IN: reset the relative position counter to zero. */
  hal_bit_t *ina;                 /**< HAL OUT: current state of encoder input A (PDO 0x6000/0x09). */
  hal_bit_t *inb;                 /**< HAL OUT: current state of encoder input B (PDO 0x6000/0x0a). */
  hal_bit_t *expol_stall;         /**< HAL OUT: extrapolation stall flag — encoder stopped during extrapolation (PDO 0x6000/0x08). */
  hal_bit_t *tx_toggle;           /**< HAL OUT: TxPDO toggle bit; alternates each new PDO (PDO 0x1800/0x09 or 0x1804/0x09). */
  hal_bit_t *set_raw_count;       /**< HAL IO: pulse high to preset the hardware counter; cleared when acknowledged. */
  hal_s32_t *set_raw_count_val;   /**< HAL IN: value to load into the counter when set_raw_count is asserted. */
  hal_s32_t *raw_count;           /**< HAL OUT: raw 32-bit counter value from the terminal (PDO 0x6000/0x11). */
  hal_u32_t *raw_period;          /**< HAL OUT: raw 32-bit period measurement from the terminal (PDO 0x6000/0x14). */
  hal_s32_t *count;               /**< HAL OUT: relative position count (zeroed on reset or index event). */
  hal_float_t *pos_scale;         /**< HAL IO: counts per user unit; reciprocal applied internally. */
  hal_float_t *pos;               /**< HAL OUT: scaled position in user units. */
  hal_float_t *period;            /**< HAL OUT: encoder period in seconds (raw_period * LCEC_EL5152_PERIOD_SCALE). */

  unsigned int set_count_pdo_os;          /**< Byte offset of the set-counter command bit in output process data. */
  unsigned int set_count_pdo_bp;          /**< Bit position of the set-counter command within the byte. */
  unsigned int set_count_val_pdo_os;      /**< Byte offset of the 32-bit counter preset value in output process data. */
  unsigned int set_count_done_pdo_os;     /**< Byte offset of set-counter-done acknowledgement bit in input data. */
  unsigned int set_count_done_pdo_bp;     /**< Bit position of set-counter-done within the byte. */
  unsigned int expol_stall_pdo_os;        /**< Byte offset of extrapolation-stall status bit in input process data. */
  unsigned int expol_stall_pdo_bp;        /**< Bit position of extrapolation-stall within the byte. */
  unsigned int ina_pdo_os;               /**< Byte offset of input A status bit in input process data. */
  unsigned int ina_pdo_bp;               /**< Bit position of input A within the byte. */
  unsigned int inb_pdo_os;               /**< Byte offset of input B status bit in input process data. */
  unsigned int inb_pdo_bp;               /**< Bit position of input B within the byte. */
  unsigned int tx_toggle_pdo_os;         /**< Byte offset of TxPDO toggle bit in input process data. */
  unsigned int tx_toggle_pdo_bp;         /**< Bit position of TxPDO toggle within the byte. */
  unsigned int count_pdo_os;            /**< Byte offset of the 32-bit counter value in input process data. */
  unsigned int period_pdo_os;           /**< Byte offset of the 32-bit period value in input process data. */

  int do_init;          /**< Non-zero until the first valid read; triggers counter zeroing. */
  int32_t last_count;   /**< Previous raw counter value used to compute relative delta. */
  int last_index;       /**< Previous state of the index pin; used to detect rising edges. */
  double old_scale;     /**< Last observed pos_scale value; used to detect pin changes. */
  double scale;         /**< Cached reciprocal of pos_scale (1.0 / pos_scale). */
} lcec_el5152_chan_t;

/**
 * @brief Top-level HAL data structure for the EL5152 slave.
 */
typedef struct {
  lcec_el5152_chan_t chans[LCEC_EL5152_CHANS]; /**< Per-channel state for each extended incremental encoder. */
  int last_operational; /**< Tracks whether the slave was operational on the previous cycle. */
} lcec_el5152_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_el5152_chan_t, index), "%s.%s.%s.enc-%d-index" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5152_chan_t, index_ena), "%s.%s.%s.enc-%d-index-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5152_chan_t, reset), "%s.%s.%s.enc-%d-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5152_chan_t, ina), "%s.%s.%s.enc-%d-ina" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5152_chan_t, inb), "%s.%s.%s.enc-%d-inb" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5152_chan_t, expol_stall), "%s.%s.%s.enc-%d-expol-stall" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5152_chan_t, tx_toggle), "%s.%s.%s.enc-%d-tx-toggle" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5152_chan_t, set_raw_count), "%s.%s.%s.enc-%d-set-raw-count" },
  { HAL_S32, HAL_IN, offsetof(lcec_el5152_chan_t, set_raw_count_val), "%s.%s.%s.enc-%d-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5152_chan_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5152_chan_t, count), "%s.%s.%s.enc-%d-count" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el5152_chan_t, raw_period), "%s.%s.%s.enc-%d-raw-period" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5152_chan_t, pos), "%s.%s.%s.enc-%d-pos" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5152_chan_t, period), "%s.%s.%s.enc-%d-period" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el5152_chan_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5152_channel1_in[] = {
   {0x0000, 0x00, 2}, // Gap
   {0x6000, 0x03, 1}, // Set counter done
   {0x0000, 0x00, 4}, // Gap
   {0x6000, 0x08, 1}, // Extrapolation stall
   {0x6000, 0x09, 1}, // State input A
   {0x6000, 0x0a, 1}, // State input B
   {0x0000, 0x00, 3}, // Gap
   {0x1c32, 0x20, 1}, // Sync error
   {0x0000, 0x00, 1}, // Gap
   {0x1800, 0x09, 1}, // TxPDO toggle
   {0x6000, 0x11, 32} // Counter value
};

static ec_pdo_entry_info_t lcec_el5152_channel2_in[] = {
   {0x0000, 0x00, 2}, // Gap
   {0x6010, 0x03, 1}, // Set counter done
   {0x0000, 0x00, 4}, // Gap
   {0x6010, 0x08, 1}, // Extrapolation stall
   {0x6010, 0x09, 1}, // State input A
   {0x6010, 0x0a, 1}, // State input B
   {0x0000, 0x00, 3}, // Gap
   {0x1c32, 0x20, 1}, // Sync error
   {0x0000, 0x00, 1}, // Gap
   {0x1804, 0x09, 1}, // TxPDO toggle
   {0x6010, 0x11, 32} // Counter value
};

static ec_pdo_entry_info_t lcec_el5152_channel1_period[] = {
   {0x6000, 0x14, 32} // Period value
};

static ec_pdo_entry_info_t lcec_el5152_channel2_period[] = {
   {0x6010, 0x14, 32} // Period value
};

static ec_pdo_entry_info_t lcec_el5152_channel1_out[] = {
   {0x0000, 0x00, 1}, // Gap
   {0x0000, 0x00, 1}, // Gap
   {0x7000, 0x03, 1}, // Set counter
   {0x0000, 0x00, 1}, // Gap
   {0x0000, 0x00, 4}, // Gap
   {0x0000, 0x00, 8}, // Gap
   {0x7000, 0x11, 32} // Set counter value
};

static ec_pdo_entry_info_t lcec_el5152_channel2_out[] = {
   {0x0000, 0x00, 1}, // Gap
   {0x0000, 0x00, 1}, // Gap
   {0x7010, 0x03, 1}, // Set counter
   {0x0000, 0x00, 1}, // Gap
   {0x0000, 0x00, 4}, // Gap
   {0x0000, 0x00, 8}, // Gap
   {0x7010, 0x11, 32} // Set counter value
};

static ec_pdo_info_t lcec_el5152_pdos_out[] = {
    {0x1600,  7, lcec_el5152_channel1_out},
    {0x1602,  7, lcec_el5152_channel2_out}
};

static ec_pdo_info_t lcec_el5152_pdos_in[] = {
    {0x1A00, 11, lcec_el5152_channel1_in},
    {0x1A02,  1, lcec_el5152_channel1_period},
    {0x1A04, 11, lcec_el5152_channel2_in},
    {0x1A06,  1, lcec_el5152_channel2_period}
};

static ec_sync_info_t lcec_el5152_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_el5152_pdos_out},
    {3, EC_DIR_INPUT,  4, lcec_el5152_pdos_in},
    {0xff}
};

void lcec_el5152_read(struct lcec_slave *slave, long period);
void lcec_el5152_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EL5152 EtherCAT slave driver.
 *
 * Allocates HAL memory, configures sync managers, registers all PDO entries
 * for both encoder channels including the independent per-channel period PDOs
 * (0x1A02 and 0x1A06), and exports HAL pins including period measurement and
 * index latch support per channel.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5152_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el5152_data_t *hal_data;
  int i;
  lcec_el5152_chan_t *chan;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el5152_read;
  slave->proc_write = lcec_el5152_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el5152_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5152_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el5152_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize pins
  for (i=0; i<LCEC_EL5152_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x03, &chan->set_count_done_pdo_os, &chan->set_count_done_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x08, &chan->expol_stall_pdo_os, &chan->expol_stall_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x09, &chan->ina_pdo_os, &chan->ina_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0a, &chan->inb_pdo_os, &chan->inb_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1800 + (i << 2), 0x09, &chan->tx_toggle_pdo_os, &chan->tx_toggle_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->count_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x14, &chan->period_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x03, &chan->set_count_pdo_os, &chan->set_count_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x11, &chan->set_count_val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
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

/**
 * @brief EtherCAT cyclic read callback for the EL5152.
 *
 * Reads all input PDO data for each channel: A/B input states,
 * extrapolation-stall, TxPDO toggle, 32-bit counter value, and 32-bit period.
 * Handles counter-preset acknowledgement and software Z-index latch via rising
 * edge detection on the index pin when index_ena is set. Updates relative
 * count, scaled position, and period in seconds every cycle.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5152_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5152_data_t *hal_data = (lcec_el5152_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i, idx_flag;
  lcec_el5152_chan_t *chan;
  int32_t idx_count, raw_count, raw_delta;
  uint32_t raw_period;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL5152_CHANS; i++) {
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
    *(chan->ina) = EC_READ_BIT(&pd[chan->ina_pdo_os], chan->ina_pdo_bp);
    *(chan->inb) = EC_READ_BIT(&pd[chan->inb_pdo_os], chan->inb_pdo_bp);
    *(chan->expol_stall) = EC_READ_BIT(&pd[chan->expol_stall_pdo_os], chan->expol_stall_pdo_bp);
    *(chan->tx_toggle) = EC_READ_BIT(&pd[chan->tx_toggle_pdo_os], chan->tx_toggle_pdo_bp);

    // read raw values
    raw_count = EC_READ_S32(&pd[chan->count_pdo_os]);
    raw_period = EC_READ_U32(&pd[chan->period_pdo_os]);

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
      *(chan->raw_period) = raw_period;
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

    // scale period
    *(chan->period) = ((double) (*(chan->raw_period))) * LCEC_EL5152_PERIOD_SCALE;
  }

  hal_data->last_operational = 1;
}

/**
 * @brief EtherCAT cyclic write callback for the EL5152.
 *
 * Writes output PDO data for each channel: the set-counter command bit and
 * the 32-bit counter preset value.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5152_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5152_data_t *hal_data = (lcec_el5152_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el5152_chan_t *chan;

  // set outputs
  for (i=0; i<LCEC_EL5152_CHANS; i++) {
    chan = &hal_data->chans[i];

    // set output data
    EC_WRITE_BIT(&pd[chan->set_count_pdo_os], chan->set_count_pdo_bp, *(chan->set_raw_count));
    EC_WRITE_S32(&pd[chan->set_count_val_pdo_os], *(chan->set_raw_count_val));
  }
}
