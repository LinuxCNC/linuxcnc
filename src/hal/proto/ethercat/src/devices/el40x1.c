/**
 * @file el40x1.c
 * @brief Driver implementation for Beckhoff EL40x1 1-channel analog output terminals.
 *
 * Handles initialization and real-time write processing for the EL4001,
 * EL4011, EL4021, and EL4031 single-channel analog output terminals.
 * The driver scales a HAL float input (via scale/offset HAL pins) to a
 * signed 16-bit value that is written into the EtherCAT process data image.
 * Output limiting (min-dc / max-dc) and absolute-value mode are also
 * supported through dedicated HAL pins.
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
#include "el40x1.h"

/**
 * @brief Per-channel HAL data for a single EL40x1 output.
 *
 * All pointer fields correspond to HAL pins that are exported during
 * initialization and accessed by the real-time write callback.
 */
typedef struct {
  gomc_hal_bit_t *pos;       /**< Output HIGH when the commanded value is positive. */
  gomc_hal_bit_t *neg;       /**< Output HIGH when the commanded value is negative. */
  gomc_hal_bit_t *enable;    /**< Input: when 0 the DAC output is forced to zero. */
  gomc_hal_bit_t *absmode;   /**< Input: when 1 the absolute value of @c value is used. */
  gomc_hal_float_t *value;   /**< Input: desired analog output value (in user units). */
  gomc_hal_float_t *scale;   /**< IO: full-scale user-unit value mapping to duty cycle ±1.0. */
  gomc_hal_float_t *offset;  /**< IO: DC offset added to the scaled value before clamping. */
  double old_scale;     /**< Shadow copy of @c scale used to detect changes. */
  double scale_recip;   /**< Reciprocal of @c scale, recomputed when scale changes. */
  gomc_hal_float_t *min_dc;  /**< IO: minimum allowable duty cycle (clamped to [-1, 1]). */
  gomc_hal_float_t *max_dc;  /**< IO: maximum allowable duty cycle (clamped to [-1, 1]). */
  gomc_hal_float_t *curr_dc; /**< Output: actual duty cycle sent to the terminal this cycle. */
  gomc_hal_s32_t *raw_val;   /**< Output: raw 16-bit signed integer written to the PDO. */
  unsigned int val_pdo_os; /**< Byte offset of the output value entry in the process image. */
} lcec_el40x1_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_el40x1_data_t, scale), "%s.%s.%s.aout-scale" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_el40x1_data_t, offset), "%s.%s.%s.aout-offset" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_el40x1_data_t, min_dc), "%s.%s.%s.aout-min-dc" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_el40x1_data_t, max_dc), "%s.%s.%s.aout-max-dc" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_el40x1_data_t, curr_dc), "%s.%s.%s.aout-curr-dc" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el40x1_data_t, enable), "%s.%s.%s.aout-enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el40x1_data_t, absmode), "%s.%s.%s.aout-absmode" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IN, offsetof(lcec_el40x1_data_t, value), "%s.%s.%s.aout-value" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el40x1_data_t, raw_val), "%s.%s.%s.aout-raw" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el40x1_data_t, pos), "%s.%s.%s.aout-pos" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el40x1_data_t, neg), "%s.%s.%s.aout-neg" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el40x1_channel[] = {
    {0x7000, 1, 16}  // output
};

static ec_pdo_info_t lcec_el40x1_pdos_in[] = {
    {0x1600, 1, lcec_el40x1_channel},
};

static ec_sync_info_t lcec_el40x1_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_el40x1_pdos_in},
    {0xff}
};

void lcec_el40x1_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialize an EL40x1 slave device.
 *
 * Registers the write callback, allocates and zeroes HAL memory, configures
 * the EtherCAT sync manager and PDO entry, exports all HAL pins, and sets
 * default values for scale (1.0), min-dc (-1.0), and max-dc (1.0).
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array to populate.
 * @return 0 on success, -EIO if HAL memory allocation fails or pin export fails.
 */
int lcec_el40x1_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_el40x1_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el40x1_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el40x1_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el40x1_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el40x1_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->val_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // set default pin values
  *(hal_data->scale) = 1.0;
  *(hal_data->min_dc) = -1.0;
  *(hal_data->max_dc) = 1.0;

  // init other fields
  hal_data->old_scale = *(hal_data->scale) + 1.0;
  hal_data->scale_recip = 1.0;

  return 0;
}

/**
 * @brief Real-time write callback for the EL40x1.
 *
 * Called every servo cycle.  Performs the following steps:
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
void lcec_el40x1_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el40x1_data_t *hal_data = (lcec_el40x1_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  double tmpval, tmpdc, raw_val;

  // validate duty cycle limits, both limits must be between
  // 0.0 and 1.0 (inclusive) and max must be greater then min
  if (*(hal_data->max_dc) > 1.0) {
    *(hal_data->max_dc) = 1.0;
  }
  if (*(hal_data->min_dc) > *(hal_data->max_dc)) {
    *(hal_data->min_dc) = *(hal_data->max_dc);
  }
  if (*(hal_data->min_dc) < -1.0) {
    *(hal_data->min_dc) = -1.0;
  }
  if (*(hal_data->max_dc) < *(hal_data->min_dc)) {
    *(hal_data->max_dc) = *(hal_data->min_dc);
  }

  // do scale calcs only when scale changes
  if (*(hal_data->scale) != hal_data->old_scale) {
    // validate the new scale value
    if ((*(hal_data->scale) < 1e-20) && (*(hal_data->scale) > -1e-20)) {
      // value too small, divide by zero is a bad thing
      *(hal_data->scale) = 1.0;
    }
    // get ready to detect future scale changes
    hal_data->old_scale = *(hal_data->scale);
    // we will need the reciprocal
    hal_data->scale_recip = 1.0 / *(hal_data->scale);
  }

  // get command
  tmpval = *(hal_data->value);
  if (*(hal_data->absmode) && (tmpval < 0)) {
    tmpval = -tmpval;
  }

  // convert value command to duty cycle
  tmpdc = tmpval * hal_data->scale_recip + *(hal_data->offset);
  if (tmpdc < *(hal_data->min_dc)) {
    tmpdc = *(hal_data->min_dc);
  }
  if (tmpdc > *(hal_data->max_dc)) {
    tmpdc = *(hal_data->max_dc);
  }

  // set output values
  if (*(hal_data->enable) == 0) {
    raw_val = 0;
    *(hal_data->pos) = 0;
    *(hal_data->neg) = 0;
    *(hal_data->curr_dc) = 0;
  } else {
    raw_val = (double)0x7fff * tmpdc;
    if (raw_val > (double)0x7fff) {
      raw_val = (double)0x7fff;
    }
    if (raw_val < (double)-0x7fff) {
      raw_val = (double)-0x7fff;
    }
    *(hal_data->pos) = (*(hal_data->value) > 0);
    *(hal_data->neg) = (*(hal_data->value) < 0);
    *(hal_data->curr_dc) = tmpdc;
  }

  // update value
  EC_WRITE_S16(&pd[hal_data->val_pdo_os], (int16_t)raw_val);
  *(hal_data->raw_val) = (int32_t)raw_val;
}
