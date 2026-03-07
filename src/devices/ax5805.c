/**
 * @file ax5805.c
 * @brief Driver for the Beckhoff AX5805 TwinSAFE drive option card.
 *
 * The AX5805 is the FSoE safety card for AX5100/AX5200 drives.  It must sit
 * at EtherCAT index (drive_index + 1).  Pre-init locates the drive, inherits
 * its FSoE config, and calculates the PDO count (4 base entries + 3 per data
 * channel).  Init registers all FSoE PDO entries and exports HAL pins for:
 *   - fsoe-master-cmd / fsoe-master-connid / fsoe-master-crc[-0/-1]
 *   - fsoe-slave-cmd  / fsoe-slave-connid  / fsoe-slave-crc[-0/-1]
 *   - fsoe-in-sto[-0/-1]  (Safe Torque Off status per axis)
 * The read callback copies FSoE data and updates all HAL pin values.
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
#include "ax5805.h"
#include "ax5100.h"
#include "ax5200.h"

/**
 * @brief Internal HAL data for the AX5805 TwinSAFE card.
 */
typedef struct {
  hal_u32_t *fsoe_master_cmd;      /**< HAL OUT: FSoE master command byte. */
  hal_u32_t *fsoe_master_crc0;     /**< HAL OUT: FSoE master CRC for channel 0. */
  hal_u32_t *fsoe_master_crc1;     /**< HAL OUT: FSoE master CRC for channel 1 (dual-axis only). */
  hal_u32_t *fsoe_master_connid;   /**< HAL OUT: FSoE master connection ID. */

  hal_u32_t *fsoe_slave_cmd;       /**< HAL OUT: FSoE slave command byte. */
  hal_u32_t *fsoe_slave_crc0;      /**< HAL OUT: FSoE slave CRC for channel 0. */
  hal_u32_t *fsoe_slave_crc1;      /**< HAL OUT: FSoE slave CRC for channel 1 (dual-axis only). */
  hal_u32_t *fsoe_slave_connid;    /**< HAL OUT: FSoE slave connection ID. */

  hal_bit_t *fsoe_in_sto0;         /**< HAL OUT: Safe Torque Off status for axis 0. */
  hal_bit_t *fsoe_in_sto1;         /**< HAL OUT: Safe Torque Off status for axis 1 (dual-axis only). */

  unsigned int fsoe_master_cmd_os;    /**< PDO byte offset: FSoE master command. */
  unsigned int fsoe_master_crc0_os;   /**< PDO byte offset: FSoE master CRC channel 0. */
  unsigned int fsoe_master_crc1_os;   /**< PDO byte offset: FSoE master CRC channel 1. */
  unsigned int fsoe_master_connid_os; /**< PDO byte offset: FSoE master connection ID. */

  unsigned int fsoe_slave_cmd_os;     /**< PDO byte offset: FSoE slave command. */
  unsigned int fsoe_slave_crc0_os;    /**< PDO byte offset: FSoE slave CRC channel 0. */
  unsigned int fsoe_slave_crc1_os;    /**< PDO byte offset: FSoE slave CRC channel 1. */
  unsigned int fsoe_slave_connid_os;  /**< PDO byte offset: FSoE slave connection ID. */

  unsigned int fsoe_in_sto0_os;    /**< PDO byte offset: STO input axis 0. */
  unsigned int fsoe_in_sto0_bp;    /**< PDO bit position: STO input axis 0. */
  unsigned int fsoe_in_sto1_os;    /**< PDO byte offset: STO input axis 1. */
  unsigned int fsoe_in_sto1_bp;    /**< PDO bit position: STO input axis 1. */

} lcec_ax5805_data_t;

static const lcec_pindesc_t slave_pins_1ch[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc0), "%s.%s.%s.fsoe-master-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc0), "%s.%s.%s.fsoe-slave-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto0), "%s.%s.%s.fsoe-in-sto" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_pins_2ch[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc0), "%s.%s.%s.fsoe-master-crc-0" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc1), "%s.%s.%s.fsoe-master-crc-1" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc0), "%s.%s.%s.fsoe-slave-crc-0" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc1), "%s.%s.%s.fsoe-slave-crc-1" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto0), "%s.%s.%s.fsoe-in-sto-0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto1), "%s.%s.%s.fsoe-in-sto-1" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

void lcec_ax5805_chancount(struct lcec_slave *slave);
void lcec_ax5805_read(struct lcec_slave *slave, long period);

int lcec_ax5805_preinit(struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  struct lcec_slave *ax5n_slave;

  // try to find corresponding ax5n
  ax5n_slave = lcec_slave_by_index(master, slave->index - 1);
  if (ax5n_slave == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: Unable to find corresponding AX5nxx with index %d.\n", master->name, slave->name, slave->index - 1);
    return -EINVAL;
  }

  // check for AX5nxx
  if (ax5n_slave->proc_preinit != lcec_ax5100_preinit && ax5n_slave->proc_preinit != lcec_ax5200_preinit) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: Slave with index %d is not an AX5nxx.\n", master->name, slave->name, ax5n_slave->index);
    return -EINVAL;
  }

  // call AX52xx preinit to solve dependency
  ax5n_slave->proc_preinit(ax5n_slave);

  // use FSOE config from AX5nxx
  slave->fsoeConf = ax5n_slave->fsoeConf;
  if (slave->fsoeConf == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: Corresponding AX5nxx with index %d has no FSOE config.\n", master->name, slave->name, ax5n_slave->index);
    return -EINVAL;
  }

  // set PDO count
  slave->pdo_entry_count = 4 + 3 * slave->fsoeConf->data_channels;

  return 0;
}

int lcec_ax5805_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_ax5805_data_t *hal_data;
  int err;
  const lcec_pindesc_t *slave_pins;

  // initialize callbacks
  slave->proc_read = lcec_ax5805_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ax5805_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ax5805_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x01, &hal_data->fsoe_master_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x02, &hal_data->fsoe_master_connid_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x01, &hal_data->fsoe_slave_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x02, &hal_data->fsoe_slave_connid_os, NULL);

  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x03, &hal_data->fsoe_master_crc0_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x03, &hal_data->fsoe_slave_crc0_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6640, 0x00, &hal_data->fsoe_in_sto0_os, &hal_data->fsoe_in_sto0_bp);

  if (slave->fsoeConf->data_channels >= 2) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x04, &hal_data->fsoe_master_crc1_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x04, &hal_data->fsoe_slave_crc1_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6E40, 0x00, &hal_data->fsoe_in_sto1_os, &hal_data->fsoe_in_sto1_bp);

    slave_pins = slave_pins_2ch;
  } else {
    slave_pins = slave_pins_1ch;
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

void lcec_ax5805_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ax5805_data_t *hal_data = (lcec_ax5805_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  copy_fsoe_data(slave, hal_data->fsoe_slave_cmd_os, hal_data->fsoe_master_cmd_os);

  *(hal_data->fsoe_master_cmd) = EC_READ_U8(&pd[hal_data->fsoe_master_cmd_os]);
  *(hal_data->fsoe_master_connid) = EC_READ_U16(&pd[hal_data->fsoe_master_connid_os]);
  *(hal_data->fsoe_slave_cmd) = EC_READ_U8(&pd[hal_data->fsoe_slave_cmd_os]);
  *(hal_data->fsoe_slave_connid) = EC_READ_U16(&pd[hal_data->fsoe_slave_connid_os]);

  *(hal_data->fsoe_master_crc0) = EC_READ_U16(&pd[hal_data->fsoe_master_crc0_os]);
  *(hal_data->fsoe_slave_crc0) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc0_os]);
  *(hal_data->fsoe_in_sto0) = EC_READ_BIT(&pd[hal_data->fsoe_in_sto0_os], hal_data->fsoe_in_sto0_bp);

  if (slave->fsoeConf->data_channels >= 2) {
    *(hal_data->fsoe_master_crc1) = EC_READ_U16(&pd[hal_data->fsoe_master_crc1_os]);
    *(hal_data->fsoe_slave_crc1) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc1_os]);
    *(hal_data->fsoe_in_sto1) = EC_READ_BIT(&pd[hal_data->fsoe_in_sto1_os], hal_data->fsoe_in_sto1_bp);
  }
}
