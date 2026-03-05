//
//    Copyright (C) 2012 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include "priv.h"
#include "rtapi_app.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sascha Ittner <sascha.ittner@modusoft.de>");
MODULE_DESCRIPTION("Driver for EtherCAT devices");

static lcec_master_t *first_master = NULL;
static lcec_master_t *last_master = NULL;

int comp_id = -1;

static lcec_master_data_t *global_hal_data;
ec_master_state_t global_ms;

#ifdef EC_USPACE_MASTER
static char *ipc_socket = NULL;
RTAPI_MP_STRING(ipc_socket, "EtherCAT userspace master IPC socket path (NULL = no tool access)");

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

int rtapi_app_main(void) {
  int slave_count;
  lcec_master_t *master;
  lcec_slave_t *slave;
  char name[HAL_NAME_LEN + 1];
  ec_pdo_entry_reg_t *pdo_entry_regs;
  lcec_slave_sdoconf_t *sdo_config;
  lcec_slave_idnconf_t *idn_config;
  struct timeval tv;

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
        if ((slave->proc_init(comp_id, slave, pdo_entry_regs)) != 0) {
          goto fail2;
        }
      }
      pdo_entry_regs += slave->pdo_entry_count;

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

    // terminate POD entries
    pdo_entry_regs->index = 0;

    // register PDO entries
    if (ecrt_domain_reg_pdo_entry_list(master->domain, master->pdo_entry_regs)) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s PDO entry registration failed\n", master->name);
      goto fail2;
    }

    // initialize application time
    lcec_gettimeofday(&tv);
    master->app_time_base = EC_TIMEVAL2NANO(tv);
#ifdef RTAPI_TASK_PLL_SUPPORT
    master->dc_time_valid_last = 0;
    if (master->sync_ref_cycles >= 0) {
      master->app_time_base -= rtapi_get_time();
    }
#else
    master->app_time_base -= rtapi_get_time();
    if (master->sync_ref_cycles < 0) {
      rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "unable to sync master %s cycle to reference clock, RTAPI_TASK_PLL_SUPPORT not present\n", master->name);
    }
#endif

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

#ifdef RTAPI_TASK_PLL_SUPPORT
    // set default PLL_STEP: use +/-0.1% of period
    master->hal_data->pll_step = master->app_time_period / 1000;
    // set default PLL_MAX_ERR: one period
    master->hal_data->pll_max_err = master->app_time_period;
#endif

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

void rtapi_app_exit(void) {
  lcec_master_t *master;

  // deactivate all masters
  for (master = first_master; master != NULL; master = master->next) {
    ecrt_master_deactivate(master->master);
  }

  lcec_clear_config();
#ifdef EC_USPACE_MASTER
  ecrt_lib_cleanup();
#endif
  hal_exit(comp_id);
}

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
  conf = shmem_ptr + sizeof(LCEC_CONF_HEADER_T);

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
        master = lcec_create_master(master_conf );
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

void lcec_clear_config(void) {
  lcec_master_t *master, *prev_master;
  lcec_slave_t *slave, *prev_slave;

  // iterate all masters
  master = last_master;
  while (master != NULL) {
    prev_master = master->prev;

    // iterate all masters
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

void lcec_write_all(void *arg, long period) {
  lcec_master_t *master;

  // process slaves
  for (master = first_master; master != NULL; master = master->next) {
    lcec_write_master(master, period);
  }
}

