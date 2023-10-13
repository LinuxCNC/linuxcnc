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
#include "lcec_ax5200.h"

#include "lcec_class_enc.h"

typedef struct {
  hal_bit_t *enable;
  hal_bit_t *enabled;
  hal_bit_t *halted;
  hal_bit_t *fault;

  hal_bit_t *halt;
  hal_bit_t *drive_off;

  hal_float_t *velo_cmd;

  hal_u32_t *status;
  hal_s32_t *pos_fb;
  hal_float_t *torque_fb_pct;

  unsigned int status_pdo_os;
  unsigned int pos_fb_pdo_os;
  unsigned int torque_fb_pdo_os;
  unsigned int ctrl_pdo_os;
  unsigned int vel_cmd_pdo_os;

  hal_float_t scale;
  hal_float_t vel_scale;
  hal_u32_t pos_resolution;

  lcec_class_enc_data_t enc;

  double scale_old;
  double scale_rcpt;
  double vel_output_scale;

  int toggle;

} lcec_ax5200_chan_t;

typedef struct {
  lcec_ax5200_chan_t chans[LCEC_AX5200_CHANS];
} lcec_ax5200_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_ax5200_chan_t, enable), "%s.%s.%s.ch%d.srv-enable" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5200_chan_t, enabled), "%s.%s.%s.ch%d.srv-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5200_chan_t, halted), "%s.%s.%s.ch%d.srv-halted" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5200_chan_t, fault), "%s.%s.%s.ch%d.srv-fault" },
  { HAL_BIT, HAL_IN, offsetof(lcec_ax5200_chan_t, halt), "%s.%s.%s.ch%d.srv-halt" },
  { HAL_BIT, HAL_IN, offsetof(lcec_ax5200_chan_t, drive_off), "%s.%s.%s.ch%d.srv-drive-off" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_ax5200_chan_t, velo_cmd), "%s.%s.%s.ch%d.srv-velo-cmd" },

  { HAL_U32, HAL_IN, offsetof(lcec_ax5200_chan_t, status), "%s.%s.%s.ch%d.srv-status" },
  { HAL_S32, HAL_IN, offsetof(lcec_ax5200_chan_t, pos_fb), "%s.%s.%s.ch%d.srv-pos-fb" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_ax5200_chan_t, torque_fb_pct), "%s.%s.%s.ch%d.srv-torque-fb-pct" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_ax5200_chan_t, scale), "%s.%s.%s.ch%d.scale" },
  { HAL_FLOAT, HAL_RO, offsetof(lcec_ax5200_chan_t, vel_scale), "%s.%s.%s.ch%d.vel-scale" },
  { HAL_U32, HAL_RO, offsetof(lcec_ax5200_chan_t, pos_resolution), "%s.%s.%s.ch%d.pos-resolution" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_ax5200_in_a[] = {
   {0x0087, 0x01, 16}, // status word
   {0x0033, 0x01, 32}, // position feedback
   {0x0054, 0x01, 16}  // torque feedback
};

static ec_pdo_entry_info_t lcec_ax5200_in_b[] = {
   {0x0087, 0x02, 16}, // status word
   {0x0033, 0x02, 32}, // position feedback
   {0x0054, 0x02, 16}  // torque feedback
};

static ec_pdo_entry_info_t lcec_ax5200_out_a[] = {
   {0x0086, 0x01,  16}, // control-word
   {0x0018, 0x01,  32}, // velo-command
};

static ec_pdo_entry_info_t lcec_ax5200_out_b[] = {
   {0x0086, 0x02,  16}, // control word
   {0x0018, 0x02,  32}, // velo command
};

static ec_pdo_info_t lcec_ax5200_pdos_out[] = {
    {0x0018,  2, lcec_ax5200_out_a},
    {0x1018,  2, lcec_ax5200_out_b}
};

static ec_pdo_info_t lcec_ax5200_pdos_in[] = {
    {0x0010,  3, lcec_ax5200_in_a},
    {0x1010,  3, lcec_ax5200_in_b}
};

static ec_sync_info_t lcec_ax5200_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_ax5200_pdos_out},
    {3, EC_DIR_INPUT,  2, lcec_ax5200_pdos_in},
    {0xff}
};

void lcec_ax5200_check_scales(lcec_ax5200_chan_t *hal_data);

void lcec_ax5200_read(struct lcec_slave *slave, long period);
void lcec_ax5200_write(struct lcec_slave *slave, long period);

int lcec_ax5200_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_ax5200_data_t *hal_data;
  int i;
  lcec_ax5200_chan_t *chan;
  int err;
  uint8_t idn_buf[4];
  uint32_t idn_pos_resolution;
  uint16_t idn_vel_scale;
  int16_t idn_vel_exp;
  char pfx[HAL_NAME_LEN];

  // initialize callbacks
  slave->proc_read = lcec_ax5200_read;
  slave->proc_write = lcec_ax5200_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ax5200_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ax5200_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_ax5200_syncs;

  // initialize pins
  for (i=0; i<LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];

    // read idns
    if (lcec_read_idn(slave, i, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 79), idn_buf, 4)) {
      return -EIO;
    }
    idn_pos_resolution = EC_READ_U32(idn_buf);

    if (lcec_read_idn(slave, i, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 45), idn_buf, 2)) {
      return -EIO;
    }
    idn_vel_scale = EC_READ_U16(idn_buf);
    if (lcec_read_idn(slave, i, LCEC_IDN(LCEC_IDN_TYPE_S, 0, 46), idn_buf, 2)) {
      return -EIO;
    }
    idn_vel_exp = EC_READ_S16(idn_buf);

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0087, 0x01 + i, &chan->status_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0033, 0x01 + i, &chan->pos_fb_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0054, 0x01 + i, &chan->torque_fb_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0086, 0x01 + i, &chan->ctrl_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x0018, 0x01 + i, &chan->vel_cmd_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // export params
    if ((err = lcec_param_newf_list(chan, slave_params, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // init subclasses
    rtapi_snprintf(pfx, HAL_NAME_LEN, "ch%d.enc", i);
    if ((err = class_enc_init(slave, &chan->enc, 32, pfx)) != 0) {
      return err;
    }

    // init parameters
    chan->scale = 1.0;
    chan->vel_scale = ((double) idn_vel_scale) * pow(10.0, (double) idn_vel_exp);
    chan->pos_resolution = idn_pos_resolution;

    if (chan->vel_scale > 0.0) {
      chan->vel_output_scale = 60.0 / chan->vel_scale;
    } else {
      chan->vel_output_scale = 0.0;
    }
  }

  return 0;
}

void lcec_ax5200_check_scales(lcec_ax5200_chan_t *hal_data) {
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
  }
}

void lcec_ax5200_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ax5200_data_t *hal_data = (lcec_ax5200_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_ax5200_chan_t *chan;
  uint32_t pos_cnt;

  // wait for slave to be operational
  if (!slave->state.operational) {
    for (i=0; i<LCEC_AX5200_CHANS; i++) {
      chan = &hal_data->chans[i];
      chan->enc.do_init = 1;
      *(chan->fault) = 1;
      *(chan->enabled) = 0;
      *(chan->halted) = 0;
    } 
    return;
  }

  // check inputs
  for (i=0; i<LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];

    lcec_ax5200_check_scales(chan);

    *(chan->status) = EC_READ_U16(&pd[chan->status_pdo_os]);
    *(chan->pos_fb) = EC_READ_S32(&pd[chan->pos_fb_pdo_os]);
    *(chan->torque_fb_pct) = ((double) EC_READ_S16(&pd[chan->torque_fb_pdo_os])) * 0.1;

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
  }
}

void lcec_ax5200_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ax5200_data_t *hal_data = (lcec_ax5200_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_ax5200_chan_t *chan;
  uint16_t ctrl;
  double velo_cmd_raw;

  // write outputs
  for (i=0; i<LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];

    ctrl = 0;
    if (chan->toggle) {
      ctrl |= (1 << 10); // sync
    }
    if (*(chan->enable)) {
      if (!(*(chan->halt))) {
        ctrl |= (1 << 13); // halt/restart
      }
      ctrl |= (1 << 14); // enable
      if (!(*(chan->drive_off))) {
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
}

