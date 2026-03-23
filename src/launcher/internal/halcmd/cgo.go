package halcmd

/*
#cgo CFLAGS: -I${SRCDIR}/../../../hal -I${SRCDIR}/../../.. -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include
#cgo LDFLAGS: -L${SRCDIR}/../../../../lib -llinuxcnchal -ldl

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <spawn.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include "config.h"
#include "rtapi.h"
#include "hal.h"
#include "hal_priv.h"

extern char **environ;

// Helper to convert hal_type_t to int for Go
static inline int get_hal_type(hal_type_t t) { return (int)t; }

// Helper to convert hal_pin_dir_t to int for Go
static inline int get_hal_dir(hal_pin_dir_t d) { return (int)d; }

// hal_shim_list_comps walks the HAL component list and writes each component
// name as a null-terminated string into buf (strings are concatenated).
// Returns the number of components found, or a negative errno value on error.
// Returns -ENOSPC if the buffer is too small to hold all names.
static int hal_shim_list_comps(char *buf, int buf_size) {
    int next;
    hal_comp_t *comp;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) {
        return -EINVAL;
    }

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
        comp = (hal_comp_t *)SHMPTR(next);
        name_len = (int)strlen(comp->name) + 1; // include null terminator
        if (pos + name_len > buf_size) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -ENOSPC;
        }
        memcpy(buf + pos, comp->name, name_len);
        pos += name_len;
        count++;
        next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// HAL_SHIM_MAX_COMPS is the maximum number of components tracked by the shim
// helpers. 256 exceeds any realistic LinuxCNC machine configuration.
#define HAL_SHIM_MAX_COMPS 256

// ===== In-process RT module management =====
// Replaces the former rtapi_app IPC: modules are rtapi_dlopen'd directly in this
// process.  The module table, load/unload/newinst logic are adapted from
// uspace_rtapi_app.c.

#define RT_MAX_MODULES 64
struct rt_module_entry {
    char name[256];
    void *handle;
    int in_use;
};
static struct rt_module_entry rt_modules[RT_MAX_MODULES];
static pthread_mutex_t rt_modules_lock = PTHREAD_MUTEX_INITIALIZER;

static void *rt_find_module(const char *name) {
    pthread_mutex_lock(&rt_modules_lock);
    for (int i = 0; i < RT_MAX_MODULES; i++) {
        if (rt_modules[i].in_use && strcmp(rt_modules[i].name, name) == 0) {
            void *handle = rt_modules[i].handle;
            pthread_mutex_unlock(&rt_modules_lock);
            return handle;
        }
    }
    pthread_mutex_unlock(&rt_modules_lock);
    return NULL;
}

static int rt_add_module(const char *name, void *handle) {
    pthread_mutex_lock(&rt_modules_lock);
    for (int i = 0; i < RT_MAX_MODULES; i++) {
        if (!rt_modules[i].in_use) {
            strncpy(rt_modules[i].name, name, sizeof(rt_modules[i].name) - 1);
            rt_modules[i].name[sizeof(rt_modules[i].name) - 1] = '\0';
            rt_modules[i].handle = handle;
            rt_modules[i].in_use = 1;
            pthread_mutex_unlock(&rt_modules_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&rt_modules_lock);
    return -1;
}

static void rt_remove_module(const char *name) {
    pthread_mutex_lock(&rt_modules_lock);
    for (int i = 0; i < RT_MAX_MODULES; i++) {
        if (rt_modules[i].in_use && strcmp(rt_modules[i].name, name) == 0) {
            rt_modules[i].in_use = 0;
            rt_modules[i].name[0] = '\0';
            rt_modules[i].handle = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&rt_modules_lock);
}

static void rt_remove_quotes(char *s) {
    char *src = s;
    char *dst = s;
    while (*src) {
        if (*src != '"') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

static int rt_do_one_item(char item_type_char, const char *param_name,
                          const char *param_value, void *vitem, int idx) {
    char *endp;
    switch (item_type_char) {
        case 'l': {
            long *litem = *(long **)vitem;
            litem[idx] = strtol(param_value, &endp, 0);
            if (*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "`%s' invalid for parameter `%s'",
                    param_value, param_name);
                return -1;
            }
            return 0;
        }
        case 'i': {
            int *iitem = *(int **)vitem;
            iitem[idx] = strtol(param_value, &endp, 0);
            if (*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "`%s' invalid for parameter `%s'",
                    param_value, param_name);
                return -1;
            }
            return 0;
        }
        case 's': {
            char **sitem = *(char ***)vitem;
            sitem[idx] = strdup(param_value);
            return 0;
        }
        default:
            rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: Invalid type character `%c'\n",
                param_name, item_type_char);
            return -1;
    }
}

static int rt_do_comp_args(void *module, const char *const args[], int nargs) {
    int i;
    for (i = 0; i < nargs; i++) {
        char s[1024];
        strncpy(s, args[i], sizeof(s) - 1);
        s[sizeof(s) - 1] = '\0';
        rt_remove_quotes(s);
        char *eq = strchr(s, '=');
        if (!eq) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid parameter `%s'\n", s);
            return -1;
        }
        *eq = '\0';
        char *param_name = s;
        char *param_value = eq + 1;

        char sym_name[512];
        snprintf(sym_name, sizeof(sym_name), "rtapi_info_address_%s", param_name);
        void *item = dlsym(module, sym_name);
        if (!item) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Unknown parameter `%s'\n", param_name);
            return -1;
        }

        snprintf(sym_name, sizeof(sym_name), "rtapi_info_type_%s", param_name);
        char **item_type = (char **)dlsym(module, sym_name);
        if (!item_type || !*item_type) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Unknown parameter `%s' (type information missing)\n",
                param_name);
            return -1;
        }

        snprintf(sym_name, sizeof(sym_name), "rtapi_info_size_%s", param_name);
        int *max_size_ptr = (int *)dlsym(module, sym_name);

        char item_type_char = **item_type;
        if (max_size_ptr) {
            int max_size = *max_size_ptr;
            char *tok = param_value;
            int idx = 0;
            while (tok && *tok) {
                if (idx == max_size) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: can only take %d arguments\n",
                        param_name, max_size);
                    return -1;
                }
                char *comma = strchr(tok, ',');
                char substr[256];
                if (comma) {
                    size_t len = comma - tok;
                    if (len >= sizeof(substr)) len = sizeof(substr) - 1;
                    strncpy(substr, tok, len);
                    substr[len] = '\0';
                    tok = comma + 1;
                } else {
                    strncpy(substr, tok, sizeof(substr) - 1);
                    substr[sizeof(substr) - 1] = '\0';
                    tok = NULL;
                }
                int result = rt_do_one_item(item_type_char, param_name, substr, item, idx);
                if (result != 0) return result;
                idx++;
            }
        } else {
            int result = rt_do_one_item(item_type_char, param_name, param_value, item, 0);
            if (result != 0) return result;
        }
    }
    return 0;
}

// ===== Message queue for RT thread messages =====
// RT threads cannot safely write to stdout/stderr.  Messages are queued and
// consumed by a background pthread that drains them to the appropriate stream.

#define RT_MSG_QUEUE_SIZE 128
struct rt_message_t {
    msg_level_t level;
    char msg[1024];
};
static struct rt_message_t rt_msg_queue[RT_MSG_QUEUE_SIZE];
static _Atomic int rt_msg_head = 0;
static _Atomic int rt_msg_tail = 0;
static pthread_t rt_queue_thread;
static _Atomic int rt_queue_running = 0;
static pthread_t rt_main_thread;

static void rt_msg_queue_push(msg_level_t level, const char *msg) {
    int head = atomic_load_explicit(&rt_msg_head, memory_order_relaxed);
    int next = (head + 1) % RT_MSG_QUEUE_SIZE;
    if (next == atomic_load_explicit(&rt_msg_tail, memory_order_acquire)) {
        return;  // Queue full, message dropped
    }
    rt_msg_queue[head].level = level;
    snprintf(rt_msg_queue[head].msg, sizeof(rt_msg_queue[head].msg), "%s", msg);
    atomic_store_explicit(&rt_msg_head, next, memory_order_release);
}

static int rt_msg_queue_consume_all(void) {
    int processed = 0;
    int tail = atomic_load_explicit(&rt_msg_tail, memory_order_relaxed);
    while (tail != atomic_load_explicit(&rt_msg_head, memory_order_acquire)) {
        msg_level_t level = rt_msg_queue[tail].level;
        char msg_copy[sizeof(rt_msg_queue[tail].msg)];
        strncpy(msg_copy, rt_msg_queue[tail].msg, sizeof(msg_copy) - 1);
        msg_copy[sizeof(msg_copy) - 1] = '\0';
        int next_tail = (tail + 1) % RT_MSG_QUEUE_SIZE;
        atomic_store_explicit(&rt_msg_tail, next_tail, memory_order_release);
        tail = next_tail;
        fputs(msg_copy, level == RTAPI_MSG_ALL ? stdout : stderr);
        processed++;
    }
    return processed;
}

static void *rt_queue_function(void *arg) {
    (void)arg;
    while (atomic_load(&rt_queue_running)) {
        rt_msg_queue_consume_all();
        struct timespec ts = {0, 10000000}; // 10ms
        nanosleep(&ts, NULL);
    }
    rt_msg_queue_consume_all(); // drain remaining
    return NULL;
}

static void rt_msg_handler(msg_level_t level, const char *fmt, va_list ap) {
    if (rt_queue_running && !pthread_equal(pthread_self(), rt_main_thread)) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        rt_msg_queue_push(level, buf);
    } else {
        vfprintf(level == RTAPI_MSG_ALL ? stdout : stderr, fmt, ap);
    }
}

// hal_shim_rtapi_app_init performs the in-process equivalent of rtapi_app's
// master() startup: sets up the message handler, starts the message queue
// thread, and calls halpr_rtapi_app_main().
static int hal_shim_rtapi_app_init(void) {
    int result;
    rt_main_thread = pthread_self();
    rtapi_set_msg_handler(rt_msg_handler);

    atomic_store(&rt_queue_running, 1);
    result = pthread_create(&rt_queue_thread, NULL, &rt_queue_function, NULL);
    if (result != 0) {
        atomic_store(&rt_queue_running, 0);
        errno = result;
        return -result;
    }

    result = halpr_rtapi_app_main();
    if (result != 0) {
        atomic_store(&rt_queue_running, 0);
        pthread_join(rt_queue_thread, NULL);
        return result;
    }
    return 0;
}

// hal_shim_rtapi_app_cleanup performs the in-process equivalent of rtapi_app's
// master() cleanup: calls halpr_rtapi_app_exit() and stops the message queue.
static void hal_shim_rtapi_app_cleanup(void) {
    halpr_rtapi_app_exit();
    atomic_store(&rt_queue_running, 0);
    pthread_join(rt_queue_thread, NULL);
    rt_msg_queue_consume_all();
}

// hal_shim_unload_all unloads all HAL components:
//   - Userspace components: send SIGTERM to their owning process
//   - Realtime components: unload via direct rtapi_dlclose (in-process)
// The component identified by except_id is skipped (pass 0 to not skip any).
// Returns 0 on success, or a negative errno value on error.
static int hal_shim_unload_all(int except_id) {
    int next;
    hal_comp_t *comp;
    pid_t ourpid = getpid();

    if (hal_data == NULL) {
        return -EINVAL;
    }

    // Phase 1: send SIGTERM to userspace components
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
        comp = (hal_comp_t *)SHMPTR(next);
        if (comp->type == COMPONENT_TYPE_USER && comp->pid != ourpid) {
            if (comp->comp_id != except_id) {
                kill(abs(comp->pid), SIGTERM);
            }
        }
        next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));

    // Phase 2: collect realtime component names then unload in-process
    {
        char comps[HAL_SHIM_MAX_COMPS][HAL_NAME_LEN+1];
        int n = 0;
        int i;

        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->comp_list_ptr;
        while (next != 0) {
            comp = (hal_comp_t *)SHMPTR(next);
            if (comp->type == COMPONENT_TYPE_REALTIME) {
                if (comp->comp_id != except_id && n < HAL_SHIM_MAX_COMPS) {
                    if (strstr(comp->name, HAL_PSEUDO_COMP_PREFIX) != comp->name) {
                        snprintf(comps[n], sizeof(comps[n]), "%s", comp->name);
                        n++;
                    }
                }
            }
            next = comp->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));

        // unload each realtime component in-process
        for (i = 0; i < n; i++) {
            void *w = rt_find_module(comps[i]);
            if (w != NULL) {
                int (*stop)(void) = (int (*)(void))dlsym(w, "rtapi_app_exit");
                if (stop) stop();
                rt_remove_module(comps[i]);
                rtapi_dlclose(w);
            }
        }
    }

    return 0;
}

// ===== 1a. Simple wrapper shims =====

// hal_shim_newsig wraps hal_signal_new(name, type)
static int hal_shim_newsig(const char *name, int type) {
    return hal_signal_new(name, (hal_type_t)type);
}

// hal_shim_delsig wraps hal_signal_delete(name)
static int hal_shim_delsig(const char *name) {
    return hal_signal_delete(name);
}

// hal_shim_linkps wraps hal_link(pin, sig)
static int hal_shim_linkps(const char *pin, const char *sig) {
    return hal_link(pin, sig);
}

// hal_shim_unlinkp wraps hal_unlink(pin)
static int hal_shim_unlinkp(const char *pin) {
    return hal_unlink(pin);
}

// hal_shim_addf wraps hal_add_funct_to_thread(funct, thread, position)
static int hal_shim_addf(const char *funct, const char *thread, int position) {
    return hal_add_funct_to_thread(funct, thread, position);
}

// hal_shim_delf wraps hal_del_funct_from_thread(funct, thread)
static int hal_shim_delf(const char *funct, const char *thread) {
    return hal_del_funct_from_thread(funct, thread);
}

// hal_shim_set_lock wraps hal_set_lock(lock_type)
static int hal_shim_set_lock(unsigned char lock_type) {
    return hal_set_lock(lock_type);
}

// hal_shim_get_lock wraps hal_get_lock()
static unsigned char hal_shim_get_lock(void) {
    return hal_get_lock();
}

// hal_shim_pin_alias wraps hal_pin_alias(pin_name, alias).
// alias may be NULL to remove an alias.
static int hal_shim_pin_alias(const char *pin_name, const char *alias) {
    return hal_pin_alias(pin_name, alias);
}

// hal_shim_param_alias wraps hal_param_alias(param_name, alias).
static int hal_shim_param_alias(const char *param_name, const char *alias) {
    return hal_param_alias(param_name, alias);
}

// ===== 1b. Value access helper functions =====

// hal_shim_write_value writes a value string to a HAL data location.
// This mirrors the set_common() logic from halcmd_commands.cc.
// Assumes the HAL mutex is already held by the caller.
static int hal_shim_write_value(hal_type_t type, void *d_ptr, const char *value) {
    double fval;
    long lval;
    unsigned long ulval;
    char *cp;

    switch (type) {
    case HAL_BIT:
        if (strcmp("1", value) == 0 || strcasecmp("TRUE", value) == 0) {
            *(hal_bit_t *)d_ptr = 1;
        } else if (strcmp("0", value) == 0 || strcasecmp("FALSE", value) == 0) {
            *(hal_bit_t *)d_ptr = 0;
        } else {
            return -EINVAL;
        }
        break;
    case HAL_FLOAT:
        cp = (char *)value;
        fval = strtod(value, &cp);
        if (*cp != '\0' && !isspace((unsigned char)*cp)) return -EINVAL;
        *(hal_float_t *)d_ptr = (hal_float_t)fval;
        break;
    case HAL_S32:
        cp = (char *)value;
        lval = strtol(value, &cp, 0);
        if (*cp != '\0' && !isspace((unsigned char)*cp)) return -EINVAL;
        *(hal_s32_t *)d_ptr = (hal_s32_t)lval;
        break;
    case HAL_U32:
        cp = (char *)value;
        ulval = strtoul(value, &cp, 0);
        if (*cp != '\0' && !isspace((unsigned char)*cp)) return -EINVAL;
        *(hal_u32_t *)d_ptr = (hal_u32_t)ulval;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

// hal_shim_format_value formats a HAL data value as a string into buf.
// Returns 0 on success, -EINVAL for unknown types.
// Mirrors the data_value2() logic from halcmd_commands.cc.
static int hal_shim_format_value(hal_type_t type, void *d_ptr, char *buf, int buf_size) {
    switch (type) {
    case HAL_BIT:
        snprintf(buf, buf_size, "%s", (*(hal_bit_t *)d_ptr) ? "TRUE" : "FALSE");
        break;
    case HAL_FLOAT:
        snprintf(buf, buf_size, "%.7g", (double)(*(hal_float_t *)d_ptr));
        break;
    case HAL_S32:
        snprintf(buf, buf_size, "%ld", (long)(*(hal_s32_t *)d_ptr));
        break;
    case HAL_U32:
        snprintf(buf, buf_size, "%lu", (unsigned long)(*(hal_u32_t *)d_ptr));
        break;
    default:
        snprintf(buf, buf_size, "unknown");
        return -EINVAL;
    }
    return 0;
}

// ===== 1b. Shmem access shims =====

// hal_shim_setp sets the value of a pin or parameter by name.
// Tries pin first, then parameter. Mirrors halcmd's do_setp_cmd logic.
// Returns 0 on success, negative errno on error.
static int hal_shim_setp(const char *name, const char *value) {
    hal_pin_t *pin;
    hal_param_t *param;
    hal_type_t type;
    void *d_ptr;
    int retval;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    param = halpr_find_param_by_name(name);
    if (param) {
        if (param->dir == HAL_RO) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -EPERM;
        }
        type = param->type;
        d_ptr = SHMPTR(param->data_ptr);
        retval = hal_shim_write_value(type, d_ptr, value);
        rtapi_mutex_give(&(hal_data->mutex));
        return retval;
    }

    pin = halpr_find_pin_by_name(name);
    if (pin) {
        if (pin->dir == HAL_OUT) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -EPERM;
        }
        if (pin->signal != 0) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -EBUSY;
        }
        type = pin->type;
        d_ptr = (void *)&pin->dummysig;
        retval = hal_shim_write_value(type, d_ptr, value);
        rtapi_mutex_give(&(hal_data->mutex));
        return retval;
    }

    rtapi_mutex_give(&(hal_data->mutex));
    return -ENOENT;
}

// hal_shim_getp gets the value of a pin or parameter as a string.
// Writes the value into buf (max buf_size bytes).
// Returns 0 on success, negative errno on error.
static int hal_shim_getp(const char *name, char *buf, int buf_size) {
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_param_t *param;
    hal_type_t type;
    void *d_ptr;
    int retval;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    param = halpr_find_param_by_name(name);
    if (param) {
        type = param->type;
        d_ptr = SHMPTR(param->data_ptr);
        retval = hal_shim_format_value(type, d_ptr, buf, buf_size);
        rtapi_mutex_give(&(hal_data->mutex));
        return retval;
    }

    pin = halpr_find_pin_by_name(name);
    if (pin) {
        type = pin->type;
        if (pin->signal != 0) {
            sig = (hal_sig_t *)SHMPTR(pin->signal);
            d_ptr = SHMPTR(sig->data_ptr);
        } else {
            d_ptr = (void *)&pin->dummysig;
        }
        retval = hal_shim_format_value(type, d_ptr, buf, buf_size);
        rtapi_mutex_give(&(hal_data->mutex));
        return retval;
    }

    rtapi_mutex_give(&(hal_data->mutex));
    return -ENOENT;
}

// hal_shim_sets sets the value of a signal by name.
// Returns 0 on success, negative errno on error.
static int hal_shim_sets(const char *name, const char *value) {
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;
    int retval;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    sig = halpr_find_sig_by_name(name);
    if (sig == NULL) {
        rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }

    if (sig->type != HAL_PORT && sig->writers > 0) {
        rtapi_mutex_give(&(hal_data->mutex));
        return -EBUSY;
    }

    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    retval = hal_shim_write_value(type, d_ptr, value);
    rtapi_mutex_give(&(hal_data->mutex));
    return retval;
}

// hal_shim_gets gets the value of a signal as a string.
// Writes the value into buf (max buf_size bytes).
// Returns 0 on success, negative errno on error.
static int hal_shim_gets(const char *name, char *buf, int buf_size) {
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;
    int retval;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    sig = halpr_find_sig_by_name(name);
    if (sig == NULL) {
        rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }

    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    retval = hal_shim_format_value(type, d_ptr, buf, buf_size);
    rtapi_mutex_give(&(hal_data->mutex));
    return retval;
}

// hal_shim_ptype returns the HAL type (as int) of a pin or parameter by name.
// Returns the hal_type_t value on success, negative errno on error.
static int hal_shim_ptype(const char *name) {
    hal_pin_t *pin;
    hal_param_t *param;
    int type;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    param = halpr_find_param_by_name(name);
    if (param) {
        type = (int)param->type;
        rtapi_mutex_give(&(hal_data->mutex));
        return type;
    }

    pin = halpr_find_pin_by_name(name);
    if (pin) {
        type = (int)pin->type;
        rtapi_mutex_give(&(hal_data->mutex));
        return type;
    }

    rtapi_mutex_give(&(hal_data->mutex));
    return -ENOENT;
}

// hal_shim_stype returns the HAL type (as int) of a signal by name.
// Returns the hal_type_t value on success, negative errno on error.
static int hal_shim_stype(const char *name) {
    hal_sig_t *sig;
    int type;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));

    sig = halpr_find_sig_by_name(name);
    if (sig == NULL) {
        rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }

    type = (int)sig->type;
    rtapi_mutex_give(&(hal_data->mutex));
    return type;
}

// ===== 1c. Net command shim =====

// HAL_SHIM_MAX_PINS is the maximum number of pins accepted by hal_shim_net.
#define HAL_SHIM_MAX_PINS 64

// hal_shim_net implements the "net" command.
// sig_name is the signal name. pin_names is a null-separated list of pin names.
// num_pins is the number of pins.
// Arrow tokens must be stripped by the Go caller before calling this function.
// Returns 0 on success, negative errno on error.
static int hal_shim_net(const char *sig_name, const char *pin_names, int num_pins) {
    hal_sig_t *sig;
    hal_type_t sig_type;
    int i, retval = 0;
    const char *pins[HAL_SHIM_MAX_PINS];
    const char *p;

    if (hal_data == NULL) return -EINVAL;
    if (num_pins <= 0 || num_pins > HAL_SHIM_MAX_PINS) return -EINVAL;

    // Decode null-separated pin names
    p = pin_names;
    for (i = 0; i < num_pins; i++) {
        pins[i] = p;
        p += strlen(p) + 1;
    }

    rtapi_mutex_get(&(hal_data->mutex));
    sig = halpr_find_sig_by_name(sig_name);

    if (!sig) {
        // Create signal with the type of the first pin
        hal_pin_t *pin = halpr_find_pin_by_name(pins[0]);
        if (!pin) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -ENOENT;
        }
        sig_type = pin->type;
        rtapi_mutex_give(&(hal_data->mutex));
        retval = hal_signal_new(sig_name, sig_type);
    } else {
        rtapi_mutex_give(&(hal_data->mutex));
    }

    if (retval != 0) return retval;

    // Link each pin to the signal
    for (i = 0; i < num_pins && retval == 0; i++) {
        retval = hal_link(pins[i], sig_name);
    }

    return retval;
}

// ===== 1d. Process management shims =====

// HAL_SHIM_USECS_PER_SEC converts seconds to microseconds for usleep.
#define HAL_SHIM_USECS_PER_SEC 1000000

// HAL_SHIM_POLL_USECS is the polling interval used when waiting for a
// component to become ready or to disappear.
#define HAL_SHIM_POLL_USECS 10000

// hal_shim_loadusr starts a user-space process.
// flags: 1=wait_ready, 2=wait_exit, 4=no_stdin
// wait_name: component name to wait for (if wait_ready is set), or NULL to derive from prog.
// timeout_s: timeout in seconds; 0 means use default (10 seconds).
// Returns 0 on success, negative errno on error.
static int hal_shim_loadusr(int flags, const char *wait_name, int timeout_s,
                            const char *prog, const char *const args[], int nargs) {
    int wait_ready = flags & 1;
    int wait_exit  = flags & 2;
    int no_stdin   = flags & 4;
    const char *argv[256];
    int m = 0, i;
    pid_t pid;

    if (m + 1 + nargs >= 256) return -E2BIG;

    argv[m++] = prog;
    for (i = 0; i < nargs; i++) {
        argv[m++] = args[i];
    }
    argv[m] = NULL;

    {
        posix_spawn_file_actions_t file_actions;
        posix_spawnattr_t attr;
        int spawn_ret;

        posix_spawn_file_actions_init(&file_actions);
        if (no_stdin) {
            posix_spawn_file_actions_addopen(&file_actions, 0, "/dev/null", O_RDONLY, 0);
        }
        posix_spawnattr_init(&attr);

        spawn_ret = posix_spawnp(&pid, prog, &file_actions, &attr, (char *const *)argv, environ);

        posix_spawn_file_actions_destroy(&file_actions);
        posix_spawnattr_destroy(&attr);

        if (spawn_ret != 0) {
            return -spawn_ret;
        }
    }

    // Parent process
    if (wait_exit) {
        int status;
        if (waitpid(pid, &status, 0) < 0) return -errno;
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) return -ECHILD;
        return 0;
    }

    if (wait_ready) {
        const char *comp_name = (wait_name && *wait_name) ? wait_name : prog;
        int max_us = (timeout_s > 0 ? timeout_s : 10) * HAL_SHIM_USECS_PER_SEC;
        int elapsed = 0;
        int step_us = HAL_SHIM_POLL_USECS;
        int ready = 0, exited = 0;

        while (!ready && !exited && elapsed < max_us) {
            usleep((useconds_t)step_us);
            elapsed += step_us;

            // Check if child exited prematurely
            int status;
            int ret = waitpid(pid, &status, WNOHANG);
            if (ret != 0) {
                exited = 1;
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    return -ESRCH;
                }
            }

            // Check if component became ready
            if (hal_data != NULL) {
                hal_comp_t *comp;
                rtapi_mutex_get(&(hal_data->mutex));
                comp = halpr_find_comp_by_name(comp_name);
                if (comp && comp->ready) ready = 1;
                rtapi_mutex_give(&(hal_data->mutex));
            }
        }

        if (!ready) return -ETIMEDOUT;
    }

    return 0;
}

// hal_shim_loadrt loads a realtime module in-process via rtapi_dlopen.
// Replaces the former fork+exec of rtapi_app.  The module's .so is opened
// with RTLD_GLOBAL|RTLD_NOW, module parameters are parsed via dlsym'd
// rtapi_info_* symbols, and rtapi_app_main() is called.  Waits for the
// component to appear in HAL shared memory (ready flag).
// Returns 0 on success, non-zero on error.
static int hal_shim_loadrt(const char *mod, const char *const args[], int nargs) {
    if (rt_find_module(mod) != NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: already loaded\n", mod);
        return -EEXIST;
    }

    char what[PATH_MAX];
    snprintf(what, sizeof(what), "%s/%s.so", EMC2_RTLIB_DIR, mod);
    void *module = rtapi_dlopen(what, RTLD_GLOBAL | RTLD_NOW);
    if (!module) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_dlopen: %s\n", mod, dlerror());
        return -ENOENT;
    }

    int (*start)(void) = (int (*)(void))dlsym(module, "rtapi_app_main");
    if (!start) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", mod, dlerror());
        rtapi_dlclose(module);
        return -ENOENT;
    }

    if (nargs > 0) {
        int result = rt_do_comp_args(module, args, nargs);
        if (result < 0) {
            rtapi_dlclose(module);
            return result;
        }
    }

    int result = start();
    if (result < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %s (%d)\n",
            mod, strerror(-result), result);
        rtapi_dlclose(module);
        return result;
    }

    if (rt_add_module(mod, module) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: too many modules\n", mod);
        rtapi_dlclose(module);
        return -ENOMEM;
    }

    // Wait for the component to become ready in HAL shmem, matching the
    // old behaviour of hal_shim_loadusr with wait_ready.
    if (hal_data != NULL) {
        int max_us = 10 * HAL_SHIM_USECS_PER_SEC;
        int elapsed = 0;
        int step_us = HAL_SHIM_POLL_USECS;
        int ready = 0;
        while (!ready && elapsed < max_us) {
            usleep((useconds_t)step_us);
            elapsed += step_us;
            hal_comp_t *comp;
            rtapi_mutex_get(&(hal_data->mutex));
            comp = halpr_find_comp_by_name(mod);
            if (comp && comp->ready) ready = 1;
            rtapi_mutex_give(&(hal_data->mutex));
        }
        if (!ready) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: component did not become ready within timeout\n", mod);
            return -ETIMEDOUT;
        }
    }

    return 0;
}

// hal_shim_unloadrt unloads a realtime module in-process.
// Calls rtapi_app_exit() on the module and rtapi_dlclose()s it.
static int hal_shim_unloadrt(const char *mod) {
    void *w = rt_find_module(mod);
    if (w == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: not loaded\n", mod);
        return -ENOENT;
    }
    int (*stop)(void) = (int (*)(void))dlsym(w, "rtapi_app_exit");
    if (stop) stop();
    rt_remove_module(mod);
    rtapi_dlclose(w);
    return 0;
}

// hal_shim_newinst creates a new instance of a HAL component.
static int hal_shim_newinst(const char *type, const char *name, const char *arg) {
    hal_comp_t *comp = halpr_find_comp_by_name((char *)type);
    if (!comp) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "newinst: component %s not found\n", type);
        return -ENOENT;
    }
    if (!arg) arg = "";
    return comp->make((char *)name, (char *)arg);
}

// hal_shim_unloadusr sends SIGTERM to the process owning a user-space component.
// Returns 0 on success, negative errno on error.
static int hal_shim_unloadusr(const char *comp_name) {
    hal_comp_t *comp;
    int pid;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);
    if (comp == NULL) {
        rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }
    pid = comp->pid;
    rtapi_mutex_give(&(hal_data->mutex));

    if (pid <= 0) return -EINVAL;

    if (kill(pid, SIGTERM) < 0) return -errno;
    return 0;
}

// hal_shim_waitusr waits until a user-space component disappears from HAL.
// timeout_s: timeout in seconds; 0 means use default (30 seconds).
// Returns 0 on success, -ETIMEDOUT if timeout expires.
static int hal_shim_waitusr(const char *comp_name, int timeout_s) {
    int max_us = (timeout_s > 0 ? timeout_s : 30) * HAL_SHIM_USECS_PER_SEC;
    int elapsed = 0;
    int step_us = HAL_SHIM_POLL_USECS;

    while (elapsed < max_us) {
        hal_comp_t *comp;
        usleep((useconds_t)step_us);
        elapsed += step_us;

        if (hal_data == NULL) return 0; // HAL gone means component gone

        rtapi_mutex_get(&(hal_data->mutex));
        comp = halpr_find_comp_by_name(comp_name);
        rtapi_mutex_give(&(hal_data->mutex));

        if (comp == NULL) return 0; // component has disappeared
    }
    return -ETIMEDOUT;
}

// ===== 1e. Query/list shims =====

// hal_shim_list_pins lists pin names matching pattern.
// Writes null-separated names into buf. Returns count or negative errno.
static int hal_shim_list_pins(const char *pattern, char *buf, int buf_size) {
    hal_pin_t *pin;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
        pin = (hal_pin_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, pin->name, 0) == 0) {
            name_len = (int)strlen(pin->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, pin->name, name_len);
            pos += name_len;
            count++;
        }
        next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_list_sigs lists signal names matching pattern.
static int hal_shim_list_sigs(const char *pattern, char *buf, int buf_size) {
    hal_sig_t *sig;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
        sig = (hal_sig_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, sig->name, 0) == 0) {
            name_len = (int)strlen(sig->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, sig->name, name_len);
            pos += name_len;
            count++;
        }
        next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_list_retain_sigs lists signal names that have the HAL_SIGFLAG_RETAIN
// flag set and whose name matches pattern (NULL or empty string matches all).
// This mirrors halcmd's do_list_cmd "retain" special case.
static int hal_shim_list_retain_sigs(const char *pattern, char *buf, int buf_size) {
    hal_sig_t *sig;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
        sig = (hal_sig_t *)SHMPTR(next);
        if ((sig->flags & HAL_SIGFLAG_RETAIN) &&
            (pattern == NULL || *pattern == '\0' ||
             fnmatch(pattern, sig->name, 0) == 0)) {
            name_len = (int)strlen(sig->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, sig->name, name_len);
            pos += name_len;
            count++;
        }
        next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_list_params lists parameter names matching pattern.
static int hal_shim_list_params(const char *pattern, char *buf, int buf_size) {
    hal_param_t *param;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
        param = (hal_param_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, param->name, 0) == 0) {
            name_len = (int)strlen(param->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, param->name, name_len);
            pos += name_len;
            count++;
        }
        next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_list_functs lists function names matching pattern.
static int hal_shim_list_functs(const char *pattern, char *buf, int buf_size) {
    hal_funct_t *funct;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->funct_list_ptr;
    while (next != 0) {
        funct = (hal_funct_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, funct->name, 0) == 0) {
            name_len = (int)strlen(funct->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, funct->name, name_len);
            pos += name_len;
            count++;
        }
        next = funct->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_list_threads lists thread names matching pattern.
static int hal_shim_list_threads(const char *pattern, char *buf, int buf_size) {
    hal_thread_t *thread;
    int next;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->thread_list_ptr;
    while (next != 0) {
        thread = (hal_thread_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, thread->name, 0) == 0) {
            name_len = (int)strlen(thread->name) + 1;
            if (pos + name_len > buf_size) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            memcpy(buf + pos, thread->name, name_len);
            pos += name_len;
            count++;
        }
        next = thread->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// ===== 1f. Show/status/save/debug shim structs and helpers =====

// HAL_SHIM_MAX_ITEMS is the initial result-array capacity for hal_shim_show_*
// functions.  If the real count exceeds this the shim returns -ENOSPC and the
// Go caller retries with a doubled capacity (up to showMaxCap).
#define HAL_SHIM_MAX_ITEMS 1024

// HAL_SHIM_MAX_TH_FNCTS is the maximum number of functions per thread stored
// in hal_shim_thread_info_t.funct_names.  In practice LinuxCNC threads rarely
// have more than a handful of functions; 32 is well above any real-world value.
// Functions beyond this limit are silently truncated in the output.
#define HAL_SHIM_MAX_TH_FNCTS 32

typedef struct {
    char name[HAL_NAME_LEN + 1];
    int  comp_id;
    int  type_;  // component_type_t: 0=user, 1=realtime, 2=other
    int  ready;
} hal_shim_comp_info_t;

typedef struct {
    char name[HAL_NAME_LEN + 1];
    char owner[HAL_NAME_LEN + 1];
    char signal[HAL_NAME_LEN + 1]; // empty if not linked
    // value[64]: sufficient for all HAL types — "%.7g" float ≤15 chars,
    // boolean is "TRUE"/"FALSE", s32/u32 ≤12 decimal digits.
    char value[64];
    int  type_;  // hal_type_t
    int  dir;    // hal_pin_dir_t
} hal_shim_pin_info_t;

typedef struct {
    char name[HAL_NAME_LEN + 1];
    char owner[HAL_NAME_LEN + 1];
    // value[64]: sufficient for all HAL types (see hal_shim_pin_info_t).
    char value[64];
    int  type_;  // hal_type_t
    int  dir;    // hal_param_dir_t
} hal_shim_param_info_t;

typedef struct {
    char name[HAL_NAME_LEN + 1];
    // value[64]: sufficient for all HAL types (see hal_shim_pin_info_t).
    char value[64];
    int  type_;    // hal_type_t
    int  readers;
    int  writers;
    int  bidirs;
} hal_shim_sig_info_t;

typedef struct {
    char name[HAL_NAME_LEN + 1];
    char owner[HAL_NAME_LEN + 1];
    int  users;
} hal_shim_funct_info_t;

typedef struct {
    char name[HAL_NAME_LEN + 1];
    long period;   // period in nanoseconds
    int  running;  // non-zero if threads are started
    int  nfuncts;
    char funct_names[HAL_SHIM_MAX_TH_FNCTS][HAL_NAME_LEN + 1];
} hal_shim_thread_info_t;

typedef struct {
    int shmem_avail;
    int lock;
} hal_shim_status_t;

// hal_shim_show_comps fills arr with up to max_items components matching pattern.
// Returns the number filled, or negative errno on error.
static int hal_shim_show_comps(const char *pattern, hal_shim_comp_info_t *arr, int max_items) {
    int next, count = 0;
    hal_comp_t *comp;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
        comp = (hal_comp_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, comp->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", comp->name);
            arr[count].comp_id = comp->comp_id;
            arr[count].type_   = (int)comp->type;
            arr[count].ready   = comp->ready;
            count++;
        }
        next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_show_pins fills arr with up to max_items pins matching pattern.
static int hal_shim_show_pins(const char *pattern, hal_shim_pin_info_t *arr, int max_items) {
    int next, count = 0;
    hal_pin_t  *pin;
    hal_sig_t  *sig;
    hal_comp_t *comp;
    void       *d_ptr;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
        pin = (hal_pin_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, pin->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", pin->name);
            arr[count].type_ = (int)pin->type;
            arr[count].dir  = (int)pin->dir;

            if (pin->owner_ptr != 0) {
                comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
                snprintf(arr[count].owner, sizeof(arr[count].owner), "%s", comp->name);
            } else {
                arr[count].owner[0] = '\0';
            }

            if (pin->signal != 0) {
                sig = (hal_sig_t *)SHMPTR(pin->signal);
                snprintf(arr[count].signal, sizeof(arr[count].signal), "%s", sig->name);
                d_ptr = SHMPTR(sig->data_ptr);
            } else {
                arr[count].signal[0] = '\0';
                d_ptr = (void *)&pin->dummysig;
            }
            hal_shim_format_value(pin->type, d_ptr,
                                  arr[count].value, sizeof(arr[count].value));
            count++;
        }
        next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_show_params fills arr with up to max_items parameters matching pattern.
static int hal_shim_show_params(const char *pattern, hal_shim_param_info_t *arr, int max_items) {
    int next, count = 0;
    hal_param_t *param;
    hal_comp_t  *comp;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
        param = (hal_param_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, param->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", param->name);
            arr[count].type_ = (int)param->type;
            arr[count].dir  = (int)param->dir;

            if (param->owner_ptr != 0) {
                comp = (hal_comp_t *)SHMPTR(param->owner_ptr);
                snprintf(arr[count].owner, sizeof(arr[count].owner), "%s", comp->name);
            } else {
                arr[count].owner[0] = '\0';
            }
            hal_shim_format_value(param->type, SHMPTR(param->data_ptr),
                                  arr[count].value, sizeof(arr[count].value));
            count++;
        }
        next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_show_sigs fills arr with up to max_items signals matching pattern.
static int hal_shim_show_sigs(const char *pattern, hal_shim_sig_info_t *arr, int max_items) {
    int next, count = 0;
    hal_sig_t *sig;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
        sig = (hal_sig_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, sig->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", sig->name);
            arr[count].type_   = (int)sig->type;
            arr[count].readers = sig->readers;
            arr[count].writers = sig->writers;
            arr[count].bidirs  = sig->bidirs;
            hal_shim_format_value(sig->type, SHMPTR(sig->data_ptr),
                                  arr[count].value, sizeof(arr[count].value));
            count++;
        }
        next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_show_functs fills arr with up to max_items functions matching pattern.
static int hal_shim_show_functs(const char *pattern, hal_shim_funct_info_t *arr, int max_items) {
    int next, count = 0;
    hal_funct_t *funct;
    hal_comp_t  *comp;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->funct_list_ptr;
    while (next != 0) {
        funct = (hal_funct_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, funct->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", funct->name);
            arr[count].users = funct->users;
            if (funct->owner_ptr != 0) {
                comp = (hal_comp_t *)SHMPTR(funct->owner_ptr);
                snprintf(arr[count].owner, sizeof(arr[count].owner), "%s", comp->name);
            } else {
                arr[count].owner[0] = '\0';
            }
            count++;
        }
        next = funct->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_show_threads fills arr with up to max_items threads matching pattern.
static int hal_shim_show_threads(const char *pattern, hal_shim_thread_info_t *arr, int max_items) {
    int next, count = 0;
    hal_thread_t      *tptr;
    hal_list_t        *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t       *funct;

    if (hal_data == NULL) return -EINVAL;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->thread_list_ptr;
    while (next != 0) {
        tptr = (hal_thread_t *)SHMPTR(next);
        if (pattern == NULL || *pattern == '\0' ||
            fnmatch(pattern, tptr->name, 0) == 0) {
            if (count >= max_items) {
                rtapi_mutex_give(&(hal_data->mutex));
                return -ENOSPC;
            }
            snprintf(arr[count].name, sizeof(arr[count].name), "%s", tptr->name);
            arr[count].period  = tptr->period;
            arr[count].running = hal_data->threads_running;
            arr[count].nfuncts = 0;

            list_root  = &(tptr->funct_list);
            list_entry = list_next(list_root);
            while (list_entry != list_root &&
                   arr[count].nfuncts < HAL_SHIM_MAX_TH_FNCTS) {
                fentry = (hal_funct_entry_t *)list_entry;
                funct  = (hal_funct_t *)SHMPTR(fentry->funct_ptr);
                snprintf(arr[count].funct_names[arr[count].nfuncts],
                         HAL_NAME_LEN + 1, "%s", funct->name);
                arr[count].nfuncts++;
                list_entry = list_next(list_entry);
            }
            count++;
        }
        next = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// hal_shim_status reads HAL shared-memory status into *st.
// Returns 0 on success or -EINVAL if hal_data is NULL.
static int hal_shim_status(hal_shim_status_t *st) {
    if (hal_data == NULL) return -EINVAL;
    rtapi_mutex_get(&(hal_data->mutex));
    st->shmem_avail = (int)hal_data->shmem_avail;
    st->lock        = (int)hal_data->lock;
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}

// hal_shim_debug sets the RTAPI message level (0–5).
// Returns 0 on success or a negative errno value on error.
static int hal_shim_debug(int level) {
    return rtapi_set_msg_level(level);
}

// buf_write_line copies line into buf[*pos] as a null-terminated entry and
// advances *pos.  Returns 0 on success or -ENOSPC if the buffer is full.
static int buf_write_line(char *buf, int *pos, int buf_size, const char *line) {
    int len = (int)strlen(line);
    if (*pos + len + 1 > buf_size) return -ENOSPC;
    memcpy(buf + *pos, line, len);
    *pos += len;
    buf[(*pos)++] = '\0';
    return 0;
}

// hal_shim_save serializes the current HAL state as halcmd command strings.
// type selects what to save: "all", "allu", "comp", "alias", "sig", "signal",
// "sigu", "link", "linka", "net", "neta", "netl", "netla", "netal", "param",
// "parameter", "thread".  Lines are written null-separated into buf.
// Returns the number of lines written, or a negative errno value on error.
static int hal_shim_save(const char *type, char *buf, int buf_size) {
    int pos   = 0;
    int count = 0;
    int next;
    char tmp[2048]; // large enough for any single halcmd line

    if (hal_data == NULL) return -EINVAL;
    if (type == NULL || *type == '\0') type = "all";

    int do_comps   = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"comp")==0);
    int do_aliases = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"alias")==0);
    int do_sigs    = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"sig")==0 || strcmp(type,"signal")==0 ||
                      strcmp(type,"sigu")==0);
    int do_nets    = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"net")==0 || strcmp(type,"neta")==0 ||
                      strcmp(type,"netl")==0 || strcmp(type,"netla")==0 ||
                      strcmp(type,"netal")==0);
    int do_links   = (strcmp(type,"link")==0 || strcmp(type,"linka")==0);
    int do_params  = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"param")==0 || strcmp(type,"parameter")==0);
    int do_threads = (strcmp(type,"all")==0 || strcmp(type,"allu")==0 ||
                      strcmp(type,"thread")==0);

    if (!do_comps && !do_aliases && !do_sigs && !do_nets &&
        !do_links && !do_params && !do_threads) {
        return -EINVAL;
    }

#define SAVE_LINE(fmt, ...) do { \
    snprintf(tmp, sizeof(tmp), fmt, ##__VA_ARGS__); \
    if (buf_write_line(buf, &pos, buf_size, tmp) != 0) return -ENOSPC; \
    count++; \
} while (0)

    // save realtime components
    if (do_comps) {
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->comp_list_ptr;
        while (next != 0) {
            hal_comp_t *comp = (hal_comp_t *)SHMPTR(next);
            if (comp->type == COMPONENT_TYPE_REALTIME) {
                if (comp->insmod_args == 0) {
                    SAVE_LINE("#loadrt %s  (not loaded by loadrt, no args saved)",
                              comp->name);
                } else {
                    SAVE_LINE("loadrt %s %s", comp->name,
                              (char *)SHMPTR(comp->insmod_args));
                }
            }
            next = comp->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

    // save aliases
    if (do_aliases) {
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->pin_list_ptr;
        while (next != 0) {
            hal_pin_t *pin = (hal_pin_t *)SHMPTR(next);
            if (pin->oldname != 0) {
                hal_oldname_t *oldname = (hal_oldname_t *)SHMPTR(pin->oldname);
                SAVE_LINE("alias pin %s %s", oldname->name, pin->name);
            }
            next = pin->next_ptr;
        }
        next = hal_data->param_list_ptr;
        while (next != 0) {
            hal_param_t *param = (hal_param_t *)SHMPTR(next);
            if (param->oldname != 0) {
                hal_oldname_t *oldname = (hal_oldname_t *)SHMPTR(param->oldname);
                SAVE_LINE("alias param %s %s", oldname->name, param->name);
            }
            next = param->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

    // save signals (newsig lines)
    if (do_sigs) {
        int only_unlinked = (strcmp(type,"sigu") == 0 ||
                             strcmp(type,"allu") == 0);
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->sig_list_ptr;
        while (next != 0) {
            hal_sig_t *sig = (hal_sig_t *)SHMPTR(next);
            if (only_unlinked && (sig->readers || sig->writers)) {
                next = sig->next_ptr;
                continue;
            }
            const char *type_name;
            switch (sig->type) {
            case HAL_BIT:   type_name = "bit";   break;
            case HAL_FLOAT: type_name = "float"; break;
            case HAL_S32:   type_name = "s32";   break;
            case HAL_U32:   type_name = "u32";   break;
            default:        type_name = "unknown"; break;
            }
            SAVE_LINE("newsig %s %s", sig->name, type_name);
            next = sig->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

    // save nets
    if (do_nets) {
        int net_size = HAL_NAME_LEN * 64 + 64;
        char *net_line = (char *)malloc(net_size);
        if (!net_line) return -ENOMEM;
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->sig_list_ptr;
        while (next != 0) {
            hal_sig_t *sig = (hal_sig_t *)SHMPTR(next);
            hal_pin_t *pin = halpr_find_pin_by_sig(sig, 0);
            if (pin) {
                // net <signame> <pins...>
                int net_pos = 0;
                int wr = snprintf(net_line, net_size, "net %s", sig->name);
                if (wr > 0 && wr < net_size) net_pos = wr;
                pin = halpr_find_pin_by_sig(sig, 0);
                while (pin != 0 && net_pos < net_size - 1) {
                    wr = snprintf(net_line + net_pos, net_size - net_pos,
                                  " %s", pin->name);
                    if (wr > 0 && wr < net_size - net_pos) net_pos += wr;
                    pin = halpr_find_pin_by_sig(sig, pin);
                }
                if (buf_write_line(buf, &pos, buf_size, net_line) != 0) {
                    rtapi_mutex_give(&(hal_data->mutex));
                    free(net_line);
                    return -ENOSPC;
                }
                count++;
            }
            next = sig->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
        free(net_line);
    }

    // save links (linkps lines)
    if (do_links) {
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->pin_list_ptr;
        while (next != 0) {
            hal_pin_t *pin = (hal_pin_t *)SHMPTR(next);
            if (pin->signal != 0) {
                hal_sig_t *sig = (hal_sig_t *)SHMPTR(pin->signal);
                SAVE_LINE("linkps %s %s", pin->name, sig->name);
            }
            next = pin->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

    // save writable parameter values
    if (do_params) {
        char val_buf[64];
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->param_list_ptr;
        while (next != 0) {
            hal_param_t *param = (hal_param_t *)SHMPTR(next);
            if (param->dir != HAL_RO) {
                hal_shim_format_value(param->type, SHMPTR(param->data_ptr),
                                      val_buf, sizeof(val_buf));
                SAVE_LINE("setp %s %s", param->name, val_buf);
            }
            next = param->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

    // save thread function assignments
    if (do_threads) {
        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->thread_list_ptr;
        while (next != 0) {
            hal_thread_t *tptr = (hal_thread_t *)SHMPTR(next);
            hal_list_t *list_root  = &(tptr->funct_list);
            hal_list_t *list_entry = list_next(list_root);
            while (list_entry != list_root) {
                hal_funct_entry_t *fentry = (hal_funct_entry_t *)list_entry;
                hal_funct_t *funct = (hal_funct_t *)SHMPTR(fentry->funct_ptr);
                SAVE_LINE("addf %s %s", funct->name, tptr->name);
                list_entry = list_next(list_entry);
            }
            next = tptr->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));
    }

#undef SAVE_LINE
    return count;
}
*/
import "C"
import (
	"bytes"
	"fmt"
	"unsafe"

	hal "linuxcnc.org/hal"
)

// halError translates a HAL C error code to a Go error.
// Returns nil if the code is 0 (success).
func halError(code int, op string) error {
	if code == 0 {
		return nil
	}

	// Map common HAL error codes to meaningful messages
	// These are based on standard errno values and HAL conventions
	var message string
	switch code {
	case -1:
		message = "general HAL error or operation not permitted (HAL may be locked)"
	case -11: // -EAGAIN
		message = "resource temporarily unavailable"
	case -12: // -ENOMEM
		message = "insufficient HAL shared memory"
	case -13: // -EPERM
		message = "operation not permitted (pin/param is read-only or output-only)"
	case -16: // -EBUSY
		message = "resource busy or already in use"
	case -17: // -EEXIST
		message = "object already exists"
	case -22: // -EINVAL
		message = "invalid argument or name"
	case -23: // -ENFILE
		message = "too many open files or components"
	case -28: // -ENOSPC
		message = "no space left in HAL shared memory"
	case -110: // -ETIMEDOUT
		message = "operation timed out"
	default:
		message = fmt.Sprintf("HAL error (code %d)", code)
	}

	return &hal.Error{Code: code, Message: message, Op: op}
}

// halStartThreads wraps hal_start_threads() to start all HAL realtime threads.
func halStartThreads() error {
	ret := C.hal_start_threads()
	return halError(int(ret), "hal_start_threads")
}

// halStopThreads wraps hal_stop_threads() to stop all HAL realtime threads.
func halStopThreads() error {
	ret := C.hal_stop_threads()
	return halError(int(ret), "hal_stop_threads")
}

// halListComponents wraps hal_shim_list_comps() to return all HAL component names.
// Returns a slice of component name strings, or an error on failure.
func halListComponents() ([]string, error) {
	// Allocate a buffer sized for HAL_SHIM_MAX_COMPS components, each with a
	// name up to HAL_NAME_LEN+1 bytes (HAL_NAME_LEN is 127, defined in hal.h).
	bufSize := int(C.HAL_SHIM_MAX_COMPS) * (int(C.HAL_NAME_LEN) + 1)
	buf := make([]byte, bufSize)

	ret := C.hal_shim_list_comps((*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(bufSize))
	if ret < 0 {
		return nil, halError(int(ret), "hal_shim_list_comps")
	}
	if ret == 0 {
		return []string{}, nil
	}

	// Scan only the portion of the buffer that was actually written.
	end := 0
	remaining := int(ret)
	for end < len(buf) && remaining > 0 {
		if buf[end] == 0 {
			remaining--
		}
		end++
	}

	parts := bytes.Split(buf[:end], []byte{0})
	names := make([]string, 0, int(ret))
	for _, p := range parts {
		if len(p) > 0 {
			names = append(names, string(p))
		}
	}
	return names, nil
}

// halUnloadAll wraps hal_shim_unload_all() to exit all HAL components except
// the one identified by exceptID.
func halUnloadAll(exceptID int) error {
	ret := C.hal_shim_unload_all(C.int(exceptID))
	return halError(int(ret), "hal_shim_unload_all")
}

// ===== Go wrappers for 1a simple shims =====

// halNewSig wraps hal_shim_newsig() to create a new HAL signal.
func halNewSig(name string, halType hal.PinType) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_newsig(cName, C.int(halType))
	return halError(int(ret), "hal_shim_newsig")
}

// halDelSig wraps hal_shim_delsig() to delete a HAL signal.
func halDelSig(name string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_delsig(cName)
	return halError(int(ret), "hal_shim_delsig")
}

// halLinkPS wraps hal_shim_linkps() to link a pin to a signal.
func halLinkPS(pin, sig string) error {
	cPin := C.CString(pin)
	defer C.free(unsafe.Pointer(cPin))
	cSig := C.CString(sig)
	defer C.free(unsafe.Pointer(cSig))
	ret := C.hal_shim_linkps(cPin, cSig)
	return halError(int(ret), "hal_shim_linkps")
}

// halUnlinkP wraps hal_shim_unlinkp() to unlink a pin from its signal.
func halUnlinkP(pin string) error {
	cPin := C.CString(pin)
	defer C.free(unsafe.Pointer(cPin))
	ret := C.hal_shim_unlinkp(cPin)
	return halError(int(ret), "hal_shim_unlinkp")
}

// halAddF wraps hal_shim_addf() to add a function to a thread.
func halAddF(funct, thread string, pos int) error {
	cFunct := C.CString(funct)
	defer C.free(unsafe.Pointer(cFunct))
	cThread := C.CString(thread)
	defer C.free(unsafe.Pointer(cThread))
	ret := C.hal_shim_addf(cFunct, cThread, C.int(pos))
	return halError(int(ret), "hal_shim_addf")
}

// halDelF wraps hal_shim_delf() to remove a function from a thread.
func halDelF(funct, thread string) error {
	cFunct := C.CString(funct)
	defer C.free(unsafe.Pointer(cFunct))
	cThread := C.CString(thread)
	defer C.free(unsafe.Pointer(cThread))
	ret := C.hal_shim_delf(cFunct, cThread)
	return halError(int(ret), "hal_shim_delf")
}

// halSetLock wraps hal_shim_set_lock() to set the HAL lock level.
func halSetLock(lockType int) error {
	ret := C.hal_shim_set_lock(C.uchar(lockType))
	return halError(int(ret), "hal_shim_set_lock")
}

// halGetLock wraps hal_shim_get_lock() to get the current HAL lock level.
func halGetLock() int {
	return int(C.hal_shim_get_lock())
}

// halPinAlias wraps hal_shim_pin_alias() to set or clear a pin alias.
// Pass an empty alias to remove the existing alias.
func halPinAlias(pinName, alias string) error {
	cPin := C.CString(pinName)
	defer C.free(unsafe.Pointer(cPin))
	var cAlias *C.char
	if alias != "" {
		cAlias = C.CString(alias)
		defer C.free(unsafe.Pointer(cAlias))
	}
	ret := C.hal_shim_pin_alias(cPin, cAlias)
	return halError(int(ret), "hal_shim_pin_alias")
}

// halParamAlias wraps hal_shim_param_alias() to set or clear a parameter alias.
// Pass an empty alias to remove the existing alias.
func halParamAlias(paramName, alias string) error {
	cParam := C.CString(paramName)
	defer C.free(unsafe.Pointer(cParam))
	var cAlias *C.char
	if alias != "" {
		cAlias = C.CString(alias)
		defer C.free(unsafe.Pointer(cAlias))
	}
	ret := C.hal_shim_param_alias(cParam, cAlias)
	return halError(int(ret), "hal_shim_param_alias")
}

// halAlias creates an alias for a pin or parameter.
// kind must be "pin" or "param".
func halAlias(kind, name, alias string) error {
	switch kind {
	case "pin":
		return halPinAlias(name, alias)
	case "param":
		return halParamAlias(name, alias)
	default:
		return fmt.Errorf("alias: unknown kind %q: must be \"pin\" or \"param\"", kind)
	}
}

// halUnAlias removes an alias from a pin or parameter.
// kind must be "pin" or "param".
func halUnAlias(kind, name string) error {
	switch kind {
	case "pin":
		return halPinAlias(name, "")
	case "param":
		return halParamAlias(name, "")
	default:
		return fmt.Errorf("unalias: unknown kind %q: must be \"pin\" or \"param\"", kind)
	}
}

// ===== Go wrappers for 1b shmem access shims =====

// halSetP wraps hal_shim_setp() to set a pin or parameter value by name.
func halSetP(name, value string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	cValue := C.CString(value)
	defer C.free(unsafe.Pointer(cValue))
	ret := C.hal_shim_setp(cName, cValue)
	return halError(int(ret), "hal_shim_setp")
}

// halGetP wraps hal_shim_getp() to get a pin or parameter value as a string.
func halGetP(name string) (string, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	buf := make([]byte, 256)
	ret := C.hal_shim_getp(cName, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(len(buf)))
	if ret < 0 {
		return "", halError(int(ret), "hal_shim_getp")
	}
	return C.GoString((*C.char)(unsafe.Pointer(unsafe.SliceData(buf)))), nil
}

// halSetS wraps hal_shim_sets() to set a signal value by name.
func halSetS(name, value string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	cValue := C.CString(value)
	defer C.free(unsafe.Pointer(cValue))
	ret := C.hal_shim_sets(cName, cValue)
	return halError(int(ret), "hal_shim_sets")
}

// halGetS wraps hal_shim_gets() to get a signal value as a string.
func halGetS(name string) (string, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	buf := make([]byte, 256)
	ret := C.hal_shim_gets(cName, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(len(buf)))
	if ret < 0 {
		return "", halError(int(ret), "hal_shim_gets")
	}
	return C.GoString((*C.char)(unsafe.Pointer(unsafe.SliceData(buf)))), nil
}

// halPType wraps hal_shim_ptype() to get the type of a pin or parameter.
func halPType(name string) (hal.PinType, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_ptype(cName)
	if ret < 0 {
		return 0, halError(int(ret), "hal_shim_ptype")
	}
	return hal.PinType(ret), nil
}

// halSType wraps hal_shim_stype() to get the type of a signal.
func halSType(name string) (hal.PinType, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_stype(cName)
	if ret < 0 {
		return 0, halError(int(ret), "hal_shim_stype")
	}
	return hal.PinType(ret), nil
}

// ===== Go wrapper for 1c net shim =====

// halNet wraps hal_shim_net() to connect pins to a signal.
// pinNames must not include arrow tokens (=>, <=, <=>).
func halNet(sigName string, pinNames []string) error {
	if len(pinNames) == 0 {
		return halError(-22, "hal_shim_net")
	}
	cSig := C.CString(sigName)
	defer C.free(unsafe.Pointer(cSig))

	// Build null-separated pin names buffer
	var buf []byte
	for _, pin := range pinNames {
		buf = append(buf, []byte(pin)...)
		buf = append(buf, 0)
	}

	ret := C.hal_shim_net(cSig, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(len(pinNames)))
	return halError(int(ret), "hal_shim_net")
}

// ===== Go wrappers for 1d process management shims =====

// halLoadRT wraps hal_shim_loadrt() to load a realtime HAL module in-process.
func halLoadRT(mod string, args []string) error {
	cMod := C.CString(mod)
	defer C.free(unsafe.Pointer(cMod))

	if len(args) == 0 {
		ret := C.hal_shim_loadrt(cMod, nil, 0)
		return halError(int(ret), "hal_shim_loadrt")
	}

	cArgs := make([]*C.char, len(args))
	for i, arg := range args {
		cArgs[i] = C.CString(arg)
		defer C.free(unsafe.Pointer(cArgs[i]))
	}
	ret := C.hal_shim_loadrt(cMod, (**C.char)(unsafe.Pointer(&cArgs[0])), C.int(len(cArgs)))
	return halError(int(ret), "hal_shim_loadrt")
}

// halUnloadRT wraps hal_shim_unloadrt() to unload a realtime HAL module in-process.
func halUnloadRT(mod string) error {
	cMod := C.CString(mod)
	defer C.free(unsafe.Pointer(cMod))
	ret := C.hal_shim_unloadrt(cMod)
	return halError(int(ret), "hal_shim_unloadrt")
}

// halNewInst wraps hal_shim_newinst() to create a new component instance.
func halNewInst(compType, name, arg string) error {
	cType := C.CString(compType)
	defer C.free(unsafe.Pointer(cType))
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	cArg := C.CString(arg)
	defer C.free(unsafe.Pointer(cArg))
	ret := C.hal_shim_newinst(cType, cName, cArg)
	return halError(int(ret), "hal_shim_newinst")
}

// halRtapiAppInit wraps hal_shim_rtapi_app_init() — initializes HAL shared
// memory and starts the message queue thread.  Must be called before hal_init().
func halRtapiAppInit() error {
	ret := C.hal_shim_rtapi_app_init()
	return halError(int(ret), "hal_shim_rtapi_app_init")
}

// halRtapiInitializeApp wraps rtapi_initialize_app() — idempotently sets up
// RT rlimits, mlockall(MCL_CURRENT), signal handlers, and io privileges.
// Safe to call multiple times (guarded internally by a once flag).
func halRtapiInitializeApp() {
	C.rtapi_initialize_app()
}

// halRtapiAppCleanup wraps hal_shim_rtapi_app_cleanup() — tears down HAL
// threads, releases shared memory, and stops the message queue.
// Must be called after all components are unloaded and before hal_exit().
func halRtapiAppCleanup() {
	C.hal_shim_rtapi_app_cleanup()
}

// halLoadUSR wraps hal_shim_loadusr() to start a user-space HAL component.
// flags: 1=wait_ready, 2=wait_exit, 4=no_stdin
func halLoadUSR(flags int, waitName string, timeoutSecs int, prog string, args []string) error {
	cProg := C.CString(prog)
	defer C.free(unsafe.Pointer(cProg))

	var cWaitName *C.char
	if waitName != "" {
		cWaitName = C.CString(waitName)
		defer C.free(unsafe.Pointer(cWaitName))
	}

	if len(args) == 0 {
		ret := C.hal_shim_loadusr(C.int(flags), cWaitName, C.int(timeoutSecs), cProg, nil, 0)
		return halError(int(ret), "hal_shim_loadusr")
	}

	cArgs := make([]*C.char, len(args))
	for i, arg := range args {
		cArgs[i] = C.CString(arg)
		defer C.free(unsafe.Pointer(cArgs[i]))
	}
	ret := C.hal_shim_loadusr(C.int(flags), cWaitName, C.int(timeoutSecs), cProg,
		(**C.char)(unsafe.Pointer(&cArgs[0])), C.int(len(cArgs)))
	return halError(int(ret), "hal_shim_loadusr")
}

// halUnloadUSR wraps hal_shim_unloadusr() to send SIGTERM to a user-space component.
func halUnloadUSR(compName string) error {
	cName := C.CString(compName)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_unloadusr(cName)
	return halError(int(ret), "hal_shim_unloadusr")
}

// halWaitUSR wraps hal_shim_waitusr() to wait for a component to disappear from HAL.
func halWaitUSR(compName string, timeoutSecs int) error {
	cName := C.CString(compName)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_waitusr(cName, C.int(timeoutSecs))
	return halError(int(ret), "hal_shim_waitusr")
}

// ===== Go wrappers for 1e list shims =====

// listShimFn is a generic helper type for the hal_shim_list_* family.
type listShimFn func(pattern *C.char, buf *C.char, size C.int) C.int

func halListGeneric(pattern string, shimFn listShimFn) ([]string, error) {
	bufSize := C.int(C.HAL_SHIM_MAX_COMPS * (C.HAL_NAME_LEN + 1))
	buf := make([]byte, int(bufSize))

	var cPattern *C.char
	if pattern != "" {
		cPattern = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPattern))
	}

	ret := shimFn(cPattern, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), bufSize)
	if ret < 0 {
		return nil, halError(int(ret), "hal_shim_list")
	}
	if ret == 0 {
		return []string{}, nil
	}

	// Count nulls to find the written portion of the buffer
	end := 0
	remaining := int(ret)
	for end < len(buf) && remaining > 0 {
		if buf[end] == 0 {
			remaining--
		}
		end++
	}

	parts := bytes.Split(buf[:end], []byte{0})
	names := make([]string, 0, int(ret))
	for _, p := range parts {
		if len(p) > 0 {
			names = append(names, string(p))
		}
	}
	return names, nil
}

// halListPins returns pin names matching the given pattern (may be empty for all).
func halListPins(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_pins(p, buf, size)
	})
}

// halListSigs returns signal names matching the given pattern.
func halListSigs(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_sigs(p, buf, size)
	})
}

// halListRetainSigs returns names of retain-flagged signals matching the given pattern.
func halListRetainSigs(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_retain_sigs(p, buf, size)
	})
}

// halListParams returns parameter names matching the given pattern.
func halListParams(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_params(p, buf, size)
	})
}

// halListFuncts returns function names matching the given pattern.
func halListFuncts(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_functs(p, buf, size)
	})
}

// halListThreads returns thread names matching the given pattern.
func halListThreads(pattern string) ([]string, error) {
	return halListGeneric(pattern, func(p *C.char, buf *C.char, size C.int) C.int {
		return C.hal_shim_list_threads(p, buf, size)
	})
}

// ===== Go wrappers for 1f show/status/save/debug shims =====

// cHalTypeName converts a C hal_type_t integer to a Go type name string.
func cHalTypeName(t C.int) string {
	switch hal.PinType(t) {
	case hal.TypeBit:
		return "bit"
	case hal.TypeFloat:
		return "float"
	case hal.TypeS32:
		return "s32"
	case hal.TypeU32:
		return "u32"
	default:
		return "unknown"
	}
}

// cPinDirName converts a C hal_pin_dir_t integer to a direction string.
func cPinDirName(d C.int) string {
	switch hal.Direction(d) {
	case hal.In:
		return "IN"
	case hal.Out:
		return "OUT"
	case hal.IO:
		return "IO"
	default:
		return "unknown"
	}
}

// cParamDirName converts a C hal_param_dir_t integer to a direction string.
func cParamDirName(d C.int) string {
	switch d {
	case 64: // HAL_RO
		return "RO"
	case 192: // HAL_RW = HAL_RO | HAL_WO
		return "RW"
	default:
		return "unknown"
	}
}

// cCompTypeName converts a C component_type_t integer to a type string.
func cCompTypeName(t C.int) string {
	switch t {
	case 0: // COMPONENT_TYPE_USER
		return "user"
	case 1: // COMPONENT_TYPE_REALTIME
		return "realtime"
	default:
		return "other"
	}
}

// lockLevelName converts a HAL lock bitmask to a human-readable string.
func lockLevelName(lock C.int) string {
	switch lock {
	case 0:
		return "none"
	case 1:
		return "load"
	case 2:
		return "config"
	case 3:
		return "tune"
	case 4:
		return "params"
	case 8:
		return "run"
	case 255:
		return "all"
	default:
		return fmt.Sprintf("0x%02x", int(lock))
	}
}

// showMaxCap is the upper bound for the number of items any halShow* call
// will allocate before giving up.  The initial attempt uses HAL_SHIM_MAX_ITEMS;
// on -ENOSPC the capacity is doubled each time up to this limit.
const showMaxCap = 65536

// saveBufMax is the upper bound in bytes for the hal_shim_save output buffer.
// The initial attempt uses 64 KiB; on -ENOSPC the buffer is doubled up to this limit.
const saveBufMax = 4 * 1024 * 1024 // 4 MiB

// halShowComps returns structured information about all components matching pattern.
func halShowComps(pattern string) ([]CompInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_comp_info_t, cap)
		n := C.hal_shim_show_comps(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_comps")
		}
		result := make([]CompInfo, int(n))
		for i := range result {
			result[i] = CompInfo{
				Name: C.GoString(&arr[i].name[0]),
				ID:   int(arr[i].comp_id),
				Type: cCompTypeName(arr[i].type_),
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_comps: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halShowPins returns structured information about all pins matching pattern.
func halShowPins(pattern string) ([]PinInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_pin_info_t, cap)
		n := C.hal_shim_show_pins(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_pins")
		}
		result := make([]PinInfo, int(n))
		for i := range result {
			result[i] = PinInfo{
				Name:      C.GoString(&arr[i].name[0]),
				Type:      cHalTypeName(arr[i].type_),
				Direction: cPinDirName(arr[i].dir),
				Value:     C.GoString(&arr[i].value[0]),
				Signal:    C.GoString(&arr[i].signal[0]),
				Owner:     C.GoString(&arr[i].owner[0]),
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_pins: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halShowParams returns structured information about all parameters matching pattern.
func halShowParams(pattern string) ([]ParamInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_param_info_t, cap)
		n := C.hal_shim_show_params(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_params")
		}
		result := make([]ParamInfo, int(n))
		for i := range result {
			result[i] = ParamInfo{
				Name:      C.GoString(&arr[i].name[0]),
				Type:      cHalTypeName(arr[i].type_),
				Direction: cParamDirName(arr[i].dir),
				Value:     C.GoString(&arr[i].value[0]),
				Owner:     C.GoString(&arr[i].owner[0]),
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_params: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halShowSigs returns structured information about all signals matching pattern.
func halShowSigs(pattern string) ([]SigInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_sig_info_t, cap)
		n := C.hal_shim_show_sigs(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_sigs")
		}
		result := make([]SigInfo, int(n))
		for i := range result {
			result[i] = SigInfo{
				Name:  C.GoString(&arr[i].name[0]),
				Type:  cHalTypeName(arr[i].type_),
				Value: C.GoString(&arr[i].value[0]),
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_sigs: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halShowFuncts returns structured information about all functions matching pattern.
func halShowFuncts(pattern string) ([]FunctInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_funct_info_t, cap)
		n := C.hal_shim_show_functs(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_functs")
		}
		result := make([]FunctInfo, int(n))
		for i := range result {
			result[i] = FunctInfo{
				Name:  C.GoString(&arr[i].name[0]),
				Owner: C.GoString(&arr[i].owner[0]),
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_functs: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halShowThreads returns structured information about all threads matching pattern.
func halShowThreads(pattern string) ([]ThreadInfo, error) {
	var cPat *C.char
	if pattern != "" {
		cPat = C.CString(pattern)
		defer C.free(unsafe.Pointer(cPat))
	}

	for cap := int(C.HAL_SHIM_MAX_ITEMS); cap <= showMaxCap; cap *= 2 {
		arr := make([]C.hal_shim_thread_info_t, cap)
		n := C.hal_shim_show_threads(cPat, &arr[0], C.int(cap))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_show_threads")
		}
		result := make([]ThreadInfo, int(n))
		for i := range result {
			nf := int(arr[i].nfuncts)
			functs := make([]string, nf)
			for j := 0; j < nf; j++ {
				functs[j] = C.GoString(&arr[i].funct_names[j][0])
			}
			result[i] = ThreadInfo{
				Name:    C.GoString(&arr[i].name[0]),
				Period:  int64(arr[i].period),
				Running: arr[i].running != 0,
				Functs:  functs,
			}
		}
		return result, nil
	}
	return nil, fmt.Errorf("hal_shim_show_threads: result set exceeds maximum capacity (%d items)", showMaxCap)
}

// halStatus returns HAL shared-memory status information.
func halStatus() (*StatusInfo, error) {
	var st C.hal_shim_status_t
	ret := C.hal_shim_status(&st)
	if ret < 0 {
		return nil, halError(int(ret), "hal_shim_status")
	}
	return &StatusInfo{
		ShmemFree: int(st.shmem_avail),
		LockLevel: lockLevelName(st.lock),
	}, nil
}

// halSave serializes current HAL state as halcmd command strings.
// type selects what to save (see hal_shim_save for valid types).
// The output buffer starts at 64 KiB and doubles on -ENOSPC up to saveBufMax.
func halSave(saveType string) ([]string, error) {
	cType := C.CString(saveType)
	defer C.free(unsafe.Pointer(cType))

	for bufSize := 65536; bufSize <= saveBufMax; bufSize *= 2 {
		buf := make([]byte, bufSize)
		n := C.hal_shim_save(cType, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(bufSize))
		if n < 0 {
			if int(n) == -int(C.ENOSPC) {
				continue
			}
			return nil, halError(int(n), "hal_shim_save")
		}
		if n == 0 {
			return []string{}, nil
		}

		// Parse null-separated lines.
		end := 0
		remaining := int(n)
		for end < len(buf) && remaining > 0 {
			if buf[end] == 0 {
				remaining--
			}
			end++
		}

		parts := bytes.Split(buf[:end], []byte{0})
		lines := make([]string, 0, int(n))
		for _, p := range parts {
			if len(p) > 0 {
				lines = append(lines, string(p))
			}
		}
		return lines, nil
	}
	return nil, fmt.Errorf("hal_shim_save: output exceeds maximum buffer size (%d bytes)", saveBufMax)
}

// halSetDebug sets the RTAPI message verbosity level.
func halSetDebug(level int) error {
	ret := C.hal_shim_debug(C.int(level))
	return halError(int(ret), "hal_shim_debug")
}
