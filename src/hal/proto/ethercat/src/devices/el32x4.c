/**
 * @file el32x4.c
 * @brief LinuxCNC EtherCAT HAL driver for Beckhoff EL32x4 4-channel analog input terminals.
 *
 * Supports the EL3204 and compatible 4-channel analog input terminals.
 * Each channel provides a 16-bit signed measurement value together with overrange,
 * underrange, error, TxPDO-state, and TxPDO-toggle status flags.
 *
 * The floating-point HAL output pin is computed as:
 * @code
 *   val = bias + scale * (double)raw * 0.1
 * @endcode
 * giving a resolution of 0.1 units per LSB (e.g. 0.1 °C per count when used
 * with a temperature sensor).
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
#include "el32x4.h"

/**
 * @brief Per-channel HAL pins and PDO offsets for one EL32x4 input channel.
 */
typedef struct {
  hal_bit_t *overrange;      /**< HAL bit output: signal exceeds measurable range. */
  hal_bit_t *underrange;     /**< HAL bit output: signal is below the measurable range. */
  hal_bit_t *error;          /**< HAL bit output: channel error flag. */
  hal_bit_t *tx_state;       /**< HAL bit output: TxPDO state indicator. */
  hal_bit_t *tx_toggle;      /**< HAL bit output: TxPDO toggle bit; toggles each cycle to confirm data freshness. */
  hal_s32_t *raw_val;        /**< HAL s32 output: raw 16-bit signed ADC value from the terminal. */
  hal_float_t *scale;        /**< HAL float I/O: multiplier applied before adding bias (default 1.0). */
  hal_float_t *bias;         /**< HAL float I/O: offset added after scaling (default 0.0). */
  hal_float_t *val;          /**< HAL float output: final value = bias + scale * raw * 0.1. */
  unsigned int ovr_pdo_os;   /**< Byte offset of the overrange bit in the EtherCAT process data image. */
  unsigned int ovr_pdo_bp;   /**< Bit position of the overrange flag within its byte. */
  unsigned int udr_pdo_os;   /**< Byte offset of the underrange bit in the EtherCAT process data image. */
  unsigned int udr_pdo_bp;   /**< Bit position of the underrange flag within its byte. */
  unsigned int error_pdo_os; /**< Byte offset of the channel error bit in the EtherCAT process data image. */
  unsigned int error_pdo_bp; /**< Bit position of the channel error flag within its byte. */
  unsigned int tx_state_pdo_os;  /**< Byte offset of the TxPDO state bit in the EtherCAT process data image. */
  unsigned int tx_state_pdo_bp;  /**< Bit position of the TxPDO state flag within its byte. */
  unsigned int tx_toggle_pdo_os; /**< Byte offset of the TxPDO toggle bit in the EtherCAT process data image. */
  unsigned int tx_toggle_pdo_bp; /**< Bit position of the TxPDO toggle flag within its byte. */
  unsigned int val_pdo_os;   /**< Byte offset of the 16-bit channel value in the EtherCAT process data image. */
} lcec_el32x4_chan_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el32x4_chan_t, error), "%s.%s.%s.ain-%d-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el32x4_chan_t, overrange), "%s.%s.%s.ain-%d-overrange" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el32x4_chan_t, underrange), "%s.%s.%s.ain-%d-underrange" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el32x4_chan_t, tx_state), "%s.%s.%s.ain-%d-tx-state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el32x4_chan_t, tx_toggle), "%s.%s.%s.ain-%d-tx-toggle" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el32x4_chan_t, raw_val), "%s.%s.%s.ain-%d-raw" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el32x4_chan_t, val), "%s.%s.%s.ain-%d-val" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el32x4_chan_t, scale), "%s.%s.%s.ain-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el32x4_chan_t, bias), "%s.%s.%s.ain-%d-bias" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief Top-level HAL data structure for one EL32x4 slave, holding all channel state.
 */
typedef struct {
  lcec_el32x4_chan_t chans[LCEC_EL32x4_CHANS]; /**< Per-channel data for all four analog inputs. */
} lcec_el32x4_data_t;

void lcec_el32x4_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialise an EL32x4 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * Called once at HAL component initialisation. Registers six PDO entries per channel
 * (underrange, overrange, error, TxPDO state, TxPDO toggle, and 16-bit value) and
 * exports the HAL pins for each of the four input channels.  The scale pin is
 * initialised to 1.0.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, -EIO on HAL memory allocation failure, or a negative
 *         error code if HAL pin creation fails.
 */
int lcec_el32x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el32x4_data_t *hal_data;
  lcec_el32x4_chan_t *chan;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el32x4_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el32x4_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el32x4_data_t));
  slave->hal_data = hal_data;

  // initialize pins
  for (i=0; i<LCEC_EL32x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->udr_pdo_os, &chan->udr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->ovr_pdo_os, &chan->ovr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x07, &chan->error_pdo_os, &chan->error_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0f, &chan->tx_state_pdo_os, &chan->tx_state_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x10, &chan->tx_toggle_pdo_os, &chan->tx_toggle_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
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
 * Called every servo cycle. Reads per-channel status bits, TxPDO state/toggle flags,
 * and the 16-bit ADC value from the EtherCAT process data image, then updates the
 * corresponding HAL pins.  Returns immediately if the slave is not operational.
 *
 * @param slave   Pointer to the lcec_slave descriptor for this terminal.
 * @param period  Servo period in nanoseconds (unused, provided for callback compatibility).
 */
void lcec_el32x4_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el32x4_data_t *hal_data = (lcec_el32x4_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el32x4_chan_t *chan;
  int16_t value;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL32x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // update state
    // update state
    *(chan->overrange) = EC_READ_BIT(&pd[chan->ovr_pdo_os], chan->ovr_pdo_bp);
    *(chan->underrange) = EC_READ_BIT(&pd[chan->udr_pdo_os], chan->udr_pdo_bp);
    *(chan->error) = EC_READ_BIT(&pd[chan->error_pdo_os], chan->error_pdo_bp);
    *(chan->tx_state) = EC_READ_BIT(&pd[chan->tx_state_pdo_os], chan->tx_state_pdo_bp);
    *(chan->tx_toggle) = EC_READ_BIT(&pd[chan->tx_toggle_pdo_os], chan->tx_toggle_pdo_bp);

    // update value
    value = EC_READ_S16(&pd[chan->val_pdo_os]);
    *(chan->raw_val) = value;
    *(chan->val) = *(chan->bias) + *(chan->scale) * (double)value * 0.1;
  }
}
