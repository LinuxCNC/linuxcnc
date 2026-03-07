/**
 * @file el2904.c
 * @brief HAL driver for the Beckhoff EL2904 4-channel TwinSAFE digital output
 *        terminal.
 *
 * The EL2904 is a functional safety (IEC 61508 SIL 3 / EN ISO 13849 PLe)
 * digital output terminal.  It implements the Fail-Safe over EtherCAT (FsoE)
 * protocol so that the safety outputs are controlled by a TwinSAFE master or
 * an external FsoE master running in LinuxCNC.
 *
 * EtherCAT identifiers:
 *  - Vendor ID  : 0x00000002 (Beckhoff)
 *  - Product code: 0x0B583052
 *
 * HAL pins exported:
 *  - @c fsoe-master-cmd    (HAL_U32, OUT) — FsoE command byte received from master PDO
 *  - @c fsoe-master-crc    (HAL_U32, OUT) — FsoE CRC word received from master PDO
 *  - @c fsoe-master-connid (HAL_U32, OUT) — FsoE connection ID from master PDO
 *  - @c fsoe-slave-cmd     (HAL_U32, OUT) — FsoE command byte in slave response PDO
 *  - @c fsoe-slave-crc     (HAL_U32, OUT) — FsoE CRC word in slave response PDO
 *  - @c fsoe-slave-connid  (HAL_U32, OUT) — FsoE connection ID in slave response PDO
 *  - @c fsoe-out-0 … fsoe-out-3 (HAL_BIT, OUT) — readback of safety outputs 0–3
 *  - @c out-0 … out-3      (HAL_BIT, IN)  — non-safety output commands 0–3
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
#include "el2904.h"

/**
 * @brief HAL data structure for the EL2904 TwinSAFE digital output slave.
 *
 * Contains HAL pin pointers and PDO byte offsets / bit positions for both the
 * FsoE safety channel (master and slave frames) and the four standard digital
 * output channels.
 */
typedef struct {
  hal_u32_t *fsoe_master_cmd;       /**< HAL OUT pin: FsoE master command byte (0x7000:01). */
  hal_u32_t *fsoe_master_crc;       /**< HAL OUT pin: FsoE master CRC word (0x7000:02). */
  hal_u32_t *fsoe_master_connid;    /**< HAL OUT pin: FsoE master connection ID (0x7000:03). */

  hal_u32_t *fsoe_slave_cmd;        /**< HAL OUT pin: FsoE slave command byte (0x6000:01). */
  hal_u32_t *fsoe_slave_crc;        /**< HAL OUT pin: FsoE slave CRC word (0x6000:03). */
  hal_u32_t *fsoe_slave_connid;     /**< HAL OUT pin: FsoE slave connection ID (0x6000:04). */

  hal_bit_t *fsoe_out_0;            /**< HAL OUT pin: safety output 0 readback (0x7001:01). */
  hal_bit_t *fsoe_out_1;            /**< HAL OUT pin: safety output 1 readback (0x7001:02). */
  hal_bit_t *fsoe_out_2;            /**< HAL OUT pin: safety output 2 readback (0x7001:03). */
  hal_bit_t *fsoe_out_3;            /**< HAL OUT pin: safety output 3 readback (0x7001:04). */

  hal_bit_t *out_0;                 /**< HAL IN pin: standard output channel 0 command (0x7010:01). */
  hal_bit_t *out_1;                 /**< HAL IN pin: standard output channel 1 command (0x7010:02). */
  hal_bit_t *out_2;                 /**< HAL IN pin: standard output channel 2 command (0x7010:03). */
  hal_bit_t *out_3;                 /**< HAL IN pin: standard output channel 3 command (0x7010:04). */

  unsigned int fsoe_master_cmd_os;      /**< Byte offset of fsoe_master_cmd in the process image. */
  unsigned int fsoe_master_crc_os;      /**< Byte offset of fsoe_master_crc in the process image. */
  unsigned int fsoe_master_connid_os;   /**< Byte offset of fsoe_master_connid in the process image. */

  unsigned int fsoe_slave_cmd_os;       /**< Byte offset of fsoe_slave_cmd in the process image. */
  unsigned int fsoe_slave_crc_os;       /**< Byte offset of fsoe_slave_crc in the process image. */
  unsigned int fsoe_slave_connid_os;    /**< Byte offset of fsoe_slave_connid in the process image. */

  unsigned int fsoe_out_0_os;   /**< Byte offset of fsoe_out_0 in the process image. */
  unsigned int fsoe_out_0_bp;   /**< Bit position of fsoe_out_0 within its byte. */
  unsigned int fsoe_out_1_os;   /**< Byte offset of fsoe_out_1 in the process image. */
  unsigned int fsoe_out_1_bp;   /**< Bit position of fsoe_out_1 within its byte. */
  unsigned int fsoe_out_2_os;   /**< Byte offset of fsoe_out_2 in the process image. */
  unsigned int fsoe_out_2_bp;   /**< Bit position of fsoe_out_2 within its byte. */
  unsigned int fsoe_out_3_os;   /**< Byte offset of fsoe_out_3 in the process image. */
  unsigned int fsoe_out_3_bp;   /**< Bit position of fsoe_out_3 within its byte. */

  unsigned int out_0_os;    /**< Byte offset of out_0 in the process image. */
  unsigned int out_0_bp;    /**< Bit position of out_0 within its byte. */
  unsigned int out_1_os;    /**< Byte offset of out_1 in the process image. */
  unsigned int out_1_bp;    /**< Bit position of out_1 within its byte. */
  unsigned int out_2_os;    /**< Byte offset of out_2 in the process image. */
  unsigned int out_2_bp;    /**< Bit position of out_2 within its byte. */
  unsigned int out_3_os;    /**< Byte offset of out_3 in the process image. */
  unsigned int out_3_bp;    /**< Bit position of out_3 within its byte. */

} lcec_el2904_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_crc), "%s.%s.%s.fsoe-master-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_crc), "%s.%s.%s.fsoe-slave-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_0), "%s.%s.%s.fsoe-out-0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_1), "%s.%s.%s.fsoe-out-1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_2), "%s.%s.%s.fsoe-out-2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_3), "%s.%s.%s.fsoe-out-3" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_0), "%s.%s.%s.out-0" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_1), "%s.%s.%s.out-1" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_2), "%s.%s.%s.out-2" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_3), "%s.%s.%s.out-3" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const LCEC_CONF_FSOE_T fsoe_conf = {
  .slave_data_len = 1,
  .master_data_len = 1,
  .data_channels = 1
};

/** @brief Forward declaration of the periodic read callback. */
void lcec_el2904_read(struct lcec_slave *slave, long period);
/** @brief Forward declaration of the periodic write callback. */
void lcec_el2904_write(struct lcec_slave *slave, long period);

/**
 * @brief Pre-initialization hook — attach the FsoE configuration to the slave.
 *
 * Called by the lcec core before lcec_el2904_init().  Sets the slave's FsoE
 * configuration pointer so the core can allocate the appropriate safety PDOs.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @return 0 on success.
 */
int lcec_el2904_preinit(struct lcec_slave *slave) {
  // set fsoe config
  slave->fsoeConf = &fsoe_conf;

  return 0;
}

/**
 * @brief Initialize the EL2904 slave: allocate HAL memory, register PDOs, and
 *        export HAL pins.
 *
 * Allocates and zeroes the per-slave HAL data structure, registers all FsoE
 * master/slave frame PDO entries and the four standard output PDO entries, then
 * exports the corresponding HAL pins.
 *
 * @param comp_id        HAL component ID returned by hal_init().
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced
 *                       by the number of entries registered.
 * @return 0 on success, -EIO on memory allocation failure, or a negative HAL
 *         error code if pin export fails.
 */
int lcec_el2904_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el2904_read;
  slave->proc_write = lcec_el2904_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el2904_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el2904_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->fsoe_master_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7001, 0x01, &hal_data->fsoe_out_0_os, &hal_data->fsoe_out_0_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7001, 0x02, &hal_data->fsoe_out_1_os, &hal_data->fsoe_out_1_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7001, 0x03, &hal_data->fsoe_out_2_os, &hal_data->fsoe_out_2_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7001, 0x04, &hal_data->fsoe_out_3_os, &hal_data->fsoe_out_3_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x02, &hal_data->fsoe_master_crc_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x03, &hal_data->fsoe_master_connid_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01, &hal_data->out_0_os, &hal_data->out_0_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x02, &hal_data->out_1_os, &hal_data->out_1_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x03, &hal_data->out_2_os, &hal_data->out_2_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x04, &hal_data->out_3_os, &hal_data->out_3_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->fsoe_slave_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x03, &hal_data->fsoe_slave_crc_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x04, &hal_data->fsoe_slave_connid_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

/**
 * @brief Periodic read callback — copies FsoE frame data and safety output
 *        readbacks from the EtherCAT process data into HAL pins.
 *
 * Calls copy_fsoe_data() to mirror the slave's FsoE response frame back into
 * the master PDO area, then reads all six FsoE frame fields (cmd, crc,
 * connid for both master and slave) and the four safety-output readback bits
 * into their respective HAL OUT pins.
 *
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el2904_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data = (lcec_el2904_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  copy_fsoe_data(slave, hal_data->fsoe_slave_cmd_os, hal_data->fsoe_master_cmd_os);

  *(hal_data->fsoe_slave_cmd) = EC_READ_U8(&pd[hal_data->fsoe_slave_cmd_os]);
  *(hal_data->fsoe_slave_crc) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc_os]);
  *(hal_data->fsoe_slave_connid) = EC_READ_U16(&pd[hal_data->fsoe_slave_connid_os]);

  *(hal_data->fsoe_master_cmd) = EC_READ_U8(&pd[hal_data->fsoe_master_cmd_os]);
  *(hal_data->fsoe_master_crc) = EC_READ_U16(&pd[hal_data->fsoe_master_crc_os]);
  *(hal_data->fsoe_master_connid) = EC_READ_U16(&pd[hal_data->fsoe_master_connid_os]);

  *(hal_data->fsoe_out_0) = EC_READ_BIT(&pd[hal_data->fsoe_out_0_os], hal_data->fsoe_out_0_bp);
  *(hal_data->fsoe_out_1) = EC_READ_BIT(&pd[hal_data->fsoe_out_1_os], hal_data->fsoe_out_1_bp);
  *(hal_data->fsoe_out_2) = EC_READ_BIT(&pd[hal_data->fsoe_out_2_os], hal_data->fsoe_out_2_bp);
  *(hal_data->fsoe_out_3) = EC_READ_BIT(&pd[hal_data->fsoe_out_3_os], hal_data->fsoe_out_3_bp);
}

/**
 * @brief Periodic write callback — copies HAL output pin values to the
 *        EtherCAT process data image.
 *
 * Writes the four standard output channel bits (@c out_0 … @c out_3) from
 * their HAL IN pins into the corresponding bit positions in the master process
 * data image.  The FsoE safety outputs are managed independently by the FsoE
 * master stack and are not written here.
 *
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el2904_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data = (lcec_el2904_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  EC_WRITE_BIT(&pd[hal_data->out_0_os], hal_data->out_0_bp, *(hal_data->out_0));
  EC_WRITE_BIT(&pd[hal_data->out_1_os], hal_data->out_1_bp, *(hal_data->out_1));
  EC_WRITE_BIT(&pd[hal_data->out_2_os], hal_data->out_2_bp, *(hal_data->out_2));
  EC_WRITE_BIT(&pd[hal_data->out_3_os], hal_data->out_3_bp, *(hal_data->out_3));
}
