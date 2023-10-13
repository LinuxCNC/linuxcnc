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
#include "lcec_el6900.h"

typedef struct {
  hal_u32_t *fsoe_master_cmd;
  hal_u32_t *fsoe_master_data;
  hal_u32_t *fsoe_master_crc;
  hal_u32_t *fsoe_master_connid;

  hal_u32_t *fsoe_slave_cmd;
  hal_u32_t *fsoe_slave_data;
  hal_u32_t *fsoe_slave_crc;
  hal_u32_t *fsoe_slave_connid;

  unsigned int fsoe_master_cmd_os;
  unsigned int fsoe_master_data_os;
  unsigned int fsoe_master_crc_os;
  unsigned int fsoe_master_connid_os;

  unsigned int fsoe_slave_cmd_os;
  unsigned int fsoe_slave_data_os;
  unsigned int fsoe_slave_crc_os;
  unsigned int fsoe_slave_connid_os;
} lcec_el6900_fsoe_t;

typedef struct {

  lcec_el6900_fsoe_t *fsoe;
  int fsoe_count;

  hal_bit_t *std_in_0;
  hal_bit_t *std_in_1;
  hal_bit_t *std_in_2;
  hal_bit_t *std_in_3;
  hal_bit_t *std_in_4;
  hal_bit_t *std_in_5;
  hal_bit_t *std_in_6;
  hal_bit_t *std_in_7;
  hal_u32_t *control;

  hal_bit_t *std_out_0;
  hal_bit_t *std_out_1;
  hal_bit_t *std_out_2;
  hal_bit_t *std_out_3;
  hal_bit_t *std_out_4;
  hal_bit_t *std_out_5;
  hal_bit_t *std_out_6;
  hal_bit_t *std_out_7;
  hal_bit_t *std_out_8;
  hal_u32_t *state;
  hal_bit_t *login_active;
  hal_bit_t *input_size_missmatch;
  hal_bit_t *output_size_missmatch;
  hal_bit_t *txpdo_state;
  hal_bit_t *txpdo_toggle;

  unsigned int std_in_0_os;
  unsigned int std_in_0_bp;
  unsigned int std_in_1_os;
  unsigned int std_in_1_bp;
  unsigned int std_in_2_os;
  unsigned int std_in_2_bp;
  unsigned int std_in_3_os;
  unsigned int std_in_3_bp;
  unsigned int std_in_4_os;
  unsigned int std_in_4_bp;
  unsigned int std_in_5_os;
  unsigned int std_in_5_bp;
  unsigned int std_in_6_os;
  unsigned int std_in_6_bp;
  unsigned int std_in_7_os;
  unsigned int std_in_7_bp;

  unsigned int control_os;

  unsigned int std_out_0_os;
  unsigned int std_out_0_bp;
  unsigned int std_out_1_os;
  unsigned int std_out_1_bp;
  unsigned int std_out_2_os;
  unsigned int std_out_2_bp;
  unsigned int std_out_3_os;
  unsigned int std_out_3_bp;
  unsigned int std_out_4_os;
  unsigned int std_out_4_bp;
  unsigned int std_out_5_os;
  unsigned int std_out_5_bp;
  unsigned int std_out_6_os;
  unsigned int std_out_6_bp;
  unsigned int std_out_7_os;
  unsigned int std_out_7_bp;

  unsigned int state_os;
  unsigned int login_active_os;
  unsigned int login_active_bp;
  unsigned int input_size_missmatch_os;
  unsigned int input_size_missmatch_bp;
  unsigned int output_size_missmatch_os;
  unsigned int output_size_missmatch_bp;
  unsigned int txpdo_state_os;
  unsigned int txpdo_state_bp;
  unsigned int txpdo_toggle_os;
  unsigned int txpdo_toggle_bp;

} lcec_el6900_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_0), "%s.%s.%s.std-in-0" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_1), "%s.%s.%s.std-in-1" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_2), "%s.%s.%s.std-in-2" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_3), "%s.%s.%s.std-in-3" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_4), "%s.%s.%s.std-in-4" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_5), "%s.%s.%s.std-in-5" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_6), "%s.%s.%s.std-in-6" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el6900_data_t, std_in_7), "%s.%s.%s.std-in-7" },
  { HAL_U32, HAL_IN, offsetof(lcec_el6900_data_t, control), "%s.%s.%s.control" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_0), "%s.%s.%s.std-out-0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_1), "%s.%s.%s.std-out-1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_2), "%s.%s.%s.std-out-2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_3), "%s.%s.%s.std-out-3" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_4), "%s.%s.%s.std-out-4" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_5), "%s.%s.%s.std-out-5" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_6), "%s.%s.%s.std-out-6" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, std_out_7), "%s.%s.%s.std-out-7" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_data_t, state), "%s.%s.%s.state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, login_active), "%s.%s.%s.login-active" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, input_size_missmatch), "%s.%s.%s.input-size-missmatch" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, output_size_missmatch), "%s.%s.%s.output-size-missmatch" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, txpdo_state), "%s.%s.%s.txpdo-state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, txpdo_toggle), "%s.%s.%s.txpdo-toggle" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_cmd), "%s.%s.%s.fsoe-%d-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_data), "%s.%s.%s.fsoe-%d-master-data" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_crc), "%s.%s.%s.fsoe-%d-master-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_connid), "%s.%s.%s.fsoe-%d-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-%d-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_data), "%s.%s.%s.fsoe-%d-slave-data" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_crc), "%s.%s.%s.fsoe-%d-slave-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_connid), "%s.%s.%s.fsoe-%d-slave-connid" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};


void lcec_el6900_read(struct lcec_slave *slave, long period);
void lcec_el6900_write(struct lcec_slave *slave, long period);

int lcec_el6900_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {

  lcec_master_t *master = slave->master;
  lcec_el6900_data_t *hal_data;
  lcec_el6900_fsoe_t *fsoe_data;
  lcec_slave_modparam_t *p;
  int fsoe_idx, index, err;
  struct lcec_slave *fsoe_slave;

  // initialize callbacks
  slave->proc_read = lcec_el6900_read;
  slave->proc_write = lcec_el6900_write;

  // count fsoe slaves
  for (fsoe_idx = 0, p = slave->modparams; p != NULL && p->id >= 0; p++) {
    if (p->id == LCEC_EL6900_PARAM_SLAVEID) {
      fsoe_idx++;
    }
  }

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el6900_data_t) + fsoe_idx * sizeof(lcec_el6900_fsoe_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el6900_data_t));
  slave->hal_data = hal_data;

  // setup fsoe pointer
  hal_data->fsoe = (lcec_el6900_fsoe_t *) &hal_data[1];
  hal_data->fsoe_count = fsoe_idx;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x01, &hal_data->std_in_0_os, &hal_data->std_in_0_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x02, &hal_data->std_in_1_os, &hal_data->std_in_1_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x03, &hal_data->std_in_2_os, &hal_data->std_in_2_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x04, &hal_data->std_in_3_os, &hal_data->std_in_3_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x05, &hal_data->std_in_4_os, &hal_data->std_in_4_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x06, &hal_data->std_in_5_os, &hal_data->std_in_5_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x07, &hal_data->std_in_6_os, &hal_data->std_in_6_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf201, 0x08, &hal_data->std_in_7_os, &hal_data->std_in_7_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf200, 0x01, &hal_data->control_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x01, &hal_data->std_out_0_os, &hal_data->std_out_0_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x02, &hal_data->std_out_1_os, &hal_data->std_out_1_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x03, &hal_data->std_out_2_os, &hal_data->std_out_2_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x04, &hal_data->std_out_3_os, &hal_data->std_out_3_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x05, &hal_data->std_out_4_os, &hal_data->std_out_4_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x06, &hal_data->std_out_5_os, &hal_data->std_out_5_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x07, &hal_data->std_out_6_os, &hal_data->std_out_6_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf101, 0x08, &hal_data->std_out_7_os, &hal_data->std_out_7_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x01, &hal_data->state_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x08, &hal_data->login_active_os, &hal_data->login_active_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x09, &hal_data->input_size_missmatch_os, &hal_data->input_size_missmatch_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x0a, &hal_data->output_size_missmatch_os, &hal_data->output_size_missmatch_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x0f, &hal_data->txpdo_state_os, &hal_data->txpdo_state_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x10, &hal_data->txpdo_toggle_os, &hal_data->txpdo_toggle_bp);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // map and export fsoe slave data
  for (fsoe_idx = 0, fsoe_data = hal_data->fsoe, p = slave->modparams; p != NULL && p->id >= 0; p++) {
    if (p->id == LCEC_EL6900_PARAM_SLAVEID) {
      // find slave
      index = p->value.u32;
      fsoe_slave = lcec_slave_by_index(master, index);
      if (fsoe_slave == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave index %d not found\n", index);
        return -EINVAL;
      }

      fsoe_slave->fsoe_slave_offset = &fsoe_data->fsoe_slave_cmd_os;
      fsoe_slave->fsoe_master_offset = &fsoe_data->fsoe_master_cmd_os;

      // initialize POD entries
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7001 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_data_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x03, &fsoe_data->fsoe_slave_crc_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_slave_connid_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6001 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_data_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x03, &fsoe_data->fsoe_master_crc_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_master_connid_os, NULL);

      // export pins
      if ((err = lcec_pin_newf_list(fsoe_data, fsoe_pins, LCEC_MODULE_NAME, master->name, slave->name, fsoe_idx)) != 0) {
        return err;
      }

      fsoe_idx++;
      fsoe_data++;
    }
  }

  return 0;
}

void lcec_el6900_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el6900_data_t *hal_data = (lcec_el6900_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_el6900_fsoe_t *fsoe_data;
  int i;

  *(hal_data->std_out_0) = EC_READ_BIT(&pd[hal_data->std_out_0_os], hal_data->std_out_0_bp);
  *(hal_data->std_out_1) = EC_READ_BIT(&pd[hal_data->std_out_1_os], hal_data->std_out_1_bp);
  *(hal_data->std_out_2) = EC_READ_BIT(&pd[hal_data->std_out_2_os], hal_data->std_out_2_bp);
  *(hal_data->std_out_3) = EC_READ_BIT(&pd[hal_data->std_out_3_os], hal_data->std_out_3_bp);
  *(hal_data->std_out_4) = EC_READ_BIT(&pd[hal_data->std_out_4_os], hal_data->std_out_4_bp);
  *(hal_data->std_out_5) = EC_READ_BIT(&pd[hal_data->std_out_5_os], hal_data->std_out_5_bp);
  *(hal_data->std_out_6) = EC_READ_BIT(&pd[hal_data->std_out_6_os], hal_data->std_out_6_bp);
  *(hal_data->std_out_7) = EC_READ_BIT(&pd[hal_data->std_out_7_os], hal_data->std_out_7_bp);
  *(hal_data->state) = EC_READ_U8(&pd[hal_data->state_os]) & 0x03;
  *(hal_data->login_active) = EC_READ_BIT(&pd[hal_data->login_active_os], hal_data->login_active_bp);
  *(hal_data->input_size_missmatch) = EC_READ_BIT(&pd[hal_data->input_size_missmatch_os], hal_data->input_size_missmatch_bp);
  *(hal_data->output_size_missmatch) = EC_READ_BIT(&pd[hal_data->output_size_missmatch_os], hal_data->output_size_missmatch_bp);
  *(hal_data->txpdo_state) = EC_READ_BIT(&pd[hal_data->txpdo_state_os], hal_data->txpdo_state_bp);
  *(hal_data->txpdo_toggle) = EC_READ_BIT(&pd[hal_data->txpdo_toggle_os], hal_data->txpdo_toggle_bp);

  for (i = 0, fsoe_data = hal_data->fsoe; i < hal_data->fsoe_count; i++, fsoe_data++) {
    *(fsoe_data->fsoe_master_cmd) = EC_READ_U8(&pd[fsoe_data->fsoe_master_cmd_os]);
    *(fsoe_data->fsoe_master_data) = EC_READ_U8(&pd[fsoe_data->fsoe_master_data_os]);
    *(fsoe_data->fsoe_master_crc) = EC_READ_U16(&pd[fsoe_data->fsoe_master_crc_os]);
    *(fsoe_data->fsoe_master_connid) = EC_READ_U16(&pd[fsoe_data->fsoe_master_connid_os]);
    *(fsoe_data->fsoe_slave_cmd) = EC_READ_U8(&pd[fsoe_data->fsoe_slave_cmd_os]);
    *(fsoe_data->fsoe_slave_data) = EC_READ_U8(&pd[fsoe_data->fsoe_slave_data_os]);
    *(fsoe_data->fsoe_slave_crc) = EC_READ_U16(&pd[fsoe_data->fsoe_slave_crc_os]);
    *(fsoe_data->fsoe_slave_connid) = EC_READ_U16(&pd[fsoe_data->fsoe_slave_connid_os]);
  }
}

void lcec_el6900_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el6900_data_t *hal_data = (lcec_el6900_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  EC_WRITE_BIT(&pd[hal_data->std_in_0_os], hal_data->std_in_0_bp, *(hal_data->std_in_0));
  EC_WRITE_BIT(&pd[hal_data->std_in_1_os], hal_data->std_in_1_bp, *(hal_data->std_in_1));
  EC_WRITE_BIT(&pd[hal_data->std_in_2_os], hal_data->std_in_2_bp, *(hal_data->std_in_2));
  EC_WRITE_BIT(&pd[hal_data->std_in_3_os], hal_data->std_in_3_bp, *(hal_data->std_in_3));
  EC_WRITE_BIT(&pd[hal_data->std_in_4_os], hal_data->std_in_4_bp, *(hal_data->std_in_4));
  EC_WRITE_BIT(&pd[hal_data->std_in_5_os], hal_data->std_in_5_bp, *(hal_data->std_in_5));
  EC_WRITE_BIT(&pd[hal_data->std_in_6_os], hal_data->std_in_6_bp, *(hal_data->std_in_6));
  EC_WRITE_BIT(&pd[hal_data->std_in_7_os], hal_data->std_in_7_bp, *(hal_data->std_in_7));

  EC_WRITE_U16(&pd[hal_data->control_os], *(hal_data->control));
}

