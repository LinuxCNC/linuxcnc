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
#include "lcec_el2xxx.h"

typedef struct {
  hal_bit_t *out;
  hal_bit_t invert;
  unsigned int pdo_os;
  unsigned int pdo_bp;
} lcec_el2xxx_pin_t;

void lcec_el2xxx_write(struct lcec_slave *slave, long period);

int lcec_el2xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el2xxx_pin_t *hal_data;
  lcec_el2xxx_pin_t *pin;
  int i;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el2xxx_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el2xxx_pin_t) * slave->pdo_entry_count)) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el2xxx_pin_t) * slave->pdo_entry_count);
  slave->hal_data = hal_data;

  // initialize pins
  for (i=0, pin=hal_data; i<slave->pdo_entry_count; i++, pin++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x01, &pin->pdo_os, &pin->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf(HAL_BIT, HAL_IN, (void **) &(pin->out), "%s.%s.%s.dout-%d", LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
    if ((err = lcec_param_newf(HAL_BIT, HAL_RW, (void *) &(pin->invert), "%s.%s.%s.dout-%d-invert", LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(pin->out) = 0;
    pin->invert = 0;
  }

  return 0;
}

void lcec_el2xxx_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2xxx_pin_t *hal_data = (lcec_el2xxx_pin_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_el2xxx_pin_t *pin;
  int i, s;

  // set outputs
  for (i=0, pin=hal_data; i<slave->pdo_entry_count; i++, pin++) {
    s = *(pin->out);
    if (pin->invert) {
      s = !s;
    }
    EC_WRITE_BIT(&pd[pin->pdo_os], pin->pdo_bp, s);
  }
}

