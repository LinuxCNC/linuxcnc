//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_el95xx.h"

typedef struct {
  hal_bit_t *power_ok;
  hal_bit_t *overload;
  unsigned int power_ok_pdo_os;
  unsigned int power_ok_pdo_bp;
  unsigned int overload_pdo_os;
  unsigned int overload_pdo_bp;
} lcec_el95xx_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el95xx_data_t, power_ok), "%s.%s.%s.power-ok" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el95xx_data_t, overload), "%s.%s.%s.overload" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

void lcec_el95xx_read(struct lcec_slave *slave, long period);

int lcec_el95xx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el95xx_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el95xx_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el95xx_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el95xx_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->power_ok_pdo_os, &hal_data->power_ok_pdo_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x02, &hal_data->overload_pdo_os, &hal_data->overload_pdo_bp);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

void lcec_el95xx_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el95xx_data_t *hal_data = (lcec_el95xx_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  *(hal_data->power_ok) = EC_READ_BIT(&pd[hal_data->power_ok_pdo_os], hal_data->power_ok_pdo_bp);
  *(hal_data->overload) = EC_READ_BIT(&pd[hal_data->overload_pdo_os], hal_data->overload_pdo_bp);
}

