/**
 * @file generic.h
 * @brief Generic/passthrough EtherCAT slave driver for arbitrary PDO mapping.
 *
 * This driver enables any EtherCAT slave to be used without a dedicated
 * device driver.  PDO layout, sync-manager configuration, and HAL pin
 * definitions are all derived at runtime from the XML configuration file.
 *
 * The configuration is built incrementally through a state machine
 * (@ref lcec_generic_conf_state_t) driven by the parser:
 *   1. @c lcec_generic_conf_init  – allocate memory, set up slave.
 *   2. @c lcec_generic_conf_sm    – add a sync manager.
 *   3. @c lcec_generic_conf_pdo   – add a PDO to the current sync manager.
 *   4. @c lcec_generic_conf_pdo_entry – add a PDO entry (and optional HAL pin).
 *   5. @c lcec_generic_conf_complex_entry – add a sub-pin inside a multi-bit entry.
 *
 * Supported HAL pin types are BIT (single or array), S32, U32 and FLOAT.
 * FLOAT pins support scale/offset conversion and optional unsigned or IEEE
 * 754 raw interpretation.
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

#ifndef _LCEC_GENERIC_H_
#define _LCEC_GENERIC_H_

#include "../lcec.h"
#include "../conf.h"

/**
 * @brief Per-PDO-entry HAL pin descriptor for a generic slave.
 *
 * Each PDO entry that carries a HAL pin name in the XML configuration is
 * represented by one instance of this structure.  The array of instances
 * is stored in @c slave->hal_data.
 */
typedef struct {
  char name[LCEC_CONF_STR_MAXLEN]; /**< HAL pin name (without master/slave prefix). */
  hal_type_t type;                 /**< HAL pin type (BIT, S32, U32, FLOAT). */
  LCEC_PDOENT_TYPE_T subType;      /**< Sub-type for FLOAT pins (signed, unsigned, IEEE 754). */
  hal_float_t floatScale;          /**< Scale factor applied to FLOAT values on read/write. */
  hal_float_t floatOffset;         /**< Offset applied to FLOAT values after scaling on read. */
  uint8_t bitOffset;               /**< Bit offset within the PDO entry for complex sub-pins. */
  uint8_t bitLength;               /**< Number of bits occupied by this pin's value. */
  hal_pin_dir_t dir;               /**< HAL pin direction (HAL_IN or HAL_OUT). */
  void *pin[LCEC_CONF_GENERIC_MAX_SUBPINS]; /**< Pointers to the registered HAL pin values. */
  uint16_t pdo_idx;                /**< CANopen object index of the mapped PDO entry. */
  uint8_t pdo_sidx;                /**< CANopen sub-index of the mapped PDO entry. */
  unsigned int pdo_os;             /**< Byte offset of the entry in the EtherCAT process image. */
  unsigned int pdo_bp;             /**< Bit offset within @c pdo_os for bit-aligned entries. */
} lcec_generic_pin_t;

/**
 * @brief Mutable parser state used while building a generic slave configuration.
 *
 * The calling parser maintains one of these and passes it to each
 * @c lcec_generic_conf_* function in order.  Pointers advance as items are
 * consumed so that subsequent calls append to the correct array positions.
 */
typedef struct {
  ec_pdo_entry_info_t *pdo_entries; /**< Next free slot in the PDO entry array. */
  ec_pdo_info_t *pdos;              /**< Next free slot in the PDO array. */
  ec_sync_info_t *sync_managers;    /**< Next free slot in the sync manager array. */
  lcec_generic_pin_t *hal_data;     /**< Next free slot in the HAL pin array. */
  hal_pin_dir_t hal_dir;            /**< HAL direction inferred from the current sync manager. */
  LCEC_CONF_PDOENTRY_T *pe_conf;    /**< Last PDO entry config, used by complex sub-entries. */
} lcec_generic_conf_state_t;

/**
 * @brief Initialise the generic slave and allocate dynamic PDO/sync structures.
 *
 * Allocates the HAL data array (one @ref lcec_generic_pin_t per mapped PDO
 * entry), the PDO entry, PDO and sync-manager arrays from the zone allocator,
 * sets up the slave vendor/product IDs, and points @c slave->sync_info at the
 * newly allocated sync manager array when @c configPdos is set.
 *
 * @param slave       Pointer to the EtherCAT slave descriptor.
 * @param slave_conf  Parsed slave configuration from the XML file.
 * @param conf_state  Caller-allocated state structure to initialise.
 * @return 0 on success, -1 on allocation failure.
 */
int lcec_generic_conf_init(lcec_slave_t *slave, LCEC_CONF_SLAVE_T *slave_conf, lcec_generic_conf_state_t *conf_state);

/**
 * @brief Release all dynamically allocated memory for a generic slave.
 *
 * Frees the PDO entry, PDO and sync-manager arrays previously allocated by
 * @c lcec_generic_conf_init.
 *
 * @param slave Pointer to the EtherCAT slave descriptor.
 */
void lcec_generic_free_slave(lcec_slave_t *slave);

/**
 * @brief Append a sync manager to the configuration state.
 *
 * Initialises the next sync manager slot with the index, direction and PDO
 * count from @p sm_conf, derives the HAL pin direction from the EtherCAT
 * direction, and advances the sync manager pointer.
 *
 * @param state   Configuration state (updated in place).
 * @param sm_conf Parsed sync manager configuration.
 * @return 0 on success, -1 on error (missing arrays).
 */
int lcec_generic_conf_sm(lcec_generic_conf_state_t *state, LCEC_CONF_SYNCMANAGER_T *sm_conf);

/**
 * @brief Append a PDO to the configuration state.
 *
 * Initialises the next PDO slot with the index, entry count and pointer from
 * @p pdo_conf, then advances the PDO pointer.
 *
 * @param state    Configuration state (updated in place).
 * @param pdo_conf Parsed PDO configuration.
 * @return 0 on success, -1 on error (missing arrays).
 */
int lcec_generic_conf_pdo(lcec_generic_conf_state_t *state, LCEC_CONF_PDO_T *pdo_conf);

/**
 * @brief Append a PDO entry to the configuration state.
 *
 * Fills the next PDO entry slot and, when a HAL pin name is present in
 * @p pe_conf, also fills the next HAL data slot and advances it.  Stores
 * @p pe_conf for use by subsequent @c lcec_generic_conf_complex_entry calls.
 *
 * @param state   Configuration state (updated in place).
 * @param pe_conf Parsed PDO entry configuration.
 * @return 0 on success, -1 on error (missing arrays or invalid direction).
 */
int lcec_generic_conf_pdo_entry(lcec_generic_conf_state_t *state, LCEC_CONF_PDOENTRY_T *pe_conf);

/**
 * @brief Append a complex (sub-bit) HAL pin entry to the configuration state.
 *
 * Used for multi-bit PDO entries that map several HAL pins at different bit
 * offsets within the same PDO entry.  Requires a preceding call to
 * @c lcec_generic_conf_pdo_entry to establish the parent entry context.
 *
 * @param state   Configuration state (updated in place).
 * @param ce_conf Parsed complex entry configuration.
 * @return 0 on success, -1 on error (missing parent entry or HAL data).
 */
int lcec_generic_conf_complex_entry(lcec_generic_conf_state_t *state, LCEC_CONF_COMPLEXENTRY_T *ce_conf);

/**
 * @brief Initialise the generic slave HAL pins and register PDO entries.
 *
 * Iterates over the HAL data array (one entry per mapped PDO) and for each
 * entry registers the PDO with LCEC_PDO_INIT and creates the appropriate HAL
 * pin(s) based on the type: a single BIT pin, a BIT array, an S32/U32 pin, or
 * a FLOAT pin.
 *
 * @param comp_id   LinuxCNC HAL component identifier.
 * @param slave     Pointer to the EtherCAT slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_generic_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif
