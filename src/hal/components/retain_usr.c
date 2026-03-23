#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <sys/eventfd.h>

#include "rtapi.h"
#include "hal.h"
#include "../hal_priv.h"

#include "retain.h"

#define COMPNAME "retain_usr"

#define SYNC_TIMEOUT_MS	1000LL
#define POLL_PERIOD_MS	1000LL

static int exit_event;

static hal_u32_t *action_param;

typedef struct {
  hal_u32_t *update_count;
} RETAIN_HAL_T;

long long get_ms_ticks(void) {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return (long long) tp.tv_sec * 1000LL + ((long long) tp.tv_nsec / 1000000LL);
}

static void sigtermHandler(int sig) {
  uint64_t u = 1;
  if (write(exit_event, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, COMPNAME ": ERROR: error writing exit event\n");
  }
}

static void *get_shared_param_ptr(const char *name, hal_type_t type) {
  hal_param_t *param;

  // find parameter
  param = halpr_find_param_by_name(name);
  if (param == NULL) {
    return NULL;
  }

  // check datatype
  if (param->type != type) {
    return NULL;
  }

  // check for RW
  if (param->dir == HAL_RO) {
    return NULL;
  }

  return (void*) SHMPTR(param->data_ptr);
}

static int sync_action(int action) {
  long long timeout;

  // set action flag
  *action_param = action;

  // wait for finished job, check timeout
  timeout = get_ms_ticks() + SYNC_TIMEOUT_MS;
  while (*action_param == action) {
    // check for timeout
    if (get_ms_ticks() > timeout) {
      return -1;
    }
    sched_yield();
  }

  // return flag for changed data
  return (*action_param == RETAIN_ACTION_STORE);
}

static int save_vars(const char *file_name) {
  hal_data_t *hd = (hal_data_t *) hal_shmem_base;
  int err = 1;
  int ret;
  char tmp_name[256];
  FILE *f = NULL;
  int next;
  hal_sig_t *sig;

  if (snprintf(tmp_name, sizeof(tmp_name), "%s.tmp", file_name) >= sizeof(tmp_name)) {
    fprintf(stderr, COMPNAME ": ERROR: var file name too long!\n");
    goto fail0;
  }

  if ((f = fopen(tmp_name, "w")) == NULL) {
    fprintf(stderr, COMPNAME ": ERROR: unable to open temporary var file for writing!\n");
    goto fail0;
  }

  rtapi_mutex_get(&(hd->mutex));
  next = hd->sig_list_ptr;
  while (next != 0) {
    sig = SHMPTR(next);
    next = sig->next_ptr;

    // store retain signals only
    if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
      continue;
    }

    switch (sig->type) {
      case HAL_BIT:
        ret = fprintf(f, "%s %s\n", sig->name, sig->retain_val.bit ? "TRUE" : "FALSE");
        break;
      case HAL_U32:
        ret = fprintf(f, "%s %u\n", sig->name, sig->retain_val.u32);
        break;
      case HAL_S32:
        ret = fprintf(f, "%s %d\n", sig->name, sig->retain_val.s32);
        break;
      case HAL_FLOAT:
        ret = fprintf(f, "%s %.6f\n", sig->name, sig->retain_val.flt);
        break;
      default:
        ret = 0;
        break;
    }

    // handle error
    if (ret < 0) {
      rtapi_mutex_give(&(hd->mutex));
      fprintf(stderr, COMPNAME ": ERROR: unable to write value to var file!\n");
      goto fail1;
    }
  }
  rtapi_mutex_give(&(hd->mutex));

  if (fflush(f)) {
    fprintf(stderr, COMPNAME ": ERROR: unable to flush var file!\n");
    goto fail1;
  }

  if (fdatasync(fileno(f))) {
    fprintf(stderr, COMPNAME ": ERROR: unable to sync var file!\n");
    goto fail1;
  }

  if (fclose(f)) {
    fprintf(stderr, COMPNAME ": ERROR: unable to close var file!\n");
    goto fail1;
  }
  f = NULL;

  if (rename(tmp_name, file_name) < 0) {
    fprintf(stderr, COMPNAME ": ERROR: unable to rename var file!\n");
    goto fail1;
  }

  err = 0;

fail1:
  if (f != NULL) {
    fclose(f);
  }
fail0:
  return err;
}

static int load_vars(const char *file_name) {
  hal_data_t *hd = (hal_data_t *) hal_shmem_base;
  int err = 1;
  FILE *f;
  char line[1024];
  char *name, *value, *s;
  hal_sig_t *sig;
  void *data_addr;
  double flt;
  int32_t s32;
  uint32_t u32;

  if ((f = fopen(file_name, "r")) == NULL) {
    fprintf(stderr, COMPNAME ": ERROR: unable to open var file for reading!\n");
    goto fail0;
  }

  rtapi_mutex_get(&(hd->mutex));
  while(fgets(line, sizeof(line), f)) {
    // skip initial blanks
    for (name = line; *name && strchr("\t ", *name); name++);

    // skip comment lines
    if (*name == '#') {
      continue;
    }

    // search separator
    value = strchr(name, ' ');
    if (value == NULL) {
      continue;
    }
    *(value++) = 0;

    // skip empty names
    if (*name == 0) {
      continue;
    }

    // terminate value
    for (s = value; *s && !strchr("\t \r\n", *s); s++);
    *s = 0;

    // skip empty values
    if (*value == 0) {
      continue;
    }

    // find signal
    sig = halpr_find_sig_by_name(name);
    if (sig == NULL) {
      continue;
    }

    // skip signals with writers
    if (sig->writers > 0) {
      fprintf(stderr, COMPNAME ": ERROR: unable to restore signal %s (has writers)\n", sig->name);
      continue;
    }

    // set retain signals only
    if ((sig->flags & HAL_SIGFLAG_RETAIN) == 0) {
      continue;
    }

    // set value
    data_addr = SHMPTR(sig->data_ptr);
    switch (sig->type) {
      case HAL_BIT:
        if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0)) {
          *((hal_bit_t *) data_addr) = 1;
        } else if ((strcmp("0", value) == 0) || (strcasecmp("FALSE", value)) == 0) {
          *((hal_bit_t *) data_addr) = 0;
        } else {
          fprintf(stderr, COMPNAME ": ERROR: unable to restore signal %s (invalid bit value '%s')\n", sig->name, value);
        }
        break;
      case HAL_U32:
        u32 = strtoul(value, &s, 0);
        if (*s == 0) {
          *((hal_u32_t *) data_addr) = u32;
        } else {
          fprintf(stderr, COMPNAME ": ERROR: unable to restore signal %s (invalid u32 value '%s')\n", sig->name, value);
        }
        break;
      case HAL_S32:
        s32 = strtol(value, &s, 0);
        if (*s == 0) {
          *((hal_s32_t *) data_addr) = s32;
        } else {
          fprintf(stderr, COMPNAME ": ERROR: unable to restore signal %s (invalid s32 value '%s')\n", sig->name, value);
        }
        break;
      case HAL_FLOAT:
        flt = strtod(value, &s);
        if (*s == 0) {
          *((hal_float_t *) data_addr) = flt;
        } else {
          fprintf(stderr, COMPNAME ": ERROR: unable to restore signal %s (invalid float value '%s')\n", sig->name, value);
        }
        break;
      default:
        break;
    }
  }
  rtapi_mutex_give(&(hd->mutex));

  err = 0;

  fclose(f);
fail0:
  return err;
}

int main(int argc, char **argv) {
  int err = 1;
  int comp_id;
  RETAIN_HAL_T *hal;
  uint64_t u;
  char *varfile;
  long poll_period;
  fd_set rfds;
  struct timeval tv;
  int do_create, changed, do_exit;

  // get config file name
  if (argc < 2) {
    fprintf(stderr, COMPNAME ": ERROR: usage: " COMPNAME " <varfile> [poll time]\n");
    goto fail0;
  }
  varfile = argv[1];

  // get poll time
  poll_period = 0;
  if (argc >= 3) {
    poll_period = atol(argv[2]);
  }
  if (poll_period == 0) {
    poll_period = POLL_PERIOD_MS;
  }

  // init hal
  comp_id = hal_init(COMPNAME);
  if (comp_id < 0) {
    fprintf(stderr, COMPNAME ": ERROR: hal init failed!\n");
    goto fail0;
  }

  // allocate hal memory
  hal = hal_malloc(sizeof(RETAIN_HAL_T));
  if (hal == NULL) {
    fprintf(stderr, COMPNAME ": ERROR: unable to allocate HAL shared memory\n");
    goto fail1;
  }

  // register pins
  if (hal_pin_u32_new("retain-usr.update-count", HAL_OUT, &(hal->update_count), comp_id) != 0) {
    fprintf(stderr, COMPNAME ": ERROR: unable to register pin retain-usr.update-count\n");
    goto fail1;
  }
  *(hal->update_count) = 0;

  // get shared pins
  action_param = (hal_u32_t *) get_shared_param_ptr(RETAIN_ACTION_PARAM, HAL_U32);
  if (action_param == NULL) {
    fprintf(stderr, COMPNAME ": ERROR: unable to find shared parameter " RETAIN_ACTION_PARAM ". Retain RT comp loaded?\n");
    goto fail1;
  }

  // create exit event
  exit_event = eventfd(0, 0);
  if (exit_event < 0) {
    fprintf(stderr, COMPNAME ": ERROR: unable to create exit event\n");
    goto fail1;
  }

  // install signal handler
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = &sigtermHandler;
  if (sigaction(SIGTERM, &act, NULL) < 0)
  {
    fprintf(stderr, COMPNAME ": ERROR: Unable to register SIGTERM handler.");
    goto fail2;
  }

  // restore values, if var file is present
  do_create = 1;
  if (access(varfile, F_OK) == 0) {
    if (load_vars(varfile)) {
      goto fail2;
    }
    do_create = 0;
  }

  // everything is fine
  hal_ready(comp_id);

  // do first sync (will also wait for starting RT task)
  if (sync_action(RETAIN_ACTION_READ) < 0) {
    fprintf(stderr, COMPNAME ": ERROR: Timeout while waiting for sync_read.");
    goto fail2;
  }

  FD_ZERO(&rfds);
  FD_SET(exit_event, &rfds);
  do_exit = 0;
  while (1) {
    // check for changes
    changed = sync_action(RETAIN_ACTION_READ);
    if (changed >= 0) {
      if (do_create || changed) {
        save_vars(varfile);
        (*(hal->update_count))++;
      }
      do_create = 0;
    }

    // check for exit flag
    if (do_exit) {
      err = 0;
      break;
    }

    // wait poll time
    tv.tv_sec = poll_period / 1000LL;
    tv.tv_usec = (poll_period % 1000LL) * 1000LL;
    if (select(1, &rfds, NULL, NULL, &tv) < 0) {
      goto fail2;
    }

    // check for SIGTERM
    // set only a flag to do a final sync
    if (FD_ISSET(exit_event, &rfds)) {
      if (read(exit_event, &u, sizeof(uint64_t)) < 0) {
        goto fail2;
      }
      do_exit = 1;
    }
  }

fail2:
  close(exit_event);
fail1:
  hal_exit(comp_id);
fail0:
  return err;
}

