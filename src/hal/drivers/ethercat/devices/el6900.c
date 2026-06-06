/** @file el6900.c
 * @brief Driver for the Beckhoff EL6900 TwinSAFE logic controller (FsoE safety master).
 *
 * Implements HAL pin creation, PDO mapping, and cyclic read/write for the
 * EL6900. The EL6900 coordinates safety communication with one or more FsoE
 * slaves, providing command/connection-ID/CRC transparency pins and standard
 * safety digital I/O bits.
 *
 * @copyright Copyright (C) 2018-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el6900.h"

/**
 * @brief A single standard safety digital I/O bit mapped from a PDO.
 */
typedef struct {
  gomc_hal_bit_t *pin;         /**< HAL bit pin pointer (input or output depending on direction) */

  unsigned int os;        /**< PDO byte offset in the process data image */
  unsigned int bp;        /**< Bit position within the PDO byte */

} lcec_el6900_fsoe_io_t;

/**
 * @brief CRC transparency pair for one FsoE data channel.
 *
 * Each FsoE data channel carries a 16-bit CRC from both the master side
 * and the slave side. These are exposed read-only for diagnostic purposes.
 */
typedef struct {
  gomc_hal_u32_t *fsoe_master_crc;     /**< Master-side CRC value for this channel */
  gomc_hal_u32_t *fsoe_slave_crc;      /**< Slave-side CRC value for this channel */
  unsigned int fsoe_master_crc_os; /**< PDO byte offset of the master CRC */
  unsigned int fsoe_slave_crc_os;  /**< PDO byte offset of the slave CRC */
} lcec_el6900_fsoe_crc_t;

/**
 * @brief Per-FsoE-slave state and PDO offsets.
 *
 * Holds HAL pin pointers and PDO offsets for the FsoE command, connection ID
 * and CRC fields associated with one connected TwinSAFE slave.
 */
typedef struct {
  struct lcec_slave *fsoe_slave;         /**< Pointer to the associated FsoE EtherCAT slave */

  gomc_hal_u32_t *fsoe_master_cmd;            /**< FsoE command byte written by master */
  gomc_hal_u32_t *fsoe_master_connid;         /**< FsoE connection ID from master */

  gomc_hal_u32_t *fsoe_slave_cmd;             /**< FsoE command byte received from slave */
  gomc_hal_u32_t *fsoe_slave_connid;          /**< FsoE connection ID from slave */

  unsigned int fsoe_master_cmd_os;       /**< PDO byte offset: master command */
  unsigned int fsoe_master_connid_os;    /**< PDO byte offset: master connection ID */

  unsigned int fsoe_slave_cmd_os;        /**< PDO byte offset: slave command */
  unsigned int fsoe_slave_connid_os;     /**< PDO byte offset: slave connection ID */

  lcec_el6900_fsoe_crc_t *fsoe_crc;     /**< Array of CRC channel data (one entry per data channel) */
} lcec_el6900_fsoe_t;

/**
 * @brief Top-level HAL data structure for the EL6900.
 *
 * Contains controller-level status pins, standard safety I/O arrays, and a
 * flexible array of FsoE slave descriptors.
 */
typedef struct {
  int fsoe_count;                     /**< Number of configured FsoE slaves */

  gomc_hal_u32_t *control;                 /**< Control word written to the EL6900 (0xF200:01) */
  gomc_hal_u32_t *state;                   /**< State word read from the EL6900 (0xF100:01, bits 0-1) */
  gomc_hal_bit_t *login_active;            /**< TRUE when at least one FsoE connection is active */
  gomc_hal_bit_t *input_size_missmatch;    /**< TRUE when input PDO size mismatch detected */
  gomc_hal_bit_t *output_size_missmatch;   /**< TRUE when output PDO size mismatch detected */

  int std_ins_count;                  /**< Number of configured standard safety inputs */
  lcec_el6900_fsoe_io_t std_ins[LCEC_EL6900_DIO_MAX_COUNT];  /**< Standard safety input bits */

  int std_outs_count;                 /**< Number of configured standard safety outputs */
  lcec_el6900_fsoe_io_t std_outs[LCEC_EL6900_DIO_MAX_COUNT]; /**< Standard safety output bits */

  unsigned int control_os;                  /**< PDO byte offset: control word */
  unsigned int state_os;                    /**< PDO byte offset: state word */
  unsigned int login_active_os;             /**< PDO byte offset: login-active bit */
  unsigned int login_active_bp;             /**< Bit position: login-active bit */
  unsigned int input_size_missmatch_os;     /**< PDO byte offset: input size mismatch bit */
  unsigned int input_size_missmatch_bp;     /**< Bit position: input size mismatch bit */
  unsigned int output_size_missmatch_os;    /**< PDO byte offset: output size mismatch bit */
  unsigned int output_size_missmatch_bp;    /**< Bit position: output size mismatch bit */

  // must be last entry (dynamic size)
  lcec_el6900_fsoe_t fsoe[]; /**< Flexible array of per-FsoE-slave data (length = fsoe_count) */
} lcec_el6900_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_U32, GOMC_HAL_IN, offsetof(lcec_el6900_data_t, control), "%s.%s.%s.control" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_data_t, state), "%s.%s.%s.state" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el6900_data_t, login_active), "%s.%s.%s.login-active" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el6900_data_t, input_size_missmatch), "%s.%s.%s.input-size-missmatch" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el6900_data_t, output_size_missmatch), "%s.%s.%s.output-size-missmatch" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_pins[] = {
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_cmd), "%s.%s.%s.fsoe-%d-master-cmd" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_master_connid), "%s.%s.%s.fsoe-%d-master-connid" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-%d-slave-cmd" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_t, fsoe_slave_connid), "%s.%s.%s.fsoe-%d-slave-connid" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t fsoe_crc_pins[] = {
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_crc_t, fsoe_master_crc), "%s.%s.%s.fsoe-%d-master-crc%d" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el6900_fsoe_crc_t, fsoe_slave_crc), "%s.%s.%s.fsoe-%d-slave-crc%d" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

void lcec_el6900_read(struct lcec_slave *slave, long period);
void lcec_el6900_write(struct lcec_slave *slave, long period);

/**
 * @brief Map PDO entries and export HAL pins for standard safety I/O bits.
 *
 * Iterates over the slave's module parameters matching @p pid and registers
 * each matching entry as a PDO bit and a named HAL pin.
 *
 * @param slave           EtherCAT slave structure.
 * @param pdo_entry_regs  PDO entry registration array, advanced in place.
 * @param pid             Parameter ID to match (LCEC_EL6900_PARAM_STDIN_NAME or _STDOUT_NAME).
 * @param io              Array of I/O descriptors to fill.
 * @param index           PDO object index base (e.g. 0xF201 for inputs).
 * @param dir             HAL pin direction (GOMC_HAL_IN or GOMC_HAL_OUT).
 * @return Number of pins registered on success, negative errno on failure.
 */
static int init_std_pdos(struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs, int pid, lcec_el6900_fsoe_io_t *io, int index, int dir) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
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
    if ((err = lcec_pin_newf(env, master->comp_id, GOMC_HAL_BIT, dir, (void *) &io->pin, "%s.%s.%s.%s", master->instance_name, master->name, slave->name, p->value.str)) != 0) {
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
          LCEC_ERR(master, "%s.%s: slave index %d not found", master->name, slave->name, index);
          return -EINVAL;
        }

        fsoeConf = fsoe_slave->fsoeConf;
        if (fsoeConf == NULL) {
          LCEC_ERR(master, "%s.%s: slave index %d is not a fsoe slave", master->name, slave->name, index);
          return -EINVAL;
        }

        slave->pdo_entry_count += LCEC_EL6900_PARAM_SLAVE_PDOS + LCEC_EL6900_PARAM_SLAVE_CH_PDOS * fsoeConf->data_channels;
        break;

      case LCEC_EL6900_PARAM_STDIN_NAME:
        stdin_count++;
        if (stdin_count > LCEC_EL6900_DIO_MAX_COUNT) {
          LCEC_ERR(master, "%s.%s: maximum stdin count exceeded.", master->name, slave->name);
          return -EINVAL;
        }

        (slave->pdo_entry_count)++;
        break;

      case LCEC_EL6900_PARAM_STDOUT_NAME:
        stdout_count++;
        if (stdout_count > LCEC_EL6900_DIO_MAX_COUNT) {
          LCEC_ERR(master, "%s.%s: maximum stdout count exceeded.", master->name, slave->name);
          return -EINVAL;
        }

        (slave->pdo_entry_count)++;
        break;
    }
  }

  return 0;
}

int lcec_el6900_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
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
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el6900_data_t) + fsoe_idx * sizeof(lcec_el6900_fsoe_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
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
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }

  // map and export stdios
  hal_data->std_ins_count = init_std_pdos(slave, pdo_entry_regs, LCEC_EL6900_PARAM_STDIN_NAME, hal_data->std_ins, 0xf201, GOMC_HAL_IN);
  if (hal_data->std_ins_count < 0) {
    return hal_data->std_ins_count;
  }
  pdo_entry_regs += hal_data->std_ins_count;
  hal_data->std_outs_count = init_std_pdos(slave, pdo_entry_regs, LCEC_EL6900_PARAM_STDOUT_NAME, hal_data->std_outs, 0xf101, GOMC_HAL_OUT);
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
        LCEC_ERR(master, "%s.%s: slave index %d not found", master->name, slave->name, index);
        return -EINVAL;
      }
      fsoe_data->fsoe_slave = fsoe_slave;
      fsoe_slave->fsoe_slave_offset = &fsoe_data->fsoe_slave_cmd_os;
      fsoe_slave->fsoe_master_offset = &fsoe_data->fsoe_master_cmd_os;
      fsoeConf = fsoe_slave->fsoeConf;

      // alloc crc hal memory
      if ((fsoe_data->fsoe_crc = env->hal->malloc(env->hal->ctx, fsoeConf->data_channels * sizeof(lcec_el6900_fsoe_crc_t))) == NULL) {
        LCEC_ERR(master, "hal_malloc() for fsoe_slave %s.%s crc data failed", master->name, fsoe_slave->name);
        return -EIO;
      }
      memset(fsoe_data->fsoe_crc, 0, fsoeConf->data_channels * sizeof(lcec_el6900_fsoe_crc_t));

      // initialize POD entries
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_slave_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_slave_connid_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x01, &fsoe_data->fsoe_master_cmd_os, NULL);
      LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x02, &fsoe_data->fsoe_master_connid_os, NULL);

      // export pins
      if ((err = lcec_pin_newf_list(env, comp_id, fsoe_data, fsoe_pins, master->instance_name, master->name, slave->name, fsoe_idx)) != 0) {
        return err;
      }

      // map CRC PDOS
      for (index = 0, crc = fsoe_data->fsoe_crc; index < fsoeConf->data_channels; index++, crc++) {
        LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (fsoe_idx << 4), 0x03 + index, &crc->fsoe_slave_crc_os, NULL);
        LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (fsoe_idx << 4), 0x03 + index, &crc->fsoe_master_crc_os, NULL);
        if ((err = lcec_pin_newf_list(env, comp_id, crc, fsoe_crc_pins, master->instance_name, master->name, slave->name, fsoe_idx, index)) != 0) {
          return err;
        }
      }

      fsoe_idx++;
      fsoe_data++;
    }
  }

  return 0;
}

/**
 * @brief Cyclic read callback: copy PDO inputs to HAL pins.
 *
 * Reads the EL6900 state word, login-active flag, size-mismatch flags,
 * standard safety output bits (transparency), and all per-FsoE-slave
 * command/connection-ID/CRC values from the EtherCAT process data image
 * into the corresponding HAL pins.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
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

/**
 * @brief Cyclic write callback: copy HAL pin values to PDO outputs.
 *
 * Writes the control word and all configured standard safety input bits
 * from their HAL pins into the EtherCAT process data image.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
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
