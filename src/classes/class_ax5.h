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
#ifndef _LCEC_CLASS_AX5_H_
#define _LCEC_CLASS_AX5_H_

#include "lcec.h"
#include "class_enc.h"

#define LCEC_AX5_PARAM_ENABLE_FB2  1
#define LCEC_AX5_PARAM_ENABLE_DIAG 2

typedef struct {
  hal_bit_t *drive_on;

  hal_bit_t *enable;
  hal_bit_t *enabled;
  hal_bit_t *halted;
  hal_bit_t *fault;

  hal_bit_t *halt;

  hal_float_t *velo_cmd;

  int fb2_enabled;
  int diag_enabled;

  hal_u32_t *status;
  hal_float_t *torque_fb_pct;
  hal_u32_t *diag;

  unsigned int status_pdo_os;
  unsigned int pos_fb_pdo_os;
  unsigned int pos_fb2_pdo_os;
  unsigned int torque_fb_pdo_os;
  unsigned int diag_pdo_os;
  unsigned int ctrl_pdo_os;
  unsigned int vel_cmd_pdo_os;

  hal_float_t scale;
  hal_float_t scale_fb2;
  hal_float_t vel_scale;
  hal_u32_t pos_resolution;

  lcec_class_enc_data_t enc;
  lcec_class_enc_data_t enc_fb2;

  double scale_old;
  double scale_rcpt;
  double scale_fb2_old;
  double scale_fb2_rcpt;

  double vel_output_scale;

  int toggle;

} lcec_class_ax5_chan_t;

int lcec_class_ax5_pdos(struct lcec_slave *slave);
int lcec_class_ax5_init(struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs, lcec_class_ax5_chan_t *chan, int index, const char *pfx);
void lcec_class_ax5_read(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan);
void lcec_class_ax5_write(struct lcec_slave *slave, lcec_class_ax5_chan_t *chan);

#endif

