/**
 * @file el3255.c
 * @brief LinuxCNC EtherCAT HAL driver for the Beckhoff EL3255 5-channel potentiometer input terminal.
 *
 * The EL3255 reads the wiper position of up to five resistive potentiometers over EtherCAT.
 * Each channel provides a 16-bit signed value, status bits (overrange, underrange, sync-error,
 * error), and TxPDO state/toggle indicators.
 *
 * The floating-point HAL output pin is computed as:
 * @code
 *   val = bias + scale * (double)raw * (1.0 / 0x7fff)
 * @endcode
 * so with default scale=1.0 and bias=0.0 the output spans –1.0 to +1.0 across the
 * full measurement range.
 *
 * @copyright Copyright (C) 2016-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el3255.h"

/**
 * @brief Per-channel HAL pins and PDO offsets for one EL3255 potentiometer channel.
 */
typedef struct {
  hal_bit_t *overrange;      /**< HAL bit output: measurement exceeds the upper limit. */
  hal_bit_t *underrange;     /**< HAL bit output: measurement is below the lower limit. */
  hal_bit_t *error;          /**< HAL bit output: channel error flag (object 0x6000+n, subindex 0x07). */
  hal_bit_t *sync_err;       /**< HAL bit output: TxPDO synchronisation error flag. */
  hal_s32_t *raw_val;        /**< HAL s32 output: raw 16-bit signed potentiometer value. */
  hal_float_t *scale;        /**< HAL float I/O: multiplier applied to the normalised value (default 1.0). */
  hal_float_t *bias;         /**< HAL float I/O: offset added after scaling (default 0.0). */
  hal_float_t *val;          /**< HAL float output: final value = bias + scale * raw / 0x7fff. */
  unsigned int ovr_pdo_os;   /**< Byte offset of the overrange bit in the EtherCAT process data image. */
  unsigned int ovr_pdo_bp;   /**< Bit position of the overrange flag within its byte. */
  unsigned int udr_pdo_os;   /**< Byte offset of the underrange bit in the EtherCAT process data image. */
  unsigned int udr_pdo_bp;   /**< Bit position of the underrange flag within its byte. */
  unsigned int error_pdo_os; /**< Byte offset of the channel error bit in the EtherCAT process data image. */
  unsigned int error_pdo_bp; /**< Bit position of the channel error flag within its byte. */
  unsigned int sync_err_pdo_os; /**< Byte offset of the sync-error bit in the EtherCAT process data image. */
  unsigned int sync_err_pdo_bp; /**< Bit position of the sync-error flag within its byte. */
  unsigned int val_pdo_os;   /**< Byte offset of the 16-bit channel value in the EtherCAT process data image. */
} lcec_el3255_chan_t;

/**
 * @brief Top-level HAL data structure for one EL3255 slave, holding all channel state.
 */
typedef struct {
  lcec_el3255_chan_t chans[LCEC_EL3255_CHANS]; /**< Per-channel data for all five potentiometer inputs. */
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

/**
 * @brief Initialise an EL3255 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * Called once at HAL component initialisation. Registers five PDO entries per channel
 * (overrange, underrange, error, sync-error, and 16-bit value) and exports the HAL pins
 * for each of the five potentiometer channels.  The scale pin is initialised to 1.0.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, -EIO on HAL memory allocation failure, or a negative
 *         error code if HAL pin creation fails.
 */
int lcec_el3255_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
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

/**
 * @brief Cyclic read callback: transfer EtherCAT PDO data to HAL pins for all channels.
 *
 * Called every servo cycle. Reads per-channel status bits and 16-bit potentiometer
 * value from the EtherCAT process data image and updates the corresponding HAL pins.
 * Returns immediately if the slave is not in the operational state.
 *
 * @param slave   Pointer to the lcec_slave descriptor for this terminal.
 * @param period  Servo period in nanoseconds (unused, provided for callback compatibility).
 */
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
