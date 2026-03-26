/**
 * @file el41x4.c
 * @brief Driver implementation for Beckhoff EL41x4 4-channel analog output terminals.
 *
 * Handles initialization and real-time write processing for the EL4104 and
 * EL4134 four-channel extended-range analog output terminals.  PDO channel
 * mapping uses CoE indices 0x7000–0x7030 (in 0x10 steps), matching the
 * EL40x8 scheme.
 *
 * Four independent channels each scale a HAL float input (via per-channel
 * scale/offset HAL pins) to a signed 16-bit DAC value written into the
 * EtherCAT process data image.  Output limiting (min-dc / max-dc) and
 * absolute-value mode are supported per channel through HAL pins.
 *
 * @copyright Copyright (C) 2016-2026 Frank Brossette<frank.brossette@gmail.com>
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
#include "el41x4.h"

/**
 * @brief Per-channel HAL data for a single EL41x4 output channel.
 *
 * All pointer fields correspond to HAL pins exported during initialization
 * and accessed by the real-time write callback.
 */
typedef struct {
  hal_bit_t *pos;       /**< Output HIGH when the commanded value is positive. */
  hal_bit_t *neg;       /**< Output HIGH when the commanded value is negative. */
  hal_bit_t *enable;    /**< Input: when 0 the DAC output is forced to zero. */
  hal_bit_t *absmode;   /**< Input: when 1 the absolute value of @c value is used. */
  hal_float_t *value;   /**< Input: desired analog output value (in user units). */
  hal_float_t *scale;   /**< IO: full-scale user-unit value mapping to duty cycle ±1.0. */
  hal_float_t *offset;  /**< IO: DC offset added to the scaled value before clamping. */
  double old_scale;     /**< Shadow copy of @c scale used to detect changes. */
  double scale_recip;   /**< Reciprocal of @c scale, recomputed when scale changes. */
  hal_float_t *min_dc;  /**< IO: minimum allowable duty cycle (clamped to [-1, 1]). */
  hal_float_t *max_dc;  /**< IO: maximum allowable duty cycle (clamped to [-1, 1]). */
  hal_float_t *curr_dc; /**< Output: actual duty cycle sent to the terminal this cycle. */
  hal_s32_t *raw_val;   /**< Output: raw 16-bit signed integer written to the PDO. */
  unsigned int val_pdo_os; /**< Byte offset of the output value entry in the process image. */
} lcec_el41x4_chan_t;

/**
 * @brief Top-level HAL data for an EL41x4 terminal.
 *
 * Contains an array of per-channel data structures, one entry per output channel.
 */
typedef struct {
  lcec_el41x4_chan_t chans[LCEC_EL41x4_CHANS]; /**< Per-channel HAL data (4 channels). */
} lcec_el41x4_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el41x4_chan_t, scale), "%s.%s.%s.aout-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el41x4_chan_t, offset), "%s.%s.%s.aout-%d-offset" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el41x4_chan_t, min_dc), "%s.%s.%s.aout-%d-min-dc" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el41x4_chan_t, max_dc), "%s.%s.%s.aout-%d-max-dc" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el41x4_chan_t, curr_dc), "%s.%s.%s.aout-%d-curr-dc" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el41x4_chan_t, enable), "%s.%s.%s.aout-%d-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el41x4_chan_t, absmode), "%s.%s.%s.aout-%d-absmode" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_el41x4_chan_t, value), "%s.%s.%s.aout-%d-value" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el41x4_chan_t, raw_val), "%s.%s.%s.aout-%d-raw" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el41x4_chan_t, pos), "%s.%s.%s.aout-%d-pos" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el41x4_chan_t, neg), "%s.%s.%s.aout-%d-neg" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el41x4_channel1[] = {
    {0x7000, 1, 16}  // output
};

static ec_pdo_entry_info_t lcec_el41x4_channel2[] = {
    {0x7010, 1, 16}  // output
};

static ec_pdo_entry_info_t lcec_el41x4_channel3[] = {
    {0x7020, 1, 16}  // output
};

static ec_pdo_entry_info_t lcec_el41x4_channel4[] = {
    {0x7030, 1, 16}  // output
};

static ec_pdo_info_t lcec_el41x4_pdos_in[] = {
    {0x1600, 1, lcec_el41x4_channel1},
    {0x1601, 1, lcec_el41x4_channel2},
    {0x1602, 1, lcec_el41x4_channel3},
    {0x1603, 1, lcec_el41x4_channel4}
};

static ec_sync_info_t lcec_el41x4_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 4, lcec_el41x4_pdos_in},
    {0xff}
};

void lcec_el41x4_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialize an EL41x4 slave device.
 *
 * Registers the write callback, allocates and zeroes HAL memory, configures
 * the EtherCAT sync manager and PDO entries for all 4 channels (CoE indices
 * 0x7000–0x7030 in 0x10 steps), exports all HAL pins (indexed 0–3), and
 * sets default values for scale (1.0), min-dc (-1.0), and max-dc (1.0) on
 * every channel.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, -EIO if HAL memory allocation or pin export fails.
 */
int lcec_el41x4_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el41x4_data_t *hal_data;
  lcec_el41x4_chan_t *chan;
  int i;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el41x4_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el41x4_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el41x4_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el41x4_syncs;

  // initialize pins
  for (i=0; i<LCEC_EL41x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i*0x10), 0x01, &chan->val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }

    // set default pin values
    *(chan->scale) = 1.0;
    *(chan->min_dc) = -1.0;
    *(chan->max_dc) = 1.0;

    // init other fields
    chan->old_scale = *(chan->scale) + 1.0;
    chan->scale_recip = 1.0;
  }

  return 0;
}

/**
 * @brief Real-time write callback for the EL41x4.
 *
 * Called every servo cycle.  Iterates over all 4 channels and for each one:
 *  1. Validates and clamps the min-dc / max-dc limits to [-1.0, 1.0].
 *  2. Recomputes the scale reciprocal when the @c scale pin changes.
 *  3. Converts the @c value input to a duty cycle:
 *     @c dc = value / scale + offset, then clamps to [min_dc, max_dc].
 *  4. When @c enable is zero the DAC output is forced to 0x0000 and the
 *     pos/neg/curr_dc indicators are cleared.
 *  5. Otherwise maps the duty cycle to a signed 16-bit integer in the range
 *     [-0x7fff, 0x7fff] and writes it to the EtherCAT process image.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period Servo cycle period in nanoseconds (unused, reserved).
 */
void lcec_el41x4_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el41x4_data_t *hal_data = (lcec_el41x4_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el41x4_chan_t *chan;
  double tmpval, tmpdc, raw_val;

  // set outputs
  for (i=0; i<LCEC_EL41x4_CHANS; i++) {
    chan = &hal_data->chans[i];

    // validate duty cycle limits, both limits must be between
    // 0.0 and 1.0 (inclusive) and max must be greater then min
    if (*(chan->max_dc) > 1.0) {
      *(chan->max_dc) = 1.0;
    }
    if (*(chan->min_dc) > *(chan->max_dc)) {
      *(chan->min_dc) = *(chan->max_dc);
    }
    if (*(chan->min_dc) < -1.0) {
      *(chan->min_dc) = -1.0;
    }
    if (*(chan->max_dc) < *(chan->min_dc)) {
      *(chan->max_dc) = *(chan->min_dc);
    }

    // do scale calcs only when scale changes
    if (*(chan->scale) != chan->old_scale) {
      // validate the new scale value
      if ((*(chan->scale) < 1e-20) && (*(chan->scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(chan->scale) = 1.0;
      }
      // get ready to detect future scale changes
      chan->old_scale = *(chan->scale);
      // we will need the reciprocal
      chan->scale_recip = 1.0 / *(chan->scale);
    }

    // get command
    tmpval = *(chan->value);
    if (*(chan->absmode) && (tmpval < 0)) {
      tmpval = -tmpval;
    }

    // convert value command to duty cycle
    tmpdc = tmpval * chan->scale_recip + *(chan->offset);
    if (tmpdc < *(chan->min_dc)) {
      tmpdc = *(chan->min_dc);
    }
    if (tmpdc > *(chan->max_dc)) {
      tmpdc = *(chan->max_dc);
    }

    // set output values
    if (*(chan->enable) == 0) {
      raw_val = 0;
      *(chan->pos) = 0;
      *(chan->neg) = 0;
      *(chan->curr_dc) = 0;
    } else {
      raw_val = (double)0x7fff * tmpdc;
      if (raw_val > (double)0x7fff) {
        raw_val = (double)0x7fff;
      }
      if (raw_val < (double)-0x7fff) {
        raw_val = (double)-0x7fff;
      }
      *(chan->pos) = (*(chan->value) > 0);
      *(chan->neg) = (*(chan->value) < 0);
      *(chan->curr_dc) = tmpdc;
    }

    // update value
    EC_WRITE_S16(&pd[chan->val_pdo_os], (int16_t)raw_val);
    *(chan->raw_val) = (int32_t)raw_val;
  }
}
