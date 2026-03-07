/**
 * @file el31x4.c
 * @brief LinuxCNC EtherCAT HAL driver for Beckhoff EL31x4 4-channel analog input terminals.
 *
 * Supports EL3104, EL3114, EL3124, EL3144, EL3154, and EL3164 EtherCAT terminals.
 * Similar to the EL31x2 driver but provides four channels.  Status bits are
 * accessed individually via object 0x6000..0x6030 using distinct byte-offset and
 * bit-position pairs rather than a packed status byte.
 *
 * The floating-point output is computed as:
 * @code
 *   val = bias + scale * (double)raw * (1.0 / 0x7fff)
 * @endcode
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
#include "el31x4.h"

/**
 * @brief Per-channel HAL pins and PDO offsets for one EL31x4 input channel.
 */
typedef struct {
  hal_bit_t *overrange;      /**< HAL bit output: signal exceeds measurable range (object 0x6000+n, subindex 0x02). */
  hal_bit_t *underrange;     /**< HAL bit output: signal is below measurable range (object 0x6000+n, subindex 0x01). */
  hal_bit_t *error;          /**< HAL bit output: channel error flag (object 0x6000+n, subindex 0x07). */
  hal_bit_t *sync_err;       /**< HAL bit output: synchronisation error flag (object 0x6000+n, subindex 0x0E). */
  hal_s32_t *raw_val;        /**< HAL s32 output: raw 16-bit signed ADC value from the terminal. */
  hal_float_t *scale;        /**< HAL float I/O: multiplier applied to the normalised ADC value (default 1.0). */
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

/**
 * @brief Top-level HAL data structure for one EL31x4 slave, holding all channel state.
 */
typedef struct {
  lcec_el31x4_chan_t chans[LCEC_EL31x4_CHANS]; /**< Per-channel data for all four analog inputs. */
} lcec_el31x4_data_t;

void lcec_el31x4_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialise an EL31x4 slave: allocate HAL memory, register PDOs, and create HAL pins.
 *
 * Called once at HAL component initialisation. Registers five PDO entries per channel
 * (underrange, overrange, error, sync-error, and 16-bit value) and exports the HAL
 * pins for each channel.  The scale pin is initialised to 1.0.
 *
 * @param comp_id         LinuxCNC HAL component ID.
 * @param slave           Pointer to the lcec_slave descriptor for this terminal.
 * @param pdo_entry_regs  Pointer to the EtherCAT PDO entry registration array;
 *                        advanced by the number of PDO entries registered.
 * @return 0 on success, -EIO on HAL memory allocation failure, or a negative
 *         error code if HAL pin creation fails.
 */
int lcec_el31x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
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

/**
 * @brief Cyclic read callback: transfer EtherCAT PDO data to HAL pins for all channels.
 *
 * Called every servo cycle. Reads the individual status bits and 16-bit ADC value for
 * each channel from the EtherCAT process data image and updates the corresponding HAL
 * pins.  Returns immediately if the slave is not in the operational state.
 *
 * @param slave   Pointer to the lcec_slave descriptor for this terminal.
 * @param period  Servo period in nanoseconds (unused, provided for callback compatibility).
 */
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
