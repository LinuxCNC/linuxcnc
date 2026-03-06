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

/**
 * @file lcec.h
 * @brief Main header for the LinuxCNC EtherCAT HAL driver (lcec).
 *
 * Defines the core data structures, macros, constants, callback typedefs,
 * and function declarations shared across the entire lcec driver.  Every
 * EtherCAT device driver within the lcec tree includes this header.
 *
 * @par Architecture overview
 * - A single HAL component manages one or more EtherCAT @em masters.
 * - Each master owns a linked list of @em slaves (device driver instances).
 * - During the LinuxCNC servo period the lcec_read_all() / lcec_write_all()
 *   HAL functions iterate all masters and slaves, calling each slave's
 *   proc_read / proc_write callback to exchange process data.
 */
#ifndef _LCEC_H_
#define _LCEC_H_

#include "pal/pal.h"

#include "hal.h"

#include "rtapi_ctype.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "ecrt.h"
#include "conf.h"

/**
 * @defgroup lcec_list Linked-list helpers
 * @{
 */

/**
 * @brief Append @p item to the doubly-linked list bounded by @p first / @p last.
 *
 * Assumes the item type exposes @c prev and @c next pointer fields of the
 * same type.  When the list is empty both @p first and @p last must be @c NULL;
 * after the call @p item becomes the sole element and both pointers are updated.
 *
 * @param first  Pointer-to-pointer holding the list head (updated when empty).
 * @param last   Pointer-to-pointer holding the list tail (always updated).
 * @param item   Pointer to the element to append.
 */
// list macros
#define LCEC_LIST_APPEND(first, last, item) \
do {                             \
  (item)->prev = (last);         \
  if ((item)->prev != NULL) {    \
    (item)->prev->next = (item); \
  } else {                       \
    (first) = (item);            \
  }                              \
  (last) = (item);               \
} while (0);                     \

/** @} */ // lcec_list

/**
 * @defgroup lcec_pdo PDO helpers
 * @{
 */

/**
 * @brief Populate one PDO entry registration record and advance the array pointer.
 *
 * Fills all fields of the @c ec_pdo_entry_reg_t record pointed to by @p *pdo,
 * then increments the pointer so the caller's sequential walk through the
 * registration array automatically advances to the next slot.  Call this
 * macro once per PDO entry inside a slave's @c proc_init callback.
 *
 * @param pdo   Pointer-to-pointer into the PDO entry registration array.
 * @param pos   EtherCAT bus position of the slave.
 * @param vid   EtherCAT vendor ID of the slave.
 * @param pid   EtherCAT product code of the slave.
 * @param idx   Object dictionary index of the PDO entry.
 * @param sidx  Subindex of the PDO entry.
 * @param off   Pointer that will receive the byte offset within the domain image.
 * @param bpos  Pointer that will receive the bit position within the byte.
 */
// pdo macros
#define LCEC_PDO_INIT(pdo, pos, vid, pid, idx, sidx, off, bpos) \
do {                        \
  (*pdo)->position = pos;      \
  (*pdo)->vendor_id = vid;     \
  (*pdo)->product_code = pid;  \
  (*pdo)->index = idx;         \
  (*pdo)->subindex = sidx;     \
  (*pdo)->offset = off;        \
  (*pdo)->bit_position = bpos; \
  (*pdo)++;                    \
} while (0);                \

/** @} */ // lcec_pdo

/** @brief Log message prefix prepended to all LCEC diagnostic output. */
#define LCEC_MSG_PFX "LCEC: "

/**
 * @defgroup lcec_vendor_ids EtherCAT vendor IDs
 * @brief Well-known EtherCAT vendor IDs for devices supported by lcec.
 * @{
 */
// vendor ids
/** @brief Beckhoff Automation GmbH & Co. KG */
#define LCEC_BECKHOFF_VID 0x00000002
/** @brief Stöber Antriebstechnik GmbH & Co. KG */
#define LCEC_STOEBER_VID  0x000000b9
/** @brief Delta Electronics Inc. */
#define LCEC_DELTA_VID    0x000001dd
/** @brief Modusoft */
#define LCEC_MODUSOFT_VID 0x00000907
/** @brief Omron Corporation */
#define LCEC_OMRON_VID    0x00000083
/** @} */ // lcec_vendor_ids

/** @brief Interval (nanoseconds) between master/slave AL-state HAL pin refreshes. */
// State update period (ns)
#define LCEC_STATE_UPDATE_PERIOD 1000000000LL

/**
 * @defgroup lcec_idn SoE IDN builder macros
 * @brief Helpers for constructing Sercos IDN words used by SoE (Servo Drive over EtherCAT).
 * @{
 */
/** @brief IDN type flag for product-specific parameter IDNs (P-x-y notation). */
// IDN builder
#define LCEC_IDN_TYPE_P 0x8000
/** @brief IDN type flag for standard Sercos IDNs (S-x-y notation). */
#define LCEC_IDN_TYPE_S 0x0000

/**
 * @brief Build a 16-bit SoE IDN word from its components.
 *
 * The resulting value can be passed directly to ecrt_slave_config_idn() or
 * stored in an @c lcec_slave_idnconf_t entry.
 *
 * @param type   IDN type: @c LCEC_IDN_TYPE_P or @c LCEC_IDN_TYPE_S.
 * @param set    Parameter set number (0–7).
 * @param block  Block / IDN number (0–4095).
 * @return       16-bit IDN value.
 */
#define LCEC_IDN(type, set, block) (type | ((set & 0x07) << 12) | (block & 0x0fff))
/** @} */ // lcec_idn

/**
 * @defgroup lcec_fsoe FsoE size macros
 * @brief Constants and helper for calculating FsoE PDO sizes.
 *
 * FsoE (Functional Safety over EtherCAT) PDOs have a fixed framing overhead:
 * one command byte, one connection-ID word, and one CRC word per channel.
 * @{
 */
/** @brief Byte length of the FsoE command field. */
#define LCEC_FSOE_CMD_LEN 1
/** @brief Byte length of a single FsoE CRC field (one per channel). */
#define LCEC_FSOE_CRC_LEN 2
/** @brief Byte length of the FsoE connection ID field. */
#define LCEC_FSOE_CONNID_LEN 2

/**
 * @brief Calculate the total FsoE PDO byte size for given channel and data parameters.
 *
 * @param ch_count  Number of independent FsoE safety channels.
 * @param data_len  Safety data payload length per channel in bytes.
 * @return          Total FsoE PDO size in bytes.
 */
#define LCEC_FSOE_SIZE(ch_count, data_len) (LCEC_FSOE_CMD_LEN + ch_count * (data_len + LCEC_FSOE_CRC_LEN) + LCEC_FSOE_CONNID_LEN)
/** @} */ // lcec_fsoe

/** @brief Maximum number of PDO entries that lcec_syncs_t can hold per slave. */
#define LCEC_MAX_PDO_ENTRY_COUNT 32
/** @brief Maximum number of PDO mappings that lcec_syncs_t can hold per slave. */
#define LCEC_MAX_PDO_INFO_COUNT  8
/** @brief Maximum number of sync managers that lcec_syncs_t can describe per slave. */
#define LCEC_MAX_SYNC_COUNT      4

struct lcec_master;
struct lcec_slave;

/**
 * @brief Callback invoked before slave initialisation to query PDO entry counts.
 *
 * Called for every slave before any PDO memory is allocated so that each
 * slave driver can set @c slave->pdo_entry_count to the number of PDO entries
 * it requires.  FsoE logic slaves are pre-inited in a second pass to ensure
 * the @c fsoeConf data from regular slaves is available.
 *
 * @param slave  Pointer to the slave being pre-initialised.
 * @return 0 on success, negative value on error.
 */
typedef int (*lcec_slave_preinit_t) (struct lcec_slave *slave);

/**
 * @brief Callback that initialises a slave, creates HAL pins, and registers PDO entries.
 *
 * Called once per slave during module load.  The implementation must call
 * @c LCEC_PDO_INIT exactly @c slave->pdo_entry_count times to populate the
 * PDO registration array.
 *
 * @param comp_id        HAL component ID returned by hal_init().
 * @param slave          Pointer to the slave being initialised.
 * @param pdo_entry_regs Pointer-to-pointer into the PDO entry registration
 *                       array; must be advanced by exactly
 *                       @c slave->pdo_entry_count entries.
 * @return 0 on success, negative value on error.
 */
typedef int (*lcec_slave_init_t) (int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

/**
 * @brief Callback that releases all resources allocated by a slave's init callback.
 *
 * Called during module unload or on error after @c proc_init has succeeded.
 * The implementation must free any HAL objects and private heap allocations.
 *
 * @param slave  Pointer to the slave to clean up.
 */
typedef void (*lcec_slave_cleanup_t) (struct lcec_slave *slave);

/**
 * @brief Real-time read or write callback executed every servo period.
 *
 * Implementations must not block, sleep, or allocate/free memory.
 *
 * @param slave   Pointer to the slave.
 * @param period  Servo period in nanoseconds.
 */
typedef void (*lcec_slave_rw_t) (struct lcec_slave *slave, long period);

/**
 * @brief Distributed Clock synchronisation callback invoked at a specific
 *        point in the EtherCAT communication cycle.
 *
 * @param master  Pointer to the master that owns the DC clock.
 */
typedef void (*lcec_dcsync_callback_t) (struct lcec_master *master);

/**
 * @brief FsoE (Functional Safety over EtherCAT) PDO dimension parameters.
 *
 * Populated from the XML configuration and consumed by FsoE logic device
 * drivers during pre-initialisation to compute the required PDO sizes.
 */
typedef struct {
  int slave_data_len;   /**< Safety data payload length (bytes) from slave to master. */
  int master_data_len;  /**< Safety data payload length (bytes) from master to slave. */
  int data_channels;    /**< Number of independent FsoE safety data channels. */
} LCEC_CONF_FSOE_T;

/**
 * @brief Set of Distributed Clock synchronisation callbacks for one master.
 *
 * Each field is an optional function pointer invoked at a distinct point in
 * the EtherCAT cycle.  Any field may be @c NULL if no action is required at
 * that point.
 */
typedef struct {
  lcec_dcsync_callback_t cycle_start; /**< Called at the very start of each cycle, before queuing frames. */
  lcec_dcsync_callback_t pre_send;    /**< Called immediately before the EtherCAT frame is sent. */
  lcec_dcsync_callback_t post_send;   /**< Called after the EtherCAT frame has been sent. */
} lcec_dcsync_callbacks_t;

/**
 * @brief HAL pins that expose the aggregate EtherCAT master state to LinuxCNC.
 *
 * One instance is allocated per master (and one extra for the global aggregate)
 * by lcec_init_master_hal().  All pointer fields point into HAL shared memory
 * and remain valid for the lifetime of the HAL component.
 */
typedef struct lcec_master_data {
  hal_u32_t *slaves_responding; /**< Number of slaves currently visible on the bus. */
  hal_bit_t *state_init;        /**< TRUE when at least one slave is in INIT state. */
  hal_bit_t *state_preop;       /**< TRUE when at least one slave is in PRE-OP state. */
  hal_bit_t *state_safeop;      /**< TRUE when at least one slave is in SAFE-OP state. */
  hal_bit_t *state_op;          /**< TRUE when all slaves are in OP state. */
  hal_bit_t *link_up;           /**< TRUE when the EtherCAT physical link is up. */
  hal_bit_t *all_op;            /**< TRUE when the master reports all slaves operational. */
#ifdef RTAPI_TASK_PLL_SUPPORT
  hal_s32_t *pll_err;           /**< Current PLL phase error in nanoseconds. */
  hal_s32_t *pll_out;           /**< Current PLL correction output in nanoseconds. */
  hal_u32_t *pll_reset_cnt;     /**< Cumulative number of PLL resets since module load. */
#endif
} lcec_master_data_t;

/**
 * @brief HAL pins exposing a single slave's EtherCAT application-layer (AL) state.
 *
 * One instance is allocated per slave by lcec_init_slave_state_hal().  All
 * pointer fields point into HAL shared memory and remain valid for the
 * lifetime of the HAL component.
 */
typedef struct lcec_slave_state {
  hal_bit_t *online;        /**< TRUE when the slave is reachable on the bus. */
  hal_bit_t *operational;   /**< TRUE when the slave is in OP state. */
  hal_bit_t *state_init;    /**< TRUE when the slave AL state is INIT. */
  hal_bit_t *state_preop;   /**< TRUE when the slave AL state is PRE-OP. */
  hal_bit_t *state_safeop;  /**< TRUE when the slave AL state is SAFE-OP. */
  hal_bit_t *state_op;      /**< TRUE when the slave AL state is OP. */
} lcec_slave_state_t;

/**
 * @brief Runtime state for a single EtherCAT master.
 *
 * One instance is created per @c <Master> element in the XML configuration.
 * All masters are linked into a doubly-linked list rooted at the module-level
 * @c first_master / @c last_master globals in main.c.
 *
 * @note Fields below the @c dcsync_callbacks member are used exclusively
 *       by the DC synchronisation logic and must only be accessed while the
 *       master mutex is held (outside the real-time path).
 */
typedef struct lcec_master {
  struct lcec_master *prev; /**< Previous master in the global linked list, or NULL if head. */
  struct lcec_master *next; /**< Next master in the global linked list, or NULL if tail. */
  int index;                /**< Zero-based master index from the XML configuration. */
#ifdef EC_USPACE_MASTER
  int transport_type;                          /**< Transport layer type identifier (userspace build only). */
  char interface[LCEC_CONF_STR_MAXLEN];        /**< Primary network interface name (e.g. "eth0"). */
  char backup_interface[LCEC_CONF_STR_MAXLEN]; /**< Redundant network interface name, or empty string. */
  unsigned int debug_level;                    /**< EtherCAT library debug verbosity (0 = off). */
  int run_on_cpu;                              /**< CPU affinity for the master thread, -1 = no affinity. */
  ec_transport_t *transport;                   /**< Primary EtherCAT transport handle. */
  ec_transport_t *backup_transport;            /**< Redundant EtherCAT transport handle, or NULL. */
#endif
  char name[LCEC_CONF_STR_MAXLEN]; /**< Human-readable master name from the XML configuration. */
  ec_master_t *master;             /**< EtherCAT master handle from the IgH EtherCAT library. */
  unsigned long mutex;             /**< Mutex protecting concurrent access to this master. */
  int pdo_entry_count;             /**< Total PDO entries across all slaves (set during parse). */
  ec_pdo_entry_reg_t *pdo_entry_regs; /**< PDO entry registration array (length pdo_entry_count + 1). */
  ec_domain_t *domain;             /**< EtherCAT process-data domain handle. */
  uint8_t *process_data;           /**< Pointer to the mapped process-data image for the domain. */
  int process_data_len;            /**< Size of the process-data image in bytes. */
  struct lcec_slave *first_slave;  /**< Head of the slave linked list for this master. */
  struct lcec_slave *last_slave;   /**< Tail of the slave linked list for this master. */
  lcec_master_data_t *hal_data;    /**< Per-master HAL state pins. */
  long long state_update_timer;    /**< Countdown (ns) until the next AL-state HAL pin refresh. */
  uint32_t app_time_period;        /**< Application time period sent to the EtherCAT master (ns). */
  int ref_clock_sync_cycles;       /**< DC ref-clock sync cycle divider; negative selects master-to-reference mode. */
  int ref_clock_slave_idx;         /**< Bus index of the DC reference-clock slave, or -1 for default. */
  long period_last;                /**< Servo period measured during the previous cycle (ns). */
  ec_master_state_t ms;            /**< Most-recently read EtherCAT master state snapshot. */

  lcec_dcsync_callbacks_t dcsync_callbacks; /**< Optional callbacks for DC synchronisation events. */
  int ref_clock_sync_counter;               /**< Remaining cycles before the next DC ref-clock sync. */

  uint64_t app_time_ns;    /**< Application time written to the EtherCAT master each cycle (ns). */
  uint64_t ref_time_ns;    /**< Reference time snapshot used for computing the DC time offset (ns). */

  uint64_t dc_time_ns;     /**< DC system time read from the reference clock slave (ns). */
  int dc_started;          /**< Non-zero once the DC synchronisation PI controller has been seeded. */
  int64_t dc_diff_ns;      /**< Current DC phase error (application time minus DC time) in ns. */
  double dc_kp;          /**< PI proportional gain for DC synchronisation. */
  double dc_ki;          /**< PI integral gain for DC synchronisation. */
  double dc_integrator;  /**< PI integral accumulator for DC synchronisation. */
} lcec_master_t;

/**
 * @brief Distributed Clock (DC) configuration for a single slave.
 *
 * Mirrors the parameters passed to ecrt_slave_config_dc().  Values are
 * sourced from the @c <Dc> element in the XML configuration.
 */
typedef struct {
  uint16_t assignActivate; /**< AssignActivate word (see slave ESI or EtherCAT spec Table 60). */
  uint32_t sync0Cycle;     /**< SYNC0 output cycle time in nanoseconds (0 = disabled). */
  int32_t  sync0Shift;     /**< SYNC0 output shift time relative to the reference clock (ns). */
  uint32_t sync1Cycle;     /**< SYNC1 output cycle time in nanoseconds (0 = disabled). */
  int32_t  sync1Shift;     /**< SYNC1 output shift time relative to SYNC0 (ns). */
} lcec_slave_dc_t;

/**
 * @brief Watchdog timer configuration for a single slave.
 *
 * Values are passed directly to ecrt_slave_config_watchdog().  The actual
 * watchdog timeout = @c divider * @c intervals * 25 ns.
 */
typedef struct {
  uint16_t divider;   /**< Watchdog divider: number of 25 ns ticks per watchdog tick. */
  uint16_t intervals; /**< Number of watchdog ticks before the PDI watchdog fires. */
} lcec_slave_watchdog_t;

/**
 * @brief One SDO (Service Data Object) startup-configuration entry.
 *
 * SDO entries form a variable-length in-memory linked list terminated by a
 * sentinel entry with @c index == 0xffff.  Because each entry is followed
 * immediately by @c length bytes of payload, traversal must use pointer
 * arithmetic:
 * @code
 * for (p = slave->sdo_config; p->index != 0xffff;
 *      p = (lcec_slave_sdoconf_t *)&p->data[p->length]) { ... }
 * @endcode
 */
typedef struct {
  uint16_t index;    /**< Object dictionary index of the SDO. */
  int16_t  subindex; /**< Subindex, or @c LCEC_CONF_SDO_COMPLETE_SUBIDX for complete-access SDO. */
  size_t   length;   /**< Byte length of the value payload in @c data[]. */
  uint8_t  data[];   /**< Flexible array member: @c length bytes of SDO value. */
} lcec_slave_sdoconf_t;

/**
 * @brief One IDN (Identification Number) SoE startup-configuration entry.
 *
 * IDN entries form a variable-length in-memory linked list terminated by a
 * sentinel entry with @c state == 0.  Traversal must use pointer arithmetic:
 * @code
 * for (p = slave->idn_config; p->state != 0;
 *      p = (lcec_slave_idnconf_t *)&p->data[p->length]) { ... }
 * @endcode
 */
typedef struct {
  uint8_t       drive;   /**< SoE drive number (0–7). */
  uint16_t      idn;     /**< 16-bit IDN word; use @c LCEC_IDN() to construct. */
  ec_al_state_t state;   /**< AL state in which this IDN is written (PREOP or SAFEOP). */
  size_t        length;  /**< Byte length of the value payload in @c data[]. */
  uint8_t       data[];  /**< Flexible array member: @c length bytes of IDN value. */
} lcec_slave_idnconf_t;

/**
 * @brief A single module-parameter (modparam) key-value pair for a slave driver.
 *
 * Modparams allow the XML configuration to pass named, typed parameters to
 * device drivers without requiring dedicated fields in @c lcec_slave_t.
 * Use lcec_modparam_get() to look up a value by ID at init time.
 */
typedef struct {
  int                   id;    /**< Driver-defined parameter ID (see the slave driver's modparam table). */
  LCEC_CONF_MODPARAM_VAL_T value; /**< Typed value union (int / uint / float / string). */
} lcec_slave_modparam_t;

/**
 * @brief PDO layout storage for generically-configured slaves.
 *
 * The three arrays are progressively filled by the lcec_syncs_add_sync(),
 * lcec_syncs_add_pdo_info(), and lcec_syncs_add_pdo_entry() helpers and
 * then passed to ecrt_slave_config_pdos() via @c slave->sync_info.
 */
typedef struct {
  ec_pdo_entry_info_t *pdo_entries;   /**< NULL-terminated array of PDO entry descriptions. */
  ec_pdo_info_t       *pdos;          /**< NULL-terminated array of PDO mapping descriptions. */
  ec_sync_info_t      *sync_managers; /**< Array of sync-manager descriptions, terminated by EC_END. */
} lcec_generic_slave_t;

/**
 * @brief Runtime state for a single EtherCAT slave device.
 *
 * One instance is created per @c <Slave> element in the XML configuration.
 * Slaves belonging to the same master are stored in a doubly-linked list
 * rooted at @c master->first_slave / @c master->last_slave.
 *
 * The @c proc_read and @c proc_write callbacks are invoked every servo period
 * and must adhere to real-time constraints (no blocking, no memory allocation).
 */
typedef struct lcec_slave {
  struct lcec_slave  *prev;            /**< Previous slave in the master's list, or NULL if head. */
  struct lcec_slave  *next;            /**< Next slave in the master's list, or NULL if tail. */
  struct lcec_master *master;          /**< Back-pointer to the owning master. */
  int                 index;           /**< EtherCAT bus position of this slave (ring position). */
  LCEC_SLAVE_TYPE_T   type;            /**< Driver type identifier used to select the device driver. */
  char                name[LCEC_CONF_STR_MAXLEN]; /**< Human-readable slave name from the XML configuration. */
  uint32_t            vid;             /**< EtherCAT vendor ID (must match the physical device). */
  uint32_t            pid;             /**< EtherCAT product code (must match the physical device). */
  int                 pdo_entry_count; /**< Number of PDO entries this slave requires in the domain. */
  ec_sync_info_t     *sync_info;       /**< Sync-manager configuration passed to ecrt_slave_config_pdos(), or NULL. */
  ec_slave_config_t  *config;          /**< EtherCAT slave configuration handle from the IgH library. */
  ec_slave_config_state_t state;       /**< Most-recently read EtherCAT slave configuration state. */
  lcec_slave_dc_t    *dc_conf;         /**< DC synchronisation configuration, or NULL if DC is not used. */
  lcec_slave_watchdog_t *wd_conf;      /**< Watchdog configuration, or NULL to use hardware defaults. */
  lcec_slave_preinit_t proc_preinit;   /**< Pre-init callback (queries PDO count), may be NULL. */
  lcec_slave_init_t    proc_init;      /**< Init callback (creates HAL pins and registers PDOs), may be NULL. */
  lcec_slave_cleanup_t proc_cleanup;   /**< Cleanup callback (frees driver resources), may be NULL. */
  lcec_slave_rw_t      proc_read;      /**< RT read callback executed each servo period, may be NULL. */
  lcec_slave_rw_t      proc_write;     /**< RT write callback executed each servo period, may be NULL. */
  lcec_slave_state_t  *hal_state_data; /**< HAL pins for this slave's AL state. */
  void               *hal_data;        /**< Driver-private HAL data pointer (cast to driver's own struct). */
  lcec_generic_slave_t generic;        /**< PDO layout storage populated for generic slave configuration. */
  lcec_slave_sdoconf_t *sdo_config;    /**< Head of the SDO startup-config linked list, or NULL. */
  lcec_slave_idnconf_t *idn_config;    /**< Head of the IDN startup-config linked list, or NULL. */
  lcec_slave_modparam_t *modparams;    /**< Array of module parameters terminated by a zero-id entry, or NULL. */
  const LCEC_CONF_FSOE_T *fsoeConf;   /**< FsoE PDO dimension parameters, or NULL if not an FsoE slave. */
  int              is_fsoe_logic;      /**< Non-zero if this slave is an FsoE logic (gateway) device. */
  unsigned int    *fsoe_slave_offset;  /**< Domain image byte offset of the FsoE slave-to-master PDO. */
  unsigned int    *fsoe_master_offset; /**< Domain image byte offset of the FsoE master-to-slave PDO. */
} lcec_slave_t;

/**
 * @brief Descriptor for a single HAL pin or parameter, used with lcec_pin_newf_list().
 *
 * An array of these descriptors, terminated by an entry with @c fmt == NULL,
 * drives bulk creation of HAL pins or parameters.  The @c offset field is
 * added to the base pointer supplied to the list function to locate the
 * @c void** storage pointer for each pin or the @c void* address for each
 * parameter.
 */
typedef struct {
  hal_type_t    type;   /**< HAL data type (HAL_BIT, HAL_U32, HAL_S32, HAL_FLOAT). */
  hal_pin_dir_t dir;    /**< Pin/parameter direction (HAL_IN, HAL_OUT, HAL_IO, HAL_RO, HAL_RW). */
  int           offset; /**< Byte offset of the pointer field within the driver's HAL data struct. */
  const char   *fmt;    /**< printf-style name format string; NULL terminates the descriptor list. */
} lcec_pindesc_t;

/**
 * @brief Builder state for constructing a slave's sync-manager / PDO configuration.
 *
 * Holds fixed-size arrays populated by the lcec_syncs_add_sync(),
 * lcec_syncs_add_pdo_info(), and lcec_syncs_add_pdo_entry() helpers.
 * Call lcec_syncs_init() before the first add call.  After all entries have
 * been added, point @c slave->sync_info at @c syncs.syncs.
 *
 * Array capacities are bounded by @c LCEC_MAX_SYNC_COUNT,
 * @c LCEC_MAX_PDO_INFO_COUNT, and @c LCEC_MAX_PDO_ENTRY_COUNT.
 */
typedef struct {
  int              sync_count;    /**< Number of sync managers added so far. */
  ec_sync_info_t  *curr_sync;     /**< Pointer to the sync manager currently being populated. */
  ec_sync_info_t   syncs[LCEC_MAX_SYNC_COUNT + 1]; /**< Sync-manager array; last slot holds the EC_END sentinel. */

  int              pdo_info_count; /**< Number of PDO mappings added so far. */
  ec_pdo_info_t   *curr_pdo_info;  /**< Pointer to the PDO mapping currently being populated. */
  ec_pdo_info_t    pdo_infos[LCEC_MAX_PDO_INFO_COUNT]; /**< PDO mapping array. */

  int              pdo_entry_count;   /**< Number of PDO entries added so far. */
  ec_pdo_entry_info_t *curr_pdo_entry; /**< Pointer to the PDO entry currently being populated. */
  ec_pdo_entry_info_t  pdo_entries[LCEC_MAX_PDO_ENTRY_COUNT]; /**< PDO entry array. */
} lcec_syncs_t;

/**
 * @brief Read a value from a slave's SDO (Service Data Object) via CoE.
 *
 * @param slave     Slave to read from.
 * @param index     Object dictionary index.
 * @param subindex  Subindex within the object.
 * @param target    Buffer that receives the read value.
 * @param size      Size of @p target in bytes.
 * @return 0 on success, negative value on error.
 * @note Must not be called from a real-time context.
 */
int lcec_read_sdo(struct lcec_slave *slave, uint16_t index, uint8_t subindex, uint8_t *target, size_t size);

/**
 * @brief Read a value from a slave's IDN (Identification Number) via SoE.
 *
 * @param slave     Slave to read from.
 * @param drive_no  SoE drive number (0–7).
 * @param idn       16-bit IDN word; use @c LCEC_IDN() to construct.
 * @param target    Buffer that receives the read value.
 * @param size      Size of @p target in bytes.
 * @return 0 on success, negative value on error.
 * @note Must not be called from a real-time context.
 */
int lcec_read_idn(struct lcec_slave *slave, uint8_t drive_no, uint16_t idn, uint8_t *target, size_t size);

/**
 * @brief Create a single HAL pin with a printf-formatted name.
 *
 * @param type           HAL data type.
 * @param dir            Pin direction.
 * @param data_ptr_addr  Address of the pointer that will hold the pin's storage.
 * @param fmt            printf-style format string for the pin name.
 * @param ...            Format arguments.
 * @return 0 on success, negative HAL error code on failure.
 */
int lcec_pin_newf(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, ...);

/**
 * @brief Create a list of HAL pins described by a NULL-terminated lcec_pindesc_t array.
 *
 * Iterates @p list until an entry with @c fmt == NULL is found.  Each pin's
 * storage pointer is derived as @p base + descriptor->offset.
 *
 * @param base  Base pointer added to each descriptor's @c offset field.
 * @param list  NULL-terminated array of pin descriptors.
 * @param ...   Format arguments applied to every descriptor's @c fmt string.
 * @return 0 on success, negative HAL error code on the first failure.
 */
int lcec_pin_newf_list(void *base, const lcec_pindesc_t *list, ...);

/**
 * @brief Create a single HAL parameter with a printf-formatted name.
 *
 * @param type       HAL data type.
 * @param dir        Parameter direction (HAL_RO or HAL_RW).
 * @param data_addr  Address of the parameter's storage.
 * @param fmt        printf-style format string for the parameter name.
 * @param ...        Format arguments.
 * @return 0 on success, negative HAL error code on failure.
 */
int lcec_param_newf(hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, ...);

/**
 * @brief Create a list of HAL parameters described by a NULL-terminated lcec_pindesc_t array.
 *
 * @param base  Base pointer added to each descriptor's @c offset field.
 * @param list  NULL-terminated array of parameter descriptors.
 * @param ...   Format arguments applied to every descriptor's @c fmt string.
 * @return 0 on success, negative HAL error code on the first failure.
 */
int lcec_param_newf_list(void *base, const lcec_pindesc_t *list, ...);

/**
 * @brief Look up a module parameter value for a slave by its driver-defined ID.
 *
 * @param slave  Slave whose @c modparams array is searched.
 * @param id     Driver-defined parameter ID.
 * @return Pointer to the matching value union, or NULL if @p id is not found.
 */
LCEC_CONF_MODPARAM_VAL_T *lcec_modparam_get(struct lcec_slave *slave, int id);

/**
 * @brief Find a slave within a master by its EtherCAT bus position index.
 *
 * @param master  Master whose slave list is searched.
 * @param index   EtherCAT bus position (ring position) to look for.
 * @return Pointer to the matching slave, or NULL if no slave has that index.
 */
lcec_slave_t *lcec_slave_by_index(struct lcec_master *master, int index);

/**
 * @brief Copy FsoE process data between the domain image and slave HAL pins.
 *
 * Called from an FsoE slave driver's @c proc_read / @c proc_write callback.
 *
 * @param slave          FsoE slave whose data is being copied.
 * @param slave_offset   Byte offset of the slave-to-master FsoE PDO in the domain image.
 * @param master_offset  Byte offset of the master-to-slave FsoE PDO in the domain image.
 */
void copy_fsoe_data(struct lcec_slave *slave, unsigned int slave_offset, unsigned int master_offset);

/**
 * @brief Initialise an lcec_syncs_t builder to an empty state.
 *
 * Must be called before any lcec_syncs_add_* call.
 *
 * @param syncs  Builder structure to initialise.
 */
void lcec_syncs_init(lcec_syncs_t *syncs);

/**
 * @brief Append a new sync manager entry to the syncs builder.
 *
 * @param syncs          Builder structure.
 * @param dir            Data direction (EC_DIR_INPUT or EC_DIR_OUTPUT).
 * @param watchdog_mode  Watchdog mode (EC_WD_ENABLE or EC_WD_DISABLE).
 */
void lcec_syncs_add_sync(lcec_syncs_t *syncs, ec_direction_t dir, ec_watchdog_mode_t watchdog_mode);

/**
 * @brief Append a PDO mapping to the current sync manager in the syncs builder.
 *
 * @param syncs  Builder structure.
 * @param index  PDO object dictionary index (e.g. 0x1600 for the first RxPDO).
 */
void lcec_syncs_add_pdo_info(lcec_syncs_t *syncs, uint16_t index);

/**
 * @brief Append a PDO entry to the current PDO mapping in the syncs builder.
 *
 * @param syncs       Builder structure.
 * @param index       Object dictionary index of the entry (0 = gap / padding entry).
 * @param subindex    Subindex of the entry.
 * @param bit_length  Bit width of the entry.
 */
void lcec_syncs_add_pdo_entry(lcec_syncs_t *syncs, uint16_t index, uint8_t subindex, uint8_t bit_length);

#endif

