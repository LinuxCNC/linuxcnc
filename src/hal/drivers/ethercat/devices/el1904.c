/**
 * @file el1904.c
 * @brief Driver implementation for Beckhoff EL1904 4-channel TwinSAFE digital input terminal.
 *
 * The EL1904 implements the Fail-Safe over EtherCAT (FSoE) protocol. This driver
 * exposes FSoE command/CRC/connection-ID fields and the four safe digital inputs
 * as HAL pins, and copies FSoE frame data between slave and master PDO areas.
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
#include "el1904.h"

/**
 * @brief Per-channel HAL data for one EL1904 safe digital input.
 */
typedef struct {
  gomc_hal_bit_t *fsoe_in;     /**< HAL output pin: FSoE safe digital input state. */
  gomc_hal_bit_t *fsoe_in_not; /**< HAL output pin: Inverted FSoE safe digital input state. */

  unsigned int fsoe_in_os; /**< Byte offset of the safe input bit in the process data image. */
  unsigned int fsoe_in_bp; /**< Bit position within fsoe_in_os. */
} lcec_el1904_data_in_t;

/**
 * @brief Complete HAL data structure for the EL1904.
 */
typedef struct {
  gomc_hal_u32_t *fsoe_master_cmd;    /**< HAL output: FSoE master command word. */
  gomc_hal_u32_t *fsoe_master_crc;    /**< HAL output: FSoE master CRC. */
  gomc_hal_u32_t *fsoe_master_connid; /**< HAL output: FSoE master connection ID. */

  gomc_hal_u32_t *fsoe_slave_cmd;    /**< HAL output: FSoE slave command word. */
  gomc_hal_u32_t *fsoe_slave_crc;    /**< HAL output: FSoE slave CRC. */
  gomc_hal_u32_t *fsoe_slave_connid; /**< HAL output: FSoE slave connection ID. */

  lcec_el1904_data_in_t inputs[LCEC_EL1904_INPUT_COUNT]; /**< Per-channel safe input data. */

  unsigned int fsoe_master_cmd_os;    /**< PDO offset: FSoE master command. */
  unsigned int fsoe_master_crc_os;    /**< PDO offset: FSoE master CRC. */
  unsigned int fsoe_master_connid_os; /**< PDO offset: FSoE master connection ID. */

  unsigned int fsoe_slave_cmd_os;    /**< PDO offset: FSoE slave command. */
  unsigned int fsoe_slave_crc_os;    /**< PDO offset: FSoE slave CRC. */
  unsigned int fsoe_slave_connid_os; /**< PDO offset: FSoE slave connection ID. */

} lcec_el1904_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_master_crc), "%s.%s.%s.fsoe-master-crc" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_slave_crc), "%s.%s.%s.fsoe-slave-crc" },
  { GOMC_HAL_U32, GOMC_HAL_OUT, offsetof(lcec_el1904_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_in_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el1904_data_in_t, fsoe_in), "%s.%s.%s.fsoe-in-%d" },
  { GOMC_HAL_BIT, GOMC_HAL_OUT, offsetof(lcec_el1904_data_in_t, fsoe_in_not), "%s.%s.%s.fsoe-in-%d-not" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const LCEC_CONF_FSOE_T fsoe_conf = {
  .slave_data_len = 1,
  .master_data_len = 1,
  .data_channels = 1
};

/**
 * @brief EtherCAT cyclic read callback — reads FSoE frame and safe inputs.
 * @param slave  Pointer to the lcec slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el1904_read(struct lcec_slave *slave, long period);

/**
 * @brief Pre-initialise the EL1904 slave (sets FSoE configuration).
 * @param slave Pointer to the lcec slave structure.
 * @return 0 on success.
 */
int lcec_el1904_preinit(struct lcec_slave *slave) {
  // set fsoe config
  slave->fsoeConf = &fsoe_conf;

  return 0;
}

/**
 * @brief Initialize the EL1904 TwinSAFE digital input slave.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1904_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;
  lcec_el1904_data_t *hal_data;
  int i, err;
  lcec_el1904_data_in_t *in;

  // initialize callbacks
  slave->proc_read = lcec_el1904_read;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el1904_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el1904_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->fsoe_master_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x02, &hal_data->fsoe_master_crc_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x03, &hal_data->fsoe_master_connid_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->fsoe_slave_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x03, &hal_data->fsoe_slave_crc_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x04, &hal_data->fsoe_slave_connid_os, NULL);
  for (i = 0, in = hal_data->inputs; i < LCEC_EL1904_INPUT_COUNT; i++, in++) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6001, 0x01 + i, &in->fsoe_in_os, &in->fsoe_in_bp);
  }

  // export pins
  if ((err = lcec_pin_newf_list(env, comp_id, hal_data, slave_pins, master->instance_name, master->name, slave->name)) != 0) {
    return err;
  }
  for (i = 0, in = hal_data->inputs; i < LCEC_EL1904_INPUT_COUNT; i++, in++) {
    if ((err = lcec_pin_newf_list(env, comp_id, in, slave_in_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

void lcec_el1904_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el1904_data_t *hal_data = (lcec_el1904_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el1904_data_in_t *in;

  copy_fsoe_data(slave, hal_data->fsoe_slave_cmd_os, hal_data->fsoe_master_cmd_os);

  *(hal_data->fsoe_slave_cmd) = EC_READ_U8(&pd[hal_data->fsoe_slave_cmd_os]);
  *(hal_data->fsoe_slave_crc) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc_os]);
  *(hal_data->fsoe_slave_connid) = EC_READ_U16(&pd[hal_data->fsoe_slave_connid_os]);

  *(hal_data->fsoe_master_cmd) = EC_READ_U8(&pd[hal_data->fsoe_master_cmd_os]);
  *(hal_data->fsoe_master_crc) = EC_READ_U16(&pd[hal_data->fsoe_master_crc_os]);
  *(hal_data->fsoe_master_connid) = EC_READ_U16(&pd[hal_data->fsoe_master_connid_os]);

  for (i = 0, in = hal_data->inputs; i < LCEC_EL1904_INPUT_COUNT; i++, in++) {
    *(in->fsoe_in) = EC_READ_BIT(&pd[in->fsoe_in_os], in->fsoe_in_bp);
    *(in->fsoe_in_not) = ! *(in->fsoe_in);
  }
}
