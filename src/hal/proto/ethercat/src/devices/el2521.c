/**
 * @file el2521.c
 * @brief HAL driver for the Beckhoff EL2521 1-channel pulse train output
 *        (incremental encoder simulation / step generator).
 *
 * The EL2521 generates a pulse-train output whose frequency is set via the
 * @c stp-velo-cmd HAL pin and the @c stp-pos-scale parameter.  An internal
 * ramp limits acceleration; the ramp can be disabled at run-time with the
 * @c stp-ramp-disable pin.
 *
 * EtherCAT identifiers:
 *  - Vendor ID  : 0x00000002 (Beckhoff)
 *  - Product code: 0x09D93052
 *
 * HAL pins exported:
 *  - @c stp-counts     (GOMC_HAL_S32, OUT) — accumulated step count
 *  - @c stp-pos-fb     (GOMC_HAL_FLOAT, OUT) — position feedback in position units
 *  - @c stp-ramp-active (GOMC_HAL_BIT, OUT) — TRUE while frequency ramp is active
 *  - @c stp-ramp-disable (GOMC_HAL_BIT, IN) — TRUE to bypass the ramp
 *  - @c stp-in-z       (GOMC_HAL_BIT, OUT) — digital input Z state
 *  - @c stp-in-z-not   (GOMC_HAL_BIT, OUT) — inverted digital input Z state
 *  - @c stp-in-t       (GOMC_HAL_BIT, OUT) — digital input T state
 *  - @c stp-in-t-not   (GOMC_HAL_BIT, OUT) — inverted digital input T state
 *  - @c stp-enable     (GOMC_HAL_BIT, IN)  — enable pulse output
 *  - @c stp-velo-cmd   (GOMC_HAL_FLOAT, IN) — velocity command (position units/s)
 *
 * HAL parameters exported:
 *  - @c stp-freq         (GOMC_HAL_FLOAT, RO) — current output frequency (Hz)
 *  - @c stp-maxvel       (GOMC_HAL_FLOAT, RO) — maximum velocity (position units/s)
 *  - @c stp-maxaccel-fall (GOMC_HAL_FLOAT, RO) — maximum deceleration (pos units/s²)
 *  - @c stp-maxaccel-rise (GOMC_HAL_FLOAT, RO) — maximum acceleration (pos units/s²)
 *  - @c stp-pos-scale    (GOMC_HAL_FLOAT, RW) — steps per position unit
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
#include "el2521.h"

/**
 * @brief HAL data structure for the EL2521 pulse train output slave.
 *
 * Holds all HAL pin/parameter pointers, PDO byte offsets, SDO configuration
 * values read at init time, and internal state used for frequency scaling and
 * position feedback accumulation.
 */
typedef struct {
  gomc_hal_s32_t *count;		/**< HAL OUT pin: accumulated step count. */
  gomc_hal_float_t *pos_fb;		/**< HAL OUT pin: position feedback (position units). */
  gomc_hal_bit_t *ramp_active;       /**< HAL OUT pin: TRUE while frequency ramp is active. */
  gomc_hal_bit_t *ramp_disable;      /**< HAL IN pin: TRUE to disable the hardware ramp. */
  gomc_hal_bit_t *in_z;              /**< HAL OUT pin: state of digital input Z. */
  gomc_hal_bit_t *in_z_not;          /**< HAL OUT pin: inverted state of digital input Z. */
  gomc_hal_bit_t *in_t;              /**< HAL OUT pin: state of digital input T. */
  gomc_hal_bit_t *in_t_not;          /**< HAL OUT pin: inverted state of digital input T. */

  gomc_hal_bit_t *enable;		/**< HAL IN pin: enable pulse generation. */
  gomc_hal_float_t *vel_cmd;		/**< HAL IN pin: velocity command (position units/s). */

  gomc_hal_float_t pos_scale;	/**< HAL RW parameter: steps per position unit. */
  gomc_hal_float_t freq;		/**< HAL RO parameter: current output frequency (Hz). */
  gomc_hal_float_t maxvel;		/**< HAL RO parameter: maximum velocity (position units/s). */
  gomc_hal_float_t maxaccel_rise;	/**< HAL RO parameter: maximum acceleration (position units/s²). */
  gomc_hal_float_t maxaccel_fall;	/**< HAL RO parameter: maximum deceleration (position units/s²). */

  int last_operational;         /**< Non-zero when the slave was operational on the previous cycle. */
  int16_t last_hw_count;	/**< Hardware counter value read on the previous cycle. */
  double old_scale;		/**< Previous value of pos_scale; used to detect changes. */
  double scale_recip;		/**< Reciprocal of pos_scale (1/pos_scale) for fast scaling. */

  unsigned int state_pdo_os;    /**< Byte offset of the status word PDO in the process image. */
  unsigned int count_pdo_os;    /**< Byte offset of the counter value PDO in the process image. */
  unsigned int ctrl_pdo_os;     /**< Byte offset of the control word PDO in the process image. */
  unsigned int freq_pdo_os;     /**< Byte offset of the frequency value PDO in the process image. */

  uint32_t sdo_base_freq;       /**< Base frequency (SDO 0x8001:02) read from the device at init. */
  uint16_t sdo_max_freq;        /**< Maximum allowed output frequency (SDO 0x8800:02). */
  uint16_t sdo_ramp_rise;       /**< Ramp rise value in device units (SDO 0x8001:04). */
  uint16_t sdo_ramp_fall;       /**< Ramp fall value in device units (SDO 0x8001:05). */
  uint8_t sdo_ramp_factor;      /**< Ramp factor selector byte (SDO 0x8000:07); bit 0 selects ×1000 vs ×10. */

  double freqscale;             /**< Scaling factor from Hz to raw frequency word (0x7FFF / base_freq). */
  double freqscale_recip;       /**< Reciprocal of freqscale for converting raw counts back to Hz. */
  double max_freq;              /**< Maximum achievable frequency in Hz, derived from sdo_max_freq. */
  double max_ac_rise;           /**< Maximum rise acceleration in Hz/s, derived from sdo_ramp_rise. */
  double max_ac_fall;           /**< Maximum fall acceleration in Hz/s, derived from sdo_ramp_fall. */

} lcec_el2521_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_S32, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, count), "%s.%s.%s.stp-counts" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, pos_fb), "%s.%s.%s.stp-pos-fb" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, ramp_active), "%s.%s.%s.stp-ramp-active" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el2521_data_t, ramp_disable), "%s.%s.%s.stp-ramp-disable" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, in_z), "%s.%s.%s.stp-in-z" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, in_z_not), "%s.%s.%s.stp-in-z-not" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, in_t), "%s.%s.%s.stp-in-t" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el2521_data_t, in_t_not), "%s.%s.%s.stp-in-t-not" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el2521_data_t, enable), "%s.%s.%s.stp-enable" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IN, offsetof(lcec_el2521_data_t, vel_cmd), "%s.%s.%s.stp-velo-cmd" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_RO, offsetof(lcec_el2521_data_t, freq), "%s.%s.%s.stp-freq" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RO, offsetof(lcec_el2521_data_t, maxvel), "%s.%s.%s.stp-maxvel" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RO, offsetof(lcec_el2521_data_t, maxaccel_fall), "%s.%s.%s.stp-maxaccel-fall" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RO, offsetof(lcec_el2521_data_t, maxaccel_rise), "%s.%s.%s.stp-maxaccel-rise" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RW, offsetof(lcec_el2521_data_t, pos_scale), "%s.%s.%s.stp-pos-scale" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el2521_in[] = {
   {0x6000, 0x01, 16}, // state word
   {0x6000, 0x02, 16}  // counter value
};

static ec_pdo_entry_info_t lcec_el2521_out[] = {
   {0x7000, 0x01, 16}, // control word
   {0x7000, 0x02, 16}  // frequency value
};

static ec_pdo_info_t lcec_el2521_pdos_in[] = {
    {0x1A00, 2, lcec_el2521_in},
};

static ec_pdo_info_t lcec_el2521_pdos_out[] = {
    {0x1600, 2, lcec_el2521_out},
};

static ec_sync_info_t lcec_el2521_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_el2521_pdos_out},
    {3, EC_DIR_INPUT,  1, lcec_el2521_pdos_in},
    {0xff}
};

/** @brief Forward declaration of the scale-change check helper. */
void lcec_el2521_check_scale(lcec_el2521_data_t *hal_data);

/** @brief Forward declaration of the periodic read callback. */
void lcec_el2521_read(struct lcec_slave *slave, long period);
/** @brief Forward declaration of the periodic write callback. */
void lcec_el2521_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialize the EL2521 slave: read SDOs, register PDOs, and export
 *        HAL pins and parameters.
 *
 * Reads device configuration via CoE SDO (base frequency, ramp parameters,
 * maximum frequency), sets up sync manager and PDO mappings, and exports all
 * HAL pins and parameters.  Internal scaling factors are pre-computed here so
 * the real-time callbacks avoid floating-point divisions.
 *
 * @param comp_id        HAL component ID returned by hal_init().
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced
 *                       by the number of entries registered.
 * @return 0 on success, -EIO on SDO read or memory allocation failure, or a
 *         negative HAL error code if pin/parameter export fails.
 */
int lcec_el2521_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_el2521_data_t *hal_data;
  int err;
  double ramp_factor;
  uint8_t sdo_buf[4];

  // initialize callbacks
  slave->proc_read = lcec_el2521_read;
  slave->proc_write = lcec_el2521_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el2521_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el2521_data_t));
  slave->hal_data = hal_data;

  // read sdos
  if (lcec_read_sdo(slave, 0x8001, 0x02, sdo_buf, 4)) {
    return -EIO;
  }
  hal_data->sdo_base_freq = EC_READ_U32(sdo_buf);
  if (lcec_read_sdo(slave, 0x8001, 0x04, sdo_buf, 2)) {
    return -EIO;
  }
  hal_data->sdo_ramp_rise = EC_READ_U16(sdo_buf);
  if (lcec_read_sdo(slave, 0x8001, 0x05, sdo_buf, 2)) {
    return -EIO;
  }
  hal_data->sdo_ramp_fall = EC_READ_U16(sdo_buf);
  if (lcec_read_sdo(slave, 0x8000, 0x07, sdo_buf, 1)) {
    return -EIO;
  }
  hal_data->sdo_ramp_factor = EC_READ_U8(sdo_buf);
  if (lcec_read_sdo(slave, 0x8800, 0x02, sdo_buf, 2)) {
    return -EIO;
  }
  hal_data->sdo_max_freq = EC_READ_U16(sdo_buf);

  // initializer sync info
  slave->sync_info = lcec_el2521_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->state_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x02, &hal_data->count_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->ctrl_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x02, &hal_data->freq_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(env, comp_id, hal_data, slave_params, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // init parameters
  hal_data->pos_scale = 1.0;

  // init other fields
  hal_data->last_operational = 0;
  hal_data->last_hw_count = 0;

  // calculate frequency factor
  if (hal_data->sdo_base_freq != 0) {
    hal_data->freqscale = (double)0x7fff / (double)hal_data->sdo_base_freq;
    hal_data->freqscale_recip = 1 / hal_data->freqscale;
  } else {
    hal_data->freqscale = 0;
    hal_data->freqscale_recip = 0;
  }

  // calculate max frequency
  if (hal_data->sdo_max_freq != 0) {
    hal_data->max_freq = (double)hal_data->sdo_max_freq * hal_data->freqscale_recip;
  } else {
    hal_data->max_freq = (double)(hal_data->sdo_base_freq);
  }

  // calculate maximum acceleartions in Hz/s
  if ((hal_data->sdo_ramp_factor & 0x01) != 0) {
    ramp_factor = 1000;
  } else {
    ramp_factor = 10;
  }
  hal_data->max_ac_rise = ramp_factor * (double)(hal_data->sdo_ramp_rise);
  hal_data->max_ac_fall = ramp_factor * (double)(hal_data->sdo_ramp_fall);

  return 0;
}

/**
 * @brief Validate and update the position-scale reciprocal when pos_scale changes.
 *
 * Detects a change in @c hal_data->pos_scale, guards against near-zero values
 * that would cause division by zero, and updates @c old_scale and @c scale_recip.
 *
 * @param hal_data Pointer to the slave HAL data structure.
 */
void lcec_el2521_check_scale(lcec_el2521_data_t *hal_data) {
  // check for change in scale value
  if (hal_data->pos_scale != hal_data->old_scale) {
    // validate the new scale value
    if ((hal_data->pos_scale < 1e-20) && (hal_data->pos_scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      hal_data->pos_scale = 1.0;
    }
    // get ready to detect future scale changes
    hal_data->old_scale = hal_data->pos_scale;
    // we will need the reciprocal
    hal_data->scale_recip = 1.0 / hal_data->pos_scale;
  }
}

/**
 * @brief Periodic read callback — updates HAL output pins from the EtherCAT PDO.
 *
 * Reads the status word and hardware counter from the process data image,
 * decodes the ramp-active and digital input bits, accumulates the step count,
 * and computes the scaled position feedback.  Skips updates while the slave is
 * not operational.
 *
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el2521_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2521_data_t *hal_data = (lcec_el2521_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int16_t hw_count, hw_count_diff;
  uint16_t state;
  int in;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check for change in scale value
  lcec_el2521_check_scale(hal_data);

  // calculate scaled limits
  hal_data->maxvel = hal_data->max_freq * hal_data->scale_recip;
  hal_data->maxaccel_rise = hal_data->max_ac_rise * hal_data->scale_recip;
  hal_data->maxaccel_fall = hal_data->max_ac_fall * hal_data->scale_recip;

  // read state word
  state = EC_READ_U16(&pd[hal_data->state_pdo_os]);
  *(hal_data->ramp_active) = (state >> 1) & 1;
  in = (state >> 5) & 1;
  *(hal_data->in_z) = in;
  *(hal_data->in_z_not) = !in;
  in = (state >> 4) & 1;
  *(hal_data->in_t) = in;
  *(hal_data->in_t_not) = !in;

  // get counter diff
  hw_count = EC_READ_S16(&pd[hal_data->count_pdo_os]);
  hw_count_diff = hw_count - hal_data->last_hw_count;
  hal_data->last_hw_count = hw_count;
  if (!hal_data->last_operational) {
    hw_count_diff = 0;
  }

  // update raw count
  *(hal_data->count) += hw_count_diff;

  // scale position
  *(hal_data->pos_fb) = (double) (*(hal_data->count)) * hal_data->scale_recip;

  hal_data->last_operational = 1;
}

/**
 * @brief Periodic write callback — writes control word and frequency to the
 *        EtherCAT PDO.
 *
 * Converts the velocity command HAL pin (position units/s) to a raw frequency
 * word using the pre-computed @c freqscale factor, clamps to the device maximum,
 * and writes the result along with the control word (ramp-disable bit) to the
 * process data image.  Outputs zero frequency when @c enable is FALSE.
 *
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el2521_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2521_data_t *hal_data = (lcec_el2521_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t ctrl;
  int32_t freq_raw;

  // check for change in scale value
  lcec_el2521_check_scale(hal_data);

  // write control word
  ctrl = 0;
  if (*(hal_data->ramp_disable)) {
    ctrl |= (1 << 1);
  }
  EC_WRITE_S16(&pd[hal_data->ctrl_pdo_os], ctrl);

  // update frequency
  if (*(hal_data->enable)) {
    hal_data->freq = *(hal_data->vel_cmd) * hal_data->pos_scale;
  } else {
    hal_data->freq = 0;
  }

  // output frequency
  freq_raw = hal_data->freq * hal_data->freqscale;
  if (freq_raw > 0x7fff) {
    freq_raw = 0x7fff;
  }
  if (freq_raw < -0x7fff) {
    freq_raw = -0x7fff;
  }
  EC_WRITE_S16(&pd[hal_data->freq_pdo_os], freq_raw);
}
