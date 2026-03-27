/**
 * @file deasda.c
 * @brief Driver for the Delta ASDA-A2 EtherCAT servo drive.
 *
 * Configures the drive for cyclic synchronous velocity (CSV) mode via SDO
 * writes, then maps CiA-402 PDOs.  Implements automatic fault reset with
 * configurable retry count and inter-reset cycle count.
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
#include "deasda.h"

#include "../classes/class_enc.h"

/** @brief Default encoder pulses per revolution for the ASDA-A2. */
#define DEASDA_PULSES_PER_REV_DEFLT (1280000)
/** @brief RPM feedback scale: drive reports speed in units of 0.1 RPM. */
#define DEASDA_RPM_FACTOR           (0.1)
/** @brief Reciprocal of DEASDA_RPM_FACTOR (used to convert RPM → drive units). */
#define DEASDA_RPM_RCPT             (1.0 / DEASDA_RPM_FACTOR)
/** @brief Multiplier to convert rev/s → RPM. */
#define DEASDA_RPM_MUL              (60.0)
/** @brief Multiplier to convert RPM → rev/s. */
#define DEASDA_RPM_DIV              (1.0 / 60.0)

/** @brief Number of servo cycles to hold fault-reset state during auto-reset. */
#define DEASDA_FAULT_AUTORESET_CYCLES  100
/** @brief Maximum number of automatic fault-reset attempts before giving up. */
#define DEASDA_FAULT_AUTORESET_RETRIES 3

/**
 * @brief HAL data structure for the Delta ASDA-A2 servo drive.
 */
typedef struct {
  gomc_hal_float_t *vel_fb;              /**< HAL OUT: Velocity feedback (scale units/s). */
  gomc_hal_float_t *vel_fb_rpm;          /**< HAL OUT: Velocity feedback (RPM). */
  gomc_hal_float_t *vel_fb_rpm_abs;      /**< HAL OUT: Absolute velocity feedback (RPM). */
  gomc_hal_float_t *vel_rpm;             /**< HAL OUT: Velocity command sent to drive (RPM). */
  gomc_hal_bit_t *ready;                 /**< HAL OUT: CiA-402 ready-to-switch-on. */
  gomc_hal_bit_t *switched_on;          /**< HAL OUT: CiA-402 switched-on. */
  gomc_hal_bit_t *oper_enabled;          /**< HAL OUT: CiA-402 operation-enabled. */
  gomc_hal_bit_t *fault;                 /**< HAL OUT: Gated fault (suppressed during auto-reset). */
  gomc_hal_bit_t *volt_enabled;          /**< HAL OUT: CiA-402 voltage-enabled. */
  gomc_hal_bit_t *quick_stoped;          /**< HAL OUT: CiA-402 quick-stop active (inverted). */
  gomc_hal_bit_t *on_disabled;           /**< HAL OUT: CiA-402 switch-on-disabled. */
  gomc_hal_bit_t *warning;               /**< HAL OUT: CiA-402 warning. */
  gomc_hal_bit_t *remote;                /**< HAL OUT: Drive is remote-controlled. */
  gomc_hal_bit_t *at_speed;              /**< HAL OUT: Motor has reached target velocity. */
  gomc_hal_bit_t *limit_active;          /**< HAL OUT: A torque/speed limit is active. */
  gomc_hal_bit_t *zero_speed;            /**< HAL OUT: Motor is at zero speed. */
  gomc_hal_bit_t *switch_on;             /**< HAL IN:  CiA-402 switch-on command. */
  gomc_hal_bit_t *enable_volt;           /**< HAL IN:  CiA-402 enable-voltage. */
  gomc_hal_bit_t *quick_stop;            /**< HAL IN:  CiA-402 quick-stop (active-low). */
  gomc_hal_bit_t *enable;                /**< HAL IN:  CiA-402 enable-operation. */
  gomc_hal_bit_t *fault_reset;           /**< HAL IN:  CiA-402 fault-reset. */
  gomc_hal_bit_t *halt;                  /**< HAL IN:  CiA-402 halt. */
  gomc_hal_float_t *vel_cmd;             /**< HAL IN:  Velocity command (scale units/s). */

  gomc_hal_float_t pos_scale;            /**< HAL RW param: Position/velocity scale factor. */
  gomc_hal_float_t extenc_scale;         /**< HAL RW param: External encoder scale factor. */
  gomc_hal_u32_t pprev;                  /**< HAL RW param: Encoder pulses per revolution. */
  gomc_hal_u32_t fault_autoreset_cycles; /**< HAL RW param: Cycles per auto-reset phase. */
  gomc_hal_u32_t fault_autoreset_retries;/**< HAL RW param: Auto-reset retry limit. */

  lcec_class_enc_data_t enc;        /**< Main encoder state (class_enc). */
  lcec_class_enc_data_t extenc;     /**< External encoder state (class_enc). */

  gomc_hal_float_t pos_scale_old;        /**< Last seen pos_scale (change detection). */
  double pos_scale_rcpt;            /**< Cached reciprocal of pos_scale. */

  unsigned int status_pdo_os;       /**< PDO offset: CiA-402 status word (0x6041). */
  unsigned int currpos_pdo_os;      /**< PDO offset: actual position (0x6064). */
  unsigned int currvel_pdo_os;      /**< PDO offset: actual velocity (0x606C). */
  unsigned int extenc_pdo_os;       /**< PDO offset: external encoder (0x2511). */
  unsigned int control_pdo_os;      /**< PDO offset: CiA-402 control word (0x6040). */
  unsigned int cmdvel_pdo_os;       /**< PDO offset: target velocity (0x60FF). */

  gomc_hal_bit_t last_switch_on;         /**< Previous value of switch_on (edge detection). */
  gomc_hal_bit_t internal_fault;         /**< Raw fault bit from the drive status word. */

  gomc_hal_u32_t fault_reset_retry;      /**< Remaining auto-reset retries. */
  gomc_hal_u32_t fault_reset_state;      /**< Current phase of the auto-reset state machine. */
  gomc_hal_u32_t fault_reset_cycle;      /**< Cycle counter within the current reset phase. */

} lcec_deasda_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb), "%s.%s.%s.srv-vel-fb" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm), "%s.%s.%s.srv-vel-fb-rpm" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm_abs), "%s.%s.%s.srv-vel-fb-rpm-abs" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, vel_rpm), "%s.%s.%s.srv-vel-rpm" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, ready), "%s.%s.%s.srv-ready" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, switched_on), "%s.%s.%s.srv-switched-on" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, oper_enabled), "%s.%s.%s.srv-oper-enabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, fault), "%s.%s.%s.srv-fault" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, volt_enabled), "%s.%s.%s.srv-volt-enabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, quick_stoped), "%s.%s.%s.srv-quick-stoped" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, on_disabled), "%s.%s.%s.srv-on-disabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, warning), "%s.%s.%s.srv-warning" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, remote), "%s.%s.%s.srv-remote" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, at_speed), "%s.%s.%s.srv-at-speed" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, limit_active), "%s.%s.%s.srv-limit-active" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_deasda_data_t, zero_speed), "%s.%s.%s.srv-zero-speed" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, switch_on), "%s.%s.%s.srv-switch-on" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, enable_volt), "%s.%s.%s.srv-enable-volt" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, quick_stop), "%s.%s.%s.srv-quick-stop" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, enable), "%s.%s.%s.srv-enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, fault_reset), "%s.%s.%s.srv-fault-reset" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, halt), "%s.%s.%s.srv-halt" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IN, offsetof(lcec_deasda_data_t, vel_cmd), "%s.%s.%s.srv-vel-cmd" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_RW, offsetof(lcec_deasda_data_t, pos_scale), "%s.%s.%s.pos-scale" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RW, offsetof(lcec_deasda_data_t, extenc_scale), "%s.%s.%s.extenc-scale" },
  { GOMC_HAL_U32, GOMC_HAL_RW, offsetof(lcec_deasda_data_t, pprev), "%s.%s.%s.srv-pulses-per-rev" },
  { GOMC_HAL_U32, GOMC_HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_cycles), "%s.%s.%s.srv-fault-autoreset-cycles" },
  { GOMC_HAL_U32, GOMC_HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_retries), "%s.%s.%s.srv-fault-autoreset-retries" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_deasda_in[] = {
   {0x6041, 0x00, 16}, // Status Word
   {0x606C, 0x00, 32}, // Current Velocity
   {0x6064, 0x00, 32}, // Current Position
   {0x2511, 0x00, 32}  // external encoder
};

static ec_pdo_entry_info_t lcec_deasda_out[] = {
   {0x6040, 0x00, 16}, // Control Word
   {0x60FF, 0x00, 32}  // Target Velocity
};

static ec_pdo_info_t lcec_deasda_pdos_out[] = {
    {0x1602,  2, lcec_deasda_out}
};

static ec_pdo_info_t lcec_deasda_pdos_in[] = {
    {0x1a02, 4, lcec_deasda_in}
};

static ec_sync_info_t lcec_deasda_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_deasda_pdos_out},
    {3, EC_DIR_INPUT,  1, lcec_deasda_pdos_in},
    {0xff}
};

void lcec_deasda_check_scales(lcec_deasda_data_t *hal_data);

void lcec_deasda_read(struct lcec_slave *slave, long period);
void lcec_deasda_write(struct lcec_slave *slave, long period);

int lcec_deasda_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_deasda_data_t *hal_data;
  int err;
  uint32_t tu;
  int8_t ti;

  // initialize callbacks
  slave->proc_read = lcec_deasda_read;
  slave->proc_write = lcec_deasda_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_deasda_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_deasda_data_t));
  slave->hal_data = hal_data;

  // set to cyclic synchronous velocity mode
  if (ecrt_slave_config_sdo8(slave->config, 0x6060, 0x00, 9) != 0) {
    LCEC_ERR(master, "fail to configure slave %s.%s sdo velo mode", master->name, slave->name);
  }

  // set interpolation time period
  tu = master->app_time_period;
  ti = -9;
  while (tu > 0 && ((tu % 10) == 0 || tu > 255)) { tu /=  10; ti++; }
  if (ecrt_slave_config_sdo8(slave->config, 0x60C2, 0x01, (uint8_t)tu) != 0) {
    LCEC_ERR(master, "fail to configure slave %s.%s sdo ipol time period units", master->name, slave->name);
  }
  if (ecrt_slave_config_sdo8(slave->config, 0x60C2, 0x02, ti) != 0) {
    LCEC_ERR(master, "fail to configure slave %s.%s sdo ipol time period index", master->name, slave->name);
  }

  // initialize sync info
  slave->sync_info = lcec_deasda_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6041, 0x00, &hal_data->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x606C, 0x00, &hal_data->currvel_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6064, 0x00, &hal_data->currpos_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x2511, 0x00, &hal_data->extenc_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6040, 0x00, &hal_data->control_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60FF, 0x00, &hal_data->cmdvel_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(env, comp_id, hal_data, slave_params, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // init subclasses
  if ((err = class_enc_init(slave, &hal_data->enc, 32, "enc")) != 0) {
    return err;
  }
  if ((err = class_enc_init(slave, &hal_data->extenc, 32, "extenc")) != 0) {
    return err;
  }

  // initialize variables
  hal_data->pos_scale = 1.0;
  hal_data->extenc_scale = 1.0;
  hal_data->fault_autoreset_cycles = DEASDA_FAULT_AUTORESET_CYCLES;
  hal_data->fault_autoreset_retries = DEASDA_FAULT_AUTORESET_RETRIES;
  hal_data->pos_scale_old = hal_data->pos_scale + 1.0;
  hal_data->pos_scale_rcpt = 1.0;
  hal_data->pprev = DEASDA_PULSES_PER_REV_DEFLT;
  hal_data->last_switch_on = 0;
  hal_data->internal_fault = 0;

  hal_data->fault_reset_retry = 0;
  hal_data->fault_reset_state = 0;
  hal_data->fault_reset_cycle = 0;

  return 0;
}

void lcec_deasda_check_scales(lcec_deasda_data_t *hal_data) {
  // check for change in scale value
  if (hal_data->pos_scale != hal_data->pos_scale_old) {

    // scale value has changed, test and update it
    if ((hal_data->pos_scale < 1e-20) && (hal_data->pos_scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      hal_data->pos_scale = 1.0;
    }

    // save new scale to detect future changes
    hal_data->pos_scale_old = hal_data->pos_scale;

    // we actually want the reciprocal
    hal_data->pos_scale_rcpt = 1.0 / hal_data->pos_scale;
  }
}

void lcec_deasda_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_deasda_data_t *hal_data = (lcec_deasda_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t status;
  int32_t speed_raw;
  double rpm;
  uint32_t pos_cnt;

  // wait for slave to be operational
  if (!slave->state.operational) {
    *(hal_data->ready)        = 0;
    *(hal_data->switched_on)  = 0;
    *(hal_data->oper_enabled) = 0;
    *(hal_data->fault)        = 1;
    *(hal_data->volt_enabled) = 0;
    *(hal_data->quick_stoped) = 0;
    *(hal_data->on_disabled)  = 0;
    *(hal_data->warning)      = 0;
    *(hal_data->remote)       = 0;
    *(hal_data->at_speed)     = 0;
    *(hal_data->limit_active) = 0;
    *(hal_data->zero_speed)   = 0;
    return;
  }

  // check for change in scale value
  lcec_deasda_check_scales(hal_data);

  // read status word
  status = EC_READ_U16(&pd[hal_data->status_pdo_os]);
  *(hal_data->ready)        = (status >> 0) & 0x01;
  *(hal_data->switched_on)  = (status >> 1) & 0x01;
  *(hal_data->oper_enabled) = (status >> 2) & 0x01;
  hal_data->internal_fault  = (status >> 3) & 0x01;
  *(hal_data->volt_enabled) = (status >> 4) & 0x01;
  *(hal_data->quick_stoped) = !((status >> 5) & 0x01);
  *(hal_data->on_disabled)  = (status >> 6) & 0x01;
  *(hal_data->warning)      = (status >> 7) & 0x01;
  *(hal_data->remote)       = (status >> 9) & 0x01;
  *(hal_data->at_speed)     = (status >> 10) & 0x01;
  *(hal_data->limit_active) = (status >> 11) & 0x01;
  *(hal_data->zero_speed)   = (status >> 12) & 0x01;

  // clear pending fault reset if no fault
  if (!hal_data->internal_fault) {
    hal_data->fault_reset_retry = 0;
  }

  // generate gated fault
  if (hal_data->fault_reset_retry > 0) {
    if (hal_data->fault_reset_cycle < hal_data->fault_autoreset_cycles) {
      hal_data->fault_reset_cycle++;
    } else {
      hal_data->fault_reset_cycle = 0;
      hal_data->fault_reset_state = !hal_data->fault_reset_state;
      if (hal_data->fault_reset_state) {
        hal_data->fault_reset_retry--;
      }
    }
    *(hal_data->fault) = 0;
  } else {
    *(hal_data->fault) = hal_data->internal_fault;
  }

  // read current speed
  speed_raw = EC_READ_S32(&pd[hal_data->currvel_pdo_os]);
  rpm = (double)speed_raw * DEASDA_RPM_FACTOR;
  *(hal_data->vel_fb_rpm) = rpm;
  *(hal_data->vel_fb_rpm_abs) = fabs(rpm);
  *(hal_data->vel_fb) = rpm * DEASDA_RPM_DIV * hal_data->pos_scale;

  // update raw position counter
  pos_cnt = EC_READ_U32(&pd[hal_data->currpos_pdo_os]);
  class_enc_update(&hal_data->enc, hal_data->pprev, hal_data->pos_scale, pos_cnt, 0, 0);

  // update external encoder counter
  pos_cnt = EC_READ_U32(&pd[hal_data->extenc_pdo_os]);
  class_enc_update(&hal_data->extenc, 1, hal_data->extenc_scale, pos_cnt, 0, 0);
}

void lcec_deasda_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_deasda_data_t *hal_data = (lcec_deasda_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t control;
  double speed_raw;
  int switch_on_edge;

  // check for enable edge
  switch_on_edge = *(hal_data->switch_on) && !hal_data->last_switch_on;
  hal_data->last_switch_on = *(hal_data->switch_on);

  // check for autoreset
  if (hal_data->fault_autoreset_retries > 0 && hal_data->fault_autoreset_cycles > 0 && switch_on_edge && hal_data->internal_fault) {
    hal_data->fault_reset_retry = hal_data->fault_autoreset_retries;
    hal_data->fault_reset_state = 1;
    hal_data->fault_reset_cycle = 0;
  }

  // check for change in scale value
  lcec_deasda_check_scales(hal_data);

  // write dev ctrl
  control = 0;
  if (*(hal_data->enable_volt)) {
    control |= (1 << 1);
  }
  if (! *(hal_data->quick_stop)) {
    control |= (1 << 2);
  }
  if (*(hal_data->fault_reset)) {
    control |= (1 << 7);
  }
  if (*(hal_data->halt)) {
    control |= (1 << 8);
  }
  if (hal_data->fault_reset_retry > 0) {
      if (hal_data->fault_reset_state) {
        control |= (1 << 7);
      }
  } else {
    if (*(hal_data->switch_on)) {
      control |= (1 << 0);
    }
    if (*(hal_data->enable) && *(hal_data->switched_on)) {
      control |= (1 << 3);
    }
  }
  EC_WRITE_U16(&pd[hal_data->control_pdo_os], control);

  // calculate rpm command
  *(hal_data->vel_rpm) = *(hal_data->vel_cmd) * hal_data->pos_scale_rcpt * DEASDA_RPM_MUL;

  // set RPM
  speed_raw = *(hal_data->vel_rpm) * DEASDA_RPM_RCPT;
  if (speed_raw > (double)0x7fffffff) {
    speed_raw = (double)0x7fffffff;
  }
  if (speed_raw < (double)-0x7fffffff) {
    speed_raw = (double)-0x7fffffff;
  }
  EC_WRITE_S32(&pd[hal_data->cmdvel_pdo_os], (int32_t)speed_raw);
}
