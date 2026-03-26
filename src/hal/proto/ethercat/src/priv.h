/**
 * @file priv.h
 * @brief Internal header for the LinuxCNC EtherCAT HAL driver runtime component.
 *
 * Declares private types, global variables, and function prototypes shared
 * between the master, slave, and DC-sync implementation files.  This header
 * must not be included by device drivers or other external code; use
 * @c lcec.h instead.
 *
 * @copyright Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_PRIV_H_
#define _LCEC_PRIV_H_

#include "lcec.h"

#include "devices/generic.h"

/** @brief Forward declaration for the config output buffer (defined in conf_priv.h). */
struct lcec_conf_outbuf;

/**
 * @brief Entry in the global slave-type registry.
 *
 * Each supported slave model has one entry in the @c typelist[] array.  The
 * driver looks up the entry by @c type during slave creation to obtain the
 * vendor/product IDs and the init callbacks for that model.
 */
typedef struct lcec_typelist {
  LCEC_SLAVE_TYPE_T type;            /**< Unique enum identifier for this slave type. */
  uint32_t vid;                      /**< EtherCAT vendor ID. */
  uint32_t pid;                      /**< EtherCAT product code. */
  int pdo_entry_count;               /**< Number of PDO entries registered by this slave. */
  int is_fsoe_logic;                 /**< Non-zero if this is an FSoE logic (safety) slave. */
  lcec_slave_preinit_t proc_preinit; /**< Optional pre-init callback invoked before PDO mapping (may be NULL). */
  lcec_slave_init_t proc_init;       /**< Mandatory init callback invoked during HAL component setup. */
} lcec_typelist_t;

/**
 * @brief Traversal state used while populating a slave's configuration buffers.
 *
 * Passed to the @c lcec_slave_conf_*() helpers so they can write configuration
 * entries sequentially into the pre-allocated buffers without needing to
 * recalculate buffer offsets themselves.
 */
typedef struct {
  lcec_generic_conf_state_t generic; /**< State for generic-slave PDO configuration. */
  lcec_slave_sdoconf_t *sdo_config;  /**< Write pointer into the SDO config buffer. */
  lcec_slave_idnconf_t *idn_config;  /**< Write pointer into the IDN config buffer. */
  lcec_slave_modparam_t *modparams;  /**< Write pointer into the module-parameter array. */
} lcec_slave_conf_state_t;

extern const lcec_typelist_t typelist[]; /**< Global slave-type registry, terminated by an entry with type @c lcecSlaveTypeInvalid. */

/** @brief Create and initialise an @c lcec_master_t. @see master.c */
lcec_master_t * lcec_create_master(LCEC_CONF_MASTER_T *master_conf);
/** @brief Open the EtherCAT master (userspace or kernel build). @see master.c */
int lcec_startup_master(lcec_master_t *master);
/** @brief Release the EtherCAT master and free transport resources. @see master.c */
void lcec_shutdown_master(lcec_master_t *master);
/** @brief Allocate and export HAL pins for a master. @see master.c */
lcec_master_data_t *lcec_init_master_hal(int comp_id, const char *pfx, int global);
/** @brief Update master HAL output pins from the current EtherCAT master state. @see master.c */
void lcec_update_master_hal(lcec_master_data_t *hal_data, ec_master_state_t *ms);

/** @brief Create and initialise an @c lcec_slave_t from its configuration. @see slave.c */
lcec_slave_t *lcec_create_slave(lcec_master_t *master, LCEC_CONF_SLAVE_T *slave_conf, lcec_slave_conf_state_t *conf_state);
/** @brief Free all memory associated with a slave. @see slave.c */
void lcec_free_slave(lcec_slave_t *slave);
/** @brief Apply a distributed clock configuration to a slave. @see slave.c */
int lcec_slave_conf_dc(lcec_slave_t *slave, LCEC_CONF_DC_T *dc_conf);
/** @brief Apply a watchdog configuration to a slave. @see slave.c */
int lcec_slave_conf_wd(lcec_slave_t *slave, LCEC_CONF_WATCHDOG_T *wd_conf);
/** @brief Append an SDO configuration entry to the slave's SDO config buffer. @see slave.c */
void lcec_slave_conf_sdo(lcec_slave_conf_state_t *state, LCEC_CONF_SDOCONF_T *sdo_conf);
/** @brief Append an IDN configuration entry to the slave's IDN config buffer. @see slave.c */
void lcec_slave_conf_idn(lcec_slave_conf_state_t *state, LCEC_CONF_IDNCONF_T *idn_conf);
/** @brief Append a module parameter entry to the slave's modparam array. @see slave.c */
void lcec_slave_conf_modparam(lcec_slave_conf_state_t *state, LCEC_CONF_MODPARAM_T *modparam_conf);
/** @brief Allocate and export HAL state pins for a slave. @see slave.c */
lcec_slave_state_t *lcec_init_slave_state_hal(int comp_id, const char *instance_name, char *master_name, char *slave_name);
/** @brief Update slave HAL state pins from the current EtherCAT slave config state. @see slave.c */
void lcec_update_slave_state_hal(lcec_slave_state_t *hal_data, ec_slave_config_state_t *ss);

/**
 * @brief Create a single HAL pin using a printf-style format string and @c va_list.
 *
 * @param type           HAL data type (e.g. @c HAL_BIT, @c HAL_U32).
 * @param dir            HAL pin direction (e.g. @c HAL_IN, @c HAL_OUT).
 * @param data_ptr_addr  Address of the pointer-to-HAL-value to populate.
 * @param fmt            printf-style format string for the pin name.
 * @param ap             Argument list for @p fmt.
 * @return 0 on success, non-zero on failure.
 */
int lcec_pin_newfv(int comp_id, hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, va_list ap);

int lcec_pin_newfv_list(int comp_id, void *base, const lcec_pindesc_t *list, va_list ap);

int lcec_param_newfv(int comp_id, hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, va_list ap);

int lcec_param_newfv_list(int comp_id, void *base, const lcec_pindesc_t *list, va_list ap);

/**
 * @brief Initialise DC synchronisation callbacks for ref-clock → master mode.
 *
 * Configures @c master->dcsync_callbacks so that the master application time
 * is derived from the reference clock slave.  Used when
 * @c master->ref_clock_sync_cycles > 0.
 *
 * @param master  Master to configure.
 */
void lcec_dc_init_r2m(struct lcec_master *master);
#ifdef RTAPI_TASK_PLL_SUPPORT
/**
 * @brief Initialise DC synchronisation callbacks for master → ref-clock mode.
 *
 * Available only when @c RTAPI_TASK_PLL_SUPPORT is defined.  Configures
 * @c master->dcsync_callbacks to drive the reference clock from the RTAPI PLL,
 * enabling the EtherCAT bus to be synchronised to the LinuxCNC servo thread.
 *
 * @param master  Master to configure.
 */
void lcec_dc_init_m2r(struct lcec_master *master);
#endif

/**
 * @brief Context struct passed from the cmod plugin to the RT init/cleanup functions.
 *
 * Holds all per-instance state that was previously stored in file-scope globals
 * in main.c, enabling multi-instance support.
 */
typedef struct lcec_rt_context {
  int comp_id;                        /**< HAL component ID from hal_init_ex(). */
  const char *instance_name;          /**< Instance name from cmod New(). */
  const char *ipc_socket;             /**< IPC socket path (EC_USPACE_MASTER), or NULL. */
  lcec_master_t *first_master;        /**< Head of the master linked list (populated by lcec_parse_config). */
  lcec_master_t *last_master;         /**< Tail of the master linked list. */
  lcec_master_data_t *global_hal_data; /**< HAL pins for aggregate EtherCAT state. */
  ec_master_state_t global_ms;        /**< Aggregate master state updated each cycle. */
  int64_t dc_time_offset;            /**< Nanosecond offset between RTAPI monotonic and EtherCAT wall-clock time. */
} lcec_rt_context_t;

/**
 * @brief Initialise the EtherCAT RT component (replaces rtapi_app_main).
 *
 * Parses the configuration from the output buffer linked list, starts all
 * masters, configures all slaves, and exports the HAL read/write functions.
 * Frees the linked-list nodes after parsing.  Does NOT call hal_init() or
 * hal_ready() — the caller (cmod New) handles those.
 *
 * @param ctx  Per-instance context; comp_id and instance_name must be set.
 * @param buf  Output buffer containing the parsed XML configuration records.
 * @return 0 on success, negative on error.
 */
int lcec_rt_init(lcec_rt_context_t *ctx, struct lcec_conf_outbuf *buf);

/**
 * @brief Tear down the EtherCAT RT component (replaces rtapi_app_exit).
 *
 * Deactivates all masters, frees all configuration, and cleans up the
 * EtherCAT library.  Does NOT call hal_exit() — the caller handles that.
 *
 * @param ctx  Per-instance context.
 */
void lcec_rt_cleanup(lcec_rt_context_t *ctx);

int lcec_rt_start(lcec_rt_context_t *ctx);
void lcec_rt_stop(lcec_rt_context_t *ctx);

#endif
