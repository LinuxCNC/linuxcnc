/**
 * @file master.c
 * @brief EtherCAT master management for the LinuxCNC EtherCAT HAL driver.
 *
 * Implements creation, startup, shutdown, and real-time operation of EtherCAT
 * masters.  It exports HAL pins that reflect master-level bus state (link
 * status, slave counts, AL states) and drives the PDO read/write cycle that
 * feeds every slave driver.
 *
 * Two build targets are supported:
 *  - @b EC_USPACE_MASTER — userspace IgH master using explicit transport objects.
 *  - Kernel mode — uses @c ecrt_request_master() and optional lock callbacks.
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

#include "priv.h"

#include "rtapi_mutex.h"

/** @brief HAL pin descriptors exported for every master instance, including the global summary master. */
static const lcec_pindesc_t master_global_pins[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_master_data_t, slaves_responding), "%s.slaves-responding" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_init), "%s.state-init" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_preop), "%s.state-preop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_safeop), "%s.state-safeop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_op), "%s.state-op" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, link_up), "%s.link-up" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, all_op), "%s.all-op" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief HAL pin descriptors exported only for individual (non-global) master instances.
 *
 * These pins are conditionally compiled based on @c RTAPI_TASK_PLL_SUPPORT and
 * provide PLL synchronisation diagnostics for the DC reference clock.
 */
static const lcec_pindesc_t master_pins[] = {
#ifdef RTAPI_TASK_PLL_SUPPORT
  { HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_err), "%s.pll-err" },
  { HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_out), "%s.pll-out" },
  { HAL_U32, HAL_OUT, offsetof(lcec_master_data_t, pll_reset_cnt), "%s.pll-reset-count" },
#endif
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief Allocate and export HAL pins for a master.
 *
 * Allocates a zeroed @c lcec_master_data_t from HAL shared memory and registers
 * all HAL output pins described by @c master_global_pins.  When @p global is
 * zero the per-master pins from @c master_pins are also registered (includes
 * PLL diagnostic pins when @c RTAPI_TASK_PLL_SUPPORT is defined).
 *
 * @param pfx    HAL name prefix for all pins (e.g. @c "lcec.0").
 * @param global Non-zero when creating the aggregate "global" summary master;
 *               zero for a real master instance.
 * @return Pointer to the allocated HAL data structure, or NULL on failure.
 *
 * @note Must be called from init context, not from a real-time thread.
 */
lcec_master_data_t *lcec_init_master_hal(int comp_id, const char *pfx, int global) {
  lcec_master_data_t *hal_data;

  // alloc hal data
  if ((hal_data = hal_malloc(sizeof(lcec_master_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for %s failed\n", pfx);
    return NULL;
  }
  memset(hal_data, 0, sizeof(lcec_master_data_t));

  // export pins
  if (lcec_pin_newf_list(comp_id, hal_data, master_global_pins, pfx) != 0) {
    return NULL;
  }
  if (!global) {
    if (lcec_pin_newf_list(comp_id, hal_data, master_pins, pfx) != 0) {
      return NULL;
    }
  }

  return hal_data;
}

/**
 * @brief Update master HAL output pins from the current EtherCAT master state.
 *
 * Copies fields from the IgH @c ec_master_state_t snapshot into the HAL-visible
 * pin storage.  Each AL state bit is decomposed into individual boolean pins
 * (@c state_init, @c state_preop, @c state_safeop, @c state_op) and the
 * @c all_op pin is set only when every slave reports the OP state (al_states == 0x08).
 *
 * @param hal_data  Pointer to the HAL data block whose pins will be written.
 * @param ms        Pointer to the master state snapshot (read-only).
 *
 * @note Called from the real-time read function; must not block or allocate.
 */
void lcec_update_master_hal(lcec_master_data_t *hal_data, ec_master_state_t *ms) {
  *(hal_data->slaves_responding) = ms->slaves_responding;
  *(hal_data->state_init) = (ms->al_states & 0x01) != 0;
  *(hal_data->state_preop) = (ms->al_states & 0x02) != 0;
  *(hal_data->state_safeop) = (ms->al_states & 0x04) != 0;
  *(hal_data->state_op) = (ms->al_states & 0x08) != 0;
  *(hal_data->link_up) = ms->link_up;
  *(hal_data->all_op) = (ms->al_states == 0x08);
}

/**
 * @brief Create and initialise an @c lcec_master_t from its configuration.
 *
 * Allocates a zeroed master structure and copies identity and timing parameters
 * from @p master_conf.  Under @c EC_USPACE_MASTER the transport type, network
 * interface names, debug level, and CPU affinity are also stored.
 *
 * The master is not yet connected to the EtherCAT stack; call
 * @c lcec_startup_master() to open the physical master.
 *
 * @param master_conf  Configuration record produced by the XML parser.
 * @return Pointer to the new master, or NULL on allocation failure or invalid
 *         configuration (e.g. @c refClockSyncCycles < 0 without PLL support).
 */
lcec_master_t * lcec_create_master(LCEC_CONF_MASTER_T *master_conf) {
  lcec_master_t *master;

#ifndef RTAPI_TASK_PLL_SUPPORT
  if (master_conf->refClockSyncCycles < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Master %d: refClockSyncCycles < 0"
      " (sync master to ref) not available (RTAPI_TASK_PLL_SUPPORT missing)\n",
      master_conf->index);
    goto fail0;
  }
#endif

  // alloc master memory
  master = rtapi_calloc(sizeof(lcec_master_t));
  if (master == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate master %d structure memory\n", master_conf->index);
    goto fail0;
  }

  // initialize master
  master->index = master_conf->index;
  strncpy(master->name, master_conf->name, LCEC_CONF_STR_MAXLEN);
  master->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
  master->app_time_period = master_conf->appTimePeriod;
  master->ref_clock_sync_cycles = master_conf->refClockSyncCycles;
  master->ref_clock_slave_idx = master_conf->refClockSlaveIdx;
#ifdef EC_USPACE_MASTER
  master->transport_type = master_conf->transportType;
  strncpy(master->interface, master_conf->interface, LCEC_CONF_STR_MAXLEN);
  master->interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
  strncpy(master->backup_interface, master_conf->backupInterface, LCEC_CONF_STR_MAXLEN);
  master->backup_interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
  master->debug_level = master_conf->debugLevel;
  master->run_on_cpu = master_conf->runOnCpu;
#endif

  return master;

fail0:
  return NULL;
}

#ifdef EC_USPACE_MASTER
/**
 * @brief Open the EtherCAT master (userspace build).
 *
 * Creates the primary network transport from @c master->interface and,
 * optionally, a backup transport from @c master->backup_interface.  Then calls
 * @c ecrt_startup_master() to initialise the IgH userspace master with the
 * chosen transports, debug level, and CPU affinity.
 *
 * On failure all resources created so far are released before returning.
 *
 * @param master  Initialised master structure (created by @c lcec_create_master()).
 * @return 0 on success, -1 on failure (error message sent to RTAPI log).
 */
int lcec_startup_master(lcec_master_t *master) {
  // create main transport
  master->transport = ec_transport_create(
      (ec_transport_type_t) master->transport_type, master->interface);
  if (!master->transport) {
    rtapi_print_msg(RTAPI_MSG_ERR,
        LCEC_MSG_PFX "failed to create transport for master %s (iface %s)\n",
        master->name, master->interface);
    goto fail0;
  }

  // create backup transport (if configured)
  master->backup_transport = NULL;
  if (master->backup_interface[0]) {
    master->backup_transport = ec_transport_create(
        (ec_transport_type_t) master->transport_type, master->backup_interface);
    if (!master->backup_transport) {
      rtapi_print_msg(RTAPI_MSG_ERR,
          LCEC_MSG_PFX "failed to create backup transport for master %s (iface %s)\n",
          master->name, master->backup_interface);
      goto fail1;
    }
  }

  // startup userspace master
  master->master = ecrt_startup_master(
      master->index, master->transport, master->backup_transport,
      master->debug_level, master->run_on_cpu);
  if (!master->master) {
    rtapi_print_msg(RTAPI_MSG_ERR,
        LCEC_MSG_PFX "startup of master %s (index %d, iface %s) failed\n",
        master->name, master->index, master->interface);
    goto fail2;
  }

  return 0;

fail2:
  if (master->backup_transport) {
    ec_transport_destroy(master->backup_transport);
    master->backup_transport = NULL;
  }
fail1:
  ec_transport_destroy(master->transport);
  master->transport = NULL;
fail0:
  return -1;
}

/**
 * @brief Release the EtherCAT master and destroy its transports (userspace build).
 *
 * Calls @c ecrt_release_master() on the IgH master handle, then destroys both
 * the primary and backup transports.  Safe to call even if startup was only
 * partially completed (NULL-checks are performed internally).
 *
 * @param master  Master to shut down.
 */
void lcec_shutdown_master(lcec_master_t *master) {
  if (master->master) {
    ecrt_release_master(master->master);
  }

  // destroy transports (caller owns create/destroy lifecycle)
  if (master->transport) {
    ec_transport_destroy(master->transport);
  }
  if (master->backup_transport) {
    ec_transport_destroy(master->backup_transport);
  }
}

#else

/**
 * @brief IgH master lock callback — acquires the master mutex.
 *
 * Registered with @c ecrt_master_callbacks() so that the kernel-mode EtherCAT
 * master can serialise bus access against the real-time task.
 *
 * @param data  Opaque pointer cast to @c lcec_master_t.
 */
static void lcec_request_lock(void *data) {
  lcec_master_t *master = (lcec_master_t *) data;
  rtapi_mutex_get(&master->mutex);
}

/**
 * @brief IgH master unlock callback — releases the master mutex.
 *
 * @param data  Opaque pointer cast to @c lcec_master_t.
 */
static void lcec_release_lock(void *data) {
  lcec_master_t *master = (lcec_master_t *) data;
  rtapi_mutex_give(&master->mutex);
}

/**
 * @brief Open the EtherCAT master (kernel-mode build).
 *
 * Requests the kernel EtherCAT master by index via @c ecrt_request_master().
 * In @c __KERNEL__ builds the mutex-based lock/unlock callbacks are also
 * registered with @c ecrt_master_callbacks() to serialise bus access.
 *
 * @param master  Initialised master structure.
 * @return 0 on success, -1 on failure.
 */
int lcec_startup_master(lcec_master_t *master) {
    if (!(master->master = ecrt_request_master(master->index))) {
      rtapi_print_msg(RTAPI_MSG_ERR,
          LCEC_MSG_PFX "requesting master %s (index %d) failed\n",
          master->name, master->index);
      return -1;
    }
#ifdef __KERNEL__
    // register callbacks
    ecrt_master_callbacks(master->master, lcec_request_lock, lcec_release_lock, master);
#endif
    return 0;
}

/**
 * @brief Release the kernel EtherCAT master.
 *
 * Calls @c ecrt_release_master() if a master handle was acquired.
 *
 * @param master  Master to shut down.
 */
void lcec_shutdown_master(lcec_master_t *master) {
  if (master->master) {
    ecrt_release_master(master->master);
  }
}

#endif /* EC_USPACE_MASTER */

/**
 * @brief Real-time read function — receive PDOs and update HAL state pins.
 *
 * This function is exported as a HAL real-time function and is called once per
 * servo cycle.  It performs the following steps in order:
 *  -# Validates the servo period against @c master->app_time_period and emits a
 *     warning on the first mismatch.
 *  -# Invokes @c dcsync_callbacks.cycle_start() to stamp the master application
 *     time for distributed clock synchronisation.
 *  -# Periodically (every @c LCEC_STATE_UPDATE_PERIOD nanoseconds) queries
 *     @c ecrt_master_state() and @c ecrt_slave_config_state() for each slave.
 *  -# Calls @c ecrt_master_receive() and @c ecrt_domain_process() under the
 *     master mutex to latch fresh PDO data.
 *  -# Updates master-level and global AL-state HAL pins.
 *  -# Iterates over all slaves, refreshing state pins and calling each slave's
 *     @c proc_read callback.
 *
 * @param arg     Pointer to the @c lcec_master_t for this master.
 * @param period  Servo period in nanoseconds, as supplied by the RTAPI scheduler.
 *
 * @note Real-time safe: must not block, sleep, or allocate memory.
 * @note Acquires/releases @c master->mutex around EtherCAT stack calls.
 */
void lcec_read_master(void *arg, long period) {
  lcec_master_t *master = (lcec_master_t *) arg;
  lcec_slave_t *slave;
  int check_states;

  // check for initialized proc data pointer
  if (master->process_data == NULL) {
    return;
  }

  // check period
  if (period != master->period_last) {
    master->period_last = period;
    if (master->app_time_period != period) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Invalid appTimePeriod of %u for master %s (should be %ld).\n",
        master->app_time_period, master->name, period);
    }
  }

  // set master time in nano-seconds
  master->dcsync_callbacks.cycle_start(master);

  // get state check flag
  if (master->state_update_timer > 0) {
    check_states = 0;
    master->state_update_timer -= period;
  } else {
    check_states = 1;
    master->state_update_timer = LCEC_STATE_UPDATE_PERIOD;
  }

  // receive process data & master state
  rtapi_mutex_get(&master->mutex);
  ecrt_master_receive(master->master);
  ecrt_domain_process(master->domain);
  if (check_states) {
    ecrt_master_state(master->master, &master->ms);
  }
  rtapi_mutex_give(&master->mutex);

  // update state pins
  lcec_update_master_hal(master->hal_data, &master->ms);

  // process slaves
  for (slave = master->first_slave; slave != NULL; slave = slave->next) {
    // get slaves state
    rtapi_mutex_get(&master->mutex);
    if (check_states) {
      ecrt_slave_config_state(slave->config, &slave->state);
    }
    rtapi_mutex_give(&master->mutex);
    if (check_states) {
      lcec_update_slave_state_hal(slave->hal_state_data, &slave->state);
    }

    // process read function
    if (slave->proc_read != NULL) {
      slave->proc_read(slave, period);
    }
  }
}

/**
 * @brief Real-time write function — send PDOs to the EtherCAT bus.
 *
 * Called once per servo cycle after @c lcec_read_master().  Steps:
 *  -# Iterates over all slaves, calling each slave's @c proc_write callback to
 *     pack output data into the PDO image.
 *  -# Acquires the master mutex.
 *  -# Calls @c ecrt_domain_queue() to mark the domain's PDO data as ready.
 *  -# Invokes @c dcsync_callbacks.pre_send() for last-moment DC time stamping.
 *  -# Calls @c ecrt_master_send() to transmit the queued Ethernet frames.
 *  -# Invokes @c dcsync_callbacks.post_send() for post-send DC clock update.
 *  -# Releases the master mutex.
 *
 * @param arg     Pointer to the @c lcec_master_t for this master.
 * @param period  Servo period in nanoseconds.
 *
 * @note Real-time safe: must not block, sleep, or allocate memory.
 * @note Acquires/releases @c master->mutex around EtherCAT stack calls.
 */
void lcec_write_master(void *arg, long period) {
  lcec_master_t *master = (lcec_master_t *) arg;
  lcec_slave_t *slave;

  // check for initialized proc data pointer
  if (master->process_data == NULL) {
    return;
  }

  // process slaves
  for (slave = master->first_slave; slave != NULL; slave = slave->next) {
    if (slave->proc_write != NULL) {
      slave->proc_write(slave, period);
    }
  }

  rtapi_mutex_get(&master->mutex);

  // queue process data
  ecrt_domain_queue(master->domain);

  // sync distributed clock just before master_send to set
  // most accurate master clock time
  master->dcsync_callbacks.pre_send(master);

  // send domain data
  ecrt_master_send(master->master);

  // update the master clock
  // Note: called after ecrt_master_send() to reduce time
  // jitter in the sync_distributed_clocks() call
  master->dcsync_callbacks.post_send(master);

  rtapi_mutex_give(&master->mutex);
}
