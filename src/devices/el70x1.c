/** @file el70x1.c
 * @brief Driver for Beckhoff EL7031 and EL7041-0052 stepper motor terminals.
 *
 * Implements HAL pin creation, motor parameter SDO configuration, PDO mapping,
 * and cyclic read/write for the EL70x1 family of stepper terminals.  The
 * drive runs in position mode (SDO 0x8012:01 = 3); the 32-bit position
 * setpoint is written each cycle.  An integrated auto-reduce-torque timer
 * engages reduced torque after a configurable idle period.
 *
 * @copyright Copyright (C) 2020-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "el70x1.h"

/**
 * @brief HAL data for the EL70x1 stepper terminal.
 */
typedef struct {
  hal_bit_t auto_fault_reset;     /**< Parameter: 1 = automatically reset faults on enable */

  hal_float_t stm_pos_scale;      /**< Parameter: position scale factor (user units / raw count) */
  hal_float_t *stm_pos_cmd;       /**< IN: commanded position in user units */

  hal_float_t auto_reduce_tourque_delay;  /**< Parameter: seconds of idle before auto-reduce-torque fires */
  long long auto_reduce_tourque_timer;    /**< Internal timer (ns) counting idle time */

  hal_bit_t *stm_ready_to_enable; /**< OUT: drive ready-to-enable status */
  hal_bit_t *stm_ready;           /**< OUT: drive ready status */
  hal_bit_t *stm_warning;         /**< OUT: drive warning active */
  hal_bit_t *stm_error;           /**< OUT: drive error active */
  hal_bit_t *stm_move_pos;        /**< OUT: drive currently moving in positive direction */
  hal_bit_t *stm_move_neg;        /**< OUT: drive currently moving in negative direction */
  hal_bit_t *stm_torque_reduced;  /**< OUT: reduced torque mode currently active */
  hal_bit_t *stm_din1;            /**< OUT: digital input 1 state */
  hal_bit_t *stm_din2;            /**< OUT: digital input 2 state */
  hal_bit_t *stm_sync_err;        /**< OUT: EtherCAT sync error flag */
  hal_bit_t *stm_tx_toggle;       /**< OUT: TxPDO toggle (new data indicator) */
  hal_bit_t *stm_enable;          /**< IN: enable the stepper drive */
  hal_bit_t *stm_reset;           /**< IN: reset drive faults */
  hal_bit_t *stm_reduce_torque;   /**< IN: manually request reduced torque */
  hal_s32_t *stm_pos_cmd_raw;     /**< OUT: scaled 32-bit raw position setpoint */

  hal_s32_t stm_pos_cmd_raw_last; /**< Previous raw position command for change detection */

  unsigned int stm_ready_to_enable_pdo_os; /**< PDO byte offset: ready-to-enable */
  unsigned int stm_ready_to_enable_pdo_bp; /**< Bit position: ready-to-enable */
  unsigned int stm_ready_pdo_os;           /**< PDO byte offset: ready */
  unsigned int stm_ready_pdo_bp;           /**< Bit position: ready */
  unsigned int stm_warning_pdo_os;         /**< PDO byte offset: warning */
  unsigned int stm_warning_pdo_bp;         /**< Bit position: warning */
  unsigned int stm_error_pdo_os;           /**< PDO byte offset: error */
  unsigned int stm_error_pdo_bp;           /**< Bit position: error */
  unsigned int stm_move_pos_pdo_os;        /**< PDO byte offset: moving positive */
  unsigned int stm_move_pos_pdo_bp;        /**< Bit position: moving positive */
  unsigned int stm_move_neg_pdo_os;        /**< PDO byte offset: moving negative */
  unsigned int stm_move_neg_pdo_bp;        /**< Bit position: moving negative */
  unsigned int stm_torque_reduced_pdo_os;  /**< PDO byte offset: torque reduced */
  unsigned int stm_torque_reduced_pdo_bp;  /**< Bit position: torque reduced */
  unsigned int stm_din1_pdo_os;            /**< PDO byte offset: digital input 1 */
  unsigned int stm_din1_pdo_bp;            /**< Bit position: digital input 1 */
  unsigned int stm_din2_pdo_os;            /**< PDO byte offset: digital input 2 */
  unsigned int stm_din2_pdo_bp;            /**< Bit position: digital input 2 */
  unsigned int stm_sync_err_pdo_os;        /**< PDO byte offset: sync error */
  unsigned int stm_sync_err_pdo_bp;        /**< Bit position: sync error */
  unsigned int stm_tx_toggle_pdo_os;       /**< PDO byte offset: TxPDO toggle */
  unsigned int stm_tx_toggle_pdo_bp;       /**< Bit position: TxPDO toggle */
  unsigned int stm_ena_pdo_os;             /**< PDO byte offset: drive enable command */
  unsigned int stm_ena_pdo_bp;             /**< Bit position: drive enable command */
  unsigned int stm_reset_pdo_os;           /**< PDO byte offset: drive reset command */
  unsigned int stm_reset_pdo_bp;           /**< Bit position: drive reset command */
  unsigned int stm_reduce_torque_pdo_os;   /**< PDO byte offset: reduce-torque command */
  unsigned int stm_reduce_torque_pdo_bp;   /**< Bit position: reduce-torque command */
  unsigned int stm_pos_raw_pdo_os;         /**< PDO byte offset: 32-bit position setpoint */

} lcec_el70x1_data_t;

static ec_pdo_entry_info_t lcec_el70x1_enc_ctl[] = {
    {0x0000, 0x00,  1}, /* Gap */
    {0x7000, 0x02,  1}, /* Enable latch extern on positive edge */
    {0x7000, 0x03,  1}, /* Set counter */
    {0x7000, 0x04,  1}, /* Enable latch extern on negative edge */
    {0x0000, 0x00,  4}, /* Gap */
    {0x0000, 0x00,  8}, /* Gap */
    {0x7000, 0x11, 32}  /* Set counter value */
};

static ec_pdo_entry_info_t lcec_el70x1_stm_ctl[] = {
    {0x7010, 0x01,  1}, /* Enable */
    {0x7010, 0x02,  1}, /* Reset */
    {0x7010, 0x03,  1}, /* Reduce torque */
    {0x0000, 0x00,  5}, /* Gap */
    {0x0000, 0x00,  8}  /* Gap */
};

static ec_pdo_entry_info_t lcec_el70x1_stm_pos[] = {
    {0x7010, 0x11, 32}  /* Position */
};

static ec_pdo_entry_info_t lcec_el70x1_enc_stat[] = {
    {0x0000, 0x00,  1}, /* Gap */
    {0x6000, 0x02,  1}, /* Latch extern valid */
    {0x6000, 0x03,  1}, /* Set counter done */
    {0x6000, 0x04,  1}, /* Counter underflow */
    {0x6000, 0x05,  1}, /* Counter overflow */
    {0x0000, 0x00,  3}, /* Gap */
    {0x0000, 0x00,  4}, /* Gap */
    {0x6000, 0x0d,  1}, /* Status of extern latch */
    {0x6000, 0x0e,  1}, /* Sync error */
    {0x0000, 0x00,  1}, /* Gap */
    {0x6000, 0x10,  1}, /* TxPDO Toggle */
    {0x6000, 0x11, 32}, /* Counter value */
    {0x6000, 0x12, 32}  /* Latch value */
};

static ec_pdo_entry_info_t lcec_el70x1_stm_stat[] = {
    {0x6010, 0x01,  1}, /* Ready to enable */
    {0x6010, 0x02,  1}, /* Ready */
    {0x6010, 0x03,  1}, /* Warning */
    {0x6010, 0x04,  1}, /* Error */
    {0x6010, 0x05,  1}, /* Moving positive */
    {0x6010, 0x06,  1}, /* Moving negative */
    {0x6010, 0x07,  1}, /* Torque reduced */
    {0x0000, 0x00,  1}, /* Gap */
    {0x0000, 0x00,  3}, /* Gap */
    {0x6010, 0x0c,  1}, /* Digital input 1 */
    {0x6010, 0x0d,  1}, /* Digital input 2 */
    {0x6010, 0x0e,  1}, /* Sync error */
    {0x0000, 0x00,  1}, /* Gap */
    {0x6010, 0x10,  1}  /* TxPDO Toggle */
};

static ec_pdo_info_t lcec_el70x1_pdos_out[] = {
    {0x1601, 7, lcec_el70x1_enc_ctl}, /* ENC RxPDO-Map Control compact */
    {0x1602, 5, lcec_el70x1_stm_ctl}, /* STM RxPDO-Map Control */
    {0x1603, 1, lcec_el70x1_stm_pos}  /* STM RxPDO-Map Position */
};

static ec_pdo_info_t lcec_el70x1_pdos_in[] = {
    {0x1a01, 13, lcec_el70x1_enc_stat}, /* ENC TxPDO-Map Status compact */
    {0x1a03, 14, lcec_el70x1_stm_stat}  /* STM TxPDO-Map Status */
};

static ec_sync_info_t lcec_el70x1_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 3, lcec_el70x1_pdos_out},
    {3, EC_DIR_INPUT,  2, lcec_el70x1_pdos_in},
    {0xff}
};

static ec_pdo_entry_info_t lcec_el7041_0052_stm_ctl[] = {
    {0x7010, 0x01,  1}, /* Enable */
    {0x7010, 0x02,  1}, /* Reset */
    {0x7010, 0x03,  1}, /* Reduce torque */
    {0x0000, 0x00,  5}, /* Gap */
    {0x0000, 0x00,  8}  /* Gap */
};

static ec_pdo_entry_info_t lcec_el7041_0052_stm_pos[] = {
    {0x7010, 0x11, 32}  /* Position */
};

static ec_pdo_entry_info_t lcec_el7041_0052_stm_stat[] = {
    {0x6010, 0x01,  1}, /* Ready to enable */
    {0x6010, 0x02,  1}, /* Ready */
    {0x6010, 0x03,  1}, /* Warning */
    {0x6010, 0x04,  1}, /* Error */
    {0x6010, 0x05,  1}, /* Moving positive */
    {0x6010, 0x06,  1}, /* Moving negative */
    {0x6010, 0x07,  1}, /* Torque reduced */
    {0x0000, 0x00,  1}, /* Gap */
    {0x0000, 0x00,  3}, /* Gap */
    {0x6010, 0x0c,  1}, /* Digital input 1 */
    {0x6010, 0x0d,  1}, /* Digital input 2 */
    {0x6010, 0x0e,  1}, /* Sync error */
    {0x0000, 0x00,  1}, /* Gap */
    {0x6010, 0x10,  1}  /* TxPDO Toggle */
};

static ec_pdo_info_t lcec_el7041_0052_pdos_out[] = {
    {0x1602, 5, lcec_el7041_0052_stm_ctl}, /* STM RxPDO-Map Control */
    {0x1603, 1, lcec_el7041_0052_stm_pos}  /* STM RxPDO-Map Velocity */
};

static ec_pdo_info_t lcec_el7041_0052_pdos_in[] = {
    {0x1a03, 14, lcec_el7041_0052_stm_stat}  /* STM TxPDO-Map Status */
};

static ec_sync_info_t lcec_el7041_0052_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 2, lcec_el7041_0052_pdos_out},
    {3, EC_DIR_INPUT,  1, lcec_el7041_0052_pdos_in},
    {0xff}
};

static const lcec_pindesc_t slave_pins[] = {
  // servo pins
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_ready_to_enable), "%s.%s.%s.srv-ready-to-enable" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_ready),           "%s.%s.%s.srv-ready" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_warning),         "%s.%s.%s.srv-warning" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_error),           "%s.%s.%s.srv-error" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_move_pos),        "%s.%s.%s.srv-move-pos" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_move_neg),        "%s.%s.%s.srv-move-neg" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_torque_reduced),  "%s.%s.%s.srv-torque-reduced" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_din1),            "%s.%s.%s.srv-din1" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_din2),            "%s.%s.%s.srv-din2" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_sync_err),        "%s.%s.%s.srv-sync-error" },
  { HAL_BIT,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_tx_toggle),       "%s.%s.%s.srv-tx-toggle" },
  { HAL_BIT,   HAL_IN,  offsetof(lcec_el70x1_data_t, stm_enable),          "%s.%s.%s.srv-enable" },
  { HAL_BIT,   HAL_IN,  offsetof(lcec_el70x1_data_t, stm_reset),           "%s.%s.%s.srv-reset" },
  { HAL_BIT,   HAL_IN,  offsetof(lcec_el70x1_data_t, stm_reduce_torque),   "%s.%s.%s.srv-reduce-torque" },
  { HAL_FLOAT, HAL_IN,  offsetof(lcec_el70x1_data_t, stm_pos_cmd),         "%s.%s.%s.srv-pos-cmd" },
  { HAL_S32,   HAL_OUT, offsetof(lcec_el70x1_data_t, stm_pos_cmd_raw),     "%s.%s.%s.srv-pos-cmd-raw" },

  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el70x1_data_t, stm_pos_scale),  "%s.%s.%s.srv-pos-scale" },
  { HAL_BIT, HAL_RW, offsetof(lcec_el70x1_data_t, auto_fault_reset), "%s.%s.%s.auto-fault-reset" },
  { HAL_FLOAT, HAL_RW, offsetof(lcec_el70x1_data_t, auto_reduce_tourque_delay), "%s.%s.%s.auto-reduce-torque-delay" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

int lcec_el70x1_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);
void lcec_el70x1_read(struct lcec_slave *slave, long period);
void lcec_el70x1_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialise an EL7031 using the full encoder+stepper sync config.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7031_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  // initialize sync info
  slave->sync_info = lcec_el70x1_syncs;

  return lcec_el70x1_init(comp_id, slave, pdo_entry_regs);
}

/**
 * @brief Initialise an EL7041-0052 using the stepper-only sync config.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7041_0052_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  // initialize sync info
  slave->sync_info = lcec_el7041_0052_syncs;

  return lcec_el70x1_init(comp_id, slave, pdo_entry_regs);
}

/**
 * @brief Shared initialisation for all EL70x1 stepper terminals.
 *
 * Sets position mode via SDO (0x8012:01 = 3), applies any motor parameter
 * SDOs from module parameters, allocates HAL memory, maps PDO entries for
 * the stepper status and control registers, and exports HAL pins and params.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el70x1_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;
  lcec_el70x1_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read  = lcec_el70x1_read;
  slave->proc_write = lcec_el70x1_write;

  // set to position mode
  if (ecrt_slave_config_sdo8(slave->config, 0x8012, 0x01, 3) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo position mode\n", master->name, slave->name);
    return -1;
  }

  // set config patameters
  for (p = slave->modparams; p != NULL && p->id >= 0; p++) {
    switch(p->id) {
      case LCEC_EL70x1_PARAM_MAX_CURR:
        if (ecrt_slave_config_sdo16(slave->config, 0x8010, 0x01, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo maxCurrent\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL70x1_PARAM_RED_CURR:
        if (ecrt_slave_config_sdo16(slave->config, 0x8010, 0x02, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo redCurrent\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL70x1_PARAM_NOM_VOLT:
        if (ecrt_slave_config_sdo16(slave->config, 0x8010, 0x03, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo nomVoltage\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL70x1_PARAM_COIL_RES:
        if (ecrt_slave_config_sdo16(slave->config, 0x8010, 0x04, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo coilRes\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL70x1_PARAM_MOTOR_EMF:
        if (ecrt_slave_config_sdo16(slave->config, 0x8010, 0x05, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo motorEMF\n", master->name, slave->name);
          return -1;
        }
        break;
    }
  }

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el70x1_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el70x1_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x01, &hal_data->stm_ready_to_enable_pdo_os, &hal_data->stm_ready_to_enable_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x02, &hal_data->stm_ready_pdo_os,           &hal_data->stm_ready_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x03, &hal_data->stm_warning_pdo_os,         &hal_data->stm_warning_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x04, &hal_data->stm_error_pdo_os,           &hal_data->stm_error_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x05, &hal_data->stm_move_pos_pdo_os,        &hal_data->stm_move_pos_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x06, &hal_data->stm_move_neg_pdo_os,        &hal_data->stm_move_neg_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x07, &hal_data->stm_torque_reduced_pdo_os,  &hal_data->stm_torque_reduced_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x0c, &hal_data->stm_din1_pdo_os,            &hal_data->stm_din1_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x0d, &hal_data->stm_din2_pdo_os,            &hal_data->stm_din2_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x0e, &hal_data->stm_sync_err_pdo_os,        &hal_data->stm_sync_err_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6010, 0x10, &hal_data->stm_tx_toggle_pdo_os,       &hal_data->stm_tx_toggle_pdo_bp);

  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x01, &hal_data->stm_ena_pdo_os,             &hal_data->stm_ena_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x02, &hal_data->stm_reset_pdo_os,           &hal_data->stm_reset_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x03, &hal_data->stm_reduce_torque_pdo_os,   &hal_data->stm_reduce_torque_pdo_bp);

  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7010, 0x11, &hal_data->stm_pos_raw_pdo_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // export pins
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // initialize variables
  hal_data->stm_pos_scale = 1.0;
  hal_data->auto_fault_reset = 1;
  hal_data->auto_reduce_tourque_delay = 0.0;
  hal_data->auto_reduce_tourque_timer = 0;
  hal_data->stm_pos_cmd_raw_last = 0;

  return 0;
}

/**
 * @brief Cyclic read: update stepper drive status HAL pins.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
void lcec_el70x1_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el70x1_data_t *hal_data = (lcec_el70x1_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  *(hal_data->stm_ready_to_enable) = EC_READ_BIT(&pd[hal_data->stm_ready_to_enable_pdo_os], hal_data->stm_ready_to_enable_pdo_bp);
  *(hal_data->stm_ready) = EC_READ_BIT(&pd[hal_data->stm_ready_pdo_os], hal_data->stm_ready_pdo_bp);
  *(hal_data->stm_warning) = EC_READ_BIT(&pd[hal_data->stm_warning_pdo_os], hal_data->stm_warning_pdo_bp);
  *(hal_data->stm_error) = EC_READ_BIT(&pd[hal_data->stm_error_pdo_os], hal_data->stm_error_pdo_bp);
  *(hal_data->stm_move_pos) = EC_READ_BIT(&pd[hal_data->stm_move_pos_pdo_os], hal_data->stm_move_pos_pdo_bp);
  *(hal_data->stm_move_neg) = EC_READ_BIT(&pd[hal_data->stm_move_neg_pdo_os], hal_data->stm_move_neg_pdo_bp);
  *(hal_data->stm_torque_reduced) = EC_READ_BIT(&pd[hal_data->stm_torque_reduced_pdo_os], hal_data->stm_torque_reduced_pdo_bp);
  *(hal_data->stm_din1) = EC_READ_BIT(&pd[hal_data->stm_din1_pdo_os], hal_data->stm_din1_pdo_bp);
  *(hal_data->stm_din2) = EC_READ_BIT(&pd[hal_data->stm_din2_pdo_os], hal_data->stm_din2_pdo_bp);
  *(hal_data->stm_sync_err) = EC_READ_BIT(&pd[hal_data->stm_sync_err_pdo_os], hal_data->stm_sync_err_pdo_bp);
  *(hal_data->stm_tx_toggle) = EC_READ_BIT(&pd[hal_data->stm_tx_toggle_pdo_os], hal_data->stm_tx_toggle_pdo_bp);
}

/**
 * @brief Cyclic write: send position setpoint and control bits to drive.
 *
 * Scales the commanded position to a raw 32-bit integer, manages the
 * auto-reduce-torque timer (engages reduced torque after the configured
 * idle delay), and writes the enable, reset, reduce-torque, and position
 * PDO fields to the process data image.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds.
 */
void lcec_el70x1_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el70x1_data_t *hal_data = (lcec_el70x1_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  bool enabled, reduce_tourque;

  *(hal_data->stm_pos_cmd_raw) = (int32_t) (*(hal_data->stm_pos_cmd) * hal_data->stm_pos_scale);

  enabled = *(hal_data->stm_enable);
  if (!enabled) {
    hal_data->auto_reduce_tourque_timer = 0;
  }
  EC_WRITE_BIT(&pd[hal_data->stm_ena_pdo_os], hal_data->stm_ena_pdo_bp, enabled);

  reduce_tourque = *(hal_data->stm_reduce_torque);
  if (*(hal_data->stm_pos_cmd_raw) != hal_data->stm_pos_cmd_raw_last) {
    hal_data->stm_pos_cmd_raw_last = *(hal_data->stm_pos_cmd_raw);
    hal_data->auto_reduce_tourque_timer = 0;
  }
  if (hal_data->auto_reduce_tourque_delay > 0.0) {
    if (hal_data->auto_reduce_tourque_timer < (long long) (hal_data->auto_reduce_tourque_delay * 1e9)) {
      hal_data->auto_reduce_tourque_timer += period;
    } else {
      reduce_tourque = true;
    }
  }
  EC_WRITE_BIT(&pd[hal_data->stm_reduce_torque_pdo_os], hal_data->stm_reduce_torque_pdo_bp, reduce_tourque);

  EC_WRITE_BIT(&pd[hal_data->stm_reset_pdo_os], hal_data->stm_reset_pdo_bp, *(hal_data->stm_reset));

  EC_WRITE_S32(&pd[hal_data->stm_pos_raw_pdo_os], *(hal_data->stm_pos_cmd_raw));

}
