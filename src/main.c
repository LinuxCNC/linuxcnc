/**
 * @file main.c
 * @brief LinuxCNC EtherCAT HAL component entry point.
 *
 * Implements the RTAPI module lifecycle (rtapi_app_main() / rtapi_app_exit()),
 * the configuration parser (lcec_parse_config()), and the global real-time
 * HAL functions (lcec_read_all() / lcec_write_all()) for the lcec driver.
 *
 * @section rt_constraints Real-Time Constraints
 * lcec_read_all(), lcec_write_all(), lcec_read_master(), and
 * lcec_write_master() are exported as HAL functions and execute in the
 * LinuxCNC real-time servo thread.  These functions must @b not block, sleep,
 * or perform any dynamic memory allocation or deallocation.
 *
 * @section init_sequence Initialisation Sequence
 * -# rtapi_app_main() records the DC time base (wall clock vs. RTAPI monotonic).
 * -# Registers the HAL component with hal_init().
 * -# lcec_parse_config() reads the binary config blob from RTAPI shared memory,
 *    builds the master/slave linked lists, and allocates PDO entry arrays.
 * -# For each master: creates the EtherCAT domain, configures every slave
 *    (SDOs, IDNs, PDOs, DC, watchdog), registers PDO entries with the domain,
 *    initialises DC synchronisation, activates the master, and exports the
 *    per-master HAL read/write functions.
 * -# Exports the global @c read-all and @c write-all HAL functions.
 * -# hal_ready() signals successful initialisation to LinuxCNC.
 *
 * @copyright Copyright (C) 2012-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include "priv.h"
#include "rtapi_app.h"
#include "rtapi_mutex.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sascha Ittner <sascha.ittner@modusoft.de>");
MODULE_DESCRIPTION("Driver for EtherCAT devices");

/** @brief Head of the doubly-linked list of all configured EtherCAT masters. */
static lcec_master_t *first_master = NULL;
/** @brief Tail of the doubly-linked list of all configured EtherCAT masters. */
static lcec_master_t *last_master = NULL;

/** @brief HAL component ID returned by hal_init(); -1 before successful initialisation. */
int comp_id = -1;

/** @brief HAL pins for the aggregate (all-masters combined) EtherCAT state. */
static lcec_master_data_t *global_hal_data;
/** @brief Aggregate EtherCAT master state updated each cycle by lcec_read_all(). */
ec_master_state_t global_ms;

#ifdef EC_USPACE_MASTER
static char *ipc_socket = NULL;
RTAPI_MP_STRING(ipc_socket, "EtherCAT userspace master IPC socket path (NULL = no tool access)");

/**
 * @brief Log callback that forwards EtherCAT library messages to the RTAPI log.
 *
 * Registered with ecrt_lib_init() so that library diagnostics appear in the
 * LinuxCNC message stream prefixed with @c LCEC_MSG_PFX.
 *
 * @param level  Library-defined severity level (not mapped; all messages forwarded).
 * @param fmt    printf-style format string from the EtherCAT library.
 * @param ap     Variadic argument list for @p fmt.
 */
static void lcec_ec_log_callback(int level, const char *fmt, va_list ap) {
  char buf[256];
  rtapi_vsnprintf(buf, sizeof(buf), fmt, ap);
  rtapi_print(LCEC_MSG_PFX "%s", buf);
}
#endif

int lcec_parse_config(void);
void lcec_clear_config(void);

void lcec_read_all(void *arg, long period);
void lcec_write_all(void *arg, long period);
void lcec_read_master(void *arg, long period);
void lcec_write_master(void *arg, long period);

/** @brief Offset (ns) between RTAPI monotonic time and EtherCAT application wall-clock time. */
int64_t dc_time_offset;

/**
 * @brief LinuxCNC HAL component entry point.
 *
 * Called once by the RTAPI loader when the lcec module is inserted.
 * Performs all non-real-time initialisation as described in the file-level
 * documentation, then marks the component ready for the servo thread.
 *
 * @return 0 on success.
 * @return -EINVAL on any failure; partial resources are freed before returning.
 */
int rtapi_app_main(void) {
  int slave_count;
  lcec_master_t *master;
  lcec_slave_t *slave;
  char name[HAL_NAME_LEN + 1];
  ec_pdo_entry_reg_t *pdo_entry_regs;
  lcec_slave_sdoconf_t *sdo_config;
  lcec_slave_idnconf_t *idn_config;
  struct timeval tv;
  long long rtapi_now;

  // get time base
  lcec_gettimeofday(&tv);
  rtapi_now = rtapi_get_time();
  dc_time_offset = EC_TIMEVAL2NANO(tv) - rtapi_now;

  // connect to the HAL
  if ((comp_id = hal_init (LCEC_MODULE_NAME)) < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_init() failed\n");
    goto fail0;
  }

  // parse configuration
  if ((slave_count = lcec_parse_config()) < 0) {
    goto fail1;
  }

  // init global hal data
  if ((global_hal_data = lcec_init_master_hal(LCEC_MODULE_NAME, 1)) == NULL) {
    goto fail2;
  }

#ifdef EC_USPACE_MASTER
  // initialize userspace ethercat master library
  if (ecrt_lib_init(lcec_ec_log_callback, ipc_socket) < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "ecrt_lib_init() failed (ipc_socket=%s)\n",
        ipc_socket ? ipc_socket : "NULL");
    goto fail2;
  }
#endif

  // initialize masters
  for (master = first_master; master != NULL; master = master->next) {
    // startup master
    if (lcec_startup_master(master)) {
      goto fail2;
    }

    // create domain
    if (!(master->domain = ecrt_master_create_domain(master->master))) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s domain creation failed\n", master->name);
      goto fail2;
    }

    // initialize slaves
    pdo_entry_regs = master->pdo_entry_regs;
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      // read slave config
      if (!(slave->config = ecrt_master_slave_config(master->master, 0, slave->index, slave->vid, slave->pid))) {
        rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to read slave %s.%s configuration\n", master->name, slave->name);
        goto fail2;
      }

      // initialize sdos
      if (slave->sdo_config != NULL) {
        for (sdo_config = slave->sdo_config; sdo_config->index != 0xffff; sdo_config = (lcec_slave_sdoconf_t *) &sdo_config->data[sdo_config->length]) {
          if (sdo_config->subindex == LCEC_CONF_SDO_COMPLETE_SUBIDX) {
            if (ecrt_slave_config_complete_sdo(slave->config, sdo_config->index, &sdo_config->data[0], sdo_config->length) != 0) {
              rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo %04x (complete)\n", master->name, slave->name, sdo_config->index);
            }
          } else {
            if (ecrt_slave_config_sdo(slave->config, sdo_config->index, sdo_config->subindex, &sdo_config->data[0], sdo_config->length) != 0) {
              rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo %04x:%02x\n", master->name, slave->name, sdo_config->index, sdo_config->subindex);
            }
          }
        }
      }

      // initialize idns
      if (slave->idn_config != NULL) {
        for (idn_config = slave->idn_config; idn_config->state != 0; idn_config = (lcec_slave_idnconf_t *) &idn_config->data[idn_config->length]) {
          if (ecrt_slave_config_idn(slave->config, idn_config->drive, idn_config->idn, idn_config->state, &idn_config->data[0], idn_config->length) != 0) {
            rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s drive %d idn %c-%d-%d (state %d, length %u)\n", master->name, slave->name, idn_config->drive,
              (idn_config->idn & 0x8000) ? 'P' : 'S', (idn_config->idn >> 12) & 0x0007, idn_config->idn & 0x0fff, idn_config->state, (unsigned int) idn_config->length);
          }
        }
      }

      // setup pdos
      if (slave->proc_init != NULL) {
        ec_pdo_entry_reg_t *checkpoint = pdo_entry_regs;
        if ((slave->proc_init(comp_id, slave, &pdo_entry_regs)) != 0) {
          goto fail2;
        }
        if (pdo_entry_regs != (checkpoint + slave->pdo_entry_count)) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "Slave %s.%s configured wrong count of PDOs: required %d, configured %d\n",
            master->name, slave->name, slave->pdo_entry_count, (int) (pdo_entry_regs - checkpoint));
          goto fail2;
        }
      }

      // configure dc for this slave
      if (slave->dc_conf != NULL) {
        ecrt_slave_config_dc(slave->config, slave->dc_conf->assignActivate,
          slave->dc_conf->sync0Cycle, slave->dc_conf->sync0Shift,
          slave->dc_conf->sync1Cycle, slave->dc_conf->sync1Shift);
        rtapi_print_msg (RTAPI_MSG_DBG, LCEC_MSG_PFX "configuring DC for slave %s.%s: assignActivate=x%x sync0Cycle=%d sync0Shift=%d sync1Cycle=%d sync1Shift=%d\n",
          master->name, slave->name, slave->dc_conf->assignActivate,
          slave->dc_conf->sync0Cycle, slave->dc_conf->sync0Shift,
          slave->dc_conf->sync1Cycle, slave->dc_conf->sync1Shift);
      }

      // Configure the slave's watchdog times.
      if (slave->wd_conf != NULL) {
        ecrt_slave_config_watchdog(slave->config, slave->wd_conf->divider, slave->wd_conf->intervals);
      }

      // configure slave
      if (slave->sync_info != NULL) {
        if (ecrt_slave_config_pdos(slave->config, EC_END, slave->sync_info)) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s\n", master->name, slave->name);
          goto fail2;
        }
      }

      // export state pins
      if ((slave->hal_state_data = lcec_init_slave_state_hal(master->name, slave->name)) == NULL) {
        goto fail2;
      }
    }

    // terminate PDO entries
    pdo_entry_regs->index = 0;

    // register PDO entries
    if (ecrt_domain_reg_pdo_entry_list(master->domain, master->pdo_entry_regs)) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s PDO entry registration failed\n", master->name);
      goto fail2;
    }

    // initialize dc sync
    if (master->ref_clock_sync_cycles >= 0) {
      lcec_dc_init_r2m(master);
    } else {
#ifdef RTAPI_TASK_PLL_SUPPORT
      lcec_dc_init_m2r(master);
#else
      rtapi_print_msg(RTAPI_MSG_ERR,
          LCEC_MSG_PFX "master %s: M2R DC sync mode not available"
          " (RTAPI_TASK_PLL_SUPPORT missing).\n", master->name);
      goto fail2;
#endif
    }

    // select reference clock slave (if configured)
    if (master->ref_clock_slave_idx >= 0) {
      lcec_slave_t *ref_slave = lcec_slave_by_index(master, master->ref_clock_slave_idx);
      if (ref_slave == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            LCEC_MSG_PFX "master %s: refClockSlaveIdx %d not found\n",
            master->name, master->ref_clock_slave_idx);
        goto fail2;
      }
      if (ref_slave->config == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            LCEC_MSG_PFX "master %s: refClockSlaveIdx %d has no EtherCAT config\n",
            master->name, master->ref_clock_slave_idx);
        goto fail2;
      }
      ecrt_master_select_reference_clock(master->master, ref_slave->config);
    }

    // activating master
    if (ecrt_master_activate(master->master)) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to activate master %s\n", master->name);
      goto fail2;
    }

    // Get internal process data for domain
    master->process_data = ecrt_domain_data(master->domain);
    master->process_data_len = ecrt_domain_size(master->domain);

    // init hal data
    rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s", LCEC_MODULE_NAME, master->name);
    if ((master->hal_data = lcec_init_master_hal(name, 0)) == NULL) {
      goto fail2;
    }

    // export read function
    rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s.read", LCEC_MODULE_NAME, master->name);
    if (hal_export_funct(name, lcec_read_master, master, 0, 0, comp_id) != 0) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s read funct export failed\n", master->name);
      goto fail2;
    }
    // export write function
    rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s.write", LCEC_MODULE_NAME, master->name);
    if (hal_export_funct(name, lcec_write_master, master, 0, 0, comp_id) != 0) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s write funct export failed\n", master->name);
      goto fail2;
    }
  }

  // export read-all function
  rtapi_snprintf(name, HAL_NAME_LEN, "%s.read-all", LCEC_MODULE_NAME);
  if (hal_export_funct(name, lcec_read_all, NULL, 0, 0, comp_id) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "read-all funct export failed\n");
    goto fail2;
  }
  // export write-all function
  rtapi_snprintf(name, HAL_NAME_LEN, "%s.write-all", LCEC_MODULE_NAME);
  if (hal_export_funct(name, lcec_write_all, NULL, 0, 0, comp_id) != 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "write-all funct export failed\n");
    goto fail2;
  }

  rtapi_print_msg(RTAPI_MSG_INFO, LCEC_MSG_PFX "installed driver for %d slaves\n", slave_count);
  hal_ready (comp_id);
  return 0;

fail2:
  lcec_clear_config();
fail1:
  hal_exit(comp_id);
fail0:
  return -EINVAL;
}

/**
 * @brief LinuxCNC HAL component exit point.
 *
 * Called by the RTAPI loader when the lcec module is removed.  Deactivates
 * every EtherCAT master, releases all driver and HAL resources, and
 * unregisters the HAL component.
 *
 * @note All exported HAL real-time functions must have been removed from any
 *       thread before this function is called.
 */
void rtapi_app_exit(void) {
  lcec_master_t *master;

  // deactivate all masters
  for (master = first_master; master != NULL; master = master->next) {
    if (master->master) {
      ecrt_master_deactivate(master->master);
    }
  }

  lcec_clear_config();
#ifdef EC_USPACE_MASTER
  ecrt_lib_cleanup();
#endif
  hal_exit(comp_id);
}

/**
 * @brief Parse the lcec configuration from RTAPI shared memory.
 *
 * The userspace configurator tool (@c lcec_conf) writes a binary, type-tagged
 * record stream to a shared-memory segment keyed by @c LCEC_CONF_SHMEM_KEY.
 * This function:
 *  -# Maps the segment and validates its magic number.
 *  -# Remaps the segment at the correct full size.
 *  -# Walks the record stream, creating @c lcec_master_t and @c lcec_slave_t
 *     instances and applying DC, watchdog, SDO, IDN, and modparam settings.
 *  -# Runs slave pre-initialisation in two stages: non-FsoE slaves first so
 *     that FsoE logic devices can reference the @c fsoeConf data they set.
 *  -# Allocates the PDO entry registration array for each master.
 *
 * @return Total number of configured slaves across all masters on success.
 * @return -1 on any error; all partial allocations are freed via lcec_clear_config().
 *
 * @note Must not be called from a real-time context (performs heap allocation
 *       and RTAPI shared-memory operations).
 *
 * @sideeffect Populates the ::first_master / ::last_master global linked list.
 */
int lcec_parse_config(void) {
  int shmem_id;
  void *shmem_ptr;
  LCEC_CONF_HEADER_T *header;
  size_t length;
  void *conf;
  int slave_count;
  lcec_master_t *master;
  lcec_slave_t *slave;
  ec_pdo_entry_reg_t *pdo_entry_regs;
  LCEC_CONF_TYPE_T conf_type;
  LCEC_CONF_MASTER_T *master_conf;
  LCEC_CONF_SLAVE_T *slave_conf;
  LCEC_CONF_DC_T *dc_conf;
  LCEC_CONF_WATCHDOG_T *wd_conf;
  LCEC_CONF_SYNCMANAGER_T *sm_conf;
  LCEC_CONF_PDO_T *pdo_conf;
  LCEC_CONF_PDOENTRY_T *pe_conf;
  LCEC_CONF_COMPLEXENTRY_T *ce_conf;
  LCEC_CONF_SDOCONF_T *sdo_conf;
  LCEC_CONF_IDNCONF_T *idn_conf;
  LCEC_CONF_MODPARAM_T *modparam_conf;
  lcec_slave_conf_state_t slave_conf_state;

  // initialize list
  first_master = NULL;
  last_master = NULL;

  // try to get config header
  shmem_id = rtapi_shmem_new(LCEC_CONF_SHMEM_KEY, comp_id, sizeof(LCEC_CONF_HEADER_T));
  if (shmem_id < 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "couldn't allocate user/RT shared memory\n");
    goto fail0;
  }
  if (lcec_rtapi_shmem_getptr(shmem_id, &shmem_ptr) < 0 ) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "couldn't map user/RT shared memory\n");
    goto fail1;
  }

  // check magic, get length and close shmem
  header = shmem_ptr;
  if (header->magic != LCEC_CONF_SHMEM_MAGIC) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "lcec_conf is not loaded\n");
    goto fail1;
  }
  length = header->length;
  rtapi_shmem_delete(shmem_id, comp_id);

  // reopen shmem with proper size
  shmem_id = rtapi_shmem_new(LCEC_CONF_SHMEM_KEY, comp_id, sizeof(LCEC_CONF_HEADER_T) + length);
  if (shmem_id < 0) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "couldn't allocate user/RT shared memory\n");
    goto fail0;
  }
  if (lcec_rtapi_shmem_getptr(shmem_id, &shmem_ptr) < 0 ) {
    rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "couldn't map user/RT shared memory\n");
    goto fail1;
  }

  // get pointer to config
  conf = (uint8_t *)shmem_ptr + sizeof(LCEC_CONF_HEADER_T);

  // process config items
  slave_count = 0;
  master = NULL;
  slave = NULL;
  pe_conf = NULL;
  memset(&slave_conf_state, 0, sizeof(lcec_slave_conf_state_t));
  while((conf_type = ((LCEC_CONF_NULL_T *)conf)->confType) != lcecConfTypeNone) {
    // get type
    switch (conf_type) {
      case lcecConfTypeMaster:
        // get config token
        master_conf = (LCEC_CONF_MASTER_T *)conf;
        conf += sizeof(LCEC_CONF_MASTER_T);

        // create master
        master = lcec_create_master(master_conf);
        if (master == NULL) {
          goto fail2;
        }

        // add master to list
        LCEC_LIST_APPEND(first_master, last_master, master);
        break;

      case lcecConfTypeSlave:
        // get config token
        slave_conf = (LCEC_CONF_SLAVE_T *)conf;
        conf += sizeof(LCEC_CONF_SLAVE_T);

        slave = lcec_create_slave(master, slave_conf, &slave_conf_state);
        if (slave == NULL) {
          goto fail2;
        }

        // add slave to list
        LCEC_LIST_APPEND(master->first_slave, master->last_slave, slave);
        slave_count++;
        break;

      case lcecConfTypeDcConf:
        // get config token
        dc_conf = (LCEC_CONF_DC_T *)conf;
        conf += sizeof(LCEC_CONF_DC_T);

        // configure dc
        if (lcec_slave_conf_dc(slave, dc_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypeWatchdog:
        // get config token
        wd_conf = (LCEC_CONF_WATCHDOG_T *)conf;
        conf += sizeof(LCEC_CONF_WATCHDOG_T);

        // configure wd
        if (lcec_slave_conf_wd(slave, wd_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypeSyncManager:
        // get config token
        sm_conf = (LCEC_CONF_SYNCMANAGER_T *)conf;
        conf += sizeof(LCEC_CONF_SYNCMANAGER_T);

        // configure syncmanager
        if (lcec_generic_conf_sm(&slave_conf_state.generic, sm_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypePdo:
        // get config token
        pdo_conf = (LCEC_CONF_PDO_T *)conf;
        conf += sizeof(LCEC_CONF_PDO_T);

        // configure PDO
        if (lcec_generic_conf_pdo(&slave_conf_state.generic, pdo_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypePdoEntry:
        // get config token
        pe_conf = (LCEC_CONF_PDOENTRY_T *)conf;
        conf += sizeof(LCEC_CONF_PDOENTRY_T);

        // configure PDO entry
        if (lcec_generic_conf_pdo_entry(&slave_conf_state.generic, pe_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypeComplexEntry:
        // get config token
        ce_conf = (LCEC_CONF_COMPLEXENTRY_T *)conf;
        conf += sizeof(LCEC_CONF_COMPLEXENTRY_T);

        // configure complex entry
        if (lcec_generic_conf_complex_entry(&slave_conf_state.generic, ce_conf)) {
          goto fail2;
        }
        break;

      case lcecConfTypeSdoConfig:
        // get config token
        sdo_conf = (LCEC_CONF_SDOCONF_T *)conf;
        conf += sizeof(LCEC_CONF_SDOCONF_T) + sdo_conf->length;

        lcec_slave_conf_sdo(&slave_conf_state, sdo_conf);
        break;

      case lcecConfTypeIdnConfig:
        // get config token
        idn_conf = (LCEC_CONF_IDNCONF_T *)conf;
        conf += sizeof(LCEC_CONF_IDNCONF_T) + idn_conf->length;

        lcec_slave_conf_idn(&slave_conf_state, idn_conf);
        break;

      case lcecConfTypeModParam:
        // get config token
        modparam_conf = (LCEC_CONF_MODPARAM_T *)conf;
        conf += sizeof(LCEC_CONF_MODPARAM_T);

        lcec_slave_conf_modparam(&slave_conf_state, modparam_conf);
        break;

      default:
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unknown config item type\n");
        goto fail2;
    }
  }

  // close shmem
  rtapi_shmem_delete(shmem_id, comp_id);

  // allocate PDO entity memory
  for (master = first_master; master != NULL; master = master->next) {
    // stage 1 preinit: process all but FSOE logic devices
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      if (!slave->is_fsoe_logic && slave->proc_preinit != NULL) {
        if (slave->proc_preinit(slave) < 0) {
          goto fail2;
        }
      }
    }

    // stage 2 preinit: process only FSOE logic devices (this depends on initialized fsoeConf data)
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      if (slave->is_fsoe_logic  && slave->proc_preinit != NULL) {
        if (slave->proc_preinit(slave) < 0) {
          goto fail2;
        }
      }
    }

    // stage 3 preinit: sum required pdo mappings
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      master->pdo_entry_count += slave->pdo_entry_count;
    }

    // alloc mem for pdo mappings
    pdo_entry_regs = lcec_zalloc(sizeof(ec_pdo_entry_reg_t) * (master->pdo_entry_count + 1));
    if (pdo_entry_regs == NULL) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate master %s PDO entry memory\n", master->name);
      goto fail2;
    }
    master->pdo_entry_regs = pdo_entry_regs;
  }

  return slave_count;

fail2:
  lcec_clear_config();
fail1:
  rtapi_shmem_delete(shmem_id, comp_id);
fail0:
  return -1;
}

/**
 * @brief Free all resources allocated by lcec_parse_config().
 *
 * Iterates the master and slave linked lists in reverse order.  For each
 * slave, calls @c proc_cleanup (if set) then frees the slave object.  For
 * each master, calls lcec_shutdown_master(), frees the PDO entry registration
 * array, and frees the master object.
 *
 * Safe to call after a partial initialisation (e.g. lcec_parse_config()
 * failed mid-way); NULL pointers and empty lists are handled gracefully.
 *
 * @note Must not be called from a real-time context.
 *
 * @sideeffect Resets ::first_master and ::last_master to NULL.
 */
void lcec_clear_config(void) {
  lcec_master_t *master, *prev_master;
  lcec_slave_t *slave, *prev_slave;

  // iterate all masters
  master = last_master;
  while (master != NULL) {
    prev_master = master->prev;

    // iterate all slaves
    slave = master->last_slave;
    while (slave != NULL) {
      prev_slave = slave->prev;

      // cleanup slave
      if (slave->proc_cleanup != NULL) {
        slave->proc_cleanup(slave);
      }

      // free slave
      lcec_free_slave(slave);
      slave = prev_slave;
    }

    // release master
    lcec_shutdown_master(master);

    // free PDO entry memory
    if (master->pdo_entry_regs != NULL) {
      lcec_free(master->pdo_entry_regs);
    }

    // free master
    lcec_free(master);
    master = prev_master;
  }
}

/**
 * @brief HAL real-time function: read process data from all EtherCAT masters.
 *
 * Exported as @c lcec.read-all.  Resets the aggregate master-state counters,
 * delegates to lcec_read_master() for every configured master in list order,
 * then updates the global aggregate HAL state pins.
 *
 * @param arg     Unused; @c NULL is passed at export time.
 * @param period  Servo period in nanoseconds.
 *
 * @note Runs in real-time context.  Must not block or allocate memory.
 */
void lcec_read_all(void *arg, long period) {
  lcec_master_t *master;

  // initialize global state
  global_ms.slaves_responding = 0;
  global_ms.al_states = 0;
  global_ms.link_up = (first_master != NULL);

  // process slaves
  for (master = first_master; master != NULL; master = master->next) {
    lcec_read_master(master, period);
  }

  // update global state pins
  lcec_update_master_hal(global_hal_data, &global_ms);
}

/**
 * @brief HAL real-time function: write process data to all EtherCAT masters.
 *
 * Exported as @c lcec.write-all.  Delegates to lcec_write_master() for every
 * configured master in list order.
 *
 * @param arg     Unused; @c NULL is passed at export time.
 * @param period  Servo period in nanoseconds.
 *
 * @note Runs in real-time context.  Must not block or allocate memory.
 */
void lcec_write_all(void *arg, long period) {
  lcec_master_t *master;

  // process slaves
  for (master = first_master; master != NULL; master = master->next) {
    lcec_write_master(master, period);
  }
}
