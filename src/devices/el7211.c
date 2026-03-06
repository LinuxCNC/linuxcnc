//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

/** @file el7211.c
 * @brief Driver for the Beckhoff EL7211/EL7221/EL7201-9014 servo motor terminals.
 *
 * Implements CiA-402 velocity-mode control for Beckhoff BLDC servo terminals.
 * The driver reads the 16-bit status word (0x6010:01), 32-bit actual velocity
 * (0x6010:07), 16-bit actual torque (0x6010:08), and 32-bit encoder position
 * (0x6000:11), then writes a 16-bit CiA-402 control word (0x7010:01) and
 * 32-bit velocity command (0x7010:06) each cycle.
 *
 * The EL7201-9014 variant additionally exposes digital inputs (info1) and
 * an extended error word (info2) via two extra TxPDOs.
 */

#include "hal.h"

#include "../lcec.h"
#include "el7211.h"

#include "../classes/class_enc.h"

/** @brief Period (ns) between automatic fault-reset attempts. */
#define FAULT_RESET_PERIOD_NS  100000000

/**
 * @brief HAL data structure for the EL7211 servo terminal.
 */
typedef struct {
  hal_bit_t *enable;              /**< IN: enable the servo drive */
  hal_bit_t *enabled;             /**< OUT: drive is enabled (ready + switched-on + operation) */
  hal_bit_t *fault;               /**< OUT: drive fault active */

  hal_bit_t *status_ready;        /**< OUT: CiA-402 status word bit 0 – ready to switch on */
  hal_bit_t *status_switched_on;  /**< OUT: CiA-402 status word bit 1 – switched on */
  hal_bit_t *status_operation;    /**< OUT: CiA-402 status word bit 2 – operation enabled */
  hal_bit_t *status_fault;        /**< OUT: CiA-402 status word bit 3 – fault */
  hal_bit_t *status_disabled;     /**< OUT: CiA-402 status word bit 6 – switch on disabled */
  hal_bit_t *status_warning;      /**< OUT: CiA-402 status word bit 7 – warning */
  hal_bit_t *status_limit_active; /**< OUT: CiA-402 status word bit 11 – internal limit active */

  hal_bit_t *err_adc;             /**< OUT: ADC error (EL7201-9014 only) */
  hal_bit_t *err_overcurrent;     /**< OUT: overcurrent error (EL7201-9014 only) */
  hal_bit_t *err_undervoltage;    /**< OUT: under-voltage error (EL7201-9014 only) */
  hal_bit_t *err_overvoltage;     /**< OUT: over-voltage error (EL7201-9014 only) */
  hal_bit_t *err_overtemp;        /**< OUT: over-temperature error (EL7201-9014 only) */
  hal_bit_t *err_i2t_amp;         /**< OUT: I²t amplifier overload (EL7201-9014 only) */
  hal_bit_t *err_i2t_motor;       /**< OUT: I²t motor overload (EL7201-9014 only) */
  hal_bit_t *err_encoder;         /**< OUT: encoder error (EL7201-9014 only) */
  hal_bit_t *err_watchdog;        /**< OUT: watchdog error (EL7201-9014 only) */

  hal_bit_t *input_0;             /**< OUT: digital input 0 state (EL7201-9014 only) */
  hal_bit_t *input_0_not;         /**< OUT: digital input 0 inverted (EL7201-9014 only) */
  hal_bit_t *input_1;             /**< OUT: digital input 1 state (EL7201-9014 only) */
  hal_bit_t *input_1_not;         /**< OUT: digital input 1 inverted (EL7201-9014 only) */
  hal_bit_t *input_sto;           /**< OUT: STO (safe torque off) input state (EL7201-9014 only) */

  hal_bit_t *at_speed;            /**< OUT: velocity within at-speed window */

  hal_float_t *vel_cmd;           /**< IN: commanded velocity in user units/s */
  hal_float_t *vel_fb;            /**< OUT: actual velocity in user units/s */
  hal_float_t *vel_fb_rpm;        /**< OUT: actual velocity in RPM (signed) */
  hal_float_t *vel_fb_rpm_abs;    /**< OUT: actual velocity in RPM (absolute) */
  hal_s32_t *vel_fb_raw;          /**< OUT: raw 32-bit velocity feedback value */

  hal_float_t *torque_fb;         /**< OUT: actual torque in user units */
  hal_s32_t *torque_fb_raw;       /**< OUT: raw 16-bit torque feedback value */

  hal_float_t *vel_cmd_out;       /**< OUT: actual velocity command sent (after clamping) */
  hal_s32_t *vel_cmd_out_raw;     /**< OUT: raw 32-bit velocity command sent to drive */

  hal_float_t scale;              /**< Parameter: velocity scale (user units → counts/s) */
  hal_float_t torque_scale;       /**< Parameter: torque scale (raw counts → user units) */

  hal_u32_t vel_resolution;       /**< Parameter (RO): velocity resolution from SDO 0x9010:14 */
  hal_u32_t pos_resolution;       /**< Parameter (RO): position resolution from SDO 0x9010:15 */

  hal_float_t min_vel;            /**< Parameter: minimum velocity clamp (user units/s) */
  hal_float_t max_vel;            /**< Parameter: maximum velocity clamp (user units/s) */
  hal_float_t max_accel;          /**< Parameter: maximum acceleration (user units/s²) */
  hal_float_t at_speed_window;    /**< Parameter: velocity window for at-speed detection */

  lcec_class_enc_data_t enc;      /**< Encoder sub-class instance */

  unsigned int pos_fb_pdo_os;     /**< PDO byte offset: 32-bit position feedback (0x6000:11) */
  unsigned int status_pdo_os;     /**< PDO byte offset: 16-bit CiA-402 status word (0x6010:01) */
  unsigned int vel_fb_pdo_os;     /**< PDO byte offset: 32-bit velocity feedback (0x6010:07) */
  unsigned int torque_fb_pdo_os;  /**< PDO byte offset: 16-bit torque feedback (0x6010:08) */
  unsigned int ctrl_pdo_os;       /**< PDO byte offset: 16-bit CiA-402 control word (0x7010:01) */
  unsigned int vel_cmd_pdo_os;    /**< PDO byte offset: 32-bit velocity command (0x7010:06) */
  unsigned int info1_pdo_os;      /**< PDO byte offset: info word 1 (EL7201-9014: inputs) */
  unsigned int info2_pdo_os;      /**< PDO byte offset: info word 2 (EL7201-9014: errors) */

  double vel_scale;               /**< Precomputed: vel_resolution as double for output scaling */
  double vel_rcpt;                /**< Precomputed: 1.0 / vel_resolution for input scaling */

  double scale_old;               /**< Previously applied scale value (change detection) */
  double scale_rcpt;              /**< Reciprocal of current scale */
  double vel_out_scale;           /**< Combined scale for velocity output (vel_scale * scale) */

  long fault_reset_timer;         /**< Timer (ns) counting down between fault-reset attempts */

} lcec_el7211_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_el7211_data_t, enable), "%s.%s.%s.enable" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, enabled), "%s.%s.%s.enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, fault), "%s.%s.%s.fault" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_ready), "%s.%s.%s.status-ready" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_switched_on), "%s.%s.%s.status-switched-on" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_operation), "%s.%s.%s.status-operation" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_fault), "%s.%s.%s.status-fault" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_disabled), "%s.%s.%s.status-disabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_warning), "%s.%s.%s.status-warning" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, status_limit_active), "%s.%s.%s.status-limit-active" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_el7211_data_t, vel_cmd), "%s.%s.%s.velo-cmd" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7211_data_t, vel_cmd_out), "%s.%s.%s.velo-cmd-out" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7211_data_t, vel_cmd_out_raw), "%s.%s.%s.velo-cmd-out-raw" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7211_data_t, vel_fb), "%s.%s.%s.velo-fb" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7211_data_t, vel_fb_rpm), "%s.%s.%s.velo-fb-rpm" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7211_data_t, vel_fb_rpm_abs), "%s.%s.%s.velo-fb-rpm-abs" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7211_data_t, vel_fb_raw), "%s.%s.%s.velo-fb-raw" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7211_data_t, torque_fb), "%s.%s.%s.torque-fb" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7211_data_t, torque_fb_raw), "%s.%s.%s.torque-fb-raw" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, at_speed), "%s.%s.%s.at-speed" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_pins_el7201_9014[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, input_0), "%s.%s.%s.input-0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, input_0_not), "%s.%s.%s.input-0-not" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, input_1), "%s.%s.%s.input-1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, input_1_not), "%s.%s.%s.input-1-not" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, input_sto), "%s.%s.%s.input-sto" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_adc), "%s.%s.%s.err-adc" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_overcurrent), "%s.%s.%s.err-overcurrent" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_undervoltage), "%s.%s.%s.err-undervoltage" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_overvoltage), "%s.%s.%s.err-overvoltage" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_overtemp), "%s.%s.%s.err-overtemp" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_i2t_amp), "%s.%s.%s.err-i2t-amp" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_i2t_motor), "%s.%s.%s.err-i2t-motor" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_encoder), "%s.%s.%s.err-encoder" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7211_data_t, err_watchdog), "%s.%s.%s.err-watchdog" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, scale), "%s.%s.%s.scale" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, torque_scale), "%s.%s.%s.torque-scale" },
  { HAL_U32, HAL_RO, offsetof(lcec_el7211_data_t, vel_resolution), "%s.%s.%s.vel-resolution" },
  { HAL_U32, HAL_RO, offsetof(lcec_el7211_data_t, pos_resolution), "%s.%s.%s.pos-resolution" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, min_vel), "%s.%s.%s.min-vel" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, max_vel), "%s.%s.%s.max-vel" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, max_accel), "%s.%s.%s.max-accel" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el7211_data_t, at_speed_window), "%s.%s.%s.at-speed-window" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el7211_in_pos[] = {
   {0x6000, 0x11, 32}  // actual position
};

static ec_pdo_entry_info_t lcec_el7211_in_status[] = {
   {0x6010, 0x01, 16}  // status word
};

static ec_pdo_entry_info_t lcec_el7211_in_info1[] = {
   {0x6010, 0x12, 16}  // info 1
};

static ec_pdo_entry_info_t lcec_el7211_in_info2[] = {
   {0x6010, 0x13, 16}  // info 2
};

static ec_pdo_entry_info_t lcec_el7211_in_vel[] = {
   {0x6010, 0x07, 32}  // actual velocity
};

static ec_pdo_entry_info_t lcec_el7211_in_torque[] = {
   {0x6010, 0x08, 16}  // actual torque
};

static ec_pdo_entry_info_t lcec_el7211_out_ctrl[] = {
   {0x7010, 0x01, 16}  // control word
};

static ec_pdo_entry_info_t lcec_el7211_out_cmd[] = {
   {0x7010, 0x06, 32}  // velocity command
};

static ec_pdo_info_t lcec_el7211_pdos_in[] = {
    {0x1A00, 1, lcec_el7211_in_pos},
    {0x1A01, 1, lcec_el7211_in_status},
    {0x1A02, 1, lcec_el7211_in_vel},
    {0x1A03, 1, lcec_el7211_in_torque},
};

static ec_pdo_info_t lcec_el7201_9014_pdos_in[] = {
    {0x1A00, 1, lcec_el7211_in_pos},
    {0x1A01, 1, lcec_el7211_in_status},
    {0x1A02, 1, lcec_el7211_in_vel},
    {0x1A03, 1, lcec_el7211_in_torque},
    {0x1A04, 1, lcec_el7211_in_info1},
    {0x1A05, 1, lcec_el7211_in_info2},
};

static ec_pdo_info_t lcec_el7211_pdos_out[] = {
    {0x1600, 1, lcec_el7211_out_ctrl},
    {0x1601, 1, lcec_el7211_out_cmd},
};

static ec_sync_info_t lcec_el7211_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_el7211_pdos_out},
    {3, EC_DIR_INPUT,  4, lcec_el7211_pdos_in},
    {0xff}
};

static ec_sync_info_t lcec_el7201_9014_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_el7211_pdos_out},
    {3, EC_DIR_INPUT,  6, lcec_el7201_9014_pdos_in},
    {0xff}
};

lcec_el7211_data_t *lcec_el7211_alloc_hal(lcec_master_t *master, struct lcec_slave *slave);
int lcec_el7211_export_pins(lcec_master_t *master, struct lcec_slave *slave, lcec_el7211_data_t *hal_data);
void lcec_el7211_check_scales(lcec_el7211_data_t *hal_data);
void lcec_el7211_read(struct lcec_slave *slave, long period);
void lcec_el7201_9014_read(struct lcec_slave *slave, long period);
void lcec_el7211_write(struct lcec_slave *slave, long period);

/**
 * @brief Allocate and zero HAL memory for the EL7211 data structure.
 *
 * @param master Pointer to the EtherCAT master structure (used for error messages).
 * @param slave  Pointer to the EtherCAT slave structure.
 * @return Pointer to allocated and zeroed lcec_el7211_data_t, or NULL on failure.
 */
lcec_el7211_data_t *lcec_el7211_alloc_hal(lcec_master_t *master, struct lcec_slave *slave) {
  lcec_el7211_data_t *hal_data;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el7211_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return NULL;
  }
  memset(hal_data, 0, sizeof(lcec_el7211_data_t));

  return hal_data;
}

/**
 * @brief Read velocity/position resolution SDOs and export all HAL pins.
 *
 * Reads the velocity resolution (0x9010:14) and position resolution (0x9010:15)
 * via SDO upload, then exports the standard EL7211 pins and parameters and
 * initialises the encoder sub-class.  Sets default values for scale, velocity
 * limits, and internal precomputed coefficients.
 *
 * @param master   EtherCAT master structure.
 * @param slave    EtherCAT slave structure.
 * @param hal_data Pointer to pre-allocated and zeroed HAL data.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7211_export_pins(lcec_master_t *master, struct lcec_slave *slave, lcec_el7211_data_t *hal_data) {
  int err;
  uint8_t sdo_buf[4];
  uint32_t sdo_vel_resolution;
  uint32_t sdo_pos_resolution;

  // read sdos
  if (lcec_read_sdo(slave, 0x9010, 0x14, sdo_buf, 4)) {
    return -EIO;
  }
  sdo_vel_resolution = EC_READ_U32(sdo_buf);
  if (lcec_read_sdo(slave, 0x9010, 0x15, sdo_buf, 4)) {
    return -EIO;
  }
  sdo_pos_resolution = EC_READ_U32(sdo_buf);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // init subclasses
  if ((err = class_enc_init(slave, &hal_data->enc, 32, "enc")) != 0) {
    return err;
  }

  // init parameters
  hal_data->scale = 1.0;
  hal_data->torque_scale = 1.0;
  hal_data->vel_resolution = sdo_vel_resolution;
  hal_data->pos_resolution = sdo_pos_resolution;

  // initialize variables
  if (sdo_vel_resolution > 0) {
    hal_data->vel_scale = (double) sdo_vel_resolution;
    hal_data->vel_rcpt = 1.0 / ((double) sdo_vel_resolution);
  } else {
    hal_data->vel_scale = 0.0;
    hal_data->vel_rcpt = 0.0;
  }
  hal_data->scale_old = hal_data->scale + 1.0;
  hal_data->scale_rcpt = 0.0;
  hal_data->vel_out_scale = 0.0;
  hal_data->fault_reset_timer = 0;
  hal_data->min_vel = -1e20;
  hal_data->max_vel = 1e20;
  hal_data->max_accel = 1e20;

  return 0;
}

/**
 * @brief Initialise the EL7211/EL7221 servo terminal.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7211_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el7211_data_t *hal_data;
  int err;

  if ((hal_data = lcec_el7211_alloc_hal(master, slave)) == NULL) {
    return -EIO;
  }
  slave->hal_data = hal_data;

  // initialize callbacks
  slave->proc_read = lcec_el7211_read;
  slave->proc_write = lcec_el7211_write;

  // initialize sync info
  slave->sync_info = lcec_el7211_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x11, &hal_data->pos_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x01, &hal_data->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x07, &hal_data->vel_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x08, &hal_data->torque_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01, &hal_data->ctrl_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x06, &hal_data->vel_cmd_pdo_os, NULL);

  // export pins
  if ((err = lcec_el7211_export_pins(master, slave, hal_data)) != 0) {
    return err;
  }

  return 0;
}

/**
 * @brief Initialise the EL7201-9014 servo terminal (extended diagnostics).
 *
 * Configures info1 PDO to expose digital inputs (SDO 0x8010:39 = 10) and
 * info2 PDO to expose the error word (SDO 0x8010:3A = 5), then maps the
 * two additional TxPDOs and exports extra HAL pins for inputs and errors.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7201_9014_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el7211_data_t *hal_data;
  int err;

  if ((hal_data = lcec_el7211_alloc_hal(master, slave)) == NULL) {
    return -EIO;
  }
  slave->hal_data = hal_data;

  // set info1 to inputs
  if (ecrt_slave_config_sdo8(slave->config, 0x8010, 0x39, 10) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo info1 select\n", master->name, slave->name);
    return -1;
  }

  // set info2 to errors
  if (ecrt_slave_config_sdo8(slave->config, 0x8010, 0x3a, 5) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo info2 select\n", master->name, slave->name);
    return -1;
  }

  // initialize callbacks
  slave->proc_read = lcec_el7201_9014_read;
  slave->proc_write = lcec_el7211_write;

  // initialize sync info
  slave->sync_info = lcec_el7201_9014_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x11, &hal_data->pos_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x01, &hal_data->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x07, &hal_data->vel_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x08, &hal_data->torque_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01, &hal_data->ctrl_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x06, &hal_data->vel_cmd_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x12, &hal_data->info1_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x13, &hal_data->info2_pdo_os, NULL);

  // export pins
  if ((err = lcec_el7211_export_pins(master, slave, hal_data)) != 0) {
    return err;
  }

  // export extra pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins_el7201_9014, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // initialize extra variables
  *(hal_data->input_0) = 0;
  *(hal_data->input_1) = 0;
  *(hal_data->input_sto) = 0;

  return 0;
}

/**
 * @brief Detect scale changes and recompute reciprocal and velocity output scale.
 *
 * @param hal_data Pointer to the EL7211 HAL data.
 */
void lcec_el7211_check_scales(lcec_el7211_data_t *hal_data) {
  // check for change in scale value
  if (hal_data->scale != hal_data->scale_old) {
    // scale value has changed, test and update it
    if ((hal_data->scale < 1e-20) && (hal_data->scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      hal_data->scale = 1.0;
    }
    // save new scale to detect future changes
    hal_data->scale_old = hal_data->scale;
    // we actually want the reciprocal
    hal_data->scale_rcpt = 1.0 / hal_data->scale;
    // calculate velo output scale
    hal_data->vel_out_scale = hal_data->vel_scale * hal_data->scale;
  }
}

/**
 * @brief Cyclic read: update EL7211 servo status and feedback HAL pins.
 *
 * Reads the CiA-402 status word, extracts individual status bits, computes
 * the enabled state and fault condition, reads velocity/torque feedback,
 * and updates the encoder sub-class with the current 32-bit position.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds.
 */
void lcec_el7211_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el7211_data_t *hal_data = (lcec_el7211_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t status;
  int32_t vel_raw;
  int16_t torque_raw;
  double vel;
  uint32_t pos_cnt;

  // wait for slave to be operational
  if (!slave->state.operational) {
    *(hal_data->status_ready) = 0;
    *(hal_data->status_switched_on) = 0;
    *(hal_data->status_operation) = 0;
    *(hal_data->status_fault) = 1;
    *(hal_data->status_disabled) = 0;
    *(hal_data->status_warning) = 0;
    *(hal_data->status_limit_active) = 0;
    *(hal_data->enabled) = 0;
    return;
  }

  // check for change in scale value
  lcec_el7211_check_scales(hal_data);

  // read status word
  status = EC_READ_U16(&pd[hal_data->status_pdo_os]);
  *(hal_data->status_ready) = (status >> 0) & 0x01;
  *(hal_data->status_switched_on) = (status >> 1) & 0x01;
  *(hal_data->status_operation) = (status >> 2) & 0x01;
  *(hal_data->status_fault) = (status >> 3) & 0x01;
  *(hal_data->status_disabled) = (status >> 6) & 0x01;
  *(hal_data->status_warning) = (status >> 7) & 0x01;
  *(hal_data->status_limit_active) = (status >> 11) & 0x01;

  *(hal_data->enabled) = *(hal_data->status_ready) && *(hal_data->status_switched_on) && *(hal_data->status_operation);
  *(hal_data->fault) = 0;
  if (*(hal_data->enable) && *(hal_data->status_fault)) {
    if (hal_data->fault_reset_timer > 0) {
      hal_data->fault_reset_timer -= period;
    } else {
      *(hal_data->fault) = 1;
    }
  } else {
    hal_data->fault_reset_timer = FAULT_RESET_PERIOD_NS;
  }

  // read velocity
  vel_raw = EC_READ_S32(&pd[hal_data->vel_fb_pdo_os]);
  *(hal_data->vel_fb_raw) = vel_raw;
  vel = ((double) vel_raw) * hal_data->vel_rcpt;
  *(hal_data->vel_fb) = vel * hal_data->scale_rcpt;
  vel = vel * 60.0;
  *(hal_data->vel_fb_rpm) = vel;
  *(hal_data->vel_fb_rpm_abs) = fabs(vel);

  // update at-speed
  *(hal_data->at_speed) =
    *(hal_data->vel_fb) >= (*(hal_data->vel_cmd) - hal_data->at_speed_window) &&
    *(hal_data->vel_fb) <= (*(hal_data->vel_cmd) + hal_data->at_speed_window);

  // read torque
  torque_raw = EC_READ_S16(&pd[hal_data->torque_fb_pdo_os]);
  *(hal_data->torque_fb_raw) = torque_raw;
  *(hal_data->torque_fb) = (double) torque_raw * 0.001 * hal_data->torque_scale;

  // update position feedback
  pos_cnt = EC_READ_U32(&pd[hal_data->pos_fb_pdo_os]);
  class_enc_update(&hal_data->enc, hal_data->pos_resolution, hal_data->scale_rcpt, pos_cnt, 0, 0);
}

/**
 * @brief Cyclic read for EL7201-9014: standard read plus digital inputs and error word.
 *
 * Calls the base lcec_el7211_read(), then additionally reads the info1 word
 * (digital inputs / STO) and info2 word (error flags) from their PDO offsets.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds.
 */
void lcec_el7201_9014_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el7211_data_t *hal_data = (lcec_el7211_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t info1, info2;

  lcec_el7211_read(slave, period);

  // read info1
  info1 = EC_READ_U16(&pd[hal_data->info1_pdo_os]);
  *(hal_data->input_0) = (info1 >> 0) & 0x01;
  *(hal_data->input_0_not) = !*(hal_data->input_0);
  *(hal_data->input_1) = (info1 >> 1) & 0x01;
  *(hal_data->input_1_not) = !*(hal_data->input_1);
  *(hal_data->input_sto) = (info1 >> 8) & 0x01;

  // read info2
  info2 = EC_READ_U16(&pd[hal_data->info2_pdo_os]);
  *(hal_data->err_adc) = (info2 >> 0) & 0x01;
  *(hal_data->err_overcurrent) = (info2 >> 1) & 0x01;
  *(hal_data->err_undervoltage) = (info2 >> 2) & 0x01;
  *(hal_data->err_overvoltage) = (info2 >> 3) & 0x01;
  *(hal_data->err_overtemp) = (info2 >> 4) & 0x01;
  *(hal_data->err_i2t_amp) = (info2 >> 5) & 0x01;
  *(hal_data->err_i2t_motor) = (info2 >> 6) & 0x01;
  *(hal_data->err_encoder) = (info2 >> 7) & 0x01;
  *(hal_data->err_watchdog) = (info2 >> 8) & 0x01;
}

static inline double clamp(double v, double sub, double sup) {
  if (v < sub) return sub;
  if (v > sup) return sup;
  return v;
}

/**
 * @brief Cyclic write: build CiA-402 control word and send velocity command.
 *
 * Applies acceleration limiting to the commanded velocity, builds the CiA-402
 * control word based on the current drive state machine state, and writes
 * both the control word and the scaled 32-bit velocity command to the
 * EtherCAT process data image.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds.
 */
void lcec_el7211_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el7211_data_t *hal_data = (lcec_el7211_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t control;
  double velo_cmd, velo_raw, velo_maxdelta;

  // check for change in scale value
  lcec_el7211_check_scales(hal_data);

  velo_cmd = 0.0;
  if (*(hal_data->enable)) {
    velo_cmd = clamp(*(hal_data->vel_cmd), hal_data->min_vel, hal_data->max_vel);
  }
  velo_maxdelta = hal_data->max_accel * (double) period * 1e-9;
  *(hal_data->vel_cmd_out) = clamp(velo_cmd, *(hal_data->vel_cmd_out) - velo_maxdelta, *(hal_data->vel_cmd_out) + velo_maxdelta);

  control = 0;
  if (*(hal_data->enable) || *(hal_data->vel_cmd_out) != 0) {
    if (*(hal_data->status_fault)) {
      control = 0x80;
    } else if (*(hal_data->status_disabled)) {
      control = 0x06;
    } else if (*(hal_data->status_ready)) {
      control = 0x07;
      if (*(hal_data->status_switched_on)) {
        control = 0x0f;
      }
    }
  }
  EC_WRITE_U16(&pd[hal_data->ctrl_pdo_os], control);

  // set velocity
  velo_raw = *(hal_data->vel_cmd_out) * hal_data->vel_out_scale;
  if (velo_raw > (double)0x7fffffff) {
    velo_raw = (double)0x7fffffff;
  }
  if (velo_raw < (double)-0x7fffffff) {
    velo_raw = (double)-0x7fffffff;
  }
  *(hal_data->vel_cmd_out_raw) = (int32_t) velo_raw;
  EC_WRITE_S32(&pd[hal_data->vel_cmd_pdo_os], *(hal_data->vel_cmd_out_raw));

}

