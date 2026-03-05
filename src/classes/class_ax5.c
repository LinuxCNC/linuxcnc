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

#include "lcec.h"
#include "class_enc.h"
#include "class_ax5.h"

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_class_ax5_chan_t, drive_on), "%s.%s.%s.%ssrv-drive-on" },
  { HAL_BIT, HAL_IN, offsetof(lcec_class_ax5_chan_t, enable), "%s.%s.%s.%ssrv-enable" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_class_ax5_chan_t, enabled), "%s.%s.%s.%ssrv-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_class_ax5_chan_t, halted), "%s.%s.%s.%ssrv-halted" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_class_ax5_chan_t, fault), "%s.%s.%s.%ssrv-fault" },
  { HAL_BIT, HAL_IN, offsetof(lcec_class_ax5_chan_t, halt), "%s.%s.%s.%ssrv-halt" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_class_ax5_chan_t, velo_cmd), "%s.%s.%s.%ssrv-velo-cmd" },

  { HAL_U32, HAL_IN, offsetof(lcec_class_ax5_chan_t, status), "%s.%s.%s.%ssrv-status" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_class_ax5_chan_t, torque_fb_pct), "%s.%s.%s.%ssrv-torque-fb-pct" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_diag_pins[] = {
  { HAL_U32, HAL_IN, offsetof(lcec_class_ax5_chan_t, diag), "%s.%s.%s.%ssrv-diag" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_class_ax5_chan_t, scale), "%s.%s.%s.%ssrv-scale" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_class_ax5_chan_t, vel_scale), "%s.%s.%s.%ssrv-vel-scale" },
  { HAL_U32, HAL_RO, offsetof(lcec_class_ax5_chan_t, pos_resolution), "%s.%s.%s.%ssrv-pos-resolution" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_fb2_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_class_ax5_chan_t, scale_fb2), "%s.%s.%s.%ssrv-scale-fb2" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static int get_param_flag(struct lcec_slave *slave, int id) {
  LCEC_CONF_MODPARAM_VAL_T *pval;

  pval = lcec_modparam_get(slave, id);
  if (pval == NULL) {
    return 0;
  }

  return pval->bit;
}

int lcec_class_ax5_pdos(struct lcec_slave *slave) {
  int pdo_count = 5;

  if (get_param_flag(slave, LCEC_AX5_PARAM_ENABLE_FB2)) {
    pdo_count += 1;
  }

  if (get_param_flag(slave, LCEC_AX5_PARAM_ENABLE_DIAG)) {
    pdo_count += 1;
  }

  return pdo_count;
}

int lcec_class_ax5_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, lcec_class_ax5_chan_t *chan, int index, const char *pfx) {
  lcec_master_t *master = slave->master;
  int err;
  uint8_t idn_buf[4];
  uint32_t idn_pos_resolution;
  uint16_t idn_vel_scale;
  int16_t idn_vel_exp;
  char enc_pfx[HAL_NAME_LEN];

  // read idns
  if (lcec_read_idn(slave, index, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 79), idn_buf, 4)) {
    return -EIO;
  }
  idn_pos_resolution = EC_READ_U32(idn_buf);

  if (lcec_read_idn(slave, index, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 45), idn_buf, 2)) {
    return -EIO;
  }
  idn_vel_scale = EC_READ_U16(idn_buf);
  if (lcec_read_idn(slave, index, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 46), idn_buf, 2)) {
    return -EIO;
  }
  idn_vel_exp = EC_READ_S16(idn_buf);

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0087, 0x01 + index, &chan->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0033, 0x01 + index, &chan->pos_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0054, 0x01 + index, &chan->torque_fb_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0086, 0x01 + index, &chan->ctrl_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0018, 0x01 + index, &chan->vel_cmd_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // export params
  if ((err = lcec_param_newf_list(chan, slave_params, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // initialie encoder
  rtapi_snprintf(enc_pfx, HAL_NAME_LEN, "%senc", pfx);
  if ((err = class_enc_init(slave, &chan->enc, 32, enc_pfx)) != 0) {
    return err;
  }

  chan->fb2_enabled = get_param_flag(slave, LCEC_AX5_PARAM_ENABLE_FB2);
  if (chan->fb2_enabled) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0035, 0x01 + index, &chan->pos_fb2_pdo_os, NULL);
    if ((err = lcec_param_newf_list(chan, slave_fb2_params, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
      return err;
    }

    rtapi_snprintf(enc_pfx, HAL_NAME_LEN, "%senc-fb2", pfx);
    if ((err = class_enc_init(slave, &chan->enc_fb2, 32, enc_pfx)) != 0) {
      return err;
    }
  }

  chan->diag_enabled = get_param_flag(slave, LCEC_AX5_PARAM_ENABLE_DIAG);
  if (chan->diag_enabled) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0186, 0x01 + index, &chan->diag_pdo_os, NULL);
    if ((err = lcec_pin_newf_list(chan, slave_diag_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
      return err;
    }
  }

  // init pins
  *(chan->drive_on) = 1;

  // init parameters
  chan->scale = 1.0;
  chan->scale_fb2 = 1.0;
  chan->vel_scale = ((double) idn_vel_scale) * pow(10.0, (double) idn_vel_exp);
  chan->pos_resolution = idn_pos_resolution;

  if (chan->vel_scale > 0.0) {
    chan->vel_output_scale = 60.0 / chan->vel_scale;
  } else {
    chan->vel_output_scale = 0.0;
  }

  return 0;
}

void lcec_class_ax5_check_scales(lcec_class_ax5_chan_t *chan) {
  // check for change in scale value
  if (chan->scale != chan->scale_old) {
    // scale value has changed, test and update it
    if ((chan->scale < 1e-20) && (chan->scale > -1e-20)) {
      // value too small, divide by zero is a bad thing
      chan->scale = 1.0;
    }
    // save new scale to detect future changes
    chan->scale_old = chan->scale;
    // we actually want the reciprocal
    chan->scale_rcpt = 1.0 / chan->scale;
  }

  // check fb2 for change in scale value
  if (chan->scale_fb2 != chan->scale_fb2_old) {
    // scale value has changed, test and update it
    if ((chan->scale_fb2 < 1e-20) && (chan->scale_fb2 > -1e-20)) {
      // value too small, divide by zero is a bad thing
      chan->scale_fb2 = 1.0;
    }
    // save new scale to detect future changes
    chan->scale_fb2_old = chan->scale_fb2;
    // we actually want the reciprocal
    chan->scale_fb2_rcpt = 1.0 / chan->scale_fb2;
  }
}

void lcec_class_ax5_read(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan) {
  lcec_master_t *master = slave->master;
  uint8_t *pd = master->process_data;
  uint32_t pos_cnt;

  // wait for slave to be operational
  if (!slave->state.operational) {
    chan->enc.do_init = 1;
    chan->enc_fb2.do_init = 1;
    *(chan->fault) = 1;
    *(chan->enabled) = 0;
    *(chan->halted) = 0;
    return;
  }

  // check inputs
  lcec_class_ax5_check_scales(chan);

  *(chan->status) = EC_READ_U16(&pd[chan->status_pdo_os]);

  // check fault
  *(chan->fault) = 0;
  // check error shut off status
  if (((*(chan->status) >> 13) & 1) != 0) {
    *(chan->fault) = 1;
  }
  // check ready-to-operate value
  if (((*(chan->status) >> 14) & 3) == 0) {
    *(chan->fault) = 1;
  }

  // check status
  *(chan->enabled) = (((*(chan->status) >> 14) & 3) == 3);
  *(chan->halted) = (((*(chan->status) >> 3) & 1) != 1);

  // update position feedback
  pos_cnt = EC_READ_U32(&pd[chan->pos_fb_pdo_os]);
  class_enc_update(&chan->enc, chan->pos_resolution, chan->scale_rcpt, pos_cnt, 0, 0);

  if (chan->fb2_enabled) {
    pos_cnt = EC_READ_U32(&pd[chan->pos_fb2_pdo_os]);
    class_enc_update(&chan->enc_fb2, 1, chan->scale_fb2_rcpt, pos_cnt, 0, 0);
  }

  if (chan->diag_enabled) {
    *(chan->diag) = EC_READ_U32(&pd[chan->diag_pdo_os]);
  }

  *(chan->torque_fb_pct) = ((double) EC_READ_S16(&pd[chan->torque_fb_pdo_os])) * 0.1;
}

void lcec_class_ax5_write(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan) {
  lcec_master_t *master = slave->master;
  uint8_t *pd = master->process_data;
  uint16_t ctrl;
  double velo_cmd_raw;

  // write outputs
  ctrl = 0;
  if (chan->toggle) {
    ctrl |= (1 << 10); // sync
  }

  if (*(chan->drive_on)) {
    ctrl |= (1 << 14); // enable
    if (*(chan->enable)) {
      if (!(*(chan->halt))) {
        ctrl |= (1 << 13); // halt/restart
      }
      ctrl |= (1 << 15); // drive on
    }
  }
  EC_WRITE_U16(&pd[chan->ctrl_pdo_os], ctrl);

  // set velo command
  velo_cmd_raw = *(chan->velo_cmd) * chan->scale * chan->vel_output_scale;
  if (velo_cmd_raw > (double)0x7fffffff) {
    velo_cmd_raw = (double)0x7fffffff;
  }
  if (velo_cmd_raw < (double)-0x7fffffff) {
    velo_cmd_raw = (double)-0x7fffffff;
  }
  EC_WRITE_S32(&pd[chan->vel_cmd_pdo_os], (int32_t)velo_cmd_raw);

  chan->toggle = !chan->toggle;
}

