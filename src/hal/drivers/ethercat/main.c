/**
 * @file main.c
 * @brief LinuxCNC EtherCAT HAL component — RT initialisation and real-time functions.
 *
 * Implements the RT lifecycle (lcec_rt_init() / lcec_rt_cleanup()),
 * the configuration parser (lcec_parse_config()), and the global real-time
 * HAL functions (lcec_read_all() / lcec_write_all()) for the lcec driver.
 *
 * All per-instance state is held in the lcec_rt_context_t passed by the
 * cmod plugin (conf.c).  There are no file-scope globals.
 *
 * @section rt_constraints Real-Time Constraints
 * lcec_read_all(), lcec_write_all(), lcec_read_master(), and
 * lcec_write_master() are exported as HAL functions and execute in the
 * LinuxCNC real-time servo thread.  These functions must @b not block, sleep,
 * or perform any dynamic memory allocation or deallocation.
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
#include "conf_priv.h"

/** @brief Static log context for the EtherCAT library log callback. */
static const gomc_log_t *lcec_log_g;
/** @brief Static component name for the EtherCAT library log callback. */
static const char *lcec_name_g;

#ifdef EC_USPACE_MASTER
/**
 * @brief Log callback that forwards EtherCAT library messages to the gomc log ring.
 */
static void lcec_ec_log_callback(int level, const char *fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);

  // strip trailing newlines from EC library messages
  size_t len = strlen(buf);
  while (len > 0 && buf[len - 1] == '\n') {
    buf[--len] = '\0';
  }

  // map syslog levels to gomc log levels
  switch (level) {
  case 0: // LOG_EMERG
  case 1: // LOG_ALERT
  case 2: // LOG_CRIT
  case 3: // LOG_ERR
    gomc_log_errorf(lcec_log_g, lcec_name_g, "%s", buf);
    break;
  case 4: // LOG_WARNING
    gomc_log_warnf(lcec_log_g, lcec_name_g, "%s", buf);
    break;
  case 7: // LOG_DEBUG
    gomc_log_debugf(lcec_log_g, lcec_name_g, "%s", buf);
    break;
  default: // LOG_NOTICE, LOG_INFO
    gomc_log_infof(lcec_log_g, lcec_name_g, "%s", buf);
    break;
  }
}
#endif

static int lcec_parse_config(lcec_rt_context_t *ctx, LCEC_CONF_OUTBUF_T *buf);
static void lcec_clear_config(lcec_rt_context_t *ctx);

static void lcec_read_all(void *arg, long period);
static void lcec_write_all(void *arg, long period);
void lcec_read_master(void *arg, long period);
void lcec_write_master(void *arg, long period);

/**
 * @brief Initialise the EtherCAT RT component.
 *
 * Called from the cmod New() function after hal_init_ex().
 * Parses configuration from shared memory, starts all EtherCAT masters,
 * configures all slaves, and exports the HAL read/write functions.
 *
 * Does NOT call hal_init() or hal_ready() — the caller handles those.
 *
 * @param ctx  Per-instance context; comp_id and instance_name must be set.
 * @return 0 on success, negative on error.
 */
int lcec_rt_init(lcec_rt_context_t *ctx, LCEC_CONF_OUTBUF_T *buf) {
  int slave_count;
  lcec_master_t *master;
  lcec_slave_t *slave;
  char name[GOMC_HAL_NAME_LEN + 1];
  ec_pdo_entry_reg_t *pdo_entry_regs;
  lcec_slave_sdoconf_t *sdo_config;
  lcec_slave_idnconf_t *idn_config;
  struct timeval tv;
  long long rtapi_now;
  const cmod_env_t *env = ctx->env;

  // set up static log pointer for EC library callback
  lcec_log_g = env->log;
  lcec_name_g = ctx->instance_name;

  // get time base
  gettimeofday(&tv, NULL);
  rtapi_now = env->rtapi->get_time(env->rtapi->ctx);
  ctx->dc_time_offset = EC_TIMEVAL2NANO(tv) - rtapi_now;

  // parse configuration
  if ((slave_count = lcec_parse_config(ctx, buf)) < 0) {
    goto fail0;
  }

  // init global hal data
  if ((ctx->global_hal_data = lcec_init_master_hal(env, ctx->comp_id, ctx->instance_name, 1)) == NULL) {
    goto fail1;
  }

#ifdef EC_USPACE_MASTER
  // initialize userspace ethercat master library
  if (ecrt_lib_init(lcec_ec_log_callback, ctx->ipc_socket) < 0) {
    LCEC_CTX_ERR(ctx, "ecrt_lib_init() failed (ipc_socket=%s)",
        ctx->ipc_socket ? ctx->ipc_socket : "NULL");
    goto fail1;
  }
#endif

  // initialize masters
  for (master = ctx->first_master; master != NULL; master = master->next) {
    // set per-instance fields on master
    master->comp_id = ctx->comp_id;
    strncpy(master->instance_name, ctx->instance_name, LCEC_CONF_STR_MAXLEN - 1);
    master->instance_name[LCEC_CONF_STR_MAXLEN - 1] = '\0';
    master->rt_ctx = ctx;
    master->env = ctx->env;
    master->log = ctx->env->log;
    master->comp_name = master->instance_name;

    // startup master
    if (lcec_startup_master(master)) {
      goto fail1;
    }

    // create domain
    if (!(master->domain = ecrt_master_create_domain(master->master))) {
      LCEC_CTX_ERR(ctx, "master %s domain creation failed", master->name);
      goto fail1;
    }

    // initialize slaves
    pdo_entry_regs = master->pdo_entry_regs;
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      // read slave config
      if (!(slave->config = ecrt_master_slave_config(master->master, 0, slave->index, slave->vid, slave->pid))) {
        LCEC_CTX_ERR(ctx, "fail to read slave %s.%s configuration", master->name, slave->name);
        goto fail1;
      }

      // initialize sdos
      if (slave->sdo_config != NULL) {
        for (sdo_config = slave->sdo_config; sdo_config->index != 0xffff; sdo_config = (lcec_slave_sdoconf_t *) &sdo_config->data[sdo_config->length]) {
          if (sdo_config->subindex == LCEC_CONF_SDO_COMPLETE_SUBIDX) {
            if (ecrt_slave_config_complete_sdo(slave->config, sdo_config->index, &sdo_config->data[0], sdo_config->length) != 0) {
              LCEC_CTX_ERR(ctx, "fail to configure slave %s.%s sdo %04x (complete)", master->name, slave->name, sdo_config->index);
            }
          } else {
            if (ecrt_slave_config_sdo(slave->config, sdo_config->index, sdo_config->subindex, &sdo_config->data[0], sdo_config->length) != 0) {
              LCEC_CTX_ERR(ctx, "fail to configure slave %s.%s sdo %04x:%02x", master->name, slave->name, sdo_config->index, sdo_config->subindex);
            }
          }
        }
      }

      // initialize idns
      if (slave->idn_config != NULL) {
        for (idn_config = slave->idn_config; idn_config->state != 0; idn_config = (lcec_slave_idnconf_t *) &idn_config->data[idn_config->length]) {
          if (ecrt_slave_config_idn(slave->config, idn_config->drive, idn_config->idn, idn_config->state, &idn_config->data[0], idn_config->length) != 0) {
            LCEC_CTX_ERR(ctx,
                "fail to configure slave %s.%s drive %d idn %c-%d-%d (state %d, length %u)", master->name, slave->name, idn_config->drive,
              (idn_config->idn & 0x8000) ? 'P' : 'S', (idn_config->idn >> 12) & 0x0007, idn_config->idn & 0x0fff, idn_config->state, (unsigned int) idn_config->length);
          }
        }
      }

      // setup pdos
      if (slave->proc_init != NULL) {
        ec_pdo_entry_reg_t *checkpoint = pdo_entry_regs;
        if ((slave->proc_init(ctx->comp_id, slave, &pdo_entry_regs)) != 0) {
          goto fail1;
        }
        if (pdo_entry_regs != (checkpoint + slave->pdo_entry_count)) {
          LCEC_CTX_ERR(ctx,
              "Slave %s.%s configured wrong count of PDOs: required %d, configured %d",
            master->name, slave->name, slave->pdo_entry_count, (int) (pdo_entry_regs - checkpoint));
          goto fail1;
        }
      }

      // configure dc for this slave
      if (slave->dc_conf != NULL) {
        ecrt_slave_config_dc(slave->config, slave->dc_conf->assignActivate,
          slave->dc_conf->sync0Cycle, slave->dc_conf->sync0Shift,
          slave->dc_conf->sync1Cycle, slave->dc_conf->sync1Shift);
        LCEC_CTX_DBG(ctx,
            "configuring DC for slave %s.%s: assignActivate=x%x sync0Cycle=%d sync0Shift=%d sync1Cycle=%d sync1Shift=%d",
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
          LCEC_CTX_ERR(ctx, "fail to configure slave %s.%s", master->name, slave->name);
          goto fail1;
        }
      }

      // export state pins
      if ((slave->hal_state_data = lcec_init_slave_state_hal(env, ctx->comp_id, ctx->instance_name, master->name, slave->name)) == NULL) {
        goto fail1;
      }
    }

    // terminate PDO entries
    pdo_entry_regs->index = 0;

    // register PDO entries
    if (ecrt_domain_reg_pdo_entry_list(master->domain, master->pdo_entry_regs)) {
      LCEC_CTX_ERR(ctx, "master %s PDO entry registration failed", master->name);
      goto fail1;
    }

    // init hal data
    snprintf(name, GOMC_HAL_NAME_LEN, "%s.%s", ctx->instance_name, master->name);
    if ((master->hal_data = lcec_init_master_hal(env, ctx->comp_id, name, 0)) == NULL) {
      goto fail1;
    }

    // export read function
    snprintf(name, GOMC_HAL_NAME_LEN, "%s.%s.read", ctx->instance_name, master->name);
    if (env->hal->export_funct(env->hal->ctx, name, lcec_read_master, master, 0, 0, ctx->comp_id) != 0) {
      LCEC_CTX_ERR(ctx, "master %s read funct export failed", master->name);
      goto fail1;
    }
    // export write function
    snprintf(name, GOMC_HAL_NAME_LEN, "%s.%s.write", ctx->instance_name, master->name);
    if (env->hal->export_funct(env->hal->ctx, name, lcec_write_master, master, 0, 0, ctx->comp_id) != 0) {
      LCEC_CTX_ERR(ctx, "master %s write funct export failed", master->name);
      goto fail1;
    }
  }

  // export read-all function
  snprintf(name, GOMC_HAL_NAME_LEN, "%s.read-all", ctx->instance_name);
  if (env->hal->export_funct(env->hal->ctx, name, lcec_read_all, ctx, 0, 0, ctx->comp_id) != 0) {
    LCEC_CTX_ERR(ctx, "read-all funct export failed");
    goto fail1;
  }
  // export write-all function
  snprintf(name, GOMC_HAL_NAME_LEN, "%s.write-all", ctx->instance_name);
  if (env->hal->export_funct(env->hal->ctx, name, lcec_write_all, ctx, 0, 0, ctx->comp_id) != 0) {
    LCEC_CTX_ERR(ctx, "write-all funct export failed");
    goto fail1;
  }

  LCEC_CTX_INFO(ctx, "installed driver for %d slaves", slave_count);
  return 0;

fail1:
  lcec_clear_config(ctx);
#ifdef EC_USPACE_MASTER
  ecrt_lib_cleanup();
#endif
  lcec_log_g = NULL;
  lcec_name_g = NULL;
fail0:
  return -EINVAL;
}

/**
 * @brief Tear down the EtherCAT RT component.
 *
 * Called from the cmod Destroy() function before hal_exit().
 * Deactivates every EtherCAT master, releases all driver resources,
 * and cleans up the EtherCAT library.
 *
 * Does NOT call hal_exit() — the caller handles that.
 *
 * @param ctx  Per-instance context.
 */
void lcec_rt_cleanup(lcec_rt_context_t *ctx) {
  lcec_clear_config(ctx);
#ifdef EC_USPACE_MASTER
  ecrt_lib_cleanup();
#endif
  lcec_log_g = NULL;
  lcec_name_g = NULL;
}

int lcec_rt_start(lcec_rt_context_t *ctx)  {
  lcec_master_t *master;

  // activate all masters
  for (master = ctx->first_master; master != NULL; master = master->next) {
    if (!master->master) {
      continue;
    }

    // initialize dc sync
    if (master->ref_clock_sync_cycles >= 0) {
      lcec_dc_init_r2m(master);
    } else {
#ifdef GOMC_RTAPI_TASK_PLL_SUPPORT
      lcec_dc_init_m2r(master);
#else
      LCEC_CTX_ERR(ctx,
          "master %s: M2R DC sync mode not available"
          " (GOMC_RTAPI_TASK_PLL_SUPPORT missing).", master->name);
      return -EINVAL;
#endif
    }

    // select reference clock slave (if configured)
    if (master->ref_clock_slave_idx >= 0) {
      lcec_slave_t *ref_slave = lcec_slave_by_index(master, master->ref_clock_slave_idx);
      if (ref_slave == NULL) {
        LCEC_CTX_ERR(ctx,
            "master %s: refClockSlaveIdx %d not found",
            master->name, master->ref_clock_slave_idx);
        return -EINVAL;
      }
      if (ref_slave->config == NULL) {
        LCEC_CTX_ERR(ctx,
            "master %s: refClockSlaveIdx %d has no EtherCAT config",
            master->name, master->ref_clock_slave_idx);
        return -EINVAL;
      }
      ecrt_master_select_reference_clock(master->master, ref_slave->config);
    }

    // activating master
    if (ecrt_master_activate(master->master)) {
      LCEC_CTX_ERR(ctx, "failed to activate master %s", master->name);
      return -EINVAL;
    }

    // Get internal process data for domain
    master->process_data = ecrt_domain_data(master->domain);
    master->process_data_len = ecrt_domain_size(master->domain);
  }

 return 0;
}

void lcec_rt_stop(lcec_rt_context_t *ctx)  {
  lcec_master_t *master;

  // deactivate all masters
  for (master = ctx->first_master; master != NULL; master = master->next) {
    if (master->master) {
      ecrt_master_deactivate(master->master);
    }
  }
}


/**
 * @brief Parse the lcec configuration from the output buffer linked list.
 *
 * Iterates the linked-list nodes produced by the XML parser, creating masters
 * and slaves, then runs the preinit stages and allocates PDO entry memory.
 * Frees all linked-list nodes when done (both success and error paths).
 *
 * @param ctx  Per-instance context; comp_id must be set.
 * @param buf  Output buffer containing the parsed configuration records.
 * @return Total number of configured slaves on success, -1 on error.
 */
static int lcec_parse_config(lcec_rt_context_t *ctx, LCEC_CONF_OUTBUF_T *buf) {
  LCEC_CONF_OUTBUF_ITEM_T *node;
  void *payload;
  int slave_count;
  lcec_master_t *master;
  lcec_slave_t *slave;
  ec_pdo_entry_reg_t *pdo_entry_regs;
  LCEC_CONF_TYPE_T conf_type;
  lcec_slave_conf_state_t slave_conf_state;
  const cmod_env_t *env = ctx->env;

  // initialize list
  ctx->first_master = NULL;
  ctx->last_master = NULL;

  // process config items
  slave_count = 0;
  master = NULL;
  slave = NULL;
  memset(&slave_conf_state, 0, sizeof(lcec_slave_conf_state_t));
  for (node = buf->head; node != NULL; node = node->next) {
    payload = (void *)(node + 1);
    conf_type = ((LCEC_CONF_NULL_T *)payload)->confType;

    if (conf_type == lcecConfTypeNone) {
      break;
    }

    switch (conf_type) {
      case lcecConfTypeMaster:
        master = lcec_create_master(env, (LCEC_CONF_MASTER_T *)payload);
        if (master == NULL) {
          goto fail;
        }
        master->rt_ctx = ctx;
        master->env = ctx->env;
        master->log = ctx->env->log;
        master->comp_name = master->instance_name;
        LCEC_LIST_APPEND(ctx->first_master, ctx->last_master, master);
        break;

      case lcecConfTypeSlave:
        slave = lcec_create_slave(master, (LCEC_CONF_SLAVE_T *)payload, &slave_conf_state);
        if (slave == NULL) {
          goto fail;
        }
        LCEC_LIST_APPEND(master->first_slave, master->last_slave, slave);
        slave_count++;
        break;

      case lcecConfTypeDcConf:
        if (lcec_slave_conf_dc(slave, (LCEC_CONF_DC_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypeWatchdog:
        if (lcec_slave_conf_wd(slave, (LCEC_CONF_WATCHDOG_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypeSyncManager:
        if (lcec_generic_conf_sm(&slave_conf_state.generic, (LCEC_CONF_SYNCMANAGER_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypePdo:
        if (lcec_generic_conf_pdo(&slave_conf_state.generic, (LCEC_CONF_PDO_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypePdoEntry:
        if (lcec_generic_conf_pdo_entry(&slave_conf_state.generic, (LCEC_CONF_PDOENTRY_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypeComplexEntry:
        if (lcec_generic_conf_complex_entry(&slave_conf_state.generic, (LCEC_CONF_COMPLEXENTRY_T *)payload)) {
          goto fail;
        }
        break;

      case lcecConfTypeSdoConfig:
        lcec_slave_conf_sdo(&slave_conf_state, (LCEC_CONF_SDOCONF_T *)payload, &node);
        break;

      case lcecConfTypeIdnConfig:
        lcec_slave_conf_idn(&slave_conf_state, (LCEC_CONF_IDNCONF_T *)payload, &node);
        break;

      case lcecConfTypeModParam:
        lcec_slave_conf_modparam(&slave_conf_state, (LCEC_CONF_MODPARAM_T *)payload);
        break;

      default:
        LCEC_CTX_ERR(ctx, "Unknown config item type");
        goto fail;
    }
  }

  // allocate PDO entity memory
  for (master = ctx->first_master; master != NULL; master = master->next) {
    // stage 1 preinit: process all but FSOE logic devices
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      if (!slave->is_fsoe_logic && slave->proc_preinit != NULL) {
        if (slave->proc_preinit(slave) < 0) {
          goto fail;
        }
      }
    }

    // stage 2 preinit: process only FSOE logic devices (this depends on initialized fsoeConf data)
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      if (slave->is_fsoe_logic  && slave->proc_preinit != NULL) {
        if (slave->proc_preinit(slave) < 0) {
          goto fail;
        }
      }
    }

    // stage 3 preinit: sum required pdo mappings
    for (slave = master->first_slave; slave != NULL; slave = slave->next) {
      master->pdo_entry_count += slave->pdo_entry_count;
    }

    // alloc mem for pdo mappings
    pdo_entry_regs = env->rtapi->calloc(env->rtapi->ctx, sizeof(ec_pdo_entry_reg_t) * (master->pdo_entry_count + 1));
    if (pdo_entry_regs == NULL) {
      LCEC_CTX_ERR(ctx, "Unable to allocate master %s PDO entry memory", master->name);
      goto fail;
    }
    master->pdo_entry_regs = pdo_entry_regs;
  }

  // free config linked-list nodes
  freeOutputBuffer(buf);

  return slave_count;

fail:
  lcec_clear_config(ctx);
  freeOutputBuffer(buf);
  return -1;
}

/**
 * @brief Free all resources allocated by lcec_parse_config().
 */
static void lcec_clear_config(lcec_rt_context_t *ctx) {
  lcec_master_t *master, *prev_master;
  lcec_slave_t *slave, *prev_slave;
  const cmod_env_t *env = ctx->env;

  // iterate all masters
  master = ctx->last_master;
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
      lcec_free_slave(env, slave);
      slave = prev_slave;
    }

    // release master
    lcec_shutdown_master(master);

    // free PDO entry memory
    if (master->pdo_entry_regs != NULL) {
      env->rtapi->free(env->rtapi->ctx, master->pdo_entry_regs);
    }

    // free master
    env->rtapi->free(env->rtapi->ctx, master);
    master = prev_master;
  }

  ctx->first_master = NULL;
  ctx->last_master = NULL;
}

/**
 * @brief HAL real-time function: read process data from all EtherCAT masters.
 *
 * @param arg     Pointer to the lcec_rt_context_t for this instance.
 * @param period  Servo period in nanoseconds.
 */
static void lcec_read_all(void *arg, long period) {
  lcec_rt_context_t *ctx = (lcec_rt_context_t *)arg;
  lcec_master_t *master;

  // initialize global state
  ctx->global_ms.slaves_responding = 0;
  ctx->global_ms.al_states = 0;
  ctx->global_ms.link_up = (ctx->first_master != NULL);

  // process masters
  for (master = ctx->first_master; master != NULL; master = master->next) {
    lcec_read_master(master, period);

    // accumulate global state from each master
    ctx->global_ms.slaves_responding += master->ms.slaves_responding;
    ctx->global_ms.al_states |= master->ms.al_states;
    ctx->global_ms.link_up = ctx->global_ms.link_up && master->ms.link_up;
  }

  // update global state pins
  lcec_update_master_hal(ctx->global_hal_data, &ctx->global_ms);
}

/**
 * @brief HAL real-time function: write process data to all EtherCAT masters.
 *
 * @param arg     Pointer to the lcec_rt_context_t for this instance.
 * @param period  Servo period in nanoseconds.
 */
static void lcec_write_all(void *arg, long period) {
  lcec_rt_context_t *ctx = (lcec_rt_context_t *)arg;
  lcec_master_t *master;

  // process masters
  for (master = ctx->first_master; master != NULL; master = master->next) {
    lcec_write_master(master, period);
  }
}
