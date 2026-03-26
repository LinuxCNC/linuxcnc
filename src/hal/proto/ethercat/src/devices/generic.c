/**
 * @file generic.c
 * @brief Generic passthrough slave driver for arbitrary PDO mapping.
 *
 * This driver allows any EtherCAT slave to be used without a dedicated device
 * driver by mapping PDO entries directly to HAL pins via the XML configuration
 * file.  The XML @c \<slave\> element uses @c type="generic" together with
 * nested @c \<syncManager\>, @c \<pdo\>, @c \<pdoEntry\>, and optionally
 * @c \<complexEntry\> elements to describe the process-data layout.
 *
 * The configuration parser (conf.c) calls the @c lcec_generic_conf_* helpers
 * at parse time to build the EtherCAT sync-manager / PDO / PDO-entry tables
 * in dynamically allocated memory.  At realtime init (@c lcec_generic_init)
 * HAL pins are created for every mapped entry that carries a pin name.
 *
 * Supported HAL pin types: @c HAL_BIT (single bit or bit-array up to
 * @c LCEC_CONF_GENERIC_MAX_SUBPINS), @c HAL_S32, @c HAL_U32, @c HAL_FLOAT.
 * Float pins additionally support a scale/offset and an unsigned or IEEE-754
 * sub-type (@c lcecPdoEntTypeFloatUnsigned, @c lcecPdoEntTypeFloatIeee).
 *
 * @copyright Copyright (C) 2011-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include "generic.h"

/** @brief Forward declaration — HAL read callback. */
void lcec_generic_read(struct lcec_slave *slave, long period);
/** @brief Forward declaration — HAL write callback. */
void lcec_generic_write(struct lcec_slave *slave, long period);

/** @brief Read a signed 32-bit value from the process-data image. */
hal_s32_t lcec_generic_read_s32(uint8_t *pd, lcec_generic_pin_t *hal_data);
/** @brief Read an unsigned 32-bit value from the process-data image. */
hal_u32_t lcec_generic_read_u32(uint8_t *pd, lcec_generic_pin_t *hal_data);
/** @brief Write a signed 32-bit value into the process-data image. */
void lcec_generic_write_s32(uint8_t *pd, lcec_generic_pin_t *hal_data, hal_s32_t sval);
/** @brief Write an unsigned 32-bit value into the process-data image. */
void lcec_generic_write_u32(uint8_t *pd, lcec_generic_pin_t *hal_data, hal_u32_t uval);

/**
 * @brief Initialise generic slave configuration at XML parse time.
 *
 * Called by the configuration parser for each @c type="generic" slave element.
 * Sets the slave VID/PID/PDO-entry count, allocates the @c hal_data pin array
 * and the dynamic EtherCAT sync-manager, PDO, and PDO-entry tables.  Also
 * populates @p conf_state so that subsequent @c lcec_generic_conf_sm /
 * @c lcec_generic_conf_pdo / @c lcec_generic_conf_pdo_entry calls can fill in
 * those tables incrementally.
 *
 * @param slave       Slave instance to initialise.
 * @param slave_conf  Parsed XML slave configuration (VID, PID, counts, flags).
 * @param conf_state  Parser state block; updated with pointers to allocated tables.
 * @return 0 on success, -1 on allocation failure.
 */
int lcec_generic_conf_init(lcec_slave_t *slave, LCEC_CONF_SLAVE_T *slave_conf, lcec_generic_conf_state_t *conf_state) {
  // generic slave
  slave->vid = slave_conf->vid;
  slave->pid = slave_conf->pid;
  slave->pdo_entry_count = slave_conf->pdoMappingCount;
  slave->proc_init = lcec_generic_init;

  // alloc hal memory
  if ((slave->hal_data = hal_malloc(sizeof(lcec_generic_pin_t) * slave_conf->pdoMappingCount)) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", slave->master->name, slave_conf->name);
    return -1;
  }
  memset(slave->hal_data, 0, sizeof(lcec_generic_pin_t) * slave_conf->pdoMappingCount);

  // alloc pdo entry memory
  slave->generic.pdo_entries = rtapi_calloc(sizeof(ec_pdo_entry_info_t) * slave_conf->pdoEntryCount);
  if (slave->generic.pdo_entries == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s generic pdo entry memory\n", slave->master->name, slave_conf->name);
    return -1;
  }

  // alloc pdo memory
  slave->generic.pdos = rtapi_calloc(sizeof(ec_pdo_info_t) * slave_conf->pdoCount);
  if (slave->generic.pdos == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s generic pdo memory\n", slave->master->name, slave_conf->name);
    return -1;
  }

  // alloc sync manager memory
  slave->generic.sync_managers = rtapi_calloc(sizeof(ec_sync_info_t) * (slave_conf->syncManagerCount + 1));
  if (slave->generic.sync_managers == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s generic sync manager memory\n", slave->master->name, slave_conf->name);
    return -1;
  }
  slave->generic.sync_managers->index = 0xff;

  if (slave_conf->configPdos) {
    slave->sync_info = slave->generic.sync_managers;
  }

  // init config state
  conf_state->pdo_entries = slave->generic.pdo_entries;
  conf_state->pdos = slave->generic.pdos;
  conf_state->sync_managers = slave->generic.sync_managers;

  return 0;
}

/**
 * @brief Free dynamically allocated generic slave resources.
 *
 * Called during slave destruction to release the EtherCAT sync-manager, PDO,
 * and PDO-entry arrays that were allocated by @c lcec_generic_conf_init.
 *
 * @param slave  Slave instance whose generic resources are to be freed.
 */
void lcec_generic_free_slave(lcec_slave_t *slave) {
  if (slave->generic.pdo_entries != NULL) {
    rtapi_free(slave->generic.pdo_entries);
  }
  if (slave->generic.pdos != NULL) {
    rtapi_free(slave->generic.pdos);
  }
  if (slave->generic.sync_managers != NULL) {
    rtapi_free(slave->generic.sync_managers);
  }
}

/**
 * @brief Register one sync-manager from the XML configuration.
 *
 * Fills the next entry in the sync-manager table and advances the internal
 * pointer.  The sentinel entry (@c index == 0xff) is written after each call
 * so the table is always terminated.  Also derives the HAL pin direction
 * from the EtherCAT sync-manager direction (@c EC_DIR_INPUT → @c HAL_OUT,
 * @c EC_DIR_OUTPUT → @c HAL_IN).
 *
 * @param state    Configuration parser state.
 * @param sm_conf  Parsed sync-manager configuration element.
 * @return 0 on success, -1 if the tables are uninitialised.
 */
int lcec_generic_conf_sm(lcec_generic_conf_state_t *state, LCEC_CONF_SYNCMANAGER_T *sm_conf) {
  // check for syncmanager
  if (state->sync_managers == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Sync manager for generic device missing\n");
    return -1;
  }

  // check for pdos
  if (state->pdos == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "PDOs for generic device missing\n");
    return -1;
  }

  // initialize sync manager
  state->sync_managers->index = sm_conf->index;
  state->sync_managers->dir = sm_conf->dir;
  state->sync_managers->n_pdos = sm_conf->pdoCount;
  state->sync_managers->pdos = sm_conf->pdoCount == 0 ? NULL : state->pdos;

  // get hal direction
  switch (sm_conf->dir) {
    case EC_DIR_INPUT:
      state->hal_dir = HAL_OUT;
      break;
    case EC_DIR_OUTPUT:
      state->hal_dir = HAL_IN;
      break;
    default:
      state->hal_dir = 0;
  }

  // next syncmanager
  state->sync_managers++;
  state->sync_managers->index = 0xff;

  return 0;
}

/**
 * @brief Register one PDO from the XML configuration.
 *
 * Fills the next PDO entry in the PDO table (index, entry count, pointer to
 * the first PDO-entry slot) and advances the internal PDO pointer.
 *
 * @param state     Configuration parser state.
 * @param pdo_conf  Parsed PDO configuration element.
 * @return 0 on success, -1 if the tables are uninitialised.
 */
int lcec_generic_conf_pdo(lcec_generic_conf_state_t *state, LCEC_CONF_PDO_T *pdo_conf) {
  // check for pdos
  if (state->pdos == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "PDOs for generic device missing\n");
    return -1;
  }

  // check for pdos entries
  if (state->pdo_entries == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "PDO entries for generic device missing\n");
    return -1;
  }

  // initialize pdo
  state->pdos->index = pdo_conf->index;
  state->pdos->n_entries = pdo_conf->pdoEntryCount;
  state->pdos->entries = pdo_conf->pdoEntryCount == 0 ? NULL : state->pdo_entries;

  // next pdo
  state->pdos++;
  return 0;
}

/**
 * @brief Register one PDO entry (and optional HAL pin) from the XML configuration.
 *
 * Fills the EtherCAT PDO-entry table slot (index, subindex, bit-length) and,
 * when a pin name is provided in @p pe_conf, also populates a @c lcec_generic_pin_t
 * slot with the type, direction, scale/offset, and PDO address.  The current
 * entry config is saved in @c state->pe_conf for use by any following
 * @c \<complexEntry\> elements that share the same PDO entry.
 *
 * @param state    Configuration parser state.
 * @param pe_conf  Parsed PDO-entry configuration element.
 * @return 0 on success, -1 if any required table pointer is NULL.
 */
int lcec_generic_conf_pdo_entry(lcec_generic_conf_state_t *state, LCEC_CONF_PDOENTRY_T *pe_conf) {
  // check for pdos entries
  if (state->pdo_entries == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "PDO entries for generic device missing\n");
    return -1;
  }

  // check for hal data
  if (state->hal_data == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "HAL data for generic device missing\n");
    return -1;
  }

  // check for hal dir
  if (state->hal_dir == 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "HAL direction for generic device missing\n");
    return -1;
  }

  // initialize pdo entry
  state->pdo_entries->index = pe_conf->index;
  state->pdo_entries->subindex = pe_conf->subindex;
  state->pdo_entries->bit_length = pe_conf->bitLength;

  // initialize hal data
  if (pe_conf->halPin[0] != 0) {
    strncpy(state->hal_data->name, pe_conf->halPin, LCEC_CONF_STR_MAXLEN);
    state->hal_data->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
    state->hal_data->type = pe_conf->halType;
    state->hal_data->subType = pe_conf->subType;
    state->hal_data->floatScale = pe_conf->floatScale;
    state->hal_data->floatOffset = pe_conf->floatOffset;
    state->hal_data->bitOffset = 0;
    state->hal_data->bitLength = pe_conf->bitLength;
    state->hal_data->dir = state->hal_dir;
    state->hal_data->pdo_idx = pe_conf->index;
    state->hal_data->pdo_sidx = pe_conf->subindex;
    state->hal_data++;
  }

  // remember config for complex pin
  state->pe_conf = pe_conf;

  // next pdo entry
  state->pdo_entries++;
  return 0;
}

/**
 * @brief Register a complex (bit-field) sub-pin within the current PDO entry.
 *
 * A @c \<complexEntry\> element maps a sub-range of bits inside the enclosing
 * @c \<pdoEntry\> to its own HAL pin.  The bit offset within the PDO entry is
 * taken from @p ce_conf->bitOffset; the PDO index/subindex are inherited from
 * the last @c lcec_generic_conf_pdo_entry call via @c state->pe_conf.
 *
 * @param state    Configuration parser state (must have @c pe_conf set).
 * @param ce_conf  Parsed complex-entry configuration element.
 * @return 0 on success, -1 if @c pe_conf or @c hal_data is NULL.
 */
int lcec_generic_conf_complex_entry(lcec_generic_conf_state_t *state, LCEC_CONF_COMPLEXENTRY_T *ce_conf) {
  // check for pdoEntry
  if (state->pe_conf == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "pdoEntry for generic device missing\n");
    return -1;
  }

  // check for hal data
  if (state->hal_data == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "HAL data for generic device missing\n");
    return -1;
  }

  // initialize hal data
  if (ce_conf->halPin[0] != 0) {
    strncpy(state->hal_data->name, ce_conf->halPin, LCEC_CONF_STR_MAXLEN);
    state->hal_data->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
    state->hal_data->type = ce_conf->halType;
    state->hal_data->subType = ce_conf->subType;
    state->hal_data->floatScale = ce_conf->floatScale;
    state->hal_data->floatOffset = ce_conf->floatOffset;
    state->hal_data->bitOffset = ce_conf->bitOffset;
    state->hal_data->bitLength = ce_conf->bitLength;
    state->hal_data->dir = state->hal_dir;
    state->hal_data->pdo_idx = state->pe_conf->index;
    state->hal_data->pdo_sidx = state->pe_conf->subindex;
    state->hal_data++;
  }

  return 0;
}

/**
 * @brief Realtime init — register PDO entry offsets and create HAL pins.
 *
 * Iterates over every @c lcec_generic_pin_t slot allocated at parse time.
 * For each slot, calls LCEC_PDO_INIT() to record the byte/bit offsets, then
 * creates HAL pin(s) according to the type:
 *  - @c HAL_BIT with @c bitLength == 1 → single pin.
 *  - @c HAL_BIT with @c bitLength > 1  → array of pins named @c name-0, @c name-1, …
 *  - @c HAL_S32 / @c HAL_U32 / @c HAL_FLOAT → single pin (max 32 bits).
 *
 * @param comp_id         HAL component ID.
 * @param slave           Slave instance.
 * @param pdo_entry_regs  PDO registration array; advanced by LCEC_PDO_INIT.
 * @return 0 on success, negative errno on HAL pin creation failure.
 */
int lcec_generic_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_generic_pin_t *hal_data = (lcec_generic_pin_t *) slave->hal_data;
  int i, j;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_generic_read;
  slave->proc_write = lcec_generic_write;

  // initialize pins
  for (i=0; i < slave->pdo_entry_count; i++, hal_data++) {
    // PDO mapping
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, hal_data->pdo_idx, hal_data->pdo_sidx, &hal_data->pdo_os, &hal_data->pdo_bp);

    switch (hal_data->type) {
      case HAL_BIT:
        if (hal_data->bitLength == 1) {
          // single bit pin
          err = lcec_pin_newf(comp_id, hal_data->type, hal_data->dir, &hal_data->pin[0], "%s.%s.%s.%s", master->instance_name, master->name, slave->name, hal_data->name);
          if (err != 0) {
            return err;
          }
        } else {
          // bit pin array
          for (j=0; j < LCEC_CONF_GENERIC_MAX_SUBPINS && j < hal_data->bitLength; j++) {
            err = lcec_pin_newf(comp_id, hal_data->type, hal_data->dir, &hal_data->pin[j], "%s.%s.%s.%s-%d", master->instance_name, master->name, slave->name, hal_data->name, j);
            if (err != 0) {
              return err;
            }
          }
        }
        break;

      case HAL_S32:
      case HAL_U32:
        // check data size
        if (hal_data->bitLength > 32) {
          rtapi_print_msg(RTAPI_MSG_WARN, LCEC_MSG_PFX "unable to export pin %s.%s.%s.%s: invalid process data bitlen!\n", master->instance_name, master->name, slave->name, hal_data->name);
          continue;
        }

        // export pin
        err = lcec_pin_newf(comp_id, hal_data->type, hal_data->dir, &hal_data->pin[0], "%s.%s.%s.%s", master->instance_name, master->name, slave->name, hal_data->name);
        if (err != 0) {
          return err;
        }
        break;

      case HAL_FLOAT:
        // check data size
        if (hal_data->bitLength > 32) {
          rtapi_print_msg(RTAPI_MSG_WARN, LCEC_MSG_PFX "unable to export pin %s.%s.%s.%s: invalid process data bitlen!\n", master->instance_name, master->name, slave->name, hal_data->name);
          continue;
        }

        // export pin
        err = lcec_pin_newf(comp_id, hal_data->type, hal_data->dir, &hal_data->pin[0], "%s.%s.%s.%s", master->instance_name, master->name, slave->name, hal_data->name);
        if (err != 0) {
          return err;
        }
        break;

      default:
        rtapi_print_msg(RTAPI_MSG_WARN, LCEC_MSG_PFX "unsupported pin type %d!\n", hal_data->type);
    }
  }

  return 0;
}

/**
 * @brief HAL read function — copy input PDO data to HAL output pins.
 *
 * Called every servo period.  Iterates over all @c lcec_generic_pin_t slots
 * with @c dir == @c HAL_OUT and copies the process-data bits/bytes into the
 * corresponding HAL pin value(s).  Float pins are scaled and offset-adjusted
 * using @c floatScale and @c floatOffset.
 *
 * @param slave   Slave instance.
 * @param period  Servo period in nanoseconds (unused).
 */
void lcec_generic_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_generic_pin_t *hal_data = (lcec_generic_pin_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i, j, offset;
  hal_float_t fval;

  // read data
  for (i=0; i < slave->pdo_entry_count; i++, hal_data++) {
    // skip wrong direction and uninitialized pins
    if (hal_data->dir != HAL_OUT || hal_data->pin[0] == NULL) {
      continue;
    }

    switch (hal_data->type) {
      case HAL_BIT:
        offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
        for (j=0; j < LCEC_CONF_GENERIC_MAX_SUBPINS && hal_data->pin[j] != NULL; j++, offset++) {
          *((hal_bit_t *) hal_data->pin[j]) = EC_READ_BIT(&pd[offset >> 3], offset & 0x07);
        }
        break;

      case HAL_S32:
        *((hal_s32_t *) hal_data->pin[0]) = lcec_generic_read_s32(pd, hal_data);
        break;

      case HAL_U32:
        *((hal_u32_t *) hal_data->pin[0]) = lcec_generic_read_u32(pd, hal_data);
        break;

      case HAL_FLOAT:
        if (hal_data->subType == lcecPdoEntTypeFloatUnsigned) {
          fval = lcec_generic_read_u32(pd, hal_data);
        } else if(hal_data->subType == lcecPdoEntTypeFloatIeee){
          fval = EC_READ_REAL(&pd[hal_data->pdo_os]);
        } else {
          fval = lcec_generic_read_s32(pd, hal_data);
        }

        fval *= hal_data->floatScale;
        fval += hal_data->floatOffset;
        *((hal_float_t *) hal_data->pin[0]) = fval;
        break;

      default:
        continue;
    }
  }
}

/**
 * @brief HAL write function — copy HAL input pin values to output PDO data.
 *
 * Called every servo period.  Iterates over all @c lcec_generic_pin_t slots
 * with @c dir == @c HAL_IN and writes the HAL pin value(s) into the
 * process-data image.  Float pins have their offset applied first, then are
 * scaled before being written as signed or unsigned integers.
 *
 * @param slave   Slave instance.
 * @param period  Servo period in nanoseconds (unused).
 */
void lcec_generic_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_generic_pin_t *hal_data = (lcec_generic_pin_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i, j, offset;
  hal_float_t fval;

  // write data
  for (i=0; i<slave->pdo_entry_count; i++, hal_data++) {
    // skip wrong direction and uninitialized pins
    if (hal_data->dir != HAL_IN || hal_data->pin[0] == NULL) {
      continue;
    }

    switch (hal_data->type) {
      case HAL_BIT:
        offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
        for (j=0; j < LCEC_CONF_GENERIC_MAX_SUBPINS && hal_data->pin[j] != NULL; j++, offset++) {
          EC_WRITE_BIT(&pd[offset >> 3], offset & 0x07, *((hal_bit_t *) hal_data->pin[j]));
        }
        break;

      case HAL_S32:
        lcec_generic_write_s32(pd, hal_data, *((hal_s32_t *) hal_data->pin[0]));
        break;

      case HAL_U32:
        lcec_generic_write_u32(pd, hal_data, *((hal_u32_t *) hal_data->pin[0]));
        break;

      case HAL_FLOAT:
        fval = *((hal_float_t *) hal_data->pin[0]);
        fval += hal_data->floatOffset;
        fval *= hal_data->floatScale;

        if (hal_data->subType == lcecPdoEntTypeFloatUnsigned) {
          lcec_generic_write_u32(pd, hal_data, (hal_u32_t) fval);
        } else {
          lcec_generic_write_s32(pd, hal_data, (hal_s32_t) fval);
        }
        break;

      default:
        continue;
    }
  }
}

/**
 * @brief Read a signed integer value from the process-data image.
 *
 * Uses fast byte-aligned EC_READ_S8/S16/S32 macros for byte-aligned 8/16/32-bit
 * fields; falls back to a bit-by-bit loop for non-aligned or non-standard widths.
 *
 * @param pd        Process-data image base pointer.
 * @param hal_data  Pin descriptor with PDO byte offset, bit position, bit length.
 * @return Sign-extended value read from the process data.
 */
hal_s32_t lcec_generic_read_s32(uint8_t *pd, lcec_generic_pin_t *hal_data) {
  int i, offset;
  hal_s32_t sval;

  if (hal_data->pdo_bp == 0 && hal_data->bitOffset == 0) {
    switch (hal_data->bitLength) {
      case 8:
        return EC_READ_S8(&pd[hal_data->pdo_os]);
      case 16:
        return EC_READ_S16(&pd[hal_data->pdo_os]);
      case 32:
        return EC_READ_S32(&pd[hal_data->pdo_os]);
    }
  }

  offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
  for (sval=0, i=0; i < hal_data->bitLength; i++, offset++) {
    if (EC_READ_BIT(&pd[offset >> 3], offset & 0x07)) {
      sval |= (1 << i);
    }
  }
  return sval;
}

/**
 * @brief Read an unsigned integer value from the process-data image.
 *
 * Uses fast byte-aligned EC_READ_U8/U16/U32 macros for byte-aligned 8/16/32-bit
 * fields; falls back to a bit-by-bit loop for non-aligned or non-standard widths.
 *
 * @param pd        Process-data image base pointer.
 * @param hal_data  Pin descriptor with PDO byte offset, bit position, bit length.
 * @return Zero-extended value read from the process data.
 */
hal_u32_t lcec_generic_read_u32(uint8_t *pd, lcec_generic_pin_t *hal_data) {
  int i, offset;
  hal_u32_t uval;

  if (hal_data->pdo_bp == 0 && hal_data->bitOffset == 0) {
    switch (hal_data->bitLength) {
      case 8:
        return EC_READ_U8(&pd[hal_data->pdo_os]);
      case 16:
        return EC_READ_U16(&pd[hal_data->pdo_os]);
      case 32:
        return EC_READ_U32(&pd[hal_data->pdo_os]);
    }
  }

  offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
  for (uval=0, i=0; i < hal_data->bitLength; i++, offset++) {
    if (EC_READ_BIT(&pd[offset >> 3], offset & 0x07)) {
      uval |= (1 << i);
    }
  }
  return uval;
}

/**
 * @brief Write a signed integer value into the process-data image.
 *
 * Clamps @p sval to the representable range of @c bitLength signed bits before
 * writing.  Uses fast EC_WRITE_S8/S16/S32 macros for byte-aligned standard widths;
 * falls back to a bit-by-bit loop otherwise.
 *
 * @param pd        Process-data image base pointer.
 * @param hal_data  Pin descriptor with PDO byte offset, bit position, bit length.
 * @param sval      Value to write (clamped to range).
 */
void lcec_generic_write_s32(uint8_t *pd, lcec_generic_pin_t *hal_data, hal_s32_t sval) {
  int i, offset;

  hal_s32_t lim = ((1LL << hal_data->bitLength) >> 1) - 1LL;
  if (sval > lim) sval = lim;
  lim = ~lim;
  if (sval < lim) sval = lim;

  if (hal_data->pdo_bp == 0 && hal_data->bitOffset == 0) {
    switch (hal_data->bitLength) {
      case 8:
        EC_WRITE_S8(&pd[hal_data->pdo_os], sval);
        return;
      case 16:
        EC_WRITE_S16(&pd[hal_data->pdo_os], sval);
        return;
      case 32:
        EC_WRITE_S32(&pd[hal_data->pdo_os], sval);
        return;
    }
  }

  offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
  for (i=0; i < hal_data->bitLength; i++, offset++) {
    EC_WRITE_BIT(&pd[offset >> 3], offset & 0x07, sval & 1);
    sval >>= 1;
  }
}

/**
 * @brief Write an unsigned integer value into the process-data image.
 *
 * Clamps @p uval to the representable range of @c bitLength unsigned bits before
 * writing.  Uses fast EC_WRITE_U8/U16/U32 macros for byte-aligned standard widths;
 * falls back to a bit-by-bit loop otherwise.
 *
 * @param pd        Process-data image base pointer.
 * @param hal_data  Pin descriptor with PDO byte offset, bit position, bit length.
 * @param uval      Value to write (clamped to range).
 */
void lcec_generic_write_u32(uint8_t *pd, lcec_generic_pin_t *hal_data, hal_u32_t uval) {
  int i, offset;

  hal_u32_t lim = (1LL << hal_data->bitLength) - 1LL;
  if (uval > lim) uval = lim;

  if (hal_data->pdo_bp == 0 && hal_data->bitOffset == 0) {
    switch (hal_data->bitLength) {
      case 8:
        EC_WRITE_U8(&pd[hal_data->pdo_os], uval);
        return;
      case 16:
        EC_WRITE_U16(&pd[hal_data->pdo_os], uval);
        return;
      case 32:
        EC_WRITE_U32(&pd[hal_data->pdo_os], uval);
        return;
    }
  }

  offset = ((hal_data->pdo_os << 3) | (hal_data->pdo_bp & 0x07)) + hal_data->bitOffset;
  for (i=0; i < hal_data->bitLength; i++, offset++) {
    EC_WRITE_BIT(&pd[offset >> 3], offset & 0x07, uval & 1);
    uval >>= 1;
  }
}
