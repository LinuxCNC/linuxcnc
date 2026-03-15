package hal

/*
#cgo CFLAGS: -I${SRCDIR}/.. -I${SRCDIR}/../.. -I${SRCDIR}/../../rtapi -I${SRCDIR}/../../../include -DULAPI
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -llinuxcnchal

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
#include "config.h"
#include "hal.h"
#include "../hal_priv.h"

// Helper to convert hal_type_t to int for Go
static inline int get_hal_type(hal_type_t t) { return (int)t; }

// Helper to convert hal_pin_dir_t to int for Go
static inline int get_hal_dir(hal_pin_dir_t d) { return (int)d; }

// Port helper wrappers: take hal_port_t* and dereference for the C API.
static inline bool go_hal_port_write(hal_port_t* p, const char* src, unsigned count) {
    return hal_port_write(*p, src, count);
}
static inline bool go_hal_port_peek(hal_port_t* p, char* dest, unsigned count) {
    return hal_port_peek(*p, dest, count);
}
static inline unsigned go_hal_port_readable(hal_port_t* p) {
    return hal_port_readable(*p);
}
static inline void go_hal_port_clear(hal_port_t* p) {
    hal_port_clear(*p);
}

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

// shim_systemv forks and execs argv[0] with the given argument vector,
// waits for it to exit, and returns its exit status.
// This replicates what hal_systemv() does in halcmd_commands.cc, but
// hal_systemv() is not part of liblinuxcnchal so we implement it inline.
static int shim_systemv(const char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

// hal_shim_unload_all unloads all HAL components, exactly as halcmd's
// "unload all" command does:
//   - Userspace components: send SIGTERM to their owning process
//   - Realtime components: call "rtapi_app unload <name>" via shim_systemv
// The component identified by except_id is skipped (pass 0 to not skip any).
// Returns 0 on success, or a negative errno value on error.
static int hal_shim_unload_all(int except_id) {
    int next;
    hal_comp_t *comp;
    pid_t ourpid = getpid();

    if (hal_data == NULL) {
        return -EINVAL;
    }

    // Phase 1: send SIGTERM to userspace components (same as do_unloadusr_cmd)
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

    // Phase 2: collect realtime component names then unload via rtapi_app
    // (same as do_unloadrt_cmd)
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
                    // skip pseudo-components (names starting with "__")
                    if (strstr(comp->name, HAL_PSEUDO_COMP_PREFIX) != comp->name) {
                        snprintf(comps[n], sizeof(comps[n]), "%s", comp->name);
                        n++;
                    }
                }
            }
            next = comp->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));

        // unload each realtime component via rtapi_app
        for (i = 0; i < n; i++) {
            const char *argv[4];
            argv[0] = EMC2_BIN_DIR "/rtapi_app";
            argv[1] = "unload";
            argv[2] = comps[i];
            argv[3] = NULL;
            shim_systemv(argv);
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

// hal_shim_loadrt loads a realtime module via rtapi_app.
// On USPACE: fork+exec "rtapi_app load <mod> [args]" and wait.
// Returns 0 on success, non-zero on error.
static int hal_shim_loadrt(const char *mod, const char *const args[], int nargs) {
    const char *argv[256];
    int m = 0;
    int i;

    if (m + 3 + nargs >= 256) return -E2BIG;

    argv[m++] = EMC2_BIN_DIR "/rtapi_app";
    argv[m++] = "load";
    argv[m++] = mod;
    for (i = 0; i < nargs; i++) {
        argv[m++] = args[i];
    }
    argv[m] = NULL;

    return shim_systemv(argv);
}

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

    pid = fork();
    if (pid < 0) return -errno;

    if (pid == 0) {
        // Child process
        if (no_stdin) {
            int fd = open("/dev/null", O_RDONLY);
            if (fd >= 0) {
                dup2(fd, 0);
                close(fd);
            }
        }
        execvp(prog, (char *const *)argv);
        _exit(127);
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

// hal_shim_unloadrt unloads a realtime module via rtapi_app.
static int hal_shim_unloadrt(const char *mod) {
    const char *argv[4];
    argv[0] = EMC2_BIN_DIR "/rtapi_app";
    argv[1] = "unload";
    argv[2] = mod;
    argv[3] = NULL;
    return shim_systemv(argv);
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

// hal_shim_list_generic_impl is a helper that writes null-separated names from
// a HAL linked list into buf, filtering by fnmatch pattern.
// It returns the count of names written, or -ENOSPC if the buffer is too small.
// This macro avoids code duplication across list functions.
// It is not a real function to keep the implementation simple for cgo.

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
*/
import "C"
import (
	"bytes"
	"fmt"
	"unsafe"
)

// halInit wraps hal_init() to create a new HAL component.
// Returns the component ID on success, or an error on failure.
func halInit(name string) (int, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	compID := C.hal_init(cName)
	if compID < 0 {
		return 0, halError(int(compID), "hal_init")
	}

	return int(compID), nil
}

// halReady wraps hal_ready() to mark a component as ready.
// Returns an error on failure.
func halReady(compID int) error {
	ret := C.hal_ready(C.int(compID))
	return halError(int(ret), "hal_ready")
}

// halExit wraps hal_exit() to clean up a component.
// Returns an error on failure.
func halExit(compID int) error {
	ret := C.hal_exit(C.int(compID))
	return halError(int(ret), "hal_exit")
}

// halMalloc wraps hal_malloc() to allocate memory in HAL shared memory.
// Returns an unsafe.Pointer to the allocated memory, or nil on failure.
// The allocated memory is freed when the component exits.
func halMalloc(size int) unsafe.Pointer {
	return C.hal_malloc(C.long(size))
}

// halPinBitNew wraps hal_pin_bit_new() to create a new bit (boolean) pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinBitNew(name string, dir Direction, compID int) (*C.hal_bit_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_bit_new expects hal_bit_t**, so we need sizeof(hal_bit_t *)
	ptrPtr := (**C.hal_bit_t)(halMalloc(int(unsafe.Sizeof((*C.hal_bit_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_bit_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_bit_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_bit_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinFloatNew wraps hal_pin_float_new() to create a new float pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinFloatNew(name string, dir Direction, compID int) (*C.hal_float_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_float_new expects hal_float_t**, so we need sizeof(hal_float_t *)
	ptrPtr := (**C.hal_float_t)(halMalloc(int(unsafe.Sizeof((*C.hal_float_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_float_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_float_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_float_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinS32New wraps hal_pin_s32_new() to create a new signed 32-bit integer pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinS32New(name string, dir Direction, compID int) (*C.hal_s32_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_s32_new expects hal_s32_t**, so we need sizeof(hal_s32_t *)
	ptrPtr := (**C.hal_s32_t)(halMalloc(int(unsafe.Sizeof((*C.hal_s32_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_s32_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_s32_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_s32_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinU32New wraps hal_pin_u32_new() to create a new unsigned 32-bit integer pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinU32New(name string, dir Direction, compID int) (*C.hal_u32_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_u32_new expects hal_u32_t**, so we need sizeof(hal_u32_t *)
	ptrPtr := (**C.hal_u32_t)(halMalloc(int(unsafe.Sizeof((*C.hal_u32_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_u32_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_u32_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_u32_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinPortNew wraps hal_pin_port_new() to create a new port pin.
// Returns the double-pointer (unsafe.Pointer to **hal_port_t) so the caller
// can dereference at access time. HAL updates *ptrPtr when the pin is linked
// to a signal via net, so the double-pointer must be preserved.
func halPinPortNew(name string, dir Direction, compID int) (unsafe.Pointer, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_port_new expects hal_port_t**, so we need sizeof(hal_port_t *)
	ptrPtr := (**C.hal_port_t)(halMalloc(int(unsafe.Sizeof((*C.hal_port_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_port_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_port_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_port_new")
	}

	// Return the double-pointer itself — the caller must dereference at access time
	// because HAL updates *ptrPtr when the pin is linked to a signal via net.
	return unsafe.Pointer(ptrPtr), nil
}

// halPortWrite writes data bytes to the port referenced by portPtr.
// Returns true if all bytes were written successfully.
func halPortWrite(portPtr *C.hal_port_t, data []byte) bool {
	if len(data) == 0 {
		// Nothing to write; avoid passing a nil/empty-slice pointer to the C function.
		return true
	}
	ret := C.go_hal_port_write(portPtr, (*C.char)(unsafe.Pointer(unsafe.SliceData(data))), C.uint(len(data)))
	return bool(ret)
}

// halPortPeek reads count bytes from the port without consuming them.
// Returns the bytes read, or nil if not enough data is available.
func halPortPeek(portPtr *C.hal_port_t, count uint) []byte {
	if count == 0 {
		return []byte{}
	}
	buf := make([]byte, count)
	ret := C.go_hal_port_peek(portPtr, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.uint(count))
	if !bool(ret) {
		return nil
	}
	return buf
}

// halPortReadable returns the number of bytes available to read from the port.
func halPortReadable(portPtr *C.hal_port_t) uint {
	return uint(C.go_hal_port_readable(portPtr))
}

// halPortClear empties the port of all data.
func halPortClear(portPtr *C.hal_port_t) {
	C.go_hal_port_clear(portPtr)
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
	// The C function writes exactly `ret` null-terminated strings starting at
	// offset 0.  Find the end by counting null terminators.
	end := 0
	remaining := int(ret)
	for end < len(buf) && remaining > 0 {
		if buf[end] == 0 {
			remaining--
		}
		end++
	}

	// Split the written portion on null bytes and collect non-empty strings.
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

	return newError(op, message, code)
}

// ===== Go wrappers for 1a simple shims =====

// halNewSig wraps hal_shim_newsig() to create a new HAL signal.
func halNewSig(name string, halType PinType) error {
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
func halPType(name string) (PinType, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_ptype(cName)
	if ret < 0 {
		return 0, halError(int(ret), "hal_shim_ptype")
	}
	return PinType(ret), nil
}

// halSType wraps hal_shim_stype() to get the type of a signal.
func halSType(name string) (PinType, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	ret := C.hal_shim_stype(cName)
	if ret < 0 {
		return 0, halError(int(ret), "hal_shim_stype")
	}
	return PinType(ret), nil
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

// halLoadRT wraps hal_shim_loadrt() to load a realtime HAL module.
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

// halUnloadRT wraps hal_shim_unloadrt() to unload a realtime HAL module.
func halUnloadRT(mod string) error {
	cMod := C.CString(mod)
	defer C.free(unsafe.Pointer(cMod))
	ret := C.hal_shim_unloadrt(cMod)
	return halError(int(ret), "hal_shim_unloadrt")
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

// halListItems is a generic helper for the hal_shim_list_* family.
// It calls the C shim (shimFn), splits the null-separated result, and returns
// a slice of matching name strings.  The pattern may be empty to match all.
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
