/**
 * @file el5021.c
 * @brief Driver implementation for the Beckhoff EL5021 1-channel SinCos encoder terminal.
 *
 * Provides HAL pins for a single SinCos encoder channel. The terminal
 * converts analog sine/cosine signals into a 32-bit interpolated position
 * counter. This driver supports the input C reference-mark latch, counter
 * preset, frequency and amplitude error detection, sync error, and TxPDO
 * toggle for data-freshness monitoring.
 *
 * @copyright Copyright (C) 2024-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el5021.h"

/**
 * @brief HAL pins and PDO mapping data for the EL5021 SinCos encoder.
 */
typedef struct {
  hal_bit_t *ena_latch_c;         /**< HAL IO: enable reference-mark (C input) latch; cleared automatically when latch is captured. */
  hal_bit_t *reset;               /**< HAL IN: reset the relative position counter to zero. */
  hal_bit_t *inc;                 /**< HAL OUT: status of the incremental input C (PDO 0x6000/0x0b). */
  hal_bit_t *freq_err;            /**< HAL OUT: frequency error flag — signal frequency out of range (PDO 0x6001/0x04). */
  hal_bit_t *ampl_err;            /**< HAL OUT: amplitude error flag — signal amplitude out of range (PDO 0x6001/0x05). */
  hal_bit_t *sync_err;            /**< HAL OUT: EtherCAT sync error flag (PDO 0x6000/0x0e). */
  hal_bit_t *latch_c_valid;       /**< HAL OUT: input C latch value is valid and ready to read (PDO 0x6000/0x01). */
  hal_bit_t *tx_toggle;           /**< HAL OUT: TxPDO toggle bit; alternates each new PDO cycle (PDO 0x6000/0x10). */
  hal_bit_t *set_raw_count;       /**< HAL IO: pulse high to preset the hardware counter to set_raw_count_val; cleared when acknowledged. */
  hal_s32_t *set_raw_count_val;   /**< HAL IN: value to load into the counter when set_raw_count is asserted. */
  hal_s32_t *raw_count;           /**< HAL OUT: raw 32-bit position value from the terminal (PDO 0x6000/0x11). */
  hal_s32_t *raw_latch;           /**< HAL OUT: counter value captured at the last C latch event (PDO 0x6000/0x12). */
  hal_s32_t *count;               /**< HAL OUT: relative position count (zeroed on reset or latch event). */
  hal_float_t *pos_scale;         /**< HAL IO: counts per user unit; reciprocal applied internally. */
  hal_float_t *pos;               /**< HAL OUT: scaled position in user units (count * scale). */

  unsigned int ena_latch_c_pdo_os;      /**< Byte offset of ena_latch_c output bit in process data image. */
  unsigned int ena_latch_c_pdo_bp;      /**< Bit position of ena_latch_c within the byte. */
  unsigned int set_count_pdo_os;        /**< Byte offset of set-counter command bit in process data image. */
  unsigned int set_count_pdo_bp;        /**< Bit position of set-counter command within the byte. */
  unsigned int set_count_val_pdo_os;    /**< Byte offset of the 32-bit counter preset value in output process data. */
  unsigned int set_count_done_pdo_os;   /**< Byte offset of the set-counter-done acknowledgement bit in input data. */
  unsigned int set_count_done_pdo_bp;   /**< Bit position of set-counter-done within the byte. */
  unsigned int latch_c_valid_pdo_os;    /**< Byte offset of latch-C-valid status bit in input process data. */
  unsigned int latch_c_valid_pdo_bp;    /**< Bit position of latch-C-valid within the byte. */
  unsigned int freq_err_pdo_os;         /**< Byte offset of frequency error bit in input process data. */
  unsigned int freq_err_pdo_bp;         /**< Bit position of frequency error within the byte. */
  unsigned int ampl_err_pdo_os;         /**< Byte offset of amplitude error bit in input process data. */
  unsigned int ampl_err_pdo_bp;         /**< Bit position of amplitude error within the byte. */
  unsigned int inc_pdo_os;              /**< Byte offset of input C status bit in input process data. */
  unsigned int inc_pdo_bp;              /**< Bit position of input C status within the byte. */
  unsigned int sync_err_pdo_os;         /**< Byte offset of sync error bit in input process data. */
  unsigned int sync_err_pdo_bp;         /**< Bit position of sync error within the byte. */
  unsigned int tx_toggle_pdo_os;        /**< Byte offset of TxPDO toggle bit in input process data. */
  unsigned int tx_toggle_pdo_bp;        /**< Bit position of TxPDO toggle within the byte. */
  unsigned int count_pdo_os;            /**< Byte offset of the 32-bit counter value in input process data. */
  unsigned int latch_pdo_os;            /**< Byte offset of the 32-bit latch value in input process data. */

  int do_init;          /**< Non-zero until the first valid read; triggers counter zeroing. */
  int32_t last_count;   /**< Previous raw counter value used to compute relative delta. */
  double old_scale;     /**< Last observed pos_scale value; used to detect pin changes. */
  double scale;         /**< Cached reciprocal of pos_scale (1.0 / pos_scale). */

  int last_operational; /**< Tracks whether the slave was operational on the previous cycle. */
} lcec_el5021_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IO, offsetof(lcec_el5021_data_t, ena_latch_c), "%s.%s.%s.enc-index-c-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5021_data_t, reset), "%s.%s.%s.enc-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, inc), "%s.%s.%s.enc-inc" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, freq_err), "%s.%s.%s.enc-freq-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, ampl_err), "%s.%s.%s.enc-ampl-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, sync_err), "%s.%s.%s.enc-sync-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, latch_c_valid), "%s.%s.%s.enc-latch-c-valid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5021_data_t, tx_toggle), "%s.%s.%s.enc-tx-toggle" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el5021_data_t, set_raw_count), "%s.%s.%s.enc-set-raw-count" },
  { HAL_S32, HAL_IN, offsetof(lcec_el5021_data_t, set_raw_count_val), "%s.%s.%s.enc-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5021_data_t, raw_count), "%s.%s.%s.enc-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5021_data_t, count), "%s.%s.%s.enc-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5021_data_t, raw_latch), "%s.%s.%s.enc-raw-latch" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5021_data_t, pos), "%s.%s.%s.enc-pos" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el5021_data_t, pos_scale), "%s.%s.%s.enc-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5021_in[] = {
   {0x6000, 0x01,  1}, // Latch C valid
   {0x0000, 0x00,  1}, // Gap
   {0x6000, 0x03,  1}, // Set counter done
   {0x6001, 0x04,  1}, // Frequency error
   {0x6001, 0x05,  1}, // Amplitude error
   {0x0000, 0x00,  5}, // Gap
   {0x6000, 0x0b,  1}, // Status of input C
   {0x0000, 0x00,  2}, // Gap
   {0x6000, 0x0e,  1}, // Sync error
   {0x6000, 0x0f,  1}, // TxPDO State
   {0x6000, 0x10,  1}, // TxPDO Toggle
   {0x6000, 0x11, 32}, // Counter value
   {0x6000, 0x12, 32}  // Latch value
};

static ec_pdo_entry_info_t lcec_el5021_out[] = {
   {0x7000, 0x01,  1}, // Enable latch C
   {0x0000, 0x00,  1}, // Gap
   {0x7000, 0x03,  1}, // Set counter
   {0x0000, 0x00, 13}, // Gap
   {0x7000, 0x11, 32}  // Set counter value
};

static ec_pdo_info_t lcec_el5021_pdos_out[] = {
    {0x1600,  5, lcec_el5021_out}
};

static ec_pdo_info_t lcec_el5021_pdos_in[] = {
    {0x1A00, 13, lcec_el5021_in}
};

static ec_sync_info_t lcec_el5021_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_el5021_pdos_out},
    {3, EC_DIR_INPUT,  1, lcec_el5021_pdos_in},
    {0xff}
};

void lcec_el5021_read(struct lcec_slave *slave, long period);
void lcec_el5021_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EL5021 EtherCAT slave driver.
 *
 * Allocates HAL memory, configures sync managers, registers all PDO entries
 * for both input (counter, latch, status flags) and output (latch enable,
 * counter preset), and exports HAL pins for the SinCos encoder channel.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5021_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el5021_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el5021_read;
  slave->proc_write = lcec_el5021_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el5021_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5021_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el5021_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->latch_c_valid_pdo_os, &hal_data->latch_c_valid_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x03, &hal_data->set_count_done_pdo_os, &hal_data->set_count_done_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6001, 0x04, &hal_data->freq_err_pdo_os, &hal_data->freq_err_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6001, 0x05, &hal_data->ampl_err_pdo_os, &hal_data->ampl_err_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x0b, &hal_data->inc_pdo_os, &hal_data->inc_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x0e, &hal_data->sync_err_pdo_os, &hal_data->sync_err_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x10, &hal_data->tx_toggle_pdo_os, &hal_data->tx_toggle_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x11, &hal_data->count_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x12, &hal_data->latch_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->ena_latch_c_pdo_os, &hal_data->ena_latch_c_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x03, &hal_data->set_count_pdo_os, &hal_data->set_count_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x11, &hal_data->set_count_val_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
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
 * @brief EtherCAT cyclic read callback for the EL5021.
 *
 * Reads all input PDO data: status bits (inc, freq_err, ampl_err, sync_err,
 * latch_c_valid, tx_toggle) and the 32-bit counter and latch values. Handles
 * counter-preset acknowledgement, C-latch index zeroing, and scale changes.
 * Updates the relative count and scaled pos output every cycle.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5021_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5021_data_t *hal_data = (lcec_el5021_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int32_t raw_count, raw_latch, raw_delta;

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
  *(hal_data->inc) = EC_READ_BIT(&pd[hal_data->inc_pdo_os], hal_data->inc_pdo_bp);
  *(hal_data->freq_err) = EC_READ_BIT(&pd[hal_data->freq_err_pdo_os], hal_data->freq_err_pdo_bp);
  *(hal_data->ampl_err) = EC_READ_BIT(&pd[hal_data->ampl_err_pdo_os], hal_data->ampl_err_pdo_bp);
  *(hal_data->sync_err) = EC_READ_BIT(&pd[hal_data->sync_err_pdo_os], hal_data->sync_err_pdo_bp);
  *(hal_data->latch_c_valid) = EC_READ_BIT(&pd[hal_data->latch_c_valid_pdo_os], hal_data->latch_c_valid_pdo_bp);
  *(hal_data->tx_toggle) = EC_READ_BIT(&pd[hal_data->tx_toggle_pdo_os], hal_data->tx_toggle_pdo_bp);

  // read raw values
  raw_count = EC_READ_S32(&pd[hal_data->count_pdo_os]);
  raw_latch = EC_READ_S32(&pd[hal_data->latch_pdo_os]);

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
  }

  // handle initialization
  if (hal_data->do_init || *(hal_data->reset)) {
    hal_data->do_init = 0;
    hal_data->last_count = raw_count;
    *(hal_data->count) = 0;
  }

  // handle index
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

  hal_data->last_operational = 1;
}

/**
 * @brief EtherCAT cyclic write callback for the EL5021.
 *
 * Writes the output PDO data each cycle: the set-counter command bit, the
 * enable-latch-C bit, and the 32-bit counter preset value.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5021_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5021_data_t *hal_data = (lcec_el5021_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  // set output data
  EC_WRITE_BIT(&pd[hal_data->set_count_pdo_os], hal_data->set_count_pdo_bp, *(hal_data->set_raw_count));
  EC_WRITE_BIT(&pd[hal_data->ena_latch_c_pdo_os], hal_data->ena_latch_c_pdo_bp, *(hal_data->ena_latch_c));
  EC_WRITE_S32(&pd[hal_data->set_count_val_pdo_os], *(hal_data->set_raw_count_val));
}
