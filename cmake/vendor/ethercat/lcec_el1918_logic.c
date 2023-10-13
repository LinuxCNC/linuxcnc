//
//    Copyright (C) 2021 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_el1918_logic.h"

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
} lcec_el1918_logic_fsoe_t;

#define MAX_STDIO_COUNT 8

typedef struct {

  lcec_el1918_logic_fsoe_t *fsoe;
  int fsoe_count;

  int std_in_count;
  hal_bit_t *std_in_pins[MAX_STDIO_COUNT];
  unsigned int std_in_os;

  int std_out_count;
  hal_bit_t *std_out_pins[MAX_STDIO_COUNT];
  unsigned int std_out_os;

  hal_u32_t *state;
  hal_u32_t *cycle_counter;

  unsigned int state_os;
  unsigned int cycle_counter_os;

} lcec_el1918_logic_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_data_t, state), "%s.%s.%s.state" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_data_t, cycle_counter), "%s.%s.%s.cycle-counter" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_master_cmd), "%s.%s.%s.fsoe-%d-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_master_data), "%s.%s.%s.fsoe-%d-master-data" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_master_crc), "%s.%s.%s.fsoe-%d-master-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_master_connid), "%s.%s.%s.fsoe-%d-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-%d-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_slave_data), "%s.%s.%s.fsoe-%d-slave-data" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_slave_crc), "%s.%s.%s.fsoe-%d-slave-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el1918_logic_fsoe_t, fsoe_slave_connid), "%s.%s.%s.fsoe-%d-slave-connid" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};


void lcec_el1918_logic_read(struct lcec_slave *slave, long period);
void lcec_el1918_logic_write(struct lcec_slave *slave, long period);

int lcec_el1918_logic_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {

  lcec_master_t *master = slave->master;
  lcec_el1918_logic_data_t *hal_data;
  lcec_el1918_logic_fsoe_t *fsoe_data;
  lcec_slave_modparam_t *p;
  int fsoe_idx, index, err, count;
  struct lcec_slave *fsoe_slave;

  // initialize callbacks
  slave->proc_read = lcec_el1918_logic_read;
  slave->proc_write = lcec_el1918_logic_write;

  // count fsoe slaves
  for (fsoe_idx = 0, p = slave->modparams; p != NULL && p->id >= 0; p++) {
    if (p->id == LCEC_EL1918_LOGIC_PARAM_SLAVEID) {
      fsoe_idx++;
    }
  }

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el1918_logic_data_t) + fsoe_idx * sizeof(lcec_el1918_logic_fsoe_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el1918_logic_data_t));
  slave->hal_data = hal_data;

  // setup fsoe pointer
  hal_data->fsoe = (lcec_el1918_logic_fsoe_t *) &hal_data[1];
  hal_data->fsoe_count = fsoe_idx;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x01, &hal_data->state_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x02, &hal_data->cycle_counter_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // map and export fsoe slave data
  hal_data->std_in_count = -1;
  hal_data->std_out_count = -1;
  for (fsoe_idx = 0, fsoe_data = hal_data->fsoe, p = slave->modparams; p != NULL && p->id >= 0; p++) {
    if (p->id == LCEC_EL1918_LOGIC_PARAM_SLAVEID) {
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
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7080 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7081 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_data_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7080 + (fsoe_idx << 4), 0x03, &fsoe_data->fsoe_slave_crc_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7080 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_slave_connid_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6080 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6081 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_data_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6080 + (fsoe_idx << 4), 0x03, &fsoe_data->fsoe_master_crc_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6080 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_master_connid_os, NULL);

      // export pins
      if ((err = lcec_pin_newf_list(fsoe_data, fsoe_pins, LCEC_MODULE_NAME, master->name, slave->name, fsoe_idx)) != 0) {
        return err;
      }

      fsoe_idx++;
      fsoe_data++;
      continue;
    }

    if (p->id == LCEC_EL1918_LOGIC_PARAM_STDINCOUNT && hal_data->std_in_count < 0) {
      count = p->value.u32;
      if (count < 0) count = 0;
      if (count > 8) count = 8;
      hal_data->std_in_count = count;

      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf788, 0x00, &hal_data->std_in_os, NULL);

      for (index = 0; index < count; index++) {
        if ((err = lcec_pin_newf(HAL_BIT, HAL_IN, (void **) &(hal_data->std_in_pins[index]), "%s.%s.%s.std-in-%d", LCEC_MODULE_NAME, master->name, slave->name, index)) != 0) {
          return err;
        }

        *(hal_data->std_in_pins[index]) = 0;
      }

      continue;
    }

    if (p->id == LCEC_EL1918_LOGIC_PARAM_STDOUTCOUNT && hal_data->std_out_count < 0) {
      count = p->value.u32;
      if (count < 0) count = 0;
      if (count > 8) count = 8;
      hal_data->std_out_count = count;

      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf688, 0x00, &hal_data->std_out_os, NULL);

      for (index = 0; index < count; index++) {
        if ((err = lcec_pin_newf(HAL_BIT, HAL_OUT, (void **) &(hal_data->std_out_pins[index]), "%s.%s.%s.std-out-%d", LCEC_MODULE_NAME, master->name, slave->name, index)) != 0) {
          return err;
        }

        *(hal_data->std_out_pins[index]) = 0;
      }

      continue;
    }
  }

  return 0;
}

void lcec_el1918_logic_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el1918_logic_data_t *hal_data = (lcec_el1918_logic_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_el1918_logic_fsoe_t *fsoe_data;
  uint8_t std_out;
  int i;

  *(hal_data->state) = EC_READ_U8(&pd[hal_data->state_os]);
  *(hal_data->cycle_counter) = EC_READ_U8(&pd[hal_data->cycle_counter_os]);

  if (hal_data->std_out_count > 0) {
    std_out = EC_READ_U8(&pd[hal_data->std_out_os]);
    for (i = 0; i < hal_data->std_out_count; i++) {
      *(hal_data->std_out_pins[i]) = !!(std_out & (1 << i));
    }
  }

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

void lcec_el1918_logic_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el1918_logic_data_t *hal_data = (lcec_el1918_logic_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  uint8_t std_in;
  int i;

  if (hal_data->std_in_count > 0) {
    std_in = 0;
    for (i = 0; i < hal_data->std_in_count; i++) {
      if (*(hal_data->std_in_pins[i])) std_in |= (1 << i);
    }
    EC_WRITE_U8(&pd[hal_data->std_in_os], std_in);
  }
}

