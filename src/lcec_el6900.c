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
  hal_bit_t *pin;

  unsigned int os;
  unsigned int bp;

} lcec_el6900_fsoe_io_t;

typedef struct {
  hal_u32_t *fsoe_master_crc;
  hal_u32_t *fsoe_slave_crc;
  unsigned int fsoe_master_crc_os;
  unsigned int fsoe_slave_crc_os;
} lcec_el6900_fsoe_crc_t;

typedef struct {
  struct lcec_slave *fsoe_slave;

  hal_u32_t *fsoe_master_cmd;
  hal_u32_t *fsoe_master_connid;

  hal_u32_t *fsoe_slave_cmd;
  hal_u32_t *fsoe_slave_connid;

  unsigned int fsoe_master_cmd_os;
  unsigned int fsoe_master_connid_os;

  unsigned int fsoe_slave_cmd_os;
  unsigned int fsoe_slave_connid_os;

  lcec_el6900_fsoe_crc_t *fsoe_crc;
} lcec_el6900_fsoe_t;

typedef struct {
  int fsoe_count;

  hal_u32_t *control;
  hal_u32_t *state;
  hal_bit_t *login_active;
  hal_bit_t *input_size_missmatch;
  hal_bit_t *output_size_missmatch;

  int std_ins_count;
  lcec_el6900_fsoe_io_t std_ins[LCEC_EL6900_DIO_MAX_COUNT];

  int std_outs_count;
  lcec_el6900_fsoe_io_t std_outs[LCEC_EL6900_DIO_MAX_COUNT];

  unsigned int control_os;
  unsigned int state_os;
  unsigned int login_active_os;
  unsigned int login_active_bp;
  unsigned int input_size_missmatch_os;
  unsigned int input_size_missmatch_bp;
  unsigned int output_size_missmatch_os;
  unsigned int output_size_missmatch_bp;

  // must be last entry (dynamic size)
  lcec_el6900_fsoe_t fsoe[];
} lcec_el6900_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_U32, HAL_IN, offsetof(lcec_el6900_data_t, control), "%s.%s.%s.control" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_data_t, state), "%s.%s.%s.state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, login_active), "%s.%s.%s.login-active" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, input_size_missmatch), "%s.%s.%s.input-size-missmatch" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el6900_data_t, output_size_missmatch), "%s.%s.%s.output-size-missmatch" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_cmd), "%s.%s.%s.fsoe-%d-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_connid), "%s.%s.%s.fsoe-%d-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-%d-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_connid), "%s.%s.%s.fsoe-%d-slave-connid" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_crc_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_crc_t, fsoe_master_crc), "%s.%s.%s.fsoe-%d-master-crc%d" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el6900_fsoe_crc_t, fsoe_slave_crc), "%s.%s.%s.fsoe-%d-slave-crc%d" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

void lcec_el6900_read(struct lcec_slave *slave, long period);
void lcec_el6900_write(struct lcec_slave *slave, long period);

static int init_std_pdos(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, int pid, lcec_el6900_fsoe_io_t *io, int index, hal_pin_dir_t dir) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;
  int count, err;

  for (p = slave->modparams, count = 0; p != NULL && p->id >= 0; p++) {
    // skip not matching params
    if (p->id != pid) {
      continue;
    }

    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, index, 0x01 + count, &io->os, &io->bp);

    // export pin
    if ((err = lcec_pin_newf(HAL_BIT, dir, (void *) &io->pin, "%s.%s.%s.%s", LCEC_MODULE_NAME, master->name, slave->name, p->value.str)) != 0) {
      return err;
    }

    // next item
    io++;
    count++;
  }

  return count;
}

int lcec_el6900_preinit(struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;
  int index, stdin_count, stdout_count;
  struct lcec_slave *fsoe_slave;
  const LCEC_CONF_FSOE_T *fsoeConf;

  slave->pdo_entry_count = LCEC_EL6900_PDOS;

  stdin_count = 0;
  stdout_count = 0;
  for (p = slave->modparams; p != NULL && p->id >= 0; p++) {
    switch(p->id) {
      case LCEC_EL6900_PARAM_SLAVEID:
        // find slave
        index = p->value.u32;
        fsoe_slave = lcec_slave_by_index(master, index);
        if (fsoe_slave == NULL) {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: slave index %d not found\n", master->name, slave->name, index);
          return -EINVAL;
        }

        fsoeConf = fsoe_slave->fsoeConf;
        if (fsoeConf == NULL) {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: slave index %d is not a fsoe slave\n", master->name, slave->name, index);
          return -EINVAL;
        }

        slave->pdo_entry_count += LCEC_EL6900_PARAM_SLAVE_PDOS + LCEC_EL6900_PARAM_SLAVE_CH_PDOS * fsoeConf->data_channels;
        break;

      case LCEC_EL6900_PARAM_STDIN_NAME:
        stdin_count++;
        if (stdin_count > LCEC_EL6900_DIO_MAX_COUNT) {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: maximum stdin count exceeded.\n", master->name, slave->name);
          return -EINVAL;
        }

        (slave->pdo_entry_count)++;
        break;

      case LCEC_EL6900_PARAM_STDOUT_NAME:
        stdout_count++;
        if (stdout_count > LCEC_EL6900_DIO_MAX_COUNT) {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: maximum stdout count exceeded.\n", master->name, slave->name);
          return -EINVAL;
        }

        (slave->pdo_entry_count)++;
        break;
    }
  }

  return 0;
}

int lcec_el6900_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el6900_data_t *hal_data;
  lcec_el6900_fsoe_t *fsoe_data;
  lcec_slave_modparam_t *p;
  int fsoe_idx, index, err;
  lcec_el6900_fsoe_crc_t *crc;
  struct lcec_slave *fsoe_slave;
  const LCEC_CONF_FSOE_T *fsoeConf;

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
  hal_data->fsoe_count = fsoe_idx;
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf200, 0x01, &hal_data->control_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x01, &hal_data->state_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x08, &hal_data->login_active_os, &hal_data->login_active_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x09, &hal_data->input_size_missmatch_os, &hal_data->input_size_missmatch_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xf100, 0x0a, &hal_data->output_size_missmatch_os, &hal_data->output_size_missmatch_bp);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // map and export stdios
  hal_data->std_ins_count = init_std_pdos(slave, pdo_entry_regs, LCEC_EL6900_PARAM_STDIN_NAME, hal_data->std_ins, 0xf201, HAL_IN);
  if (hal_data->std_ins_count < 0) {
    return hal_data->std_ins_count;
  }
  pdo_entry_regs += hal_data->std_ins_count;
  hal_data->std_outs_count = init_std_pdos(slave, pdo_entry_regs, LCEC_EL6900_PARAM_STDOUT_NAME, hal_data->std_outs, 0xf101, HAL_OUT);
  if (hal_data->std_outs_count < 0) {
    return hal_data->std_outs_count;
  }
  pdo_entry_regs += hal_data->std_outs_count;

  // map and export fsoe slave data
  for (fsoe_idx = 0, fsoe_data = hal_data->fsoe, p = slave->modparams; p != NULL && p->id >= 0; p++) {
    if (p->id == LCEC_EL6900_PARAM_SLAVEID) {
      // find slave
      index = p->value.u32;
      fsoe_slave = lcec_slave_by_index(master, index);
      if (fsoe_slave == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: slave index %d not found\n", master->name, slave->name, index);
        return -EINVAL;
      }
      fsoe_data->fsoe_slave = fsoe_slave;
      fsoe_slave->fsoe_slave_offset = &fsoe_data->fsoe_slave_cmd_os;
      fsoe_slave->fsoe_master_offset = &fsoe_data->fsoe_master_cmd_os;
      fsoeConf = fsoe_slave->fsoeConf;

      // alloc crc hal memory
      if ((fsoe_data->fsoe_crc = hal_malloc(fsoeConf->data_channels * sizeof(lcec_el6900_fsoe_crc_t))) == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for fsoe_slave %s.%s crc data failed\n", master->name, fsoe_slave->name);
        return -EIO;
      }
      memset(fsoe_data->fsoe_crc, 0, fsoeConf->data_channels * sizeof(lcec_el6900_fsoe_crc_t));

      // initialize POD entries
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_slave_connid_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_master_connid_os, NULL);

      // export pins
      if ((err = lcec_pin_newf_list(fsoe_data, fsoe_pins, LCEC_MODULE_NAME, master->name, slave->name, fsoe_idx)) != 0) {
        return err;
      }

      // map CRC PDOS
      for (index = 0, crc = fsoe_data->fsoe_crc; index < fsoeConf->data_channels; index++, crc++) {
        LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x03 + index, &crc->fsoe_slave_crc_os, NULL);
        LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x03 + index, &crc->fsoe_master_crc_os, NULL);
        if ((err = lcec_pin_newf_list(crc, fsoe_crc_pins, LCEC_MODULE_NAME, master->name, slave->name, fsoe_idx, index)) != 0) {
          return err;
        }
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
  int i, crc_idx;
  lcec_el6900_fsoe_io_t *io;
  lcec_el6900_fsoe_crc_t *crc;
  struct lcec_slave *fsoe_slave;
  const LCEC_CONF_FSOE_T *fsoeConf;

  *(hal_data->state) = EC_READ_U8(&pd[hal_data->state_os]) & 0x03;
  *(hal_data->login_active) = EC_READ_BIT(&pd[hal_data->login_active_os], hal_data->login_active_bp);
  *(hal_data->input_size_missmatch) = EC_READ_BIT(&pd[hal_data->input_size_missmatch_os], hal_data->input_size_missmatch_bp);
  *(hal_data->output_size_missmatch) = EC_READ_BIT(&pd[hal_data->output_size_missmatch_os], hal_data->output_size_missmatch_bp);

  for (i = 0, io = hal_data->std_outs; i < hal_data->std_outs_count; i++, io++) {
    *(io->pin) = EC_READ_BIT(&pd[io->os], io->bp);
  }

  for (i = 0, fsoe_data = hal_data->fsoe; i < hal_data->fsoe_count; i++, fsoe_data++) {
    fsoe_slave = fsoe_data->fsoe_slave;
    fsoeConf = fsoe_slave->fsoeConf;
    *(fsoe_data->fsoe_master_cmd) = EC_READ_U8(&pd[fsoe_data->fsoe_master_cmd_os]);
    *(fsoe_data->fsoe_master_connid) = EC_READ_U16(&pd[fsoe_data->fsoe_master_connid_os]);
    *(fsoe_data->fsoe_slave_cmd) = EC_READ_U8(&pd[fsoe_data->fsoe_slave_cmd_os]);
    *(fsoe_data->fsoe_slave_connid) = EC_READ_U16(&pd[fsoe_data->fsoe_slave_connid_os]);
    for (crc_idx = 0, crc = fsoe_data->fsoe_crc; crc_idx < fsoeConf->data_channels; crc_idx++, crc++) {
      *(crc->fsoe_master_crc) = EC_READ_U16(&pd[crc->fsoe_master_crc_os]);
      *(crc->fsoe_slave_crc) = EC_READ_U16(&pd[crc->fsoe_slave_crc_os]);
    }
  }
}

void lcec_el6900_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el6900_data_t *hal_data = (lcec_el6900_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_el6900_fsoe_io_t *io;
  int i;

  EC_WRITE_U16(&pd[hal_data->control_os], *(hal_data->control));

  for (i = 0, io = hal_data->std_ins; i < hal_data->std_ins_count; i++, io++) {
    EC_WRITE_BIT(&pd[io->os], io->bp, *(io->pin));
  }
}

