#include "priv.h"

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

static const lcec_pindesc_t master_pins[] = {
#ifdef RTAPI_TASK_PLL_SUPPORT
  { HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_err), "%s.pll-err" },
  { HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_out), "%s.pll-out" },
  { HAL_U32, HAL_OUT, offsetof(lcec_master_data_t, pll_reset_cnt), "%s.pll-reset-count" },
#endif
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

lcec_master_data_t *lcec_init_master_hal(const char *pfx, int global) {
  lcec_master_data_t *hal_data;

  // alloc hal data
  if ((hal_data = hal_malloc(sizeof(lcec_master_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for %s failed\n", pfx);
    return NULL;
  }
  memset(hal_data, 0, sizeof(lcec_master_data_t));

  // export pins
  if (lcec_pin_newf_list(hal_data, master_global_pins, pfx) != 0) {
    return NULL;
  }
  if (!global) {
    if (lcec_pin_newf_list(hal_data, master_pins, pfx) != 0) {
      return NULL;
    }
  }

  return hal_data;
}

void lcec_update_master_hal(lcec_master_data_t *hal_data, ec_master_state_t *ms) {
  *(hal_data->slaves_responding) = ms->slaves_responding;
  *(hal_data->state_init) = (ms->al_states & 0x01) != 0;
  *(hal_data->state_preop) = (ms->al_states & 0x02) != 0;
  *(hal_data->state_safeop) = (ms->al_states & 0x04) != 0;
  *(hal_data->state_op) = (ms->al_states & 0x08) != 0;
  *(hal_data->link_up) = ms->link_up;
  *(hal_data->all_op) = (ms->al_states == 0x08);
}

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
  master = lcec_zalloc(sizeof(lcec_master_t));
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

static void lcec_request_lock(void *data) {
  lcec_master_t *master = (lcec_master_t *) data;
  rtapi_mutex_get(&master->mutex);
}

static void lcec_release_lock(void *data) {
  lcec_master_t *master = (lcec_master_t *) data;
  rtapi_mutex_give(&master->mutex);
}

int lcec_startup_master(lcec_master_t *master) {
    // request kernel ethercat master
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

void lcec_shutdown_master(lcec_master_t *master) {
  if (master->master) {
    ecrt_release_master(master->master);
  }
}

#endif /* EC_USPACE_MASTER */

void lcec_read_master(void *arg, long period) {
  lcec_master_t *master = (lcec_master_t *) arg;
  lcec_slave_t *slave;
  int check_states;

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

  // update global state
  global_ms.slaves_responding += master->ms.slaves_responding;
  global_ms.al_states |= master->ms.al_states;
  global_ms.link_up = global_ms.link_up && master->ms.link_up;

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

void lcec_write_master(void *arg, long period) {
  lcec_master_t *master = (lcec_master_t *) arg;
  lcec_slave_t *slave;

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

