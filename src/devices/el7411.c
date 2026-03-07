/** @file el7411.c
 * @brief Driver for the Beckhoff EL7411 BLDC motor terminal.
 *
 * Configures Hall-sensor based commutation and all motor electrical
 * parameters via CoE SDOs during slave initialisation, then reuses the
 * EL7211 CiA-402 velocity-mode HAL logic (PDO mapping, read/write callbacks,
 * and pin export) without modification.
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

#include "hal.h"

#include "../lcec.h"
#include "el7411.h"

/**
 * @brief Initialise the EL7411 BLDC motor terminal.
 *
 * Applies the following fixed SDO settings:
 * - 0x7010:03 = 9  (velocity mode)
 * - 0x8010:64 = 2  (Hall-sensor commutation)
 * - 0x800A:02 = 1  (enable Hall power supply)
 *
 * Then applies optional motor parameter SDOs from module params and
 * calls lcec_el7211_init() to complete PDO registration and HAL setup.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7411_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;

  // set to velo mode
  if (ecrt_slave_config_sdo8(slave->config, 0x7010, 0x03, 9) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo velo mode\n", master->name, slave->name);
    return -1;
  }

  // set commutation type to hall sensord
  if (ecrt_slave_config_sdo8(slave->config, 0x8010, 0x64, 2) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo commutation type\n", master->name, slave->name);
    return -1;
  }

  // enable hall power supply sensord
  if (ecrt_slave_config_sdo8(slave->config, 0x800A, 0x02, 1) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo hall enable supply\n", master->name, slave->name);
    return -1;
  }

  // set config patameters
  for (p = slave->modparams; p != NULL && p->id >= 0; p++) {
    switch(p->id) {
      case LCEC_EL7411_PARAM_DCLINK_NOM:
        if (ecrt_slave_config_sdo32(slave->config, 0x8010, 0x19, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo dcLinkNominal\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_DCLINK_MIN:
        if (ecrt_slave_config_sdo32(slave->config, 0x8010, 0x1A, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo dcLinkMin\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_DCLINK_MAX:
        if (ecrt_slave_config_sdo32(slave->config, 0x8010, 0x1B, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo dcLinkMax\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_MAX_CURR:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x11, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo maxCurrent\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_RATED_CURR:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x12, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ratedCurrent\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_RATED_VOLT:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x2F, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ratedVoltage\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_POLE_PAIRS:
        if (ecrt_slave_config_sdo8(slave->config, 0x8011, 0x13, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo polePairs\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_RESISTANCE:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x30, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo coilRes\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_INDUCTANCE:
        if (ecrt_slave_config_sdo16(slave->config, 0x8011, 0x19, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo coilInd\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_TOURQUE_CONST:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x16, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo torqueConst\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_VOLTAGE_CONST:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x31, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo voltageConst\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_ROTOR_INERTIA:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x18, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo rotorInertia\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_MAX_SPEED:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x1B, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo maxSpeed\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_RATED_SPEED:
        if (ecrt_slave_config_sdo32(slave->config, 0x8011, 0x2E, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ratedSpeed\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_TH_TIME_CONST:
        if (ecrt_slave_config_sdo16(slave->config, 0x8011, 0x2D, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ratedSpeed\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_HALL_VOLT:
        if (ecrt_slave_config_sdo32(slave->config, 0x800A, 0x11, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo hallVoltage\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL7411_PARAM_HALL_ADJUST:
        if (ecrt_slave_config_sdo8(slave->config, 0x800A, 0x13, p->value.s32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo hallAdjust\n", master->name, slave->name);
          return -1;
        }
        break;
    }
  }

  return lcec_el7211_init(comp_id, slave, pdo_entry_regs);
}
