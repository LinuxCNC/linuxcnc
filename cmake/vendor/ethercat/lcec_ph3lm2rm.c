//
//    Copyright (C) 2019 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "lcec_ph3lm2rm.h"

#include "lcec_class_enc.h"

typedef struct {
  hal_bit_t *latch_ena_pos;
  hal_bit_t *latch_ena_neg;

  hal_bit_t *error;
  hal_bit_t *latch_valid;
  hal_bit_t *latch_state;
  hal_bit_t *latch_state_not;

  hal_float_t scale;

  lcec_class_enc_data_t enc;

  unsigned int latch_ena_pos_os;
  unsigned int latch_ena_pos_bp;
  unsigned int latch_ena_neg_os;
  unsigned int latch_ena_neg_bp;
  unsigned int error_os;
  unsigned int error_bp;
  unsigned int latch_valid_os;
  unsigned int latch_valid_bp;
  unsigned int latch_state_os;
  unsigned int latch_state_bp;
  unsigned int counter_os;
  unsigned int latch_os;
} lcec_ph3lm2rm_enc_data_t;

typedef struct {
  lcec_ph3lm2rm_enc_data_t ch;

  hal_u32_t *signal_level;

  hal_bit_t *signal_level_warn;
  hal_bit_t *signal_level_err;

  hal_u32_t signal_level_warn_val;
  hal_u32_t signal_level_err_val;

  unsigned int signal_level_os;
} lcec_ph3lm2rm_lm_data_t;

typedef struct {
  lcec_ph3lm2rm_enc_data_t ch;

  hal_bit_t *latch_sel_idx;

  unsigned int latch_sel_idx_os;
  unsigned int latch_sel_idx_bp;
} lcec_ph3lm2rm_rm_data_t;

typedef struct {
  lcec_ph3lm2rm_lm_data_t lms[LCEC_PH3LM2RM_LM_COUNT];
  lcec_ph3lm2rm_rm_data_t rms[LCEC_PH3LM2RM_RM_COUNT];

  hal_bit_t *err_reset;
  hal_bit_t *sync_locked;

  unsigned int err_reset_os;
  unsigned int err_reset_bp;
  unsigned int sync_locked_os;
  unsigned int sync_locked_bp;
} lcec_ph3lm2rm_data_t;

static const lcec_pindesc_t enc_pins[] = {
  { HAL_BIT, HAL_IO, offsetof(lcec_ph3lm2rm_enc_data_t, latch_ena_pos), "%s.%s.%s.%s-latch-ena-pos" },
  { HAL_BIT, HAL_IO, offsetof(lcec_ph3lm2rm_enc_data_t, latch_ena_neg), "%s.%s.%s.%s-latch-ena-neg" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_enc_data_t, error), "%s.%s.%s.%s-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_enc_data_t, latch_valid), "%s.%s.%s.%s-latch-valid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_enc_data_t, latch_state), "%s.%s.%s.%s-latch-state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_enc_data_t, latch_state_not), "%s.%s.%s.%s-latch-state-not" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t enc_params[] = {
  { HAL_FLOAT, HAL_RW, offsetof(lcec_ph3lm2rm_enc_data_t, scale), "%s.%s.%s.%s-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t lm_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_ph3lm2rm_lm_data_t, signal_level), "%s.%s.%s.%s-signal-level" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_lm_data_t, signal_level_warn), "%s.%s.%s.%s-signal-level-warn" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_lm_data_t, signal_level_err), "%s.%s.%s.%s-signal-level-err" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t lm_params[] = {
  { HAL_U32, HAL_RW, offsetof(lcec_ph3lm2rm_lm_data_t, signal_level_warn_val), "%s.%s.%s.%s-signal-level-warn-val" },
  { HAL_U32, HAL_RW, offsetof(lcec_ph3lm2rm_lm_data_t, signal_level_err_val), "%s.%s.%s.%s-signal-level-err-val" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t rm_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_ph3lm2rm_rm_data_t, latch_sel_idx), "%s.%s.%s.%s-latch-sel-idx" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_ph3lm2rm_data_t, err_reset), "%s.%s.%s.err-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ph3lm2rm_data_t, sync_locked), "%s.%s.%s.sync-locked" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

int lcec_ph3lm2rm_enc_init(struct lcec_slave *slave, lcec_ph3lm2rm_enc_data_t *hal_data, const char *pfx, double scale);
int lcec_ph3lm2rm_lm_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, int idx, lcec_ph3lm2rm_lm_data_t *hal_data, const char *pfx);
int lcec_ph3lm2rm_rm_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, int idx, lcec_ph3lm2rm_rm_data_t *hal_data, const char *pfx);

void lcec_ph3lm2rm_read(struct lcec_slave *slave, long period);
void lcec_ph3lm2rm_write(struct lcec_slave *slave, long period);

void lcec_ph3lm2rm_enc_read(uint8_t *pd, lcec_ph3lm2rm_enc_data_t *ch);
void lcec_ph3lm2rm_enc_write(uint8_t *pd, lcec_ph3lm2rm_enc_data_t *ch);

int lcec_ph3lm2rm_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_ph3lm2rm_data_t *hal_data;
  char pfx[HAL_NAME_LEN];
  int err;
  int i;
  lcec_ph3lm2rm_rm_data_t *rm;
  lcec_ph3lm2rm_lm_data_t *lm;

  // initialize callbacks
  slave->proc_read = lcec_ph3lm2rm_read;
  slave->proc_write = lcec_ph3lm2rm_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ph3lm2rm_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ph3lm2rm_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000, 0x01, &hal_data->err_reset_os, &hal_data->err_reset_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01, &hal_data->sync_locked_os, &hal_data->sync_locked_bp);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  // init subclasses
  for (i=0, lm = hal_data->lms; i<LCEC_PH3LM2RM_LM_COUNT; i++, lm++, pdo_entry_regs += LCEC_PH3LM2RM_LM_PDOS) {
    rtapi_snprintf(pfx, HAL_NAME_LEN, "lm%d", i);
    if ((err = lcec_ph3lm2rm_lm_init(slave, pdo_entry_regs, 0x10 + i, lm, pfx)) != 0) {
      return err;
    }
  }
  for (i=0, rm = hal_data->rms; i<LCEC_PH3LM2RM_RM_COUNT; i++, rm++, pdo_entry_regs += LCEC_PH3LM2RM_RM_PDOS) {
    rtapi_snprintf(pfx, HAL_NAME_LEN, "rm%d", i);
    if ((err = lcec_ph3lm2rm_rm_init(slave, pdo_entry_regs, 0x20 + i, rm, pfx)) != 0) {
      return err;
    }
  }

  return 0;
}

int lcec_ph3lm2rm_enc_init(struct lcec_slave *slave, lcec_ph3lm2rm_enc_data_t *hal_data, const char *pfx, double scale) {
  lcec_master_t *master = slave->master;
  int err;

  // init encoder
  if ((err = class_enc_init(slave, &hal_data->enc, 32, pfx)) != 0) {
    return err;
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, enc_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, enc_params, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // initialize variables
  hal_data->scale = scale;

  return 0;
}

int lcec_ph3lm2rm_lm_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, int ios, lcec_ph3lm2rm_lm_data_t *hal_data, const char *pfx) {
  lcec_master_t *master = slave->master;
  int err;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + ios, 0x01, &hal_data->ch.latch_ena_pos_os, &hal_data->ch.latch_ena_pos_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + ios, 0x02, &hal_data->ch.latch_ena_neg_os, &hal_data->ch.latch_ena_neg_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x01, &hal_data->ch.error_os, &hal_data->ch.error_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x02, &hal_data->ch.latch_valid_os, &hal_data->ch.latch_valid_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x03, &hal_data->ch.latch_state_os, &hal_data->ch.latch_state_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x04, &hal_data->signal_level_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x05, &hal_data->ch.counter_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x06, &hal_data->ch.latch_os, NULL);

  // init channel
  if ((err = lcec_ph3lm2rm_enc_init(slave, &hal_data->ch, pfx, 0.0005)) != 0) {
    return err;
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, lm_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  // export parameters
  if ((err = lcec_param_newf_list(hal_data, lm_params, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  return 0;
}

int lcec_ph3lm2rm_rm_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, int ios, lcec_ph3lm2rm_rm_data_t *hal_data, const char *pfx) {
  lcec_master_t *master = slave->master;
  int err;

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + ios, 0x01, &hal_data->latch_sel_idx_os, &hal_data->latch_sel_idx_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + ios, 0x02, &hal_data->ch.latch_ena_pos_os, &hal_data->ch.latch_ena_pos_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + ios, 0x03, &hal_data->ch.latch_ena_neg_os, &hal_data->ch.latch_ena_neg_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x01, &hal_data->ch.error_os, &hal_data->ch.error_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x02, &hal_data->ch.latch_valid_os, &hal_data->ch.latch_valid_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x03, &hal_data->ch.latch_state_os, &hal_data->ch.latch_state_bp);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x04, &hal_data->ch.counter_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + ios, 0x05, &hal_data->ch.latch_os, NULL);

  // init channel
  if ((err = lcec_ph3lm2rm_enc_init(slave, &hal_data->ch, pfx, 1.0)) != 0) {
    return err;
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, rm_pins, LCEC_MODULE_NAME, master->name, slave->name, pfx)) != 0) {
    return err;
  }

  return 0;
}

void lcec_ph3lm2rm_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ph3lm2rm_data_t *hal_data = (lcec_ph3lm2rm_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_ph3lm2rm_rm_data_t *rm;
  lcec_ph3lm2rm_lm_data_t *lm;

  *(hal_data->sync_locked) = EC_READ_BIT(&pd[hal_data->sync_locked_os], hal_data->sync_locked_bp);

  for (i=0, lm = hal_data->lms; i<LCEC_PH3LM2RM_LM_COUNT; i++, lm++) {
    lcec_ph3lm2rm_enc_read(pd, &lm->ch);
    *(lm->signal_level) = EC_READ_U32(&pd[lm->signal_level_os]);
    *(lm->signal_level_warn) = lm->signal_level_warn_val > 0 && *(lm->signal_level) < lm->signal_level_warn_val;
    *(lm->signal_level_err) = lm->signal_level_err_val > 0 && *(lm->signal_level) < lm->signal_level_err_val;
  }

  for (i=0, rm = hal_data->rms; i<LCEC_PH3LM2RM_RM_COUNT; i++, rm++) {
    lcec_ph3lm2rm_enc_read(pd, &rm->ch);
  }
}

void lcec_ph3lm2rm_enc_read(uint8_t *pd, lcec_ph3lm2rm_enc_data_t *ch) {
  uint32_t counter, latch; 

  // read bit values
  *(ch->error) = EC_READ_BIT(&pd[ch->error_os], ch->error_bp);
  *(ch->latch_valid) = EC_READ_BIT(&pd[ch->latch_valid_os], ch->latch_valid_bp);
  *(ch->latch_state) = EC_READ_BIT(&pd[ch->latch_state_os], ch->latch_state_bp);
  *(ch->latch_state_not) = !*(ch->latch_state);

  // read counter values
  counter = EC_READ_U32(&pd[ch->counter_os]);
  latch = EC_READ_U32(&pd[ch->latch_os]);

  // update encoder
  class_enc_update(&ch->enc, 0, ch->scale, counter, latch, *(ch->latch_valid));

  // reset latch enable, if captured
  if (*(ch->latch_valid)) {
    *(ch->latch_ena_pos) = 0;
    *(ch->latch_ena_neg) = 0;
  }
}

void lcec_ph3lm2rm_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ph3lm2rm_data_t *hal_data = (lcec_ph3lm2rm_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_ph3lm2rm_rm_data_t *rm;
  lcec_ph3lm2rm_lm_data_t *lm;

  EC_WRITE_BIT(&pd[hal_data->err_reset_os], hal_data->err_reset_bp, *(hal_data->err_reset));

  for (i=0, lm = hal_data->lms; i<LCEC_PH3LM2RM_LM_COUNT; i++, lm++) {
    lcec_ph3lm2rm_enc_write(pd, &lm->ch);
  }

  for (i=0, rm = hal_data->rms; i<LCEC_PH3LM2RM_RM_COUNT; i++, rm++) {
    lcec_ph3lm2rm_enc_write(pd, &rm->ch);
    EC_WRITE_BIT(&pd[rm->latch_sel_idx_os], rm->latch_sel_idx_bp, *(rm->latch_sel_idx));
  }
}

void lcec_ph3lm2rm_enc_write(uint8_t *pd, lcec_ph3lm2rm_enc_data_t *ch) {
  // write bit values
  EC_WRITE_BIT(&pd[ch->latch_ena_pos_os], ch->latch_ena_pos_bp, *(ch->latch_ena_pos));
  EC_WRITE_BIT(&pd[ch->latch_ena_neg_os], ch->latch_ena_neg_bp, *(ch->latch_ena_neg));
}

