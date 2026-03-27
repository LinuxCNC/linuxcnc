/**
 * @file dems300.c
 * @brief Delta MS300 Variable Frequency Drive (VFD) EtherCAT HAL driver implementation.
 *
 * This driver implements the LinuxCNC HAL interface for the Delta MS300 VFD
 * over EtherCAT. It supports velocity control, fault handling with optional
 * automatic reset, ramp time configuration, and status/diagnostic feedback.
 *
 * @copyright Copyright (C) 2021-2026 Dominik Braun <dominik.braun@eventor.de>
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
#include "dems300.h"

/** @brief Delay in nanoseconds before an automatic fault reset is attempted. */
#define MS300_FAULT_AUTORESET_DELAY_NS 100000000LL
/** @brief CiA 402 velocity mode operation code. */
#define OPMODE_VELOCITY 2

/**
 * @brief Per-slave HAL data structure for the Delta MS300 VFD driver.
 *
 * Holds all HAL pin/parameter pointers and internal state for one MS300 slave.
 */
typedef struct {
  gomc_hal_float_t *vel_fb_rpm;              /**< Velocity feedback in RPM. */
  gomc_hal_float_t *vel_fb_rpm_abs;          /**< Absolute value of velocity feedback in RPM. */
  gomc_hal_float_t *vel_rpm_cmd;             /**< Velocity command in RPM. */

  gomc_hal_bit_t *stat_switch_on_ready;      /**< Status: drive ready to switch on. */
  gomc_hal_bit_t *stat_switched_on;          /**< Status: drive switched on. */
  gomc_hal_bit_t *stat_op_enabled;           /**< Status: operation enabled. */
  gomc_hal_bit_t *stat_fault;                /**< Status: drive fault active. */
  gomc_hal_bit_t *stat_volt_enabled;         /**< Status: DC bus voltage enabled. */
  gomc_hal_bit_t *stat_quick_stoped;         /**< Status: quick stop active. */
  gomc_hal_bit_t *stat_switch_on_disabled;   /**< Status: switch-on disabled. */
  gomc_hal_bit_t *stat_warning;              /**< Status: warning present. */
  gomc_hal_bit_t *stat_remote;               /**< Status: drive in remote control mode. */
  gomc_hal_bit_t *stat_at_speed;             /**< Status: drive at commanded speed. */

  gomc_hal_bit_t *quick_stop;                /**< Input: assert quick stop. */
  gomc_hal_bit_t *enable;                    /**< Input: enable drive operation. */
  gomc_hal_bit_t *fault_reset;               /**< Input: reset active fault. */
  gomc_hal_bit_t *halt;                      /**< Input: halt drive. */

  gomc_hal_s32_t *mode_op_display;           /**< Modes of operation display (read from drive). */

  gomc_hal_float_t *act_current;             /**< Actual output current in amps. */
  gomc_hal_u32_t *warn_code;                 /**< Warning code from drive. */
  gomc_hal_u32_t *error_code;                /**< Error/fault code from drive. */
  gomc_hal_float_t *drive_temp;              /**< IGBT temperature in degrees C. */

  gomc_hal_u32_t *vel_ramp_up;               /**< Acceleration ramp time in units of 100 ms. */
  gomc_hal_u32_t *vel_ramp_down;             /**< Deceleration ramp time in units of 100 ms. */

  gomc_hal_bit_t auto_fault_reset;           /**< Parameter: enable automatic fault reset on enable edge. */
  gomc_hal_float_t vel_scale;                /**< Parameter: velocity scaling factor (RPM per raw unit). */

  double vel_scale_old;                 /**< Previous velocity scale value for change detection. */
  double vel_scale_rcpt;                /**< Reciprocal of vel_scale for efficient scaling. */

  unsigned int status_pdo_os;           /**< PDO offset for status word (0x6041). */
  unsigned int currvel_pdo_os;          /**< PDO offset for velocity demand (0x6043). */
  unsigned int mode_op_display_pdo_os;  /**< PDO offset for modes of operation display (0x6061). */

  unsigned int control_pdo_os;          /**< PDO offset for control word (0x6040). */
  unsigned int cmdvel_pdo_os;           /**< PDO offset for target velocity (0x6042). */
  unsigned int mode_op_pdo_os;          /**< PDO offset for modes of operation (0x6060). */

  unsigned int current_pdo_os;          /**< PDO offset for output current (0x3021:05). */
  unsigned int warn_err_pdo_os;         /**< PDO offset for warn/error code (0x3021:01). */
  unsigned int temp_pdo_os;             /**< PDO offset for IGBT temperature (0x3022:0f). */

  unsigned int ramp_up_pdo_os;          /**< PDO offset for ramp up time (0x604F). */
  unsigned int ramp_down_pdo_os;        /**< PDO offset for ramp down time (0x6050). */

  gomc_hal_bit_t enable_old;                 /**< Previous enable state for edge detection. */
  gomc_hal_bit_t internal_fault;             /**< Internal fault flag derived from status word. */

  long long auto_fault_reset_delay;     /**< Remaining nanoseconds of auto-reset delay. */

} lcec_dems300_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, vel_fb_rpm), "%s.%s.%s.vel-fb-rpm" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, vel_fb_rpm_abs), "%s.%s.%s.vel-fb-rpm-abs" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_switch_on_ready), "%s.%s.%s.stat-switch-on-ready" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_switched_on), "%s.%s.%s.stat-switched-on" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_op_enabled), "%s.%s.%s.stat-op-enabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_fault), "%s.%s.%s.stat-fault" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_volt_enabled), "%s.%s.%s.stat-volt-enabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_quick_stoped), "%s.%s.%s.stat-quick-stoped" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_switch_on_disabled), "%s.%s.%s.stat-switch-on-disabled" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_warning), "%s.%s.%s.stat-warning" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_remote), "%s.%s.%s.stat-remote" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, stat_at_speed), "%s.%s.%s.stat-at-speed" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, act_current), "%s.%s.%s.act-current" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, warn_code), "%s.%s.%s.warn-code" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, error_code), "%s.%s.%s.error-code" },
  { GOMC_HAL_FLOAT, GOMC_HAL_OUT, offsetof(lcec_dems300_data_t, drive_temp), "%s.%s.%s.drive-temp" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, quick_stop), "%s.%s.%s.quick-stop" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, enable), "%s.%s.%s.enable" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, fault_reset), "%s.%s.%s.fault-reset" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, halt), "%s.%s.%s.halt" },
  { GOMC_HAL_FLOAT, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, vel_rpm_cmd), "%s.%s.%s.vel-rpm-cmd" },
  { GOMC_HAL_U32, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, vel_ramp_up), "%s.%s.%s.vel-ramp-up" },
  { GOMC_HAL_U32, GOMC_HAL_IN, offsetof(lcec_dems300_data_t, vel_ramp_down), "%s.%s.%s.vel-ramp-down" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { GOMC_HAL_BIT, GOMC_HAL_RW, offsetof(lcec_dems300_data_t, auto_fault_reset), "%s.%s.%s.auto-fault-reset" },
  { GOMC_HAL_FLOAT, GOMC_HAL_RW, offsetof(lcec_dems300_data_t, vel_scale), "%s.%s.%s.vel-scale" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_dems300_in[] = {
  {0x6041, 0x00, 16}, // Status Word
  {0x6043, 0x00, 16}, // Velocity Demand
  {0x6061, 0x00,  8}, // Mode of Operation Display
  {0x0000, 0x00,  8}  // Gap
};

static ec_pdo_entry_info_t lcec_dems300_in2[] = {
  {0x3021, 0x05, 16}, // Output Current
  {0x3021, 0x01, 16}, // Warn HB Error LB Code
  {0x3022, 0x0f, 16}  // IGBT Temp
};

static ec_pdo_entry_info_t lcec_dems300_out[] = {
  {0x6040, 0x00, 16}, // Control Word
  {0x6042, 0x00, 16}, // Target Velocity
  {0x6060, 0x00,  8}, // Modes of Operation
  {0x0000, 0x00,  8}  // Gap
};

static ec_pdo_entry_info_t lcec_dems300_out2[] = {
  {0x6050, 0x00, 32}, // ramp down time 100ms
  {0x604f, 0x00, 32} // ramp up time 100ms
};
static ec_pdo_info_t lcec_dems300_pdos_out[] = {
   {0x1600,  4, lcec_dems300_out},
   {0x1601,  2, lcec_dems300_out2}
};

static ec_pdo_info_t lcec_dems300_pdos_in[] = {
    {0x1a00, 4, lcec_dems300_in},
    {0x1a01, 3, lcec_dems300_in2}
};

static ec_sync_info_t lcec_dems300_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_dems300_pdos_out},
    {3, EC_DIR_INPUT,  2, lcec_dems300_pdos_in},
    {0xff}
};

void lcec_dems300_check_scales(lcec_dems300_data_t *hal_data);

void lcec_dems300_read(struct lcec_slave *slave, long period);
void lcec_dems300_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialize the Delta MS300 EtherCAT slave driver.
 *
 * Allocates HAL memory, registers PDO entries, exports HAL pins and parameters,
 * and sets default initial values for the slave.
 *
 * @param comp_id   LinuxCNC HAL component ID.
 * @param slave     Pointer to the lcec_slave structure for this device.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced by LCEC_PDO_INIT.
 * @return 0 on success, negative error code on failure.
 */
int lcec_dems300_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_dems300_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_dems300_read;
  slave->proc_write = lcec_dems300_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_dems300_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_dems300_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_dems300_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6041, 0x00, &hal_data->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6043, 0x00, &hal_data->currvel_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6061, 0x00, &hal_data->mode_op_display_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x3021, 0x05, &hal_data->current_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x3021, 0x01, &hal_data->warn_err_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x3022, 0x0f, &hal_data->temp_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6040, 0x00, &hal_data->control_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6042, 0x00, &hal_data->cmdvel_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6060, 0x00, &hal_data->mode_op_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6050, 0x00, &hal_data->ramp_down_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x604f, 0x00, &hal_data->ramp_up_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(env, comp_id, hal_data, slave_params, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // initialize variables
  hal_data->enable_old = 0;
  hal_data->internal_fault = 0;

  hal_data->auto_fault_reset = 1;
  hal_data->vel_scale = 1.0;
  hal_data->vel_scale_old = hal_data->vel_scale + 1.0;
  hal_data->vel_scale_rcpt = 1.0;

  return 0;
}

/**
 * @brief Check and update the velocity scale reciprocal when the scale changes.
 *
 * Detects changes to @p hal_data->vel_scale, guards against near-zero values,
 * and recomputes the reciprocal used for velocity unit conversion.
 *
 * @param hal_data  Pointer to the driver's HAL data structure.
 */
void lcec_dems300_check_scales(lcec_dems300_data_t *hal_data) {
  // check for change in scale value
  if (hal_data->vel_scale != hal_data->vel_scale_old) {
    // scale value has changed, test and update it
    if ((hal_data->vel_scale < 1e-20) && (hal_data->vel_scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      hal_data->vel_scale = 1.0;
    }
    // save new scale to detect future changes
    hal_data->vel_scale_old = hal_data->vel_scale;
    // we actually want the reciprocal
    hal_data->vel_scale_rcpt = 1.0 / hal_data->vel_scale;
  }
}

/**
 * @brief EtherCAT cyclic read handler for the Delta MS300 VFD.
 *
 * Reads process data from the EtherCAT domain, updates all HAL output pins
 * (status bits, velocity feedback, current, temperature, fault/warning codes),
 * and manages the auto fault reset countdown timer.
 *
 * @param slave   Pointer to the lcec_slave structure.
 * @param period  Servo thread period in nanoseconds.
 */
void lcec_dems300_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_dems300_data_t *hal_data = (lcec_dems300_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t status,error;
  int8_t opmode_in;
  int32_t speed_raw;
  double rpm;

  // check for change in scale value
  lcec_dems300_check_scales(hal_data);

  // read current
  *(hal_data->act_current) = (double) EC_READ_U16(&pd[hal_data->current_pdo_os]) / 100.0;
  // read temp
  *(hal_data->drive_temp) = (double) EC_READ_U16(&pd[hal_data->temp_pdo_os]) / 10.0;
  // read warn and error code
  error = EC_READ_U16(&pd[hal_data->warn_err_pdo_os]);
  *(hal_data->error_code) = error & 0xff; //low byte
  *(hal_data->warn_code) = error >> 8; //high byte

  // wait for slave to be operational
  if (!slave->state.operational) {
    *(hal_data->stat_switch_on_ready)    = 0;
    *(hal_data->stat_switched_on)        = 0;
    *(hal_data->stat_op_enabled)         = 0;
    *(hal_data->stat_fault)              = 1;
    *(hal_data->stat_volt_enabled)       = 0;
    *(hal_data->stat_quick_stoped)       = 0;
    *(hal_data->stat_switch_on_disabled) = 0;
    *(hal_data->stat_warning)            = 0;
    *(hal_data->stat_remote)             = 0;
    *(hal_data->stat_at_speed)           = 0;
    return;
  }

  // read Modes of Operation
  opmode_in = EC_READ_S8(&pd[hal_data->mode_op_display_pdo_os]);

  // read status word
  status = EC_READ_U16(&pd[hal_data->status_pdo_os]);
  *(hal_data->stat_switch_on_ready)    = (status >> 0) & 1;
  *(hal_data->stat_switched_on)        = (status >> 1) & 1;
  *(hal_data->stat_op_enabled)         = (status >> 2) & 1;
//  *(hal_data->stat_fault)             = (status >> 3) & 1;
  hal_data->internal_fault             = (status >> 3) & 0x01;
  *(hal_data->stat_volt_enabled)       = (status >> 4) & 1;
  *(hal_data->stat_quick_stoped)       = (status >> 5) & 1;
  *(hal_data->stat_switch_on_disabled) = (status >> 6) & 1;
  *(hal_data->stat_warning)            = (status >> 7) & 1;
  *(hal_data->stat_remote)             = (status >> 9) & 1;
  *(hal_data->stat_at_speed)           = (status >> 10) & 0x01;

  // set fault if op mode is wrong
  if (opmode_in != 2) {
    hal_data->internal_fault  = 1;
    LCEC_ERR(master, "MS300 slave %s.%s not sending velo mode", master->name, slave->name);
  }

  // update fault output
  if (hal_data->auto_fault_reset_delay > 0) {
    hal_data->auto_fault_reset_delay -= period;
    *(hal_data->stat_fault) = 0;
  } else {
    *(hal_data->stat_fault) = hal_data->internal_fault;
  }

  // read current speed
  speed_raw = EC_READ_S16(&pd[hal_data->currvel_pdo_os]);
  rpm = (double)speed_raw * hal_data->vel_scale_rcpt;
  *(hal_data->vel_fb_rpm) = rpm;
  *(hal_data->vel_fb_rpm_abs) = fabs(rpm);

}

/**
 * @brief EtherCAT cyclic write handler for the Delta MS300 VFD.
 *
 * Builds and writes the CiA 402 control word, velocity command, and ramp
 * times to the EtherCAT process data image. Also handles fault reset logic
 * including the automatic reset on rising edge of the enable pin.
 *
 * @param slave   Pointer to the lcec_slave structure.
 * @param period  Servo thread period in nanoseconds.
 */
void lcec_dems300_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_dems300_data_t *hal_data = (lcec_dems300_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t control;
  double speed_raw;
  int8_t opmode;
  int enable_edge;

  // check for change in scale value
  lcec_dems300_check_scales(hal_data);

  //set drive OP mode
  opmode = OPMODE_VELOCITY;
  EC_WRITE_S8(&pd[hal_data->mode_op_pdo_os], (int8_t)opmode);

  // check for enable edge
  enable_edge = *(hal_data->enable) && !hal_data->enable_old;
  hal_data->enable_old = *(hal_data->enable);

  // write control register
  control = (!*(hal_data->fault_reset) << 2); // quick stop

  if (*(hal_data->stat_fault)) {
    if (*(hal_data->fault_reset)) {
      control |= (1 << 7); // fault reset
    }
    if (hal_data->auto_fault_reset && enable_edge) {
      hal_data->auto_fault_reset_delay = MS300_FAULT_AUTORESET_DELAY_NS;
      control |= (1 << 7); // fault reset
    }
  } else {
    if (*(hal_data->enable)) {
      control |= (1 << 1); // enable voltage
      if (*(hal_data->stat_switch_on_ready)) {
        control |= (1 << 0); // switch on
        if (*(hal_data->stat_switched_on)) {
          control |= (1 << 3); // enable op
        }
      }
    }
    //set velo control bits
    if (*(hal_data->stat_op_enabled)) {
      control |= (1 << 4); // rfg enable
      control |= (1 << 5); // rfg unlock
      control |= (1 << 6); // rfg use ref
    }
  }

  //halt
  control |= (*(hal_data->halt) << 8); // halt

  EC_WRITE_U16(&pd[hal_data->control_pdo_os], control);

  //write ramp times
  EC_WRITE_U32(&pd[hal_data->ramp_up_pdo_os], *(hal_data->vel_ramp_up));
  EC_WRITE_U32(&pd[hal_data->ramp_down_pdo_os], *(hal_data->vel_ramp_down));

  // set RPM
  speed_raw = *(hal_data->vel_rpm_cmd) * hal_data->vel_scale;
  if (speed_raw > (double)0x7fff) {
    speed_raw = (double)0x7fff;
  }
  if (speed_raw < (double)-0x7fff) {
    speed_raw = (double)-0x7fff;
  }
  EC_WRITE_S16(&pd[hal_data->cmdvel_pdo_os], (int16_t)speed_raw);
}
