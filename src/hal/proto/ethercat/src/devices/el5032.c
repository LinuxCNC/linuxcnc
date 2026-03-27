/**
 * @file el5032.c
 * @brief Driver implementation for the Beckhoff EL5032 2-channel EnDat 2.2 encoder terminal.
 *
 * Provides HAL pins for two EnDat 2.2 absolute encoder channels. Each channel
 * delivers a 64-bit absolute position value (split into lo/hi 32-bit HAL
 * pins), status bits (warning, error, ready, diagnostic, TxPDO state), and a
 * 2-bit cycle counter for data-freshness monitoring. Both absolute and
 * relative (incremental) position modes are supported.
 *
 * @copyright Copyright (C) 2023-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el5032.h"

/**
 * @brief Per-channel HAL pins and PDO mapping data for one EL5032 EnDat channel.
 */
typedef struct {
  gomc_hal_bit_t *reset;         /**< HAL IN: reset the relative position counter to zero. */
  gomc_hal_bit_t *abs_mode;      /**< HAL IN: when high, pos outputs the raw absolute position; otherwise relative. */
  gomc_hal_bit_t *warn;          /**< HAL OUT: warning flag from the EnDat encoder (PDO 0x6000/0x01). */
  gomc_hal_bit_t *error;         /**< HAL OUT: error flag from the EnDat encoder (PDO 0x6000/0x02). */
  gomc_hal_bit_t *ready;         /**< HAL OUT: encoder ready flag — communication established (PDO 0x6000/0x03). */
  gomc_hal_bit_t *diag;          /**< HAL OUT: diagnostic flag from the EnDat encoder (PDO 0x6000/0x0d). */
  gomc_hal_bit_t *tx_state;      /**< HAL OUT: TxPDO state bit indicating PDO validity (PDO 0x6000/0x0e). */
  gomc_hal_u32_t *cyc_cnt;       /**< HAL OUT: 2-bit cycle counter; increments each new PDO frame (PDO 0x6000/0x0f). */
  gomc_hal_u32_t *raw_count_lo;  /**< HAL OUT: lower 32 bits of the 64-bit absolute position counter. */
  gomc_hal_u32_t *raw_count_hi;  /**< HAL OUT: upper 32 bits of the 64-bit absolute position counter. */
  gomc_hal_s32_t *count;         /**< HAL OUT: relative position count (zeroed on reset or initialisation). */
  gomc_hal_float_t *pos;         /**< HAL OUT: scaled position in user units. */
  gomc_hal_float_t *pos_scale;   /**< HAL IO: counts per user unit; reciprocal applied internally. */

  unsigned int warn_os;       /**< Byte offset of warn bit in process data image. */
  unsigned int warn_bp;       /**< Bit position of warn within the byte. */
  unsigned int error_os;      /**< Byte offset of error bit in process data image. */
  unsigned int error_bp;      /**< Bit position of error within the byte. */
  unsigned int ready_os;      /**< Byte offset of ready bit in process data image. */
  unsigned int ready_bp;      /**< Bit position of ready within the byte. */
  unsigned int diag_os;       /**< Byte offset of diag bit in process data image. */
  unsigned int diag_bp;       /**< Bit position of diag within the byte. */
  unsigned int tx_state_os;   /**< Byte offset of tx_state bit in process data image. */
  unsigned int tx_state_bp;   /**< Bit position of tx_state within the byte. */
  unsigned int cyc_cnt_os;    /**< Byte offset of the 2-bit cycle counter in process data image. */
  unsigned int cyc_cnt_bp;    /**< Bit position of the cycle counter within the byte. */
  unsigned int count_pdo_os;  /**< Byte offset of the 64-bit counter value in process data image. */

  int do_init;          /**< Non-zero until the first valid read; triggers counter zeroing. */
  int64_t last_count;   /**< Previous raw 64-bit counter value used to compute relative delta. */
  double old_scale;     /**< Last observed pos_scale value; used to detect pin changes. */
  double scale;         /**< Cached reciprocal of pos_scale (1.0 / pos_scale). */
} lcec_el5032_chan_t;

/**
 * @brief Top-level HAL data structure for the EL5032 slave.
 */
typedef struct {
  lcec_el5032_chan_t chans[LCEC_EL5032_CHANS]; /**< Per-channel state for each EnDat encoder. */
  int last_operational; /**< Tracks whether the slave was operational on the previous cycle. */
} lcec_el5032_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el5032_chan_t, reset), "%s.%s.%s.enc-%d-reset" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el5032_chan_t, abs_mode), "%s.%s.%s.enc-%d-abs-mode" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, warn), "%s.%s.%s.enc-%d-warn" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, error), "%s.%s.%s.enc-%d-error" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, ready), "%s.%s.%s.enc-%d-ready" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, diag), "%s.%s.%s.enc-%d-diag" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, tx_state), "%s.%s.%s.enc-%d-tx-state" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, cyc_cnt), "%s.%s.%s.enc-%d-cyc-cnt" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, raw_count_lo), "%s.%s.%s.enc-%d-raw-count-lo" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, raw_count_hi), "%s.%s.%s.enc-%d-raw-count-hi" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, count), "%s.%s.%s.enc-%d-count" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_el5032_chan_t, pos), "%s.%s.%s.enc-%d-pos" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_el5032_chan_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5032_channel1_in[] = {
   {0x6000, 0x01,  1}, // warning
   {0x6000, 0x02,  1}, // error
   {0x6000, 0x03,  1}, // ready
   {0x0000, 0x00,  5}, // Gap
   {0x0000, 0x00,  4}, // Gap
   {0x6000, 0x0d,  1}, // diag
   {0x6000, 0x0e,  1}, // TxPDO state
   {0x6000, 0x0f,  2}, // cycle counter
   {0x6000, 0x11, 64}, // counter
};

static ec_pdo_entry_info_t lcec_el5032_channel2_in[] = {
   {0x6010, 0x01,  1}, // warning
   {0x6010, 0x02,  1}, // error
   {0x6010, 0x03,  1}, // ready
   {0x0000, 0x00,  5}, // Gap
   {0x0000, 0x00,  4}, // Gap
   {0x6010, 0x0d,  1}, // diag
   {0x6010, 0x0e,  1}, // TxPDO state
   {0x6010, 0x0f,  2}, // cycle counter
   {0x6010, 0x11, 64}, // counter
};

static ec_pdo_info_t lcec_el5032_pdos_in[] = {
    {0x1A00, 9, lcec_el5032_channel1_in},
    {0x1A01, 9, lcec_el5032_channel2_in}
};

static ec_sync_info_t lcec_el5032_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 0, NULL},
    {3, EC_DIR_INPUT,  2, lcec_el5032_pdos_in},
    {0xff}
};

void lcec_el5032_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EL5032 EtherCAT slave driver.
 *
 * Allocates HAL memory, configures the sync manager, registers all PDO
 * entries for both EnDat encoder channels (warning, error, ready, diag,
 * tx_state, cycle counter, and 64-bit position), and exports HAL pins
 * including the split raw_count_lo / raw_count_hi outputs.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5032_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_el5032_data_t *hal_data;
  int i;
  lcec_el5032_chan_t *chan;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el5032_read;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el5032_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5032_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_el5032_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize pins
  for (i=0; i<LCEC_EL5032_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->warn_os, &chan->warn_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->error_os, &chan->error_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x03, &chan->ready_os, &chan->ready_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0d, &chan->diag_os, &chan->diag_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0e, &chan->tx_state_os, &chan->tx_state_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0f, &chan->cyc_cnt_os, &chan->cyc_cnt_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->count_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(chan->pos_scale) = 1.0;

    // initialize variables
    chan->do_init = 1;
    chan->last_count = 0;
    chan->old_scale = *(chan->pos_scale) + 1.0;
    chan->scale = 1.0;
  }

  return 0;
}

/**
 * @brief EtherCAT cyclic read callback for the EL5032.
 *
 * Reads all input PDO data for each channel: status bits (warn, error, ready,
 * diag, tx_state), the 2-bit cycle counter, and the 64-bit absolute position.
 * The 64-bit value is split into lo/hi HAL pins and used for both absolute
 * (when abs_mode is set) and relative position tracking. Counter is zeroed
 * on reset or on first valid cycle after a slave reconnect.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5032_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5032_data_t *hal_data = (lcec_el5032_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el5032_chan_t *chan;
  int64_t raw_count, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL5032_CHANS; i++) {
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
    *(chan->warn) = EC_READ_BIT(&pd[chan->warn_os], chan->warn_bp);
    *(chan->error) = EC_READ_BIT(&pd[chan->error_os], chan->error_bp);
    *(chan->ready) = EC_READ_BIT(&pd[chan->ready_os], chan->ready_bp);
    *(chan->diag) = EC_READ_BIT(&pd[chan->diag_os], chan->diag_bp);
    *(chan->tx_state) = EC_READ_BIT(&pd[chan->tx_state_os], chan->tx_state_bp);

    // get cycle counter
    *(chan->cyc_cnt) = (EC_READ_U8(&pd[chan->cyc_cnt_os]) >> chan->cyc_cnt_bp) & 0x03;

    // read raw values
    raw_count = EC_READ_S64(&pd[chan->count_pdo_os]);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      chan->last_count = raw_count;
    }

    // update raw values
    *(chan->raw_count_lo) = raw_count;
    *(chan->raw_count_hi) = raw_count >> 32;

    // handle initialization
    if (chan->do_init || *(chan->reset)) {
      chan->do_init = 0;
      chan->last_count = raw_count;
      *(chan->count) = 0;
    }

    // compute net counts
    raw_delta = raw_count - chan->last_count;
    chan->last_count = raw_count;
    *(chan->count) += raw_delta;

    // scale count to make floating point position
    if (*(chan->abs_mode)) {
      *(chan->pos) = raw_count * chan->scale;
    } else {
      *(chan->pos) = *(chan->count) * chan->scale;
    }
  }

  hal_data->last_operational = 1;
}
