#include "priv.h"

/**
 * @file util.c
 * @brief General-purpose utility functions for the LinuxCNC EtherCAT HAL driver.
 *
 * This file provides helper routines used across the driver:
 *   - CoE SDO and SoE IDN mailbox reads for slave configuration at startup.
 *   - HAL pin and parameter creation helpers (single and bulk/list variants).
 *   - Module-parameter lookup by ID.
 *   - Slave lookup by EtherCAT bus position.
 *   - FsoE (Fail-Safe over EtherCAT) process-data copy helper.
 *   - Sync-manager / PDO configuration builder (lcec_syncs_*).
 *
 * None of the functions in this file are called from real-time context;
 * they are used during component initialisation only.
 */

/**
 * @brief Read an SDO value from a slave device via the CoE mailbox.
 *
 * Performs a blocking SDO upload (read) using @c ecrt_master_sdo_upload().
 * This function is @b not safe to call from real-time context; it must only
 * be used during the initialisation phase (e.g., from a slave's @c proc_init
 * callback).
 *
 * @param slave     Slave to read from.
 * @param index     Object dictionary index (e.g., @c 0x1018).
 * @param subindex  Object dictionary sub-index.
 * @param target    Buffer that receives the SDO data.
 * @param size      Expected (and required) size of the data in bytes.
 * @return 0 on success, -1 if the upload fails or the returned size
 *         does not match @p size.
 *
 * @note Logs a descriptive error via @c rtapi_print_msg() on failure,
 *       including the SDO index, subindex, error code, and abort code.
 */
int lcec_read_sdo(struct lcec_slave *slave, uint16_t index, uint8_t subindex, uint8_t *target, size_t size) {
  lcec_master_t *master = slave->master;
  int err;
  size_t result_size;
  uint32_t abort_code;

  if ((err = ecrt_master_sdo_upload(master->master, slave->index, index, subindex, target, size, &result_size, &abort_code))) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Failed to execute SDO upload (0x%04x:0x%02x, error %d, abort_code %08x)\n",
      master->name, slave->name, index, subindex, err, abort_code);
    return -1;
  }

  if (result_size != size) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Invalid result size on SDO upload (0x%04x:0x%02x, req: %u, res: %u)\n",
      master->name, slave->name, index, subindex, (unsigned int) size, (unsigned int) result_size);
    return -1;
  }

  return 0;
}

/**
 * @brief Read an IDN value from a servo drive via the SoE mailbox.
 *
 * Performs a blocking IDN read using @c ecrt_master_read_idn() for drives
 * that support the Servo Drive Profile over EtherCAT (SoE/IEC 61800-7-204).
 * This function is @b not safe to call from real-time context.
 *
 * The IDN encoding follows the SoE standard:
 *   - Bit 15 set → parameter IDN ('P'), clear → standard IDN ('S').
 *   - Bits 14–12 → parameter set number.
 *   - Bits 11–0  → IDN number within the set.
 *
 * @param slave     Slave to read from.
 * @param drive_no  Drive number within the slave (0 for single-axis devices).
 * @param idn       Encoded IDN identifier.
 * @param target    Buffer that receives the IDN data.
 * @param size      Expected (and required) size of the data in bytes.
 * @return 0 on success, -1 if the read fails or the returned size
 *         does not match @p size.
 *
 * @note Logs a descriptive error via @c rtapi_print_msg() on failure,
 *       including the decoded IDN and SoE error code.
 */
int lcec_read_idn(struct lcec_slave *slave, uint8_t drive_no, uint16_t idn, uint8_t *target, size_t size) {
  lcec_master_t *master = slave->master;
  int err;
  size_t result_size;
  uint16_t error_code;

  if ((err = ecrt_master_read_idn(master->master, slave->index, drive_no, idn, target, size, &result_size, &error_code))) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Failed to execute IDN read (drive %u idn %c-%u-%u, error %d, error_code %08x)\n",
      master->name, slave->name, drive_no, (idn & 0x8000) ? 'P' : 'S', (idn >> 12) & 0x0007, idn & 0x0fff, err, error_code);
    return -1;
  }

  if (result_size != size) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Invalid result size on IDN read (drive %u idn %c-%d-%d, req: %u, res: %u)\n",
      master->name, slave->name, drive_no, (idn & 0x8000) ? 'P' : 'S', (idn >> 12) & 0x0007, idn & 0x0fff, (unsigned int) size, (unsigned int) result_size);
    return -1;
  }

  return 0;
}

/**
 * @brief Create a single HAL pin with a @c va_list-based format string.
 *
 * Formats the pin name from @p fmt and @p ap, calls @c hal_pin_new(), and
 * zero-initialises the pin value.  This is the @c va_list back-end shared by
 * lcec_pin_newf() and lcec_pin_newfv_list().
 *
 * @param type           HAL data type (e.g., @c HAL_BIT, @c HAL_U32).
 * @param dir            HAL pin direction (@c HAL_IN, @c HAL_OUT, or @c HAL_IO).
 * @param data_ptr_addr  Address of the driver's pointer-to-HAL-value field.
 *                       On success @c hal_pin_new() sets @c *data_ptr_addr
 *                       to point into the HAL shared memory area.
 * @param fmt            printf-style format string for the pin name.
 * @param ap             Argument list matching @p fmt.
 * @return 0 on success, @c -ENOMEM if the formatted name exceeds
 *         @c HAL_NAME_LEN, or the negative error code from @c hal_pin_new().
 */
int lcec_pin_newfv(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, va_list ap) {
  char name[HAL_NAME_LEN + 1];
  int sz;
  int err;

  sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
  if(sz == -1 || sz > HAL_NAME_LEN) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "length %d too long for name starting '%s'\n", sz, name);
    return -ENOMEM;
  }

  err = hal_pin_new(name, type, dir, data_ptr_addr, comp_id);
  if (err) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "exporting pin %s failed\n", name);
    return err;
  }

  switch (type) {
    case HAL_BIT:
      **((hal_bit_t **) data_ptr_addr) = 0;
      break;
    case HAL_FLOAT:
      **((hal_float_t **) data_ptr_addr) = 0.0;
      break;
    case HAL_S32:
      **((hal_s32_t **) data_ptr_addr) = 0;
      break;
    case HAL_U32:
      **((hal_u32_t **) data_ptr_addr) = 0;
      break;
    default:
      break;
  }

  return 0;
}

/**
 * @brief Create a single HAL pin with a printf-style variadic format string.
 *
 * Convenience wrapper around lcec_pin_newfv() that accepts a variadic
 * argument list instead of a @c va_list.
 *
 * @param type           HAL data type.
 * @param dir            HAL pin direction.
 * @param data_ptr_addr  Address of the driver's pointer-to-HAL-value field.
 * @param fmt            printf-style format string for the pin name.
 * @param ...            Format arguments.
 * @return 0 on success, negative error code on failure.
 */
int lcec_pin_newf(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, ...) {
  va_list ap;
  int err;

  va_start(ap, fmt);
  err = lcec_pin_newfv(type, dir, data_ptr_addr, fmt, ap);
  va_end(ap);

  return err;
}

/**
 * @brief Create HAL pins for every entry in a descriptor list (va_list form).
 *
 * Iterates over @p list until an entry with @c type == @c HAL_TYPE_UNSPECIFIED
 * is encountered.  For each entry the pointer field located at
 * @c (base + entry->offset) is passed to lcec_pin_newfv() along with a copy
 * of @p ap so that each descriptor's format string receives the same set of
 * format arguments (e.g., the slave name and channel index).
 *
 * @param base  Base address of the driver's HAL data struct.
 * @param list  Descriptor array terminated by an entry whose @c type field is
 *              @c HAL_TYPE_UNSPECIFIED.
 * @param ap    @c va_list of format arguments consumed by each descriptor's
 *              @c fmt string.
 * @return 0 on success, negative error code from lcec_pin_newfv() on the
 *         first failure (remaining descriptors are not processed).
 */
int lcec_pin_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap) {
  va_list ac;
  int err;
  const lcec_pindesc_t *p;

  for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
    va_copy(ac, ap);
    err = lcec_pin_newfv(p->type, p->dir, (void **) ((uint8_t *)base + p->offset), p->fmt, ac);
    va_end(ac);
    if (err) {
      return err;
    }
  }

  return 0;
}

/**
 * @brief Create HAL pins for every entry in a descriptor list.
 *
 * Variadic convenience wrapper around lcec_pin_newfv_list().  The variadic
 * arguments are forwarded to each descriptor's format string in turn.
 *
 * @param base  Base address of the driver's HAL data struct.
 * @param list  Descriptor array terminated by a @c HAL_TYPE_UNSPECIFIED entry.
 * @param ...   Format arguments consumed by each descriptor's @c fmt string.
 * @return 0 on success, negative error code on the first failure.
 */
int lcec_pin_newf_list(void *base, const lcec_pindesc_t *list, ...) {
  va_list ap;
  int err;

  va_start(ap, list);
  err = lcec_pin_newfv_list(base, list, ap);
  va_end(ap);

  return err;
}

/**
 * @brief Create a single HAL parameter with a @c va_list-based format string.
 *
 * Formats the parameter name from @p fmt and @p ap, calls @c hal_param_new(),
 * and zero-initialises the parameter value.  This is the @c va_list back-end
 * shared by lcec_param_newf() and lcec_param_newfv_list().
 *
 * @param type       HAL data type (e.g., @c HAL_FLOAT, @c HAL_S32).
 * @param dir        HAL parameter direction (@c HAL_RO or @c HAL_RW).
 * @param data_addr  Address of the parameter value storage within the
 *                   driver's HAL data struct.  Unlike pins, parameters store
 *                   their value directly (not via a pointer).
 * @param fmt        printf-style format string for the parameter name.
 * @param ap         Argument list matching @p fmt.
 * @return 0 on success, @c -ENOMEM if the name is too long, or the
 *         negative error code from @c hal_param_new().
 */
int lcec_param_newfv(hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, va_list ap) {
  char name[HAL_NAME_LEN + 1];
  int sz;
  int err;

  sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
  if(sz == -1 || sz > HAL_NAME_LEN) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "length %d too long for name starting '%s'\n", sz, name);
    return -ENOMEM;
  }

  err = hal_param_new(name, type, dir, data_addr, comp_id);
  if (err) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "exporting param %s failed\n", name);
    return err;
  }

  switch (type) {
    case HAL_BIT:
      *((hal_bit_t *) data_addr) = 0;
      break;
    case HAL_FLOAT:
      *((hal_float_t *) data_addr) = 0.0;
      break;
    case HAL_S32:
      *((hal_s32_t *) data_addr) = 0;
      break;
    case HAL_U32:
      *((hal_u32_t *) data_addr) = 0;
      break;
    default:
      break;
  }

  return 0;
}

/**
 * @brief Create a single HAL parameter with a printf-style variadic format string.
 *
 * Convenience wrapper around lcec_param_newfv().
 *
 * @param type       HAL data type.
 * @param dir        HAL parameter direction (@c HAL_RO or @c HAL_RW).
 * @param data_addr  Address of the parameter value storage.
 * @param fmt        printf-style format string for the parameter name.
 * @param ...        Format arguments.
 * @return 0 on success, negative error code on failure.
 */
int lcec_param_newf(hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, ...) {
  va_list ap;
  int err;

  va_start(ap, fmt);
  err = lcec_param_newfv(type, dir, data_addr, fmt, ap);
  va_end(ap);

  return err;
}

/**
 * @brief Create HAL parameters for every entry in a descriptor list (va_list form).
 *
 * Iterates over @p list until a @c HAL_TYPE_UNSPECIFIED terminator is found.
 * For each entry the value address at @c (base + entry->offset) is passed to
 * lcec_param_newfv().
 *
 * @param base  Base address of the driver's HAL data struct.
 * @param list  Descriptor array terminated by a @c HAL_TYPE_UNSPECIFIED entry.
 * @param ap    @c va_list of format arguments for each descriptor's @c fmt string.
 * @return 0 on success, negative error code from lcec_param_newfv() on the
 *         first failure.
 */
int lcec_param_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap) {
  va_list ac;
  int err;
  const lcec_pindesc_t *p;

  for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
    va_copy(ac, ap);
    err = lcec_param_newfv(p->type, p->dir, (void *) ((uint8_t *)base + p->offset), p->fmt, ac);
    va_end(ac);
    if (err) {
      return err;
    }
  }

  return 0;
}

/**
 * @brief Create HAL parameters for every entry in a descriptor list.
 *
 * Variadic convenience wrapper around lcec_param_newfv_list().
 *
 * @param base  Base address of the driver's HAL data struct.
 * @param list  Descriptor array terminated by a @c HAL_TYPE_UNSPECIFIED entry.
 * @param ...   Format arguments for each descriptor's @c fmt string.
 * @return 0 on success, negative error code on the first failure.
 */
int lcec_param_newf_list(void *base, const lcec_pindesc_t *list, ...) {
  va_list ap;
  int err;

  va_start(ap, list);
  err = lcec_param_newfv_list(base, list, ap);
  va_end(ap);

  return err;
}

/**
 * @brief Look up a module parameter value by its ID.
 *
 * Searches the slave's @c modparams array for an entry whose @c id field
 * matches @p id.  The array is terminated by an entry with a negative @c id.
 *
 * Module parameters are key-value pairs set in the XML configuration and
 * consumed by device driver @c proc_init callbacks to customise slave
 * behaviour (e.g., encoder resolution, operating mode).
 *
 * @param slave  Slave whose module-parameter list is searched.
 * @param id     Driver-defined parameter identifier.
 * @return Pointer to the matching @c LCEC_CONF_MODPARAM_VAL_T value union,
 *         or NULL if the slave has no module parameters or no entry with
 *         the given @p id exists.
 */
LCEC_CONF_MODPARAM_VAL_T *lcec_modparam_get(struct lcec_slave *slave, int id) {
  lcec_slave_modparam_t *p;

  if (slave->modparams == NULL) {
    return NULL;
  }

  for (p = slave->modparams; p->id >= 0; p++) {
    if (p->id == id) {
      return &p->value;
    }
  }

  return NULL;
}

/**
 * @brief Find a slave by its EtherCAT bus position index.
 *
 * Performs a linear search through the master's slave list, comparing
 * @c slave->index against @p index.
 *
 * @param master  Master whose slave list is searched.
 * @param index   EtherCAT ring position of the target slave (0-based).
 * @return Pointer to the matching @c lcec_slave_t, or NULL if no slave
 *         with the given index is found.
 */
lcec_slave_t *lcec_slave_by_index(struct lcec_master *master, int index) {
  lcec_slave_t *slave;

  for (slave = master->first_slave; slave != NULL; slave = slave->next) {
    if (slave->index == index) {
      return slave;
    }
  }

  return NULL;
}

/**
 * @brief Copy FsoE (Fail-Safe over EtherCAT) process-data between domain offsets.
 *
 * Propagates the FsoE PDO payload bytes within the EtherCAT domain image:
 *   - Copies @p slave_offset → @c fsoe_slave_offset (slave-to-master direction).
 *   - Copies @c fsoe_master_offset → @p master_offset (master-to-slave direction).
 *
 * The size of each copy is determined by the slave's @c fsoeConf descriptor
 * via the @c LCEC_FSOE_SIZE() macro, which accounts for the number of
 * data channels and the per-direction payload length.
 *
 * This function is called from real-time context (servo period) and must
 * not block or allocate memory.  It is a no-op if @c slave->fsoeConf is NULL.
 *
 * @param slave          Slave whose FsoE configuration drives the copy sizes.
 * @param slave_offset   Domain image byte offset of the raw slave-to-master data.
 * @param master_offset  Domain image byte offset of the raw master-to-slave data.
 *
 * @note Side effect: modifies bytes in @c master->process_data at the
 *       @c fsoe_slave_offset and @p master_offset locations.
 */
void copy_fsoe_data(struct lcec_slave *slave, unsigned int slave_offset, unsigned int master_offset) {
  lcec_master_t *master = slave->master;
  uint8_t *pd = master->process_data;
  const LCEC_CONF_FSOE_T *fsoeConf = slave->fsoeConf;

  if (fsoeConf == NULL) {
    return;
  }

  if (slave->fsoe_slave_offset != NULL) {
    memcpy(&pd[*(slave->fsoe_slave_offset)], &pd[slave_offset], LCEC_FSOE_SIZE(fsoeConf->data_channels, fsoeConf->slave_data_len));
  }

  if (slave->fsoe_master_offset != NULL) {
    memcpy(&pd[master_offset], &pd[*(slave->fsoe_master_offset)], LCEC_FSOE_SIZE(fsoeConf->data_channels, fsoeConf->master_data_len));
  }
}

/**
 * @brief Initialise an @c lcec_syncs_t builder to an empty state.
 *
 * Zero-fills the entire structure so that all counters and pointers start
 * in a consistent state before the first lcec_syncs_add_sync() call.
 * Must be called before any other @c lcec_syncs_* function.
 *
 * @param syncs  Builder state to initialise.
 */
void lcec_syncs_init(lcec_syncs_t *syncs) {
  memset(syncs, 0, sizeof(lcec_syncs_t));
}

/**
 * @brief Append a sync manager to the PDO configuration builder.
 *
 * Allocates the next @c ec_sync_info_t slot in @c syncs->syncs[], sets its
 * direction and watchdog mode, and advances @c syncs->sync_count.  The
 * following @c 0xFF-terminated sentinel is written immediately after the new
 * entry so that the array is always valid for passing to
 * @c ecrt_slave_config_pdos().
 *
 * Subsequent calls to lcec_syncs_add_pdo_info() will attach PDOs to this
 * sync manager.
 *
 * @param syncs          Builder state.
 * @param dir            Direction of this sync manager (@c EC_DIR_INPUT or
 *                       @c EC_DIR_OUTPUT).
 * @param watchdog_mode  Watchdog mode (@c EC_WD_DEFAULT, @c EC_WD_ENABLE, or
 *                       @c EC_WD_DISABLE).
 *
 * @note Logs an error and returns without modifying @p syncs if the maximum
 *       number of sync managers (@c LCEC_MAX_SYNC_COUNT) would be exceeded.
 */
void lcec_syncs_add_sync(lcec_syncs_t *syncs, ec_direction_t dir, ec_watchdog_mode_t watchdog_mode) {
  if (syncs->sync_count >= LCEC_MAX_SYNC_COUNT) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "too many syncs (max %d)\n", LCEC_MAX_SYNC_COUNT);
    return;
  }
  syncs->curr_sync = &syncs->syncs[syncs->sync_count];

  syncs->curr_sync->index = syncs->sync_count;
  syncs->curr_sync->dir = dir;
  syncs->curr_sync->watchdog_mode = watchdog_mode;

  (syncs->sync_count)++;
  syncs->syncs[syncs->sync_count].index = 0xff;
}

/**
 * @brief Append a PDO mapping entry to the current sync manager.
 *
 * Allocates the next @c ec_pdo_info_t slot in @c syncs->pdo_infos[], sets its
 * object-dictionary index, increments the current sync manager's PDO count,
 * and sets the sync manager's @c pdos pointer on the first call after each
 * lcec_syncs_add_sync().
 *
 * Subsequent calls to lcec_syncs_add_pdo_entry() will attach entries to this
 * PDO.
 *
 * @param syncs  Builder state (must have had at least one lcec_syncs_add_sync()
 *               call since the last lcec_syncs_init()).
 * @param index  Object-dictionary index of the PDO (e.g., @c 0x1600 for the
 *               first receive PDO mapping).
 *
 * @note Logs an error and returns without modifying @p syncs if the maximum
 *       number of PDO infos (@c LCEC_MAX_PDO_INFO_COUNT) would be exceeded.
 */
void lcec_syncs_add_pdo_info(lcec_syncs_t *syncs, uint16_t index) {
  if (syncs->pdo_info_count >= LCEC_MAX_PDO_INFO_COUNT) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "too many PDO infos (max %d)\n", LCEC_MAX_PDO_INFO_COUNT);
    return;
  }
  syncs->curr_pdo_info = &syncs->pdo_infos[syncs->pdo_info_count];

  if (syncs->curr_sync->pdos == NULL) {
    syncs->curr_sync->pdos = syncs->curr_pdo_info;
  }
  (syncs->curr_sync->n_pdos)++;

  syncs->curr_pdo_info->index = index;

  (syncs->pdo_info_count)++;
}

/**
 * @brief Append a PDO entry to the current PDO mapping.
 *
 * Allocates the next @c ec_pdo_entry_info_t slot in @c syncs->pdo_entries[],
 * populates its index, sub-index, and bit length, increments the current
 * PDO's entry count, and sets the PDO's @c entries pointer on the first call
 * after each lcec_syncs_add_pdo_info().
 *
 * @param syncs       Builder state (must have had at least one
 *                    lcec_syncs_add_pdo_info() call since the last
 *                    lcec_syncs_add_sync()).
 * @param index       Object-dictionary index of the mapped object
 *                    (0 for a gap/padding entry).
 * @param subindex    Object-dictionary sub-index of the mapped object.
 * @param bit_length  Size of the entry in bits.
 *
 * @note Logs an error and returns without modifying @p syncs if the maximum
 *       number of PDO entries (@c LCEC_MAX_PDO_ENTRY_COUNT) would be exceeded.
 */
void lcec_syncs_add_pdo_entry(lcec_syncs_t *syncs, uint16_t index, uint8_t subindex, uint8_t bit_length) {
  if (syncs->pdo_entry_count >= LCEC_MAX_PDO_ENTRY_COUNT) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "too many PDO entries (max %d)\n", LCEC_MAX_PDO_ENTRY_COUNT);
    return;
  }
  syncs->curr_pdo_entry = &syncs->pdo_entries[syncs->pdo_entry_count];

  if (syncs->curr_pdo_info->entries == NULL) {
    syncs->curr_pdo_info->entries = syncs->curr_pdo_entry;
  }
  (syncs->curr_pdo_info->n_entries)++;

  syncs->curr_pdo_entry->index = index;
  syncs->curr_pdo_entry->subindex = subindex;
  syncs->curr_pdo_entry->bit_length = bit_length;

  (syncs->pdo_entry_count)++;
}

