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
#ifndef _LCEC_GENERIC_H_
#define _LCEC_GENERIC_H_

#include "lcec.h"
#include "lcec_conf.h"

typedef struct {
  char name[LCEC_CONF_STR_MAXLEN];
  hal_type_t type;
  LCEC_PDOENT_TYPE_T subType;
  hal_float_t floatScale;
  hal_float_t floatOffset;
  uint8_t bitOffset;
  uint8_t bitLength;
  hal_pin_dir_t dir;
  void *pin[LCEC_CONF_GENERIC_MAX_SUBPINS];
  uint16_t pdo_idx;
  uint8_t pdo_sidx;
  unsigned int pdo_os;
  unsigned int pdo_bp;
} lcec_generic_pin_t;

int lcec_generic_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif

