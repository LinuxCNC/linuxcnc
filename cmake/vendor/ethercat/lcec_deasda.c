//
//    Copyright (C) 2014 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include "lcec.h"
#include "lcec_deasda.h"

#include "lcec_class_enc.h"

#define DEASDA_PULSES_PER_REV_DEFLT (1280000)
#define DEASDA_RPM_FACTOR           (0.1)
#define DEASDA_RPM_RCPT             (1.0 / DEASDA_RPM_FACTOR)
#define DEASDA_RPM_MUL              (60.0)
#define DEASDA_RPM_DIV              (1.0 / 60.0)

#define DEASDA_FAULT_AUTORESET_CYCLES  100
#define DEASDA_FAULT_AUTORESET_RETRIES 3

typedef struct {
  hal_float_t *vel_fb;
  hal_float_t *vel_fb_rpm;
  hal_float_t *vel_fb_rpm_abs;
  hal_float_t *vel_rpm;
  hal_bit_t *ready;
  hal_bit_t *switched_on;
  hal_bit_t *oper_enabled;
  hal_bit_t *fault;
  hal_bit_t *volt_enabled;
  hal_bit_t *quick_stoped;
  hal_bit_t *on_disabled;
  hal_bit_t *warning;
  hal_bit_t *remote;
  hal_bit_t *at_speed;
  hal_bit_t *limit_active;
  hal_bit_t *zero_speed;
  hal_bit_t *switch_on;
  hal_bit_t *enable_volt;
  hal_bit_t *quick_stop;
  hal_bit_t *enable;
  hal_bit_t *fault_reset;
  hal_bit_t *halt;
  hal_float_t *vel_cmd;

  hal_float_t pos_scale;
  hal_float_t extenc_scale;
  hal_u32_t pprev;
  hal_u32_t fault_autoreset_cycles;
  hal_u32_t fault_autoreset_retries;

  lcec_class_enc_data_t enc;
  lcec_class_enc_data_t extenc;

  hal_float_t pos_scale_old;
  double pos_scale_rcpt;

  unsigned int status_pdo_os;
  unsigned int currpos_pdo_os;
  unsigned int currvel_pdo_os;
  unsigned int extenc_pdo_os;
  unsigned int control_pdo_os;
  unsigned int cmdvel_pdo_os;

  hal_bit_t last_switch_on;
  hal_bit_t internal_fault;

  hal_u32_t fault_reset_retry;
  hal_u32_t fault_reset_state;
  hal_u32_t fault_reset_cycle;

} lcec_deasda_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb), "%s.%s.%s.srv-vel-fb" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm), "%s.%s.%s.srv-vel-fb-rpm" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm_abs), "%s.%s.%s.srv-vel-fb-rpm-abs" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_rpm), "%s.%s.%s.srv-vel-rpm" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, ready), "%s.%s.%s.srv-ready" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, switched_on), "%s.%s.%s.srv-switched-on" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, oper_enabled), "%s.%s.%s.srv-oper-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, fault), "%s.%s.%s.srv-fault" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, volt_enabled), "%s.%s.%s.srv-volt-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, quick_stoped), "%s.%s.%s.srv-quick-stoped" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, on_disabled), "%s.%s.%s.srv-on-disabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, warning), "%s.%s.%s.srv-warning" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, remote), "%s.%s.%s.srv-remote" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, at_speed), "%s.%s.%s.srv-at-speed" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, limit_active), "%s.%s.%s.srv-limit-active" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, zero_speed), "%s.%s.%s.srv-zero-speed" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, switch_on), "%s.%s.%s.srv-switch-on" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, enable_volt), "%s.%s.%s.srv-enable-volt" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, quick_stop), "%s.%s.%s.srv-quick-stop" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, enable), "%s.%s.%s.srv-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, fault_reset), "%s.%s.%s.srv-fault-reset" },
  { HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, halt), "%s.%s.%s.srv-halt" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_deasda_data_t, vel_cmd), "%s.%s.%s.srv-vel-cmd" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_deasda_data_t, pos_scale), "%s.%s.%s.pos-scale" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_deasda_data_t, extenc_scale), "%s.%s.%s.extenc-scale" },
  { HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, pprev), "%s.%s.%s.srv-pulses-per-rev" },
  { HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_cycles), "%s.%s.%s.srv-fault-autoreset-cycles" },
  { HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_retries), "%s.%s.%s.srv-fault-autoreset-retries" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
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

int lcec_deasda_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_deasda_data_t *hal_data;
  int err;
  uint32_t tu;
  int8_t ti;

  // initialize callbacks
  slave->proc_read = lcec_deasda_read;
  slave->proc_write = lcec_deasda_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_deasda_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_deasda_data_t));
  slave->hal_data = hal_data;

  // set to cyclic synchronous velocity mode
  if (ecrt_slave_config_sdo8(slave->config, 0x6060, 0x00, 9) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo velo mode\n", master->name, slave->name);
  }

  // set interpolation time period
  tu = master->app_time_period;
  ti = -9;
  while ((tu % 10) == 0 || tu > 255) { tu /=  10; ti++; }
  if (ecrt_slave_config_sdo8(slave->config, 0x60C2, 0x01, (uint8_t)tu) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ipol time period units\n", master->name, slave->name);
  }
  if (ecrt_slave_config_sdo8(slave->config, 0x60C2, 0x02, ti) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ipol time period index\n", master->name, slave->name);
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

