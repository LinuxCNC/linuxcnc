/** @file em7004.c
 * @brief Driver for the Beckhoff EM7004 4-channel stepper motor module.
 *
 * Implements HAL pin export, PDO mapping, and cyclic read/write for all
 * sub-devices of the EM7004: 16 digital inputs, 16 digital outputs, 4 analog
 * outputs, and 4 incremental encoders with external latch and preset support.
 *
 * @copyright Copyright (C) 2014-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "em7004.h"

/**
 * @brief Per-channel HAL data for one digital input.
 */
typedef struct {
  gomc_hal_bit_t *in;     /**< OUT: digital input state */
  gomc_hal_bit_t *in_not; /**< OUT: inverted digital input state */
  unsigned int pdo_os; /**< PDO byte offset */
  unsigned int pdo_bp; /**< Bit position within PDO byte */
} lcec_em7004_din_t;

/**
 * @brief Per-channel HAL data for one digital output.
 */
typedef struct {
  gomc_hal_bit_t *out;    /**< IN: desired output state */
  gomc_hal_bit_t invert;  /**< Parameter: 1 = invert the output */
  unsigned int pdo_os; /**< PDO byte offset */
  unsigned int pdo_bp; /**< Bit position within PDO byte */
} lcec_em7004_dout_t;

/**
 * @brief Per-channel HAL data for one analog output.
 */
typedef struct {
  gomc_hal_bit_t *pos;         /**< OUT: output is in positive direction */
  gomc_hal_bit_t *neg;         /**< OUT: output is in negative direction */
  gomc_hal_bit_t *enable;      /**< IN: enable this output channel */
  gomc_hal_bit_t *absmode;     /**< IN: use absolute value mode */
  gomc_hal_float_t *value;     /**< IN: commanded value in user units */
  gomc_hal_float_t *scale;     /**< IO: scale factor (user units → duty cycle) */
  gomc_hal_float_t *offset;    /**< IO: offset added after scaling */
  double old_scale;       /**< Previously applied scale (change detection) */
  double scale_recip;     /**< Reciprocal of current scale */
  gomc_hal_float_t *min_dc;    /**< IO: minimum duty cycle clamp */
  gomc_hal_float_t *max_dc;    /**< IO: maximum duty cycle clamp */
  gomc_hal_float_t *curr_dc;   /**< OUT: current duty cycle being applied */
  gomc_hal_s32_t *raw_val;     /**< OUT: raw 16-bit output value written to PDO */
  unsigned int val_pdo_os; /**< PDO byte offset of the 16-bit value */
} lcec_em7004_aout_t;

/**
 * @brief Per-channel HAL data for one incremental encoder.
 */
typedef struct {
  gomc_hal_bit_t *ena_latch_ext_pos;  /**< IO: enable external latch on positive edge */
  gomc_hal_bit_t *ena_latch_ext_neg;  /**< IO: enable external latch on negative edge */
  gomc_hal_bit_t *reset;              /**< IN: reset accumulated count to zero */
  gomc_hal_bit_t *ina;                /**< OUT: encoder channel A state */
  gomc_hal_bit_t *inb;                /**< OUT: encoder channel B state */
  gomc_hal_bit_t *ingate;             /**< OUT: encoder gate input state */
  gomc_hal_bit_t *inext;              /**< OUT: external latch input state */
  gomc_hal_bit_t *latch_ext_valid;    /**< OUT: external latch valid flag */
  gomc_hal_bit_t *set_raw_count;      /**< IO: write 1 to preset the counter */
  gomc_hal_s32_t *set_raw_count_val;  /**< IN: preset value for the counter */
  gomc_hal_s32_t *raw_count;          /**< OUT: raw 16-bit counter value */
  gomc_hal_s32_t *raw_latch;          /**< OUT: raw 16-bit latch value */
  gomc_hal_s32_t *count;              /**< OUT: accumulated count */
  gomc_hal_float_t *pos_scale;        /**< IO: counts per user unit */
  gomc_hal_float_t *pos;              /**< OUT: position in user units */

  unsigned int ena_latch_ext_pos_pdo_os; /**< PDO byte offset: enable latch pos edge */
  unsigned int ena_latch_ext_pos_pdo_bp; /**< Bit position: enable latch pos edge */
  unsigned int ena_latch_ext_neg_pdo_os; /**< PDO byte offset: enable latch neg edge */
  unsigned int ena_latch_ext_neg_pdo_bp; /**< Bit position: enable latch neg edge */
  unsigned int set_count_pdo_os;         /**< PDO byte offset: set counter command */
  unsigned int set_count_pdo_bp;         /**< Bit position: set counter command */
  unsigned int set_count_val_pdo_os;     /**< PDO byte offset: preset value */
  unsigned int set_count_done_pdo_os;    /**< PDO byte offset: set counter done */
  unsigned int set_count_done_pdo_bp;    /**< Bit position: set counter done */
  unsigned int latch_ext_valid_pdo_os;   /**< PDO byte offset: latch valid flag */
  unsigned int latch_ext_valid_pdo_bp;   /**< Bit position: latch valid flag */
  unsigned int ina_pdo_os;               /**< PDO byte offset: channel A state */
  unsigned int ina_pdo_bp;               /**< Bit position: channel A state */
  unsigned int inb_pdo_os;               /**< PDO byte offset: channel B state */
  unsigned int inb_pdo_bp;               /**< Bit position: channel B state */
  unsigned int ingate_pdo_os;            /**< PDO byte offset: gate input state */
  unsigned int ingate_pdo_bp;            /**< Bit position: gate input state */
  unsigned int inext_pdo_os;             /**< PDO byte offset: external latch input */
  unsigned int inext_pdo_bp;             /**< Bit position: external latch input */
  unsigned int count_pdo_os;            /**< PDO byte offset: 16-bit counter value */
  unsigned int latch_pdo_os;            /**< PDO byte offset: 16-bit latch value */

  int do_init;          /**< Flag: 1 on first read after startup */
  int16_t last_count;   /**< Previous raw count for delta computation */
  double old_scale;     /**< Previously applied scale (change detection) */
  double scale;         /**< Reciprocal of current pos_scale */
} lcec_em7004_enc_t;

/**
 * @brief Top-level HAL data for the EM7004 module.
 */
typedef struct {
  lcec_em7004_din_t dins[LCEC_EM7004_DIN_COUNT];    /**< Digital input channels */
  lcec_em7004_dout_t douts[LCEC_EM7004_DOUT_COUNT]; /**< Digital output channels */
  lcec_em7004_aout_t aouts[LCEC_EM7004_AOUT_COUNT]; /**< Analog output channels */
  lcec_em7004_enc_t encs[LCEC_EM7004_ENC_COUNT];    /**< Encoder channels */
  int last_operational; /**< 1 when slave was operational on previous cycle */
} lcec_em7004_data_t;

static const lcec_pindesc_t slave_din_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_din_t, in), "%s.%s.%s.din-%d" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_din_t, in_not), "%s.%s.%s.din-%d-not" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_dout_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_em7004_dout_t, out), "%s.%s.%s.dout-%d" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_dout_params[] = {
  { GOMC_HAL_BIT, GOMC_HAL_RW, offsetof(lcec_em7004_dout_t, invert), "%s.%s.%s.dout-%d-invert" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_aout_pins[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_em7004_aout_t, scale), "%s.%s.%s.aout-%d-scale" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_em7004_aout_t, offset), "%s.%s.%s.aout-%d-offset" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_em7004_aout_t, min_dc), "%s.%s.%s.aout-%d-min-dc" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_em7004_aout_t, max_dc), "%s.%s.%s.aout-%d-max-dc" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_em7004_aout_t, curr_dc), "%s.%s.%s.aout-%d-curr-dc" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_em7004_aout_t, enable), "%s.%s.%s.aout-%d-enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_em7004_aout_t, absmode), "%s.%s.%s.aout-%d-absmode" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IN, offsetof(lcec_em7004_aout_t, value), "%s.%s.%s.aout-%d-value" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_em7004_aout_t, raw_val), "%s.%s.%s.aout-%d-raw" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_aout_t, pos), "%s.%s.%s.aout-%d-pos" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_aout_t, neg), "%s.%s.%s.aout-%d-neg" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_enc_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_IO, offsetof(lcec_em7004_enc_t, ena_latch_ext_pos), "%s.%s.%s.enc-%d-index-ext-pos-enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IO, offsetof(lcec_em7004_enc_t, ena_latch_ext_neg), "%s.%s.%s.enc-%d-index-ext-neg-enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_em7004_enc_t, reset), "%s.%s.%s.enc-%d-reset" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, ina), "%s.%s.%s.enc-%d-ina" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, inb), "%s.%s.%s.enc-%d-inb" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, ingate), "%s.%s.%s.enc-%d-ingate" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, inext), "%s.%s.%s.enc-%d-inext" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, latch_ext_valid), "%s.%s.%s.enc-%d-latch-ext-valid" },
  { GOMC_HAL_BIT, GOMC_HAL_IO, offsetof(lcec_em7004_enc_t, set_raw_count), "%s.%s.%s.enc-%d-set-raw-count" },
  { GOMC_HAL_S32, GOMC_HAL_IN, offsetof(lcec_em7004_enc_t, set_raw_count_val), "%s.%s.%s.enc-%d-set-raw-count-val" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, count), "%s.%s.%s.enc-%d-count" },
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, raw_latch), "%s.%s.%s.enc-%d-raw-latch" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_em7004_enc_t, pos), "%s.%s.%s.enc-%d-pos" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IO, offsetof(lcec_em7004_enc_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief Cyclic read and write: forward declarations.
 */
void lcec_em7004_read(struct lcec_slave *slave, long period);
void lcec_em7004_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EM7004 stepper module and map all PDOs.
 *
 * Sets up PDO entries and HAL pins for all 16 digital inputs, 16 digital
 * outputs, 4 analog outputs, and 4 encoder channels.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_em7004_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_em7004_data_t *hal_data;
  lcec_em7004_din_t *din;
  lcec_em7004_dout_t *dout;
  lcec_em7004_aout_t *aout;
  lcec_em7004_enc_t *enc;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_em7004_read;
  slave->proc_write = lcec_em7004_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_em7004_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_em7004_data_t));
  slave->hal_data = hal_data;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize digital input pins
  for (i=0, din=hal_data->dins; i<LCEC_EM7004_DIN_COUNT; i++, din++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, i + 1, &din->pdo_os, &din->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, din, slave_din_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  // initialize digital output pins
  for (i=0, dout=hal_data->douts; i<LCEC_EM7004_DOUT_COUNT; i++, dout++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, i + 1, &dout->pdo_os, &dout->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, dout, slave_dout_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
    if ((err = lcec_param_newf_list(env, comp_id, dout, slave_dout_params, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  // initialize analog output pins
  for (i=0, aout=hal_data->aouts; i<LCEC_EM7004_AOUT_COUNT; i++, aout++) {
    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x11, &aout->val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, aout, slave_aout_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }

    // set default pin values
    *(aout->scale) = 1.0;
    *(aout->min_dc) = -1.0;
    *(aout->max_dc) = 1.0;

    // init other fields
    aout->old_scale = *(aout->scale) + 1.0;
    aout->scale_recip = 1.0;
  }

  // initialize encoder pins
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x02, &enc->latch_ext_valid_pdo_os, &enc->latch_ext_valid_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x03, &enc->set_count_done_pdo_os, &enc->set_count_done_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x09, &enc->ina_pdo_os, &enc->ina_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0a, &enc->inb_pdo_os, &enc->inb_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0c, &enc->ingate_pdo_os, &enc->ingate_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x0d, &enc->inext_pdo_os, &enc->inext_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x11, &enc->count_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060 + (i << 4), 0x12, &enc->latch_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x02, &enc->ena_latch_ext_pos_pdo_os, &enc->ena_latch_ext_pos_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x03, &enc->set_count_pdo_os, &enc->set_count_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x04, &enc->ena_latch_ext_neg_pdo_os, &enc->ena_latch_ext_neg_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7060 + (i << 4), 0x11, &enc->set_count_val_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, enc, slave_enc_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(enc->pos_scale) = 1.0;

    // initialize variables
    enc->do_init = 1;
    enc->last_count = 0;
    enc->old_scale = *(enc->pos_scale) + 1.0;
    enc->scale = 1.0;
  }

  return 0;
}

/**
 * @brief Cyclic read: update all digital input and encoder HAL pins.
 *
 * Reads all 16 digital input bits and updates in/in_not pins.  For each
 * encoder channel, handles scale changes, reads counter and latch values,
 * processes preset and external-latch events, accumulates the count delta,
 * and computes the scaled floating-point position.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
void lcec_em7004_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_em7004_data_t *hal_data = (lcec_em7004_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_em7004_din_t *din;
  lcec_em7004_enc_t *enc;
  int i, s;
  int16_t raw_count, raw_latch, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check digital inputs
  for (i=0, din=hal_data->dins; i<LCEC_EM7004_DIN_COUNT; i++, din++) {
    s = EC_READ_BIT(&pd[din->pdo_os], din->pdo_bp);
    *(din->in) = s;
    *(din->in_not) = !s;
  }

  // read encoder data
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    // check for change in scale value
    if (*(enc->pos_scale) != enc->old_scale) {
      // scale value has changed, test and update it
      if ((*(enc->pos_scale) < 1e-20) && (*(enc->pos_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(enc->pos_scale) = 1.0;
      }
      // save new scale to detect future changes
      enc->old_scale = *(enc->pos_scale);
      // we actually want the reciprocal
      enc->scale = 1.0 / *(enc->pos_scale);
    }

    // get bit states
    *(enc->ina) = EC_READ_BIT(&pd[enc->ina_pdo_os], enc->ina_pdo_bp);
    *(enc->inb) = EC_READ_BIT(&pd[enc->inb_pdo_os], enc->inb_pdo_bp);
    *(enc->ingate) = EC_READ_BIT(&pd[enc->ingate_pdo_os], enc->ingate_pdo_bp);
    *(enc->inext) = EC_READ_BIT(&pd[enc->inext_pdo_os], enc->inext_pdo_bp);
    *(enc->latch_ext_valid) = EC_READ_BIT(&pd[enc->latch_ext_valid_pdo_os], enc->latch_ext_valid_pdo_bp);

    // read raw values
    raw_count = EC_READ_S16(&pd[enc->count_pdo_os]);
    raw_latch = EC_READ_S16(&pd[enc->latch_pdo_os]);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      enc->last_count = raw_count;
    }

    // check for counter set done
    if (EC_READ_BIT(&pd[enc->set_count_done_pdo_os], enc->set_count_done_pdo_bp)) {
      enc->last_count = raw_count;
      *(enc->set_raw_count) = 0;
    }

    // update raw values
    if (! *(enc->set_raw_count)) {
      *(enc->raw_count) = raw_count;
    }

    // handle initialization
    if (enc->do_init || *(enc->reset)) {
      enc->do_init = 0;
      enc->last_count = raw_count;
      *(enc->count) = 0;
    }

    // handle index
    if (*(enc->latch_ext_valid)) {
      *(enc->raw_latch) = raw_latch;
      enc->last_count = raw_latch;
      *(enc->count) = 0;
      *(enc->ena_latch_ext_pos) = 0;
      *(enc->ena_latch_ext_neg) = 0;
    }

    // compute net counts
    raw_delta = raw_count - enc->last_count;
    enc->last_count = raw_count;
    *(enc->count) += raw_delta;

    // scale count to make floating point position
    *(enc->pos) = *(enc->count) * enc->scale;
  }

  hal_data->last_operational = 1;
}

/**
 * @brief Cyclic write: update digital outputs, analog outputs, and encoder controls.
 *
 * Writes all 16 digital output bits (optionally inverted), computes duty-cycle
 * clamping and raw 16-bit values for all 4 analog outputs, then writes the
 * encoder set-count and latch-enable control bits.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
void lcec_em7004_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_em7004_data_t *hal_data = (lcec_em7004_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_em7004_dout_t *dout;
  lcec_em7004_aout_t *aout;
  lcec_em7004_enc_t *enc;
  int i, s;
  double tmpval, tmpdc, raw_val;

  // set digital outputs
  for (i=0, dout=hal_data->douts; i<LCEC_EM7004_DOUT_COUNT; i++, dout++) {
    s = *(dout->out);
    if (dout->invert) {
      s = !s;
    }
    EC_WRITE_BIT(&pd[dout->pdo_os], dout->pdo_bp, s);
  }

  // set analog outputs
  for (i=0, aout=hal_data->aouts; i<LCEC_EM7004_AOUT_COUNT; i++, aout++) {
    // validate duty cycle limits, both limits must be between
    // 0.0 and 1.0 (inclusive) and max must be greater then min
    if (*(aout->max_dc) > 1.0) {
      *(aout->max_dc) = 1.0;
    }
    if (*(aout->min_dc) > *(aout->max_dc)) {
      *(aout->min_dc) = *(aout->max_dc);
    }
    if (*(aout->min_dc) < -1.0) {
      *(aout->min_dc) = -1.0;
    }
    if (*(aout->max_dc) < *(aout->min_dc)) {
      *(aout->max_dc) = *(aout->min_dc);
    }

    // do scale calcs only when scale changes
    if (*(aout->scale) != aout->old_scale) {
      // validate the new scale value
      if ((*(aout->scale) < 1e-20) && (*(aout->scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(aout->scale) = 1.0;
      }
      // get ready to detect future scale changes
      aout->old_scale = *(aout->scale);
      // we will need the reciprocal
      aout->scale_recip = 1.0 / *(aout->scale);
    }

    // get command
    tmpval = *(aout->value);
    if (*(aout->absmode) && (tmpval < 0)) {
      tmpval = -tmpval;
    }

    // convert value command to duty cycle
    tmpdc = tmpval * aout->scale_recip + *(aout->offset);
    if (tmpdc < *(aout->min_dc)) {
      tmpdc = *(aout->min_dc);
    }
    if (tmpdc > *(aout->max_dc)) {
      tmpdc = *(aout->max_dc);
    }

    // set output values
    if (*(aout->enable) == 0) {
      raw_val = 0;
      *(aout->pos) = 0;
      *(aout->neg) = 0;
      *(aout->curr_dc) = 0;
    } else {
      raw_val = (double)0x7fff * tmpdc;
      if (raw_val > (double)0x7fff) {
        raw_val = (double)0x7fff;
      }
      if (raw_val < (double)-0x7fff) {
        raw_val = (double)-0x7fff;
      }
      *(aout->pos) = (*(aout->value) > 0);
      *(aout->neg) = (*(aout->value) < 0);
      *(aout->curr_dc) = tmpdc;
    }

    // update value
    EC_WRITE_S16(&pd[aout->val_pdo_os], (int16_t)raw_val);
    *(aout->raw_val) = (int32_t)raw_val;
  }

  // write encoder data
  for (i=0, enc=hal_data->encs; i<LCEC_EM7004_ENC_COUNT; i++, enc++) {
    EC_WRITE_BIT(&pd[enc->set_count_pdo_os], enc->set_count_pdo_bp, *(enc->set_raw_count));
    EC_WRITE_BIT(&pd[enc->ena_latch_ext_pos_pdo_os], enc->ena_latch_ext_pos_pdo_bp, *(enc->ena_latch_ext_pos));
    EC_WRITE_BIT(&pd[enc->ena_latch_ext_neg_pdo_os], enc->ena_latch_ext_neg_pdo_bp, *(enc->ena_latch_ext_neg));
    EC_WRITE_S16(&pd[enc->set_count_val_pdo_os], *(enc->set_raw_count_val));
  }
}
