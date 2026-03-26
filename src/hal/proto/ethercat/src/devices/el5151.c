/**
 * @file el5151.c
 * @brief Driver implementation for the Beckhoff EL5151 1-channel incremental encoder terminal (extended).
 *
 * Provides HAL pins for a single incremental encoder channel with an extended
 * feature set. Compared to the EL5101, the EL5151 offers a 32-bit position
 * counter, direct readback of all inputs (A, B, C, EXT), extrapolation-stall
 * detection, EtherCAT sync-error reporting, a TxPDO toggle bit, 32-bit period
 * measurement, and separate C-index and external (positive/negative edge)
 * latch inputs.
 *
 * @copyright Copyright (C) 2012-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el5151.h"

/**
 * @brief HAL pins and PDO mapping data for the EL5151 incremental encoder.
 */
typedef struct {
  hal_bit_t *ena_latch_c;         /**< HAL IO: enable C-index latch; auto-cleared when latch is captured. */
  hal_bit_t *ena_latch_ext_pos;   /**< HAL IO: enable external latch on positive edge; auto-cleared on capture. */
  hal_bit_t *ena_latch_ext_neg;   /**< HAL IO: enable external latch on negative edge; auto-cleared on capture. */
  hal_bit_t *reset;               /**< HAL IN: reset the relative position counter to zero. */
  hal_bit_t *ina;                 /**< HAL OUT: current state of encoder input A (PDO 0x6000/0x09). */
  hal_bit_t *inb;                 /**< HAL OUT: current state of encoder input B (PDO 0x6000/0x0a). */
  hal_bit_t *inc;                 /**< HAL OUT: current state of encoder input C / index (PDO 0x6000/0x0b). */
  hal_bit_t *inext;               /**< HAL OUT: current state of external latch input (PDO 0x6000/0x0d). */
  hal_bit_t *expol_stall;         /**< HAL OUT: extrapolation stall flag — encoder stopped during extrapolation (PDO 0x6000/0x08). */
  hal_bit_t *sync_err;            /**< HAL OUT: EtherCAT sync error flag (PDO 0x1c32/0x20). */
  hal_bit_t *latch_c_valid;       /**< HAL OUT: C-index latch value is valid (PDO 0x6000/0x01). */
  hal_bit_t *latch_ext_valid;     /**< HAL OUT: external latch value is valid (PDO 0x6000/0x02). */
  hal_bit_t *tx_toggle;           /**< HAL OUT: TxPDO toggle bit; alternates each new PDO (PDO 0x1800/0x09). */
  hal_bit_t *set_raw_count;       /**< HAL IO: pulse high to preset the hardware counter; cleared when acknowledged. */
  hal_s32_t *set_raw_count_val;   /**< HAL IN: value to load into the counter when set_raw_count is asserted. */
  hal_s32_t *raw_count;           /**< HAL OUT: raw 32-bit counter value from the terminal (PDO 0x6000/0x11). */
  hal_s32_t *raw_latch;           /**< HAL OUT: counter value captured at the last latch event (PDO 0x6000/0x12). */
  hal_u32_t *raw_period;          /**< HAL OUT: raw 32-bit period measurement from the terminal (PDO 0x6000/0x14). */
  hal_s32_t *count;               /**< HAL OUT: relative position count (zeroed on reset or latch event). */
  hal_float_t *pos_scale;         /**< HAL IO: counts per user unit; reciprocal applied internally. */
  hal_float_t *pos;               /**< HAL OUT: scaled position in user units. */
  hal_float_t *period;            /**< HAL OUT: encoder period in seconds (raw_period * LCEC_EL5151_PERIOD_SCALE). */

  unsigned int ena_latch_c_pdo_os;          /**< Byte offset of ena_latch_c output bit in process data. */
  unsigned int ena_latch_c_pdo_bp;          /**< Bit position of ena_latch_c within the byte. */
  unsigned int ena_latch_ext_pos_pdo_os;    /**< Byte offset of ena_latch_ext_pos output bit in process data. */
  unsigned int ena_latch_ext_pos_pdo_bp;    /**< Bit position of ena_latch_ext_pos within the byte. */
  unsigned int ena_latch_ext_neg_pdo_os;    /**< Byte offset of ena_latch_ext_neg output bit in process data. */
  unsigned int ena_latch_ext_neg_pdo_bp;    /**< Bit position of ena_latch_ext_neg within the byte. */
  unsigned int set_count_pdo_os;            /**< Byte offset of the set-counter command bit in output process data. */
  unsigned int set_count_pdo_bp;            /**< Bit position of the set-counter command within the byte. */
  unsigned int set_count_val_pdo_os;        /**< Byte offset of the 32-bit counter preset value in output process data. */
  unsigned int set_count_done_pdo_os;       /**< Byte offset of set-counter-done acknowledgement bit in input data. */
  unsigned int set_count_done_pdo_bp;       /**< Bit position of set-counter-done within the byte. */
  unsigned int latch_c_valid_pdo_os;        /**< Byte offset of latch-C-valid status bit in input process data. */
  unsigned int latch_c_valid_pdo_bp;        /**< Bit position of latch-C-valid within the byte. */
  unsigned int latch_ext_valid_pdo_os;      /**< Byte offset of latch-extern-valid status bit in input process data. */
  unsigned int latch_ext_valid_pdo_bp;      /**< Bit position of latch-extern-valid within the byte. */
  unsigned int expol_stall_pdo_os;          /**< Byte offset of extrapolation-stall status bit in input process data. */
  unsigned int expol_stall_pdo_bp;          /**< Bit position of extrapolation-stall within the byte. */
  unsigned int ina_pdo_os;                  /**< Byte offset of input A status bit in input process data. */
  unsigned int ina_pdo_bp;                  /**< Bit position of input A within the byte. */
  unsigned int inb_pdo_os;                  /**< Byte offset of input B status bit in input process data. */
  unsigned int inb_pdo_bp;                  /**< Bit position of input B within the byte. */
  unsigned int inc_pdo_os;                  /**< Byte offset of input C status bit in input process data. */
  unsigned int inc_pdo_bp;                  /**< Bit position of input C within the byte. */
  unsigned int inext_pdo_os;                /**< Byte offset of external input status bit in input process data. */
  unsigned int inext_pdo_bp;                /**< Bit position of external input within the byte. */
  unsigned int sync_err_pdo_os;             /**< Byte offset of sync-error bit in input process data. */
  unsigned int sync_err_pdo_bp;             /**< Bit position of sync-error within the byte. */
  unsigned int tx_toggle_pdo_os;            /**< Byte offset of TxPDO toggle bit in input process data. */
  unsigned int tx_toggle_pdo_bp;            /**< Bit position of TxPDO toggle within the byte. */
  unsigned int count_pdo_os;               /**< Byte offset of the 32-bit counter value in input process data. */
  unsigned int latch_pdo_os;               /**< Byte offset of the 32-bit latch value in input process data. */
  unsigned int period_pdo_os;              /**< Byte offset of the 32-bit period value in input process data. */

  int do_init;          /**< Non-zero until the first valid read; triggers counter zeroing. */
  int32_t last_count;   /**< Previous raw counter value used to compute relative delta. */
  double old_scale;     /**< Last observed pos_scale value; used to detect pin changes. */
  double scale;         /**< Cached reciprocal of pos_scale (1.0 / pos_scale). */

  int last_operational; /**< Tracks whether the slave was operational on the previous cycle. */
} lcec_el5151_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IO, offsetof(lcec_el5151_data_t, ena_latch_c), "%s.%s.%s.enc-index-c-enable" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5151_data_t, ena_latch_ext_pos), "%s.%s.%s.enc-index-ext-pos-enable" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5151_data_t, ena_latch_ext_neg), "%s.%s.%s.enc-index-ext-neg-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5151_data_t, reset), "%s.%s.%s.enc-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, ina), "%s.%s.%s.enc-ina" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, inb), "%s.%s.%s.enc-inb" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, inc), "%s.%s.%s.enc-inc" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, inext), "%s.%s.%s.enc-inext" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, expol_stall), "%s.%s.%s.enc-expol-stall" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, sync_err), "%s.%s.%s.enc-sync-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, latch_c_valid), "%s.%s.%s.enc-latch-c-valid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, latch_ext_valid), "%s.%s.%s.enc-latch-ext-valid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5151_data_t, tx_toggle), "%s.%s.%s.enc-tx-toggle" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5151_data_t, set_raw_count), "%s.%s.%s.enc-set-raw-count" },
  { HAL_S32, HAL_IN, offsetof(lcec_el5151_data_t, set_raw_count_val), "%s.%s.%s.enc-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5151_data_t, raw_count), "%s.%s.%s.enc-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5151_data_t, count), "%s.%s.%s.enc-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5151_data_t, raw_latch), "%s.%s.%s.enc-raw-latch" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el5151_data_t, raw_period), "%s.%s.%s.enc-raw-period" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5151_data_t, pos), "%s.%s.%s.enc-pos" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5151_data_t, period), "%s.%s.%s.enc-period" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el5151_data_t, pos_scale), "%s.%s.%s.enc-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5151_in[] = {
   {0x6000, 0x01,  1}, // Latch C valid
   {0x6000, 0x02,  1}, // Latch extern valid
   {0x6000, 0x03,  1}, // Set counter done
   {0x0000, 0x00,  4}, // Gap
   {0x6000, 0x08,  1}, // Extrapolation stall
   {0x6000, 0x09,  1}, // Status of input A
   {0x6000, 0x0a,  1}, // Status of input B
   {0x6000, 0x0b,  1}, // Status of input C
   {0x0000, 0x00,  1}, // Gap
   {0x6000, 0x0d,  1}, // Status of extern latch
   {0x1c32, 0x20,  1}, // Sync error
   {0x0000, 0x00,  1}, // Gap
   {0x1800, 0x09,  1}, // TxPDO Toggle
   {0x6000, 0x11, 32}, // Counter value
   {0x6000, 0x12, 32}  // Latch value
};

static ec_pdo_entry_info_t lcec_el5151_period[] = {
   {0x6000, 0x14, 32} // Period value
};

static ec_pdo_entry_info_t lcec_el5151_out[] = {
   {0x7000, 0x01,  1}, // Enable latch C
   {0x7000, 0x02,  1}, // Enable latch extern on positive edge
   {0x7000, 0x03,  1}, // Set counter
   {0x7000, 0x04,  1}, // Enable latch extern on negative edge
   {0x0000, 0x00,  4}, // Gap
   {0x0000, 0x00,  8}, // Gap
   {0x7000, 0x11, 32}  // Set counter value
};

static ec_pdo_info_t lcec_el5151_pdos_out[] = {
    {0x1600,  7, lcec_el5151_out}
};

static ec_pdo_info_t lcec_el5151_pdos_in[] = {
    {0x1A00, 15, lcec_el5151_in},
    {0x1A02,  1, lcec_el5151_period}
};

static ec_sync_info_t lcec_el5151_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_el5151_pdos_out},
    {3, EC_DIR_INPUT,  2, lcec_el5151_pdos_in},
    {0xff}
};

void lcec_el5151_read(struct lcec_slave *slave, long period);
void lcec_el5151_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EL5151 EtherCAT slave driver.
 *
 * Allocates HAL memory, configures sync managers, registers all PDO entries
 * (input status bits, counter, latch, period, and output control/preset), and
 * exports HAL pins for the extended single-channel incremental encoder.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5151_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el5151_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el5151_read;
  slave->proc_write = lcec_el5151_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el5151_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5151_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el5151_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->latch_c_valid_pdo_os, &hal_data->latch_c_valid_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x02, &hal_data->latch_ext_valid_pdo_os, &hal_data->latch_ext_valid_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x03, &hal_data->set_count_done_pdo_os, &hal_data->set_count_done_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x08, &hal_data->expol_stall_pdo_os, &hal_data->expol_stall_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x09, &hal_data->ina_pdo_os, &hal_data->ina_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x0a, &hal_data->inb_pdo_os, &hal_data->inb_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x0b, &hal_data->inc_pdo_os, &hal_data->inc_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x0d, &hal_data->inext_pdo_os, &hal_data->inext_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1c32, 0x20, &hal_data->sync_err_pdo_os, &hal_data->sync_err_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1800, 0x09, &hal_data->tx_toggle_pdo_os, &hal_data->tx_toggle_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x11, &hal_data->count_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x12, &hal_data->latch_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x14, &hal_data->period_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->ena_latch_c_pdo_os, &hal_data->ena_latch_c_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x02, &hal_data->ena_latch_ext_pos_pdo_os, &hal_data->ena_latch_ext_pos_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x03, &hal_data->set_count_pdo_os, &hal_data->set_count_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x04, &hal_data->ena_latch_ext_neg_pdo_os, &hal_data->ena_latch_ext_neg_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x11, &hal_data->set_count_val_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // initialize pins
  *(hal_data->pos_scale) = 1.0;

  // initialize variables
  hal_data->do_init = 1;
  hal_data->last_count = 0;
  hal_data->old_scale = *(hal_data->pos_scale) + 1.0;
  hal_data->scale = 1.0;

  return 0;
}

/**
 * @brief EtherCAT cyclic read callback for the EL5151.
 *
 * Reads all input PDO data: A/B/C/EXT input states, extrapolation-stall,
 * sync-error, C-latch and ext-latch valid flags, TxPDO toggle, 32-bit counter
 * value, 32-bit latch value, and 32-bit period. Handles counter-preset
 * acknowledgement and index latch events (C and external edge). Updates
 * relative count, scaled position, and period in seconds every cycle.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5151_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5151_data_t *hal_data = (lcec_el5151_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int32_t raw_count, raw_latch, raw_delta;
  uint32_t raw_period;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check for change in scale value
  if (*(hal_data->pos_scale) != hal_data->old_scale) {
    // scale value has changed, test and update it
    if ((*(hal_data->pos_scale) < 1e-20) && (*(hal_data->pos_scale) > -1e-20)) {
      // value too small, divide by zero is a bad thing
      *(hal_data->pos_scale) = 1.0;
    }
    // save new scale to detect future changes
    hal_data->old_scale = *(hal_data->pos_scale);
    // we actually want the reciprocal
    hal_data->scale = 1.0 / *(hal_data->pos_scale);
  }

  // get bit states
  *(hal_data->ina) = EC_READ_BIT(&pd[hal_data->ina_pdo_os], hal_data->ina_pdo_bp);
  *(hal_data->inb) = EC_READ_BIT(&pd[hal_data->inb_pdo_os], hal_data->inb_pdo_bp);
  *(hal_data->inc) = EC_READ_BIT(&pd[hal_data->inc_pdo_os], hal_data->inc_pdo_bp);
  *(hal_data->inext) = EC_READ_BIT(&pd[hal_data->inext_pdo_os], hal_data->inext_pdo_bp);
  *(hal_data->expol_stall) = EC_READ_BIT(&pd[hal_data->expol_stall_pdo_os], hal_data->expol_stall_pdo_bp);
  *(hal_data->sync_err) = EC_READ_BIT(&pd[hal_data->sync_err_pdo_os], hal_data->sync_err_pdo_bp);
  *(hal_data->latch_c_valid) = EC_READ_BIT(&pd[hal_data->latch_c_valid_pdo_os], hal_data->latch_c_valid_pdo_bp);
  *(hal_data->latch_ext_valid) = EC_READ_BIT(&pd[hal_data->latch_ext_valid_pdo_os], hal_data->latch_ext_valid_pdo_bp);
  *(hal_data->tx_toggle) = EC_READ_BIT(&pd[hal_data->tx_toggle_pdo_os], hal_data->tx_toggle_pdo_bp);

  // read raw values
  raw_count = EC_READ_S32(&pd[hal_data->count_pdo_os]);
  raw_latch = EC_READ_S32(&pd[hal_data->latch_pdo_os]);
  raw_period = EC_READ_U32(&pd[hal_data->period_pdo_os]);

  // check for operational change of slave
  if (!hal_data->last_operational) {
    hal_data->last_count = raw_count;
  }

  // check for counter set done
  if (EC_READ_BIT(&pd[hal_data->set_count_done_pdo_os], hal_data->set_count_done_pdo_bp)) {
    hal_data->last_count = raw_count;
    *(hal_data->set_raw_count) = 0;
  }

  // update raw values
  if (! *(hal_data->set_raw_count)) {
    *(hal_data->raw_count) = raw_count;
    *(hal_data->raw_period) = raw_period;
  }

  // handle initialization
  if (hal_data->do_init || *(hal_data->reset)) {
    hal_data->do_init = 0;
    hal_data->last_count = raw_count;
    *(hal_data->count) = 0;
  }

  // handle index
  if (*(hal_data->latch_ext_valid)) {
    *(hal_data->raw_latch) = raw_latch;
    hal_data->last_count = raw_latch;
    *(hal_data->count) = 0;
    *(hal_data->ena_latch_ext_pos) = 0;
    *(hal_data->ena_latch_ext_neg) = 0;
  }
  if (*(hal_data->latch_c_valid)) {
    *(hal_data->raw_latch) = raw_latch;
    hal_data->last_count = raw_latch;
    *(hal_data->count) = 0;
    *(hal_data->ena_latch_c) = 0;
  }

  // compute net counts
  raw_delta = raw_count - hal_data->last_count;
  hal_data->last_count = raw_count;
  *(hal_data->count) += raw_delta;

  // scale count to make floating point position
  *(hal_data->pos) = *(hal_data->count) * hal_data->scale;

  // scale period
  *(hal_data->period) = ((double) (*(hal_data->raw_period))) * LCEC_EL5151_PERIOD_SCALE;

  hal_data->last_operational = 1;
}

/**
 * @brief EtherCAT cyclic write callback for the EL5151.
 *
 * Writes the output PDO data each cycle: set-counter command bit, enable
 * C-latch bit, enable external latch positive-edge bit, enable external
 * latch negative-edge bit, and the 32-bit counter preset value.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5151_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5151_data_t *hal_data = (lcec_el5151_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  // set output data
  EC_WRITE_BIT(&pd[hal_data->set_count_pdo_os], hal_data->set_count_pdo_bp, *(hal_data->set_raw_count));
  EC_WRITE_BIT(&pd[hal_data->ena_latch_c_pdo_os], hal_data->ena_latch_c_pdo_bp, *(hal_data->ena_latch_c));
  EC_WRITE_BIT(&pd[hal_data->ena_latch_ext_pos_pdo_os], hal_data->ena_latch_ext_pos_pdo_bp, *(hal_data->ena_latch_ext_pos));
  EC_WRITE_BIT(&pd[hal_data->ena_latch_ext_neg_pdo_os], hal_data->ena_latch_ext_neg_pdo_bp, *(hal_data->ena_latch_ext_neg));
  EC_WRITE_S32(&pd[hal_data->set_count_val_pdo_os], *(hal_data->set_raw_count_val));
}
