#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "../hal_priv.h"

#include "retain.h"

/* module information */
MODULE_AUTHOR("Sascha Ittner");
MODULE_DESCRIPTION("Retain component for LinuxCNC HAL signals (RT-part)");
MODULE_LICENSE("GPL");

#define COMPNAME "retain"

typedef struct {
    hal_u32_t action;
} hal_retain_t;

static int comp_id;

static void do_sync(void *arg, long period);
static int sync_read(void);

int rtapi_app_main(void) {
  hal_retain_t *hal;

  comp_id = hal_init(COMPNAME);
  if (comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, COMPNAME ": ERROR: hal_init() failed\n");
    goto fail0;
  }

  hal = hal_malloc(sizeof(hal_retain_t));
  if (hal == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, COMPNAME ": ERROR: hal_malloc() failed\n");
    goto fail1;
  }

  if (hal_param_u32_new(RETAIN_ACTION_PARAM, HAL_RW, &(hal->action), comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, COMPNAME ": ERROR: export of param " RETAIN_ACTION_PARAM " failed\n");
    goto fail1;
  }
  if (hal_export_funct(COMPNAME ".sync", do_sync, hal, 1, 0, comp_id)) {
    rtapi_print_msg(RTAPI_MSG_ERR, COMPNAME ": ERROR: export of function " COMPNAME ".sync failed\n");
    goto fail1;
  }

  hal->action = 0;

  hal_ready(comp_id);
  return 0;

fail1:
  hal_exit(comp_id);
fail0:
  return -1;
}

void rtapi_app_exit(void)
{
  hal_exit(comp_id);
}

static void do_sync(void *arg, long period) {
  hal_retain_t *hal = (hal_retain_t *) arg;
  int ret;

  // check for read
  if (hal->action == RETAIN_ACTION_READ) {
    ret = sync_read();
    if (ret >= 0) {
      // trigger store action if data has changed
      hal->action = (ret > 0) ? RETAIN_ACTION_STORE : RETAIN_ACTION_NOOP;
    }
  }
}

static int sync_read(void) {
  int changed = 0;
  int next;
  hal_sig_t *sig;
  void *data_addr;
  hal_data_t *hd = (hal_data_t *) hal_shmem_base;

  // try to get mutex
  if (rtapi_mutex_try(&(hd->mutex))) {
    return -1;
  }

  next = hd->sig_list_ptr;
  while (next != 0) {
    sig = SHMPTR(next);
    next = sig->next_ptr;
    data_addr = SHMPTR(sig->data_ptr);

    // compare retain signals only
    if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
      continue;
    }

    switch (sig->type) {
      case HAL_BIT:
        if (sig->retain_val.bit != *((hal_bit_t *) data_addr)) {
          changed = 1;
          sig->retain_val.bit = *((hal_bit_t *) data_addr);
        }
        break;
      case HAL_U32:
        if (sig->retain_val.u32 != *((hal_u32_t *) data_addr)) {
          changed = 1;
          sig->retain_val.u32 = *((hal_u32_t *) data_addr);
        }
        break;
      case HAL_S32:
        if (sig->retain_val.s32 != *((hal_s32_t *) data_addr)) {
          changed = 1;
          sig->retain_val.s32 = *((hal_s32_t *) data_addr);
        }
        break;
      case HAL_FLOAT:
        if (sig->retain_val.flt != *((hal_float_t *) data_addr)) {
          changed = 1;
          sig->retain_val.flt = *((hal_float_t *) data_addr);
        }
        break;
      default:
        break;
    }
  }

  // release mutex
  rtapi_mutex_give(&(hd->mutex));

  return changed;
}

