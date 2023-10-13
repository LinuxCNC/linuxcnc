//
//    Copyright (C) 2019 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_omrg5.h"

#define OMRG5_PULSES_PER_REV_DEFLT (1 << 20)
#define OMRG5_FAULT_AUTORESET_DELAY_NS 100000000LL

typedef struct {
  hal_float_t *pos_cmd;
  hal_s32_t *pos_cmd_raw;
  hal_float_t *pos_fb;
  hal_float_t *pos_ferr;
  hal_float_t *torque_fb;
  hal_s32_t *pos_fb_raw;
  hal_s32_t *pos_ferr_raw;
  hal_bit_t *fault;
  hal_bit_t *fault_reset;
  hal_bit_t *enable;
  hal_u32_t *error_code;

  hal_bit_t *din_not;
  hal_bit_t *din_pot;
  hal_bit_t *din_dec;
  hal_bit_t *din_pc;
  hal_bit_t *din_ext1;
  hal_bit_t *din_ext2;
  hal_bit_t *din_ext3;
  hal_bit_t *din_mon0;
  hal_bit_t *din_mon1;
  hal_bit_t *din_mon2;
  hal_bit_t *din_pcl;
  hal_bit_t *din_ncl;
  hal_bit_t *din_stop;
  hal_bit_t *din_bkir;
  hal_bit_t *din_sf1;
  hal_bit_t *din_sf2;
  hal_bit_t *din_edm;

  hal_bit_t *stat_switchon_ready;
  hal_bit_t *stat_switched_on;
  hal_bit_t *stat_op_enabled;
  hal_bit_t *stat_fault;
  hal_bit_t *stat_volt_enabled;
  hal_bit_t *stat_quick_stop;
  hal_bit_t *stat_switchon_disabled;
  hal_bit_t *stat_warning;
  hal_bit_t *stat_remote;

  hal_float_t pos_scale;
  hal_bit_t auto_fault_reset;

  hal_float_t pos_scale_old;
  double pos_scale_rcpt;

  unsigned int error_pdo_os;
  unsigned int status_pdo_os;
  unsigned int curr_pos_pdo_os;
  unsigned int curr_torque_pdo_os;
  unsigned int curr_ferr_pdo_os;
  unsigned int latch_stat_pdo_os;
  unsigned int latch_pos1_pdo_os;
  unsigned int latch_pos2_pdo_os;
  unsigned int din_pdo_os;
  unsigned int control_pdo_os;
  unsigned int target_pos_pdo_os;
  unsigned int latch_fnk_os;
  unsigned int dout_pdo_os;

  hal_bit_t enable_old;
  long long auto_fault_reset_delay;

} lcec_omrg5_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_FLOAT, HAL_IN, offsetof(lcec_omrg5_data_t, pos_cmd), "%s.%s.%s.pos-cmd" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_omrg5_data_t, pos_fb), "%s.%s.%s.pos-fb" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_omrg5_data_t, pos_ferr), "%s.%s.%s.pos-ferr" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_omrg5_data_t, torque_fb), "%s.%s.%s.torque-fb-pct" },
  { HAL_S32, HAL_OUT, offsetof(lcec_omrg5_data_t, pos_cmd_raw), "%s.%s.%s.pos-cmd-raw" },
  { HAL_S32, HAL_OUT, offsetof(lcec_omrg5_data_t, pos_fb_raw), "%s.%s.%s.pos-fb-raw" },
  { HAL_S32, HAL_OUT, offsetof(lcec_omrg5_data_t, pos_ferr_raw), "%s.%s.%s.pos-ferr-raw" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, fault), "%s.%s.%s.fault" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, fault_reset), "%s.%s.%s.fault-reset" },
  { HAL_BIT, HAL_IN, offsetof(lcec_omrg5_data_t, enable), "%s.%s.%s.enable" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_switchon_ready), "%s.%s.%s.stat-switchon-ready" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_switched_on), "%s.%s.%s.stat-switched-on" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_op_enabled), "%s.%s.%s.stat-op-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_fault), "%s.%s.%s.stat-fault" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_volt_enabled), "%s.%s.%s.stat-volt-enabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_quick_stop), "%s.%s.%s.stat-quick-stop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_switchon_disabled), "%s.%s.%s.stat-switchon-disabled" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_warning), "%s.%s.%s.stat-warning" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, stat_remote), "%s.%s.%s.stat-remote" },
  { HAL_U32, HAL_OUT, offsetof(lcec_omrg5_data_t, error_code), "%s.%s.%s.error-code" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_not), "%s.%s.%s.din-not" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_pot), "%s.%s.%s.din-pot" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_dec), "%s.%s.%s.din-dec" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_pc), "%s.%s.%s.din-pc" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_ext1), "%s.%s.%s.din-ext1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_ext2), "%s.%s.%s.din-ext2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_ext3), "%s.%s.%s.din-ext3" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_mon0), "%s.%s.%s.din-mon0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_mon1), "%s.%s.%s.din-mon1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_mon2), "%s.%s.%s.din-mon2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_pcl), "%s.%s.%s.din-pcl" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_ncl), "%s.%s.%s.din-ncl" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_stop), "%s.%s.%s.din-stop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_bkir), "%s.%s.%s.din-bkir" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_sf1), "%s.%s.%s.din-sf1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_sf2), "%s.%s.%s.din-sf2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_omrg5_data_t, din_edm), "%s.%s.%s.din-edm" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_omrg5_data_t, pos_scale), "%s.%s.%s.pos-scale" },
  { HAL_BIT, HAL_RW, offsetof(lcec_omrg5_data_t, auto_fault_reset), "%s.%s.%s.auto-fault-reset" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_omrg5_in[] = {
   {0x603F, 0x00, 16}, // Error Code (U16)
   {0x6041, 0x00, 16}, // Status Word (U16)
   {0x6064, 0x00, 32}, // Current Position (S32)
   {0x6077, 0x00, 16}, // Current Torque (S16)
   {0x60F4, 0x00, 32}, // Current Following Error (S32)
   {0x60B9, 0x00, 16}, // Latch Status (U16)
   {0x60BA, 0x00, 32}, // Latch Pos1 (S32)
   {0x60BC, 0x00, 32}, // Latch Pos2 (S32)
   {0x60FD, 0x00, 32}  // Digital Inputs (U32)
};

static ec_pdo_entry_info_t lcec_omrg5_out[] = {
   {0x6040, 0x00, 16}, // Control Word (U16)
   {0x607A, 0x00, 32}, // Target Position (S32)
   {0x60B8, 0x00, 16}, // Latch Function (U16)
   {0x60FE, 0x01, 32}  // Physical Digital Outputs (U32)
};

static ec_pdo_info_t lcec_omrg5_pdos_out[] = {
    {0x1701,  4, lcec_omrg5_out}
};

static ec_pdo_info_t lcec_omrg5_pdos_in[] = {
    {0x1b01, 9, lcec_omrg5_in}
};

static ec_sync_info_t lcec_omrg5_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 1, lcec_omrg5_pdos_out},
    {3, EC_DIR_INPUT,  1, lcec_omrg5_pdos_in},
    {0xff}
};

void lcec_omrg5_check_scales(lcec_omrg5_data_t *hal_data);

void lcec_omrg5_read(struct lcec_slave *slave, long period);
void lcec_omrg5_write(struct lcec_slave *slave, long period);

int lcec_omrg5_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_omrg5_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_omrg5_read;
  slave->proc_write = lcec_omrg5_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_omrg5_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_omrg5_data_t));
  slave->hal_data = hal_data;

  // set to cyclic synchronous position mode
  if (ecrt_slave_config_sdo8(slave->config, 0x6060, 0x00, 8) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo velo mode\n", master->name, slave->name);
  }

  // initialize sync info
  slave->sync_info = lcec_omrg5_syncs;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x603F, 0x00, &hal_data->error_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6041, 0x00, &hal_data->status_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6064, 0x00, &hal_data->curr_pos_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6077, 0x00, &hal_data->curr_torque_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60F4, 0x00, &hal_data->curr_ferr_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60B9, 0x00, &hal_data->latch_stat_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60BA, 0x00, &hal_data->latch_pos1_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60BC, 0x00, &hal_data->latch_pos2_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60FD, 0x00, &hal_data->din_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6040, 0x00, &hal_data->control_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x607A, 0x00, &hal_data->target_pos_pdo_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60B8, 0x00, &hal_data->latch_fnk_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x60FE, 0x01, &hal_data->dout_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // initialize variables
  hal_data->pos_scale = (double) OMRG5_PULSES_PER_REV_DEFLT;
  hal_data->pos_scale_old = hal_data->pos_scale + 1.0;
  hal_data->pos_scale_rcpt = 1.0;
  hal_data->auto_fault_reset = 1;

  return 0;
}

void lcec_omrg5_check_scales(lcec_omrg5_data_t *hal_data) {
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

void lcec_omrg5_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_omrg5_data_t *hal_data = (lcec_omrg5_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint16_t status;
  uint32_t din;

  // check for change in scale value
  lcec_omrg5_check_scales(hal_data);

  // read status
  status = EC_READ_U16(&pd[hal_data->status_pdo_os]);
  *(hal_data->stat_switchon_ready)    = (status >> 0) & 1;
  *(hal_data->stat_switched_on)       = (status >> 1) & 1;
  *(hal_data->stat_op_enabled)        = (status >> 2) & 1;
  *(hal_data->stat_fault)             = (status >> 3) & 1;
  *(hal_data->stat_volt_enabled)      = (status >> 4) & 1;
  *(hal_data->stat_quick_stop)        = (status >> 5) & 1;
  *(hal_data->stat_switchon_disabled) = (status >> 6) & 1;
  *(hal_data->stat_warning)           = (status >> 7) & 1;
  *(hal_data->stat_remote)            = (status >> 9) & 1;

  // read digital inputs
  din = EC_READ_U32(&pd[hal_data->din_pdo_os]);
  *(hal_data->din_not)  = (din >> 0) & 1;
  *(hal_data->din_pot)  = (din >> 1) & 1;
  *(hal_data->din_dec)  = (din >> 2) & 1;
  *(hal_data->din_pc)   = (din >> 16) & 1;
  *(hal_data->din_ext1) = (din >> 17) & 1;
  *(hal_data->din_ext2) = (din >> 18) & 1;
  *(hal_data->din_ext3) = (din >> 19) & 1;
  *(hal_data->din_mon0) = (din >> 20) & 1;
  *(hal_data->din_mon1) = (din >> 21) & 1;
  *(hal_data->din_mon2) = (din >> 22) & 1;
  *(hal_data->din_pcl)  = (din >> 23) & 1;
  *(hal_data->din_ncl)  = (din >> 24) & 1;
  *(hal_data->din_stop) = (din >> 25) & 1;
  *(hal_data->din_bkir) = (din >> 26) & 1;
  *(hal_data->din_sf1)  = (din >> 27) & 1;
  *(hal_data->din_sf2)  = (din >> 28) & 1;
  *(hal_data->din_edm)  = (din >> 29) & 1;

  // read position feedback
  *(hal_data->pos_fb_raw) = EC_READ_S32(&pd[hal_data->curr_pos_pdo_os]);
  *(hal_data->pos_fb) = ((double) *(hal_data->pos_fb_raw)) * hal_data->pos_scale_rcpt;

  // read following error
  *(hal_data->pos_ferr_raw) = EC_READ_S32(&pd[hal_data->curr_ferr_pdo_os]);
  *(hal_data->pos_ferr) = ((double) *(hal_data->pos_ferr_raw)) * hal_data->pos_scale_rcpt;

  // read torque error
  *(hal_data->torque_fb) = ((double) EC_READ_S16(&pd[hal_data->curr_torque_pdo_os])) * 0.1;

  // read error code
  *(hal_data->error_code) = EC_READ_U16(&pd[hal_data->error_pdo_os]);

  // update fault output
  if (hal_data->auto_fault_reset_delay > 0) {
    hal_data->auto_fault_reset_delay -= period;
    *(hal_data->fault) = 0;
  } else {
    *(hal_data->fault) = *(hal_data->stat_fault) && *(hal_data->enable);
  }
}

void lcec_omrg5_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_omrg5_data_t *hal_data = (lcec_omrg5_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int enable_edge;
  uint16_t control;

  // detect enable edge
  enable_edge = *(hal_data->enable) && !hal_data->enable_old;
  hal_data->enable_old = *(hal_data->enable);

  // write control register
  control = (1 << 2); // quick stop is not supported
  if (*(hal_data->stat_fault)) {
    if (*(hal_data->fault_reset)) {
      control |= (1 << 7); // fault reset
    }
    if (hal_data->auto_fault_reset && enable_edge) {
      hal_data->auto_fault_reset_delay = OMRG5_FAULT_AUTORESET_DELAY_NS;
      control |= (1 << 7); // fault reset
    }
  } else {
    if (*(hal_data->enable)) {
      control |= (1 << 1); // enable voltage
      if (*(hal_data->stat_switchon_ready)) {
        control |= (1 << 0); // switch on
        if (*(hal_data->stat_switched_on)) {
          control |= (1 << 3); // enable op
        }
      }
    }
  }
  EC_WRITE_U16(&pd[hal_data->control_pdo_os], control);

  // write position command
  *(hal_data->pos_cmd_raw) = (int32_t) (*(hal_data->pos_cmd) * hal_data->pos_scale);
  EC_WRITE_S32(&pd[hal_data->target_pos_pdo_os], *(hal_data->pos_cmd_raw));
}

