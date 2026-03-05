#include "priv.h"

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, online), "%s.%s.%s.slave-online" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, operational), "%s.%s.%s.slave-oper" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_init), "%s.%s.%s.slave-state-init" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_preop), "%s.%s.%s.slave-state-preop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_safeop), "%s.%s.%s.slave-state-safeop" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_op), "%s.%s.%s.slave-state-op" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

lcec_slave_t *lcec_create_slave(lcec_master_t *master, LCEC_CONF_SLAVE_T *slave_conf, lcec_slave_conf_state_t *conf_state) {
  lcec_slave_t *slave;
  const lcec_typelist_t *type;

  // reset config state
  memset(conf_state, 0, sizeof(lcec_slave_conf_state_t));

  // check for master
  if (master == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Master node for slave missing\n");
    goto fail0;
  }

  // check for valid slave type
  if (slave_conf->type == lcecSlaveTypeGeneric) {
    type = NULL;
  } else {
    for (type = typelist; type->type != slave_conf->type && type->type != lcecSlaveTypeInvalid; type++);
    if (type->type == lcecSlaveTypeInvalid) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Invalid slave type %d\n", slave_conf->type);
      goto fail0;
    }
  }

  // create new slave
  slave = lcec_zalloc(sizeof(lcec_slave_t));
  if (slave == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s structure memory\n", master->name, slave_conf->name);
    goto fail0;
  }

  slave->index = slave_conf->index;
  slave->type = slave_conf->type;
  strncpy(slave->name, slave_conf->name, LCEC_CONF_STR_MAXLEN);
  slave->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
  slave->master = master;

  if (type != NULL) {
    // normal slave
    slave->vid = type->vid;
    slave->pid = type->pid;
    slave->pdo_entry_count = type->pdo_entry_count;
    slave->is_fsoe_logic = type->is_fsoe_logic;
    slave->proc_preinit = type->proc_preinit;
    slave->proc_init = type->proc_init;
  } else {
    if (lcec_generic_conf_init(slave, slave_conf, &conf_state->generic)) {
      goto fail1;
    }
  }

  // alloc sdo config memory
  if (slave_conf->sdoConfigLength > 0) {
    slave->sdo_config = lcec_zalloc(slave_conf->sdoConfigLength + sizeof(lcec_slave_sdoconf_t));
    if (slave->sdo_config == NULL) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s sdo entry memory\n", master->name, slave_conf->name);
      goto fail1;
    }
  }

  // alloc idn config memory
  if (slave_conf->idnConfigLength > 0) {
    slave->idn_config = lcec_zalloc(slave_conf->idnConfigLength + sizeof(lcec_slave_idnconf_t));
    if (slave->idn_config == NULL) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s idn entry memory\n", master->name, slave_conf->name);
      goto fail1;
    }
  }

  // alloc modparam memory
  if (slave_conf->modParamCount > 0) {
    slave->modparams = lcec_zalloc(sizeof(lcec_slave_modparam_t) * (slave_conf->modParamCount + 1));
    if (slave->modparams == NULL) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s modparam memory\n", master->name, slave_conf->name);
      goto fail1;
    }
    slave->modparams[slave_conf->modParamCount].id = -1;
  }

  // init config state
  conf_state->generic.hal_data = slave->hal_data;
  conf_state->sdo_config = slave->sdo_config;
  conf_state->idn_config = slave->idn_config;
  conf_state->modparams = slave->modparams;

  return slave;

fail1:
  lcec_free_slave(slave);
fail0:
  return NULL;
}

void lcec_free_slave(lcec_slave_t *slave) {
  // free generic device stuff
  lcec_generic_free_slave(slave);

  if (slave->modparams != NULL) {
    lcec_free(slave->modparams);
  }
  if (slave->sdo_config != NULL) {
    lcec_free(slave->sdo_config);
  }
  if (slave->idn_config != NULL) {
    lcec_free(slave->idn_config);
  }
  if (slave->dc_conf != NULL) {
    lcec_free(slave->dc_conf);
  }
  if (slave->wd_conf != NULL) {
    lcec_free(slave->wd_conf);
  }
  lcec_free(slave);
}

int lcec_slave_conf_dc(lcec_slave_t *slave, LCEC_CONF_DC_T *dc_conf) {
  lcec_slave_dc_t *dc;

  // check for slave
  if (slave == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Slave node for dc config missing\n");
    return -1;
  }

  // check for double dc config
  if (slave->dc_conf != NULL) {
    rtapi_print_msg(RTAPI_MSG_WARN, LCEC_MSG_PFX "Double dc config for slave %s.%s\n", slave->master->name, slave->name);
    return -1;
  }

  // create new dc config
  dc = lcec_zalloc(sizeof(lcec_slave_dc_t));
  if (dc == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s dc config memory\n", slave->master->name, slave->name);
    return -1;
  }

  // initialize dc conf
  dc->assignActivate = dc_conf->assignActivate;
  dc->sync0Cycle = dc_conf->sync0Cycle;
  dc->sync0Shift = dc_conf->sync0Shift;
  dc->sync1Cycle = dc_conf->sync1Cycle;
  dc->sync1Shift = dc_conf->sync1Shift;

  // add to slave
  slave->dc_conf = dc;

  return 0;
}

int lcec_slave_conf_wd(lcec_slave_t *slave, LCEC_CONF_WATCHDOG_T *wd_conf) {
  lcec_slave_watchdog_t *wd;

  // check for slave
  if (slave == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Slave node for watchdog config missing\n");
    return -1;
  }

  // check for double wd config
  if (slave->wd_conf != NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Double watchdog config for slave %s.%s\n", slave->master->name, slave->name);
    return -1;
  }

  // create new wd config
  wd = lcec_zalloc(sizeof(lcec_slave_watchdog_t));
  if (wd == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s watchdog config memory\n", slave->master->name, slave->name);
    return -1;
  }

  // initialize wd conf
  wd->divider = wd_conf->divider;
  wd->intervals = wd_conf->intervals;

  // add to slave
  slave->wd_conf = wd;

  return 0;
}

void lcec_slave_conf_sdo(lcec_slave_conf_state_t *state, LCEC_CONF_SDOCONF_T *sdo_conf) {
  // copy attributes
  state->sdo_config->index = sdo_conf->index;
  state->sdo_config->subindex = sdo_conf->subindex;
  state->sdo_config->length = sdo_conf->length;

  // copy data
  memcpy(state->sdo_config->data, sdo_conf->data, state->sdo_config->length);

  state->sdo_config = (lcec_slave_sdoconf_t *) &state->sdo_config->data[state->sdo_config->length];
  state->sdo_config->index = 0xffff;
}

void lcec_slave_conf_idn(lcec_slave_conf_state_t *state, LCEC_CONF_IDNCONF_T *idn_conf) {
  // copy attributes
  state->idn_config->drive = idn_conf->drive;
  state->idn_config->idn = idn_conf->idn;
  state->idn_config->state = idn_conf->state;
  state->idn_config->length = idn_conf->length;

  // copy data
  memcpy(state->idn_config->data, idn_conf->data, state->idn_config->length);

  state->idn_config = (lcec_slave_idnconf_t *) &state->idn_config->data[state->idn_config->length];
}

void lcec_slave_conf_modparam(lcec_slave_conf_state_t *state, LCEC_CONF_MODPARAM_T *modparam_conf) {
  // copy attributes
  state->modparams->id = modparam_conf->id;
  state->modparams->value = modparam_conf->value;

  // next entry
  state->modparams++;
}

lcec_slave_state_t *lcec_init_slave_state_hal(char *master_name, char *slave_name) {
  lcec_slave_state_t *hal_data;

  // alloc hal data
  if ((hal_data = hal_malloc(sizeof(lcec_slave_state_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for %s.%s.%s failed\n", LCEC_MODULE_NAME, master_name, slave_name);
    return NULL;
  }
  memset(hal_data, 0, sizeof(lcec_slave_state_t));

  // export pins
  if (lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master_name, slave_name) != 0) {
    return NULL;
  }

  return hal_data;
}

void lcec_update_slave_state_hal(lcec_slave_state_t *hal_data, ec_slave_config_state_t *ss) {
  *(hal_data->online) = ss->online;
  *(hal_data->operational) = ss->operational;
  *(hal_data->state_init) = (ss->al_state & 0x01) != 0;
  *(hal_data->state_preop) = (ss->al_state & 0x02) != 0;
  *(hal_data->state_safeop) = (ss->al_state & 0x04) != 0;
  *(hal_data->state_op) = (ss->al_state & 0x08) != 0;
}

