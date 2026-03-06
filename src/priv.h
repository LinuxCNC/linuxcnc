#ifndef _LCEC_PRIV_H_
#define _LCEC_PRIV_H_

#include "lcec.h"

#include "devices/generic.h"

typedef struct lcec_typelist {
  LCEC_SLAVE_TYPE_T type;
  uint32_t vid;
  uint32_t pid;
  int pdo_entry_count;
  int is_fsoe_logic;
  lcec_slave_preinit_t proc_preinit;
  lcec_slave_init_t proc_init;
} lcec_typelist_t;

typedef struct {
  lcec_generic_conf_state_t generic;
  lcec_slave_sdoconf_t *sdo_config;
  lcec_slave_idnconf_t *idn_config;
  lcec_slave_modparam_t *modparams;  
} lcec_slave_conf_state_t;

extern const lcec_typelist_t typelist[];

extern int comp_id;
extern ec_master_state_t global_ms;
extern int64_t dc_time_offset;

lcec_master_t * lcec_create_master(LCEC_CONF_MASTER_T *master_conf);
int lcec_startup_master(lcec_master_t *master);
void lcec_shutdown_master(lcec_master_t *master);
lcec_master_data_t *lcec_init_master_hal(const char *pfx, int global);
void lcec_update_master_hal(lcec_master_data_t *hal_data, ec_master_state_t *ms);

lcec_slave_t *lcec_create_slave(lcec_master_t *master, LCEC_CONF_SLAVE_T *slave_conf, lcec_slave_conf_state_t *conf_state);
void lcec_free_slave(lcec_slave_t *slave);
int lcec_slave_conf_dc(lcec_slave_t *slave, LCEC_CONF_DC_T *dc_conf);
int lcec_slave_conf_wd(lcec_slave_t *slave, LCEC_CONF_WATCHDOG_T *wd_conf);
void lcec_slave_conf_sdo(lcec_slave_conf_state_t *state, LCEC_CONF_SDOCONF_T *sdo_conf);
void lcec_slave_conf_idn(lcec_slave_conf_state_t *state, LCEC_CONF_IDNCONF_T *idn_conf);
void lcec_slave_conf_modparam(lcec_slave_conf_state_t *state, LCEC_CONF_MODPARAM_T *modparam_conf);
lcec_slave_state_t *lcec_init_slave_state_hal(char *master_name, char *slave_name);
void lcec_update_slave_state_hal(lcec_slave_state_t *hal_data, ec_slave_config_state_t *ss);

int lcec_pin_newfv(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, va_list ap);
int lcec_pin_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap);
int lcec_param_newfv(hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, va_list ap);
int lcec_param_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap);

void lcec_dc_init_r2m(struct lcec_master *master);
#ifdef RTAPI_TASK_PLL_SUPPORT
void lcec_dc_init_m2r(struct lcec_master *master);
#endif

#endif
