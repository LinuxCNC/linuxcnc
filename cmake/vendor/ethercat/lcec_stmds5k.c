//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_stmds5k.h"

#include "lcec_class_enc.h"

#define STMDS5K_PCT_REG_FACTOR (0.5 * (double)0x7fff)
#define STMDS5K_PCT_REG_DIV    (2.0 / (double)0x7fff)
#define STMDS5K_TORQUE_DIV     (8.0 / (double)0x7fff)
#define STMDS5K_TORQUE_REF_DIV (0.01)
#define STMDS5K_RPM_FACTOR     (60.0)
#define STMDS5K_RPM_DIV        (1.0 / 60.0)
#define STMDS5K_PPREV          0x1000000

typedef struct {
  struct {
    ec_pdo_entry_info_t ctrl;
    ec_pdo_entry_info_t n_cmd;
    ec_pdo_entry_info_t m_max;
  } out_ch1;
  struct {
    ec_pdo_entry_info_t status;
    ec_pdo_entry_info_t n_fb;
    ec_pdo_entry_info_t m_fb;
    ec_pdo_entry_info_t cmd_status;
  } in_ch1;
  struct {
    ec_pdo_entry_info_t rotor;
    ec_pdo_entry_info_t extenc;
  } in_ch2;
  struct {
    ec_pdo_info_t ch1;
  } out;
  struct {
    ec_pdo_info_t ch1;
    ec_pdo_info_t ch2;
  } in;
  struct {
    ec_sync_info_t mb_out;
    ec_sync_info_t mb_in;
    ec_sync_info_t pdo_out;
    ec_sync_info_t pdo_in;
    ec_sync_info_t eot;
  } syncs;
} lcec_stmds5k_syncs_t;

typedef struct {
  uint16_t pdo_index;
  uint64_t pprev;
  int used_bits;
  int shift_bits;
} lcec_stmds5k_extenc_conf_t;

typedef struct {
  hal_float_t *vel_cmd;
  hal_float_t *vel_fb;
  hal_float_t *vel_fb_rpm;
  hal_float_t *vel_fb_rpm_abs;
  hal_float_t *vel_rpm;
  hal_float_t *torque_fb;
  hal_float_t *torque_fb_abs;
  hal_float_t *torque_fb_pct;
  hal_float_t *torque_lim;
  hal_bit_t *stopped;
  hal_bit_t *at_speed;
  hal_bit_t *overload;
  hal_bit_t *ready;
  hal_bit_t *error;
  hal_bit_t *toggle;
  hal_bit_t *loc_ena;
  hal_bit_t *enable;
  hal_bit_t *err_reset;
  hal_bit_t *fast_ramp;
  hal_bit_t *brake;

  hal_float_t speed_max_rpm;
  hal_float_t speed_max_rpm_sp;
  hal_float_t torque_reference;
  hal_float_t pos_scale;
  hal_float_t extenc_scale;
  double speed_max_rpm_sp_rcpt;

  double pos_scale_old;
  double pos_scale_rcpt;
  double extenc_scale_old;
  double extenc_scale_rcpt;

  lcec_class_enc_data_t enc;
  lcec_class_enc_data_t extenc;

  const lcec_stmds5k_extenc_conf_t *extenc_conf;

  unsigned int dev_state_pdo_os;
  unsigned int speed_mot_pdo_os;
  unsigned int torque_mot_pdo_os;
  unsigned int speed_state_pdo_os;
  unsigned int pos_mot_pdo_os;
  unsigned int dev_ctrl_pdo_os;
  unsigned int speed_sp_rel_pdo_os;
  unsigned int torque_max_pdo_os;
  unsigned int extinc_pdo_os;

  lcec_stmds5k_syncs_t syncs;

} lcec_stmds5k_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_FLOAT, HAL_IN, offsetof(lcec_stmds5k_data_t, vel_cmd), "%s.%s.%s.srv-vel-cmd" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, vel_fb), "%s.%s.%s.srv-vel-fb" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, vel_fb_rpm), "%s.%s.%s.srv-vel-fb-rpm" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, vel_fb_rpm_abs), "%s.%s.%s.srv-vel-fb-rpm-abs" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, vel_rpm), "%s.%s.%s.srv-vel-rpm" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, torque_fb), "%s.%s.%s.srv-torque-fb" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, torque_fb_abs), "%s.%s.%s.srv-torque-fb-abs" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_stmds5k_data_t, torque_fb_pct), "%s.%s.%s.srv-torque-fb-pct" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_stmds5k_data_t, torque_lim), "%s.%s.%s.srv-torque-lim" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, stopped), "%s.%s.%s.srv-stopped" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, at_speed), "%s.%s.%s.srv-at-speed" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, overload), "%s.%s.%s.srv-overload" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, ready), "%s.%s.%s.srv-ready" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, error), "%s.%s.%s.srv-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, toggle), "%s.%s.%s.srv-toggle" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_stmds5k_data_t, loc_ena), "%s.%s.%s.srv-loc-ena" },
  { HAL_BIT, HAL_IN, offsetof(lcec_stmds5k_data_t, enable), "%s.%s.%s.srv-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_stmds5k_data_t, err_reset), "%s.%s.%s.srv-err-reset" },
  { HAL_BIT, HAL_IN, offsetof(lcec_stmds5k_data_t, fast_ramp), "%s.%s.%s.srv-fast-ramp" },
  { HAL_BIT, HAL_IN, offsetof(lcec_stmds5k_data_t, brake), "%s.%s.%s.srv-brake" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_stmds5k_data_t, pos_scale), "%s.%s.%s.srv-pos-scale" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_stmds5k_data_t, torque_reference), "%s.%s.%s.srv-torque-ref" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_stmds5k_data_t, speed_max_rpm), "%s.%s.%s.srv-max-rpm" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_stmds5k_data_t, speed_max_rpm_sp), "%s.%s.%s.srv-max-rpm-sp" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_stmds5k_data_t, extenc_scale), "%s.%s.%s.extenc-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_stmds5k_syncs_t lcec_stmds5k_syncs_tmpl = {
  .out_ch1 = {
     {0x20b4, 0x00, 8},  // A180 Device Control Byte
     {0x26e6, 0x00, 16}, // D230 n-Soll Relativ
     {0x24e6, 0x00, 16}  // C230 M-Max
  },
  .in_ch1 = {
     {0x28c8, 0x00, 8},  // E200 Device Status Byte
     {0x2864, 0x00, 16}, // E100 n-Motor
     {0x2802, 0x00, 16}, // E02 M-Motor gefiltert
     {0x26c8, 0x00, 16}  // D200 Drehzahlsollwert-Statuswort
  },
  .in_ch2 = {
     {0x2809, 0x00, 32}, // E09 Rotorlage
     {0x0000, 0x00, 32}, // extenc
  },
  .out = {
    {0x1600, 3, NULL} // out_ch1 
  },
  .in = {
    {0x1a00, 4, NULL}, // in_ch1
    {0x1a01, 1, NULL}  // in_ch2
  },
  .syncs = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, NULL}, // out
    {3, EC_DIR_INPUT,  2, NULL}, // in
    {0xff}
  }
};

static const lcec_stmds5k_extenc_conf_t lcec_stmds5k_extenc_conf[] = {
  // 0: E154, EnDat/SSI
  { 0x289a, 0x100000, 32, 0 },
  // 1: E154, Resolver
  { 0x289a, 0x10000, 16, 0 },
  // 2: E154, incremental encoder
  { 0x289a, 0, 16, 0 },
  // 3: E155, multiturn EnDat/SSI
  { 0x289b, 0x100000, 32, 0 },
  // 4: E155, singelturn EnDat/Resolver
  { 0x289b, 0x100000000, 32, 0 },
  // 5: E155, incremental encoder
  { 0x289b, 0, 16, 16, },
  // 6: E156, multiturn EnDat/SSI
  { 0x289c, 0x100000, 32, 0 },
  // 7: E156, singelturn EnDat/Resolver
  { 0x289c, 0x100000000, 32, 0 },
  // 8: E156, incremental encoder
  { 0x289c, 0, 16, 16 },
};

const lcec_stmds5k_extenc_conf_t *lcec_stmds5k_get_extenc_conf(uint32_t type);
void lcec_stmds5k_check_scales(lcec_stmds5k_data_t *hal_data);

void lcec_stmds5k_read(struct lcec_slave *slave, long period);
void lcec_stmds5k_write(struct lcec_slave *slave, long period);

int lcec_stmds5k_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_stmds5k_data_t *hal_data;
  int err;
  uint8_t sdo_buf[4];
  double sdo_torque_reference;
  double sdo_speed_max_rpm;
  double sdo_speed_max_rpm_sp;
  LCEC_CONF_MODPARAM_VAL_T *pval;
  int enc_bits;
  const lcec_stmds5k_extenc_conf_t *extenc_conf;

  // get encoder bits
  pval = lcec_modparam_get(slave, LCEC_STMDS5K_PARAM_MULTITURN);
  enc_bits = (pval != NULL && pval->bit) ? 32 : 24;

  // get extenc config
  extenc_conf = NULL; 
  pval = lcec_modparam_get(slave, LCEC_STMDS5K_PARAM_EXTENC);
  if (pval != NULL) {
    extenc_conf = lcec_stmds5k_get_extenc_conf(pval->u32);
    if (extenc_conf == NULL) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "invalied extenc type %u for slave %s.%s\n", pval->u32, master->name, slave->name);
      return -EINVAL;
    }
  }

  // initialize callbacks
  slave->proc_read = lcec_stmds5k_read;
  slave->proc_write = lcec_stmds5k_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_stmds5k_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -ENOMEM;
  }
  memset(hal_data, 0, sizeof(lcec_stmds5k_data_t));
  slave->hal_data = hal_data;

  // read sdos
  // B18 : torque reference
  if (lcec_read_sdo(slave, 0x2212, 0x00, sdo_buf, 4)) {
    return -EIO;
  }
  sdo_torque_reference = (double) EC_READ_S32(sdo_buf) * STMDS5K_TORQUE_REF_DIV;
  // C01 : max rpm
  if (lcec_read_sdo(slave, 0x2401, 0x00, sdo_buf, 4)) {
    return -EIO;
  }
  sdo_speed_max_rpm = (double) EC_READ_S32(sdo_buf);
  // D02 : setpoint max rpm
  if (lcec_read_sdo(slave, 0x2602, 0x00, sdo_buf, 4)) {
    return -EIO;
  }
  sdo_speed_max_rpm_sp = (double) EC_READ_S32(sdo_buf);
  if (sdo_speed_max_rpm_sp > 1e-20 || sdo_speed_max_rpm_sp < -1e-20) {
    hal_data->speed_max_rpm_sp_rcpt = 1.0 / sdo_speed_max_rpm_sp;
  } else {
    hal_data->speed_max_rpm_sp_rcpt = 0.0;
  }

  // initialize sync info
  // copy template
  memcpy(&hal_data->syncs, &lcec_stmds5k_syncs_tmpl, sizeof(lcec_stmds5k_syncs_t));
  // fix pointers
  hal_data->syncs.out.ch1.entries = (ec_pdo_entry_info_t *) &hal_data->syncs.out_ch1;
  hal_data->syncs.in.ch1.entries = (ec_pdo_entry_info_t *) &hal_data->syncs.in_ch1;
  hal_data->syncs.in.ch2.entries = (ec_pdo_entry_info_t *) &hal_data->syncs.in_ch2;
  hal_data->syncs.syncs.pdo_out.pdos = (ec_pdo_info_t *) &hal_data->syncs.out;
  hal_data->syncs.syncs.pdo_in.pdos = (ec_pdo_info_t *) &hal_data->syncs.in;
  // map extenc
  if (extenc_conf != NULL) {
    hal_data->syncs.in_ch2.extenc.index = extenc_conf->pdo_index;
    hal_data->syncs.in.ch2.n_entries++;
  }
  // set sync info
  slave->sync_info = (ec_sync_info_t *) &hal_data->syncs.syncs;

  // initialize POD entries
  // E200 : device state byte
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x28c8, 0x00, &hal_data->dev_state_pdo_os, NULL);
  // E100 : speed motor (x 0.1% relative to C01)
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x2864, 0x00, &hal_data->speed_mot_pdo_os, NULL);
  // E02 : torque motor filterd (x 0,1 Nm)
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x2802, 0x00, &hal_data->torque_mot_pdo_os, NULL);
  // D200 : speed state word
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x26c8, 0x00, &hal_data->speed_state_pdo_os, NULL);
  // E09 : rotor position
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x2809, 0x00, &hal_data->pos_mot_pdo_os, NULL);
  // A180 : Device Control Byte
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x20b4, 0x00, &hal_data->dev_ctrl_pdo_os, NULL);
  // D230 : speed setpoint (x 0.1 % relative to D02, -200.0% .. 200.0%)
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x26e6, 0x00, &hal_data->speed_sp_rel_pdo_os, NULL);
  // C230 : maximum torque (x 1%, 0% .. 200%)
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x24e6, 0x00, &hal_data->torque_max_pdo_os, NULL);
  // external encoder
  if (extenc_conf != NULL) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, extenc_conf->pdo_index, 0x00, &hal_data->extinc_pdo_os, NULL);
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // init subclasses
  if ((err = class_enc_init(slave, &hal_data->enc, enc_bits, "enc")) != 0) {
    return err;
  }
  if (extenc_conf != NULL) {
    if ((err = class_enc_init(slave, &hal_data->extenc, extenc_conf->used_bits, "extenc")) != 0) {
      return err;
    }
  }

  // set default pin values
  *(hal_data->torque_lim) = 1.0;

  // initialize variables
  hal_data->torque_reference = sdo_torque_reference;
  hal_data->speed_max_rpm = sdo_speed_max_rpm;
  hal_data->speed_max_rpm_sp = sdo_speed_max_rpm_sp;
  hal_data->pos_scale = 1.0;
  hal_data->pos_scale_old = hal_data->pos_scale + 1.0;
  hal_data->pos_scale_rcpt = 1.0;
  hal_data->extenc_conf = extenc_conf;
  hal_data->extenc_scale = 1.0;
  hal_data->extenc_scale_old = hal_data->extenc_scale + 1.0;
  hal_data->extenc_scale_rcpt = 1.0;

  return 0;
}

const lcec_stmds5k_extenc_conf_t *lcec_stmds5k_get_extenc_conf(uint32_t type) {
  if (type >= (sizeof(lcec_stmds5k_extenc_conf) / sizeof(lcec_stmds5k_extenc_conf_t))) {
    return NULL;
  }

  return &lcec_stmds5k_extenc_conf[type];
}

void lcec_stmds5k_check_scales(lcec_stmds5k_data_t *hal_data) {
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

  if (hal_data->extenc_scale != hal_data->extenc_scale_old) {
    // scale value has changed, test and update it
    if ((hal_data->extenc_scale < 1e-20) && (hal_data->extenc_scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      hal_data->extenc_scale = 1.0;
    }
    // save new scale to detect future changes
    hal_data->extenc_scale_old = hal_data->extenc_scale;
    // we actually want the reciprocal
    hal_data->extenc_scale_rcpt = 1.0 / hal_data->extenc_scale;
  }
}

void lcec_stmds5k_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_stmds5k_data_t *hal_data = (lcec_stmds5k_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint8_t dev_state;
  uint16_t speed_state;
  int16_t speed_raw, torque_raw;
  double rpm, torque;
  uint32_t pos_cnt;

  // wait for slave to be operational
  if (!slave->state.operational) {
    *(hal_data->ready) = 0;
    *(hal_data->error) = 1;
    *(hal_data->loc_ena) = 0;
    *(hal_data->toggle) = 0;
    *(hal_data->stopped) = 0;
    *(hal_data->at_speed) = 0;
    *(hal_data->overload) = 0;
    return;
  }

  // check for change in scale value
  lcec_stmds5k_check_scales(hal_data);

  // read device state
  dev_state = EC_READ_U8(&pd[hal_data->dev_state_pdo_os]);
  *(hal_data->ready) = (dev_state >> 0) & 0x01;
  *(hal_data->error) = (dev_state >> 1) & 0x01;
  *(hal_data->loc_ena) = (dev_state >> 6) & 0x01;
  *(hal_data->toggle) = (dev_state >> 7) & 0x01;

  // read speed state
  speed_state = EC_READ_U16(&pd[hal_data->speed_state_pdo_os]);
  *(hal_data->stopped) = (speed_state >> 0) & 0x01;
  *(hal_data->at_speed) = (speed_state >> 1) & 0x01;
  *(hal_data->overload) = (speed_state >> 2) & 0x01;

  // read current speed
  speed_raw = EC_READ_S16(&pd[hal_data->speed_mot_pdo_os]);
  rpm = hal_data->speed_max_rpm * (double)speed_raw * STMDS5K_PCT_REG_DIV;
  *(hal_data->vel_fb_rpm) = rpm;
  *(hal_data->vel_fb_rpm_abs) = fabs(rpm);
  *(hal_data->vel_fb) = rpm * STMDS5K_RPM_DIV * hal_data->pos_scale_rcpt;

  // read torque
  // E02 : torque motor filterd (x 0,1 Nm)
  torque_raw = EC_READ_S16(&pd[hal_data->torque_mot_pdo_os]);
  torque = (double)torque_raw * STMDS5K_TORQUE_DIV;
  *(hal_data->torque_fb_pct) = fabs(torque * 100.0);
  torque = torque * hal_data->torque_reference;
  *(hal_data->torque_fb) = torque;
  *(hal_data->torque_fb_abs) = fabs(torque);

  // update position feedback
  pos_cnt = EC_READ_U32(&pd[hal_data->pos_mot_pdo_os]);
  class_enc_update(&hal_data->enc, STMDS5K_PPREV, hal_data->pos_scale_rcpt, pos_cnt, 0, 0);
  if (hal_data->extenc_conf != NULL) {
    pos_cnt = EC_READ_U32(&pd[hal_data->extinc_pdo_os]);
    class_enc_update(&hal_data->extenc, hal_data->extenc_conf->pprev, hal_data->extenc_scale_rcpt, pos_cnt >> hal_data->extenc_conf->shift_bits, 0, 0);
  }
}

void lcec_stmds5k_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_stmds5k_data_t *hal_data = (lcec_stmds5k_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint8_t dev_ctrl;
  double speed_raw, torque_raw;

  // check for change in scale value
  lcec_stmds5k_check_scales(hal_data);

  // write dev ctrl
  dev_ctrl = 0;
  if (*(hal_data->enable)) {
    dev_ctrl |= (1 << 0);
  }
  if (*(hal_data->err_reset)) {
    dev_ctrl |= (1 << 1);
  }
  if (*(hal_data->fast_ramp)) {
    dev_ctrl |= (1 << 2);
  }
  if (*(hal_data->brake)) {
    dev_ctrl |= (1 << 6);
  }
  if (! *(hal_data->toggle)) {
    dev_ctrl |= (1 << 7);
  }
  EC_WRITE_U8(&pd[hal_data->dev_ctrl_pdo_os], dev_ctrl);

  // set maximum torque
  if (*(hal_data->torque_lim) > 2.0) {
    *(hal_data->torque_lim) = 2.0;
  }
  if (*(hal_data->torque_lim) < -2.0) {
    *(hal_data->torque_lim) = -2.0;
  }
  torque_raw = *(hal_data->torque_lim) * STMDS5K_PCT_REG_FACTOR;
  if (torque_raw > (double)0x7fff) {
    torque_raw = (double)0x7fff;
  }
  if (torque_raw < (double)-0x7fff) {
    torque_raw = (double)-0x7fff;
  }
  EC_WRITE_S16(&pd[hal_data->torque_max_pdo_os], (int16_t)torque_raw);

  // calculate rpm command
  *(hal_data->vel_rpm) = *(hal_data->vel_cmd) * hal_data->pos_scale * STMDS5K_RPM_FACTOR;

  // set RPM
  if (*(hal_data->vel_rpm) > hal_data->speed_max_rpm) {
    *(hal_data->vel_rpm) = hal_data->speed_max_rpm;
  }
  if (*(hal_data->vel_rpm) < -hal_data->speed_max_rpm) {
    *(hal_data->vel_rpm) = -hal_data->speed_max_rpm;
  }
  speed_raw = *(hal_data->vel_rpm) * hal_data->speed_max_rpm_sp_rcpt * STMDS5K_PCT_REG_FACTOR;
  if (speed_raw > (double)0x7fff) {
    speed_raw = (double)0x7fff;
  }
  if (speed_raw < (double)-0x7fff) {
    speed_raw = (double)-0x7fff;
  }
  if (! *(hal_data->enable)) {
    speed_raw = 0.0;
  }
  EC_WRITE_S16(&pd[hal_data->speed_sp_rel_pdo_os], (int16_t)speed_raw);
}

