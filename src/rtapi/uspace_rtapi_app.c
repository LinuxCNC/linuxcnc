/* Copyright (C) 2006-2026 Jeff Epler <jepler@unpythonic.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#include "linuxcnc.h"

#include <stdatomic.h>

#ifdef __linux__
#include <sys/fsuid.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <spawn.h>
#include <sched.h>
#include <pthread.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include <sys/resource.h>
#include <sys/mman.h>
#ifdef __linux__
#include <malloc.h>
#include <sys/prctl.h>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

#include "rtapi.h"
#include "hal.h"
#include "hal/hal_priv.h"

/* rtapi_task structure - converted from C++ class hierarchy (rtapi_uspace.hh deleted) */
struct rtapi_task {
    int magic;
    int id;
    int owner;
    int uses_fp;
    size_t stacksize;
    int prio;
    long period;
    struct timespec nextstart;
    unsigned ratio;
    long pll_correction;
    long pll_correction_limit;
    void *arg;
    void (*taskcode)(void*);
};

#define MAX_TASKS  64
#define TASK_MAGIC    21979
#define TASK_MAGIC_INIT   ((struct rtapi_task*)(-1))

/* Declarations for compatibility with uspace_common.h */
static uid_t euid, ruid;

/* Helper function for rtapi_timespec_less */
static int rtapi_timespec_less(const struct timespec ta, const struct timespec tb) {
    if(ta.tv_sec < tb.tv_sec) return 1;
    if(ta.tv_sec > tb.tv_sec) return 0;
    return ta.tv_nsec < tb.tv_nsec;
}

/* Forward declaration of rtapi_timespec_advance */
void rtapi_timespec_advance(struct timespec *result, const struct timespec *src, unsigned long nsec);
static _Atomic int with_root_level = 0;

static void with_root_enter(void) {
    if(atomic_fetch_add(&with_root_level, 1) == 0) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

static void with_root_exit(void) {
    if(atomic_fetch_sub(&with_root_level, 1) == 1) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

void __attribute__((constructor)) init_root_func(void) {
    euid = geteuid();
    ruid = getuid();
}

#include "rtapi/uspace_common.h"

/* Module table */
#define MAX_MODULES 64
struct module_entry {
    char name[256];
    void *handle;
    int in_use;
};
static struct module_entry modules[MAX_MODULES];
static pthread_mutex_t modules_lock = PTHREAD_MUTEX_INITIALIZER;

/* Message queue */
#define MSG_QUEUE_SIZE 128
struct message_t {
    msg_level_t level;
    char msg[1024];
};
static struct message_t msg_queue[MSG_QUEUE_SIZE];
static _Atomic int msg_head = 0;
static _Atomic int msg_tail = 0;

static void msg_queue_push(msg_level_t level, const char *msg) {
    int head = atomic_load_explicit(&msg_head, memory_order_relaxed);
    int next = (head + 1) % MSG_QUEUE_SIZE;
    
    /* Check if queue is full (don't block, just drop) */
    if(next == atomic_load_explicit(&msg_tail, memory_order_acquire)) {
        return;  /* Queue full, message dropped */
    }
    
    /* Write the message */
    msg_queue[head].level = level;
    snprintf(msg_queue[head].msg, sizeof(msg_queue[head].msg), "%s", msg);
    
    /* Publish the new head (release ensures msg is visible before head update) */
    atomic_store_explicit(&msg_head, next, memory_order_release);
}

static int msg_queue_consume_all(void) {
    int processed = 0;
    int tail = atomic_load_explicit(&msg_tail, memory_order_relaxed);
    
    while(tail != atomic_load_explicit(&msg_head, memory_order_acquire)) {
        /* Copy message to local buffer before updating tail */
        msg_level_t level = msg_queue[tail].level;
        char msg_copy[sizeof(msg_queue[tail].msg)];
        strncpy(msg_copy, msg_queue[tail].msg, sizeof(msg_copy) - 1);
        msg_copy[sizeof(msg_copy) - 1] = '\0';
        
        /* Move tail forward after reading the message data */
        int next_tail = (tail + 1) % MSG_QUEUE_SIZE;
        atomic_store_explicit(&msg_tail, next_tail, memory_order_release);
        tail = next_tail;
        
        /* Now output the message (safe because we copied it) */
        fputs(msg_copy, level == RTAPI_MSG_ALL ? stdout : stderr);
        processed++;
    }
    return processed;
}

static void set_namef(const char *fmt, ...) {
    char *buf = NULL;
    va_list ap;

    va_start(ap, fmt);
    if (vasprintf(&buf, fmt, ap) < 0) {
        va_end(ap);
        return;
    }
    va_end(ap);

    int res = pthread_setname_np(pthread_self(), buf);
    if (res) {
        fprintf(stderr, "pthread_setname_np() failed for %s: %d\n", buf, res);
    }
    free(buf);
}

static pthread_t queue_thread;
static void *queue_function(void *arg) {
    (void)arg;
    set_namef("rtapi_app:mesg");
    while(1) {
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        msg_queue_consume_all();
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        struct timespec ts = {0, 10000000};
        rtapi_clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL, NULL);
    }
    return NULL;
}

static int sim_rtapi_run_threads(int fd, int (*callback)(int fd));

static void *dlsym_helper(void *handle, const char *name) {
    return dlsym(handle, name);
}

static int instance_count = 0;
static int force_exit = 0;

static void *find_module(const char *name) {
    pthread_mutex_lock(&modules_lock);
    for(int i = 0; i < MAX_MODULES; i++) {
        if(modules[i].in_use && strcmp(modules[i].name, name) == 0) {
            void *handle = modules[i].handle;
            pthread_mutex_unlock(&modules_lock);
            return handle;
        }
    }
    pthread_mutex_unlock(&modules_lock);
    return NULL;
}

static int add_module(const char *name, void *handle) {
    pthread_mutex_lock(&modules_lock);
    for(int i = 0; i < MAX_MODULES; i++) {
        if(!modules[i].in_use) {
            strncpy(modules[i].name, name, sizeof(modules[i].name) - 1);
            modules[i].name[sizeof(modules[i].name) - 1] = '\0';
            modules[i].handle = handle;
            modules[i].in_use = 1;
            pthread_mutex_unlock(&modules_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&modules_lock);
    return -1;
}

static void remove_module(const char *name) {
    pthread_mutex_lock(&modules_lock);
    for(int i = 0; i < MAX_MODULES; i++) {
        if(modules[i].in_use && strcmp(modules[i].name, name) == 0) {
            modules[i].in_use = 0;
            modules[i].name[0] = '\0';
            modules[i].handle = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&modules_lock);
}

static int do_newinst_cmd(const char *type, const char *name, const char *arg) {
    void *module = find_module("hal_lib");
    if(!module) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "newinst: hal_lib is required, but not loaded\n");
        return -1;
    }

    hal_comp_t *(*find_comp_by_name)(char*) =
        (hal_comp_t*(*)(char *))dlsym_helper(module, "halpr_find_comp_by_name");
    if(!find_comp_by_name) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "newinst: halpr_find_comp_by_name not found\n");
        return -1;
    }

    hal_comp_t *comp = find_comp_by_name((char*)type);
    if(!comp) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "newinst: component %s not found\n", type);
        return -1;
    }

    return comp->make((char*)name, (char*)arg);
}

static int do_one_item(char item_type_char, const char *param_name, const char *param_value, void *vitem, int idx) {
    char *endp;
    switch(item_type_char) {
        case 'l': {
            long *litem = *(long**) vitem;
            litem[idx] = strtol(param_value, &endp, 0);
            if(*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                        "`%s' invalid for parameter `%s'",
                        param_value, param_name);
                return -1;
            }
            return 0;
        }
        case 'i': {
            int *iitem = *(int**) vitem;
            iitem[idx] = strtol(param_value, &endp, 0);
            if(*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                        "`%s' invalid for parameter `%s'",
                        param_value, param_name);
                return -1;
            }
            return 0;
        }
        case 's': {
            char **sitem = *(char***) vitem;
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

static void remove_quotes(char *s) {
    char *src = s;
    char *dst = s;
    while(*src) {
        if(*src != '"') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

#define MAX_ARGS 64
static int do_comp_args(void *module, char **args, int nargs) {
    for(int i = 1; i < nargs; i++) {
        char *s = args[i];
        remove_quotes(s);
        char *eq = strchr(s, '=');
        if(!eq) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid parameter `%s'\n", s);
            return -1;
        }
        *eq = '\0';
        char *param_name = s;
        char *param_value = eq + 1;
        
        char sym_name[512];
        snprintf(sym_name, sizeof(sym_name), "rtapi_info_address_%s", param_name);
        void *item = dlsym_helper(module, sym_name);
        if(!item) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "Unknown parameter `%s'\n", param_name);
            return -1;
        }
        
        snprintf(sym_name, sizeof(sym_name), "rtapi_info_type_%s", param_name);
        char **item_type = (char**)dlsym_helper(module, sym_name);
        if(!item_type || !*item_type) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "Unknown parameter `%s' (type information missing)\n",
                    param_name);
            return -1;
        }

        snprintf(sym_name, sizeof(sym_name), "rtapi_info_size_%s", param_name);
        int *max_size_ptr = (int*)dlsym_helper(module, sym_name);

        char item_type_char = **item_type;
        if(max_size_ptr) {
            int max_size = *max_size_ptr;
            char *tok = param_value;
            int idx = 0;
            while(tok && *tok) {
                if(idx == max_size) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                            "%s: can only take %d arguments\n",
                            param_name, max_size);
                    return -1;
                }
                char *comma = strchr(tok, ',');
                char substr[256];
                if(comma) {
                    size_t len = comma - tok;
                    if(len >= sizeof(substr)) len = sizeof(substr) - 1;
                    strncpy(substr, tok, len);
                    substr[len] = '\0';
                    tok = comma + 1;
                } else {
                    strncpy(substr, tok, sizeof(substr) - 1);
                    substr[sizeof(substr) - 1] = '\0';
                    tok = NULL;
                }
                int result = do_one_item(item_type_char, param_name, substr, item, idx);
                if(result != 0) return result;
                idx++;
            }
        } else {
            int result = do_one_item(item_type_char, param_name, param_value, item, 0);
            if(result != 0) return result;
        }
    }
    return 0;
}

static int do_load_cmd(const char *name, char **args, int nargs) {
    void *w = find_module(name);
    if(w == NULL) {
        char what[LINELEN+1];
        snprintf(what, LINELEN, "%s/%s.so", EMC2_RTLIB_DIR, name);
        void *module = dlopen(what, RTLD_GLOBAL | RTLD_NOW);
        if(!module) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlopen: %s\n", name, dlerror());
            return -1;
        }
        
        int (*start)(void) = (int(*)(void))dlsym_helper(module, "rtapi_app_main");
        if(!start) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", name, dlerror());
            dlclose(module);
            return -1;
        }
        
        int result = do_comp_args(module, args, nargs);
        if(result < 0) {
            dlclose(module);
            return -1;
        }

        if ((result = start()) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %s (%d)\n",
                name, strerror(-result), result);
            dlclose(module);
            return result;
        }
        
        if(add_module(name, module) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: too many modules\n", name);
            dlclose(module);
            return -1;
        }
        
        instance_count++;
        return 0;
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: already exists\n", name);
        return -1;
    }
}

static int do_unload_cmd(const char *name) {
    void *w = find_module(name);
    if(w == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: not loaded\n", name);
        return -1;
    } else {
        int (*stop)(void) = (int(*)(void))dlsym_helper(w, "rtapi_app_exit");
        if(stop) stop();
        remove_module(name);
        dlclose(w);
        instance_count--;
    }
    return 0;
}

static int read_number(int fd) {
    int r = 0, neg = 1;
    char ch;

    while(1) {
        int res = read(fd, &ch, 1);
        if(res != 1) return -1;
        if(ch == '-') neg = -1;
        else if(ch == ' ') return r * neg;
        else r = 10 * r + ch - '0';
    }
}

static int read_string(int fd, char *buf, int maxlen) {
    int len = read_number(fd);
    if(len < 0 || len >= maxlen) return -1;
    if(read(fd, buf, len) != len) return -1;
    buf[len] = '\0';
    return len;
}

static int read_strings(int fd, char **args, int maxargs) {
    int count = read_number(fd);
    if(count < 0 || count > maxargs) return -1;
    
    for(int i = 0; i < count; i++) {
        args[i] = malloc(1024);
        if(!args[i]) {
            for(int j = 0; j < i; j++) free(args[j]);
            return -1;
        }
        if(read_string(fd, args[i], 1024) < 0) {
            for(int j = 0; j <= i; j++) free(args[j]);
            return -1;
        }
    }
    return count;
}

static void write_number(char *buf, int *pos, int bufsize, int num) {
    char numbuf[32];
    snprintf(numbuf, sizeof(numbuf), "%d ", num);
    int len = strlen(numbuf);
    if(*pos + len < bufsize) {
        strcpy(buf + *pos, numbuf);
        *pos += len;
    }
}

static void write_string(char *buf, int *pos, int bufsize, const char *s) {
    write_number(buf, pos, bufsize, strlen(s));
    int len = strlen(s);
    if(*pos + len < bufsize) {
        memcpy(buf + *pos, s, len);
        *pos += len;
    }
}

static int write_strings(int fd, char **strings, int count) {
    char buf[8192];
    int pos = 0;
    write_number(buf, &pos, sizeof(buf), count);
    for(int i = 0; i < count; i++) {
        write_string(buf, &pos, sizeof(buf), strings[i]);
    }
    return write(fd, buf, pos) == pos ? 0 : -1;
}

static int handle_command(char **args, int nargs) {
    if(nargs == 0) { return 0; }
    if(nargs == 1 && strcmp(args[0], "exit") == 0) {
        force_exit = 1;
        return 0;
    } else if(nargs >= 2 && strcmp(args[0], "load") == 0) {
        return do_load_cmd(args[1], args + 1, nargs - 1);
    } else if(nargs == 2 && strcmp(args[0], "unload") == 0) {
        return do_unload_cmd(args[1]);
    } else if(nargs == 3 && strcmp(args[0], "newinst") == 0) {
        return do_newinst_cmd(args[1], args[2], "");
    } else if(nargs == 4 && strcmp(args[0], "newinst") == 0) {
        return do_newinst_cmd(args[1], args[2], args[3]);
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "Unrecognized command starting with %s\n",
                args[0]);
        return -1;
    }
}

static int slave(int fd, char **args, int nargs) {
    if(write_strings(fd, args, nargs) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_app: failed to write to master: %s\n", strerror(errno));
        return -1;
    }

    int result = read_number(fd);
    return result;
}

static int callback(int fd)
{
    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t len = sizeof(client_addr);
    int fd1 = accept(fd, (struct sockaddr*)&client_addr, &len);
    if(fd1 < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_app: failed to accept connection from slave: %s\n", strerror(errno));
        return -1;
    }
    
    char *args[MAX_ARGS];
    int nargs = read_strings(fd1, args, MAX_ARGS);
    int result;
    
    if(nargs < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_app: failed to read from slave: %s\n", strerror(errno));
        close(fd1);
        return -1;
    }
    
    result = handle_command(args, nargs);
    
    for(int i = 0; i < nargs; i++) {
        free(args[i]);
    }
    
    char buf[32];
    int pos = 0;
    write_number(buf, &pos, sizeof(buf), result);
    if(write(fd1, buf, pos) != pos) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_app: failed to write to slave: %s\n", strerror(errno));
    }
    close(fd1);
    
    return !force_exit && instance_count > 0;
}

static pthread_t main_thread;

static int master(int fd, char **args, int nargs) {
    main_thread = pthread_self();
    int result;
    if((result = pthread_create(&queue_thread, NULL, &queue_function, NULL)) != 0) {
        errno = result;
        perror("pthread_create (queue function)");
        return -1;
    }
    
    char *hal_lib_args[] = {"hal_lib"};
    do_load_cmd("hal_lib", hal_lib_args, 1);
    instance_count = 0;
    
    if(nargs) {
        result = handle_command(args, nargs);
        if(result != 0) goto out;
        if(force_exit || instance_count == 0) goto out;
    }
    sim_rtapi_run_threads(fd, callback);
out:
    pthread_cancel(queue_thread);
    pthread_join(queue_thread, NULL);
    msg_queue_consume_all();
    return result;
}

static const char *get_fifo_path_internal(void) {
    static char path[512] = {0};
    if(path[0] != '\0') return path;
    
    const char *env_path = getenv("RTAPI_FIFO_PATH");
    if(env_path) {
        strncpy(path, env_path, sizeof(path) - 1);
    } else {
        const char *home = getenv("HOME");
        if(home) {
            snprintf(path, sizeof(path), "%s/.rtapi_fifo", home);
        } else {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "rtapi_app: RTAPI_FIFO_PATH and HOME are unset. rtapi fifo creation is unsafe.");
            return NULL;
        }
    }
    
    if(strlen(path) + 1 > sizeof(((struct sockaddr_un*)0)->sun_path)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_app: rtapi fifo path is too long (arch limit %zd): %s",
                sizeof(((struct sockaddr_un*)0)->sun_path), path);
        return NULL;
    }
    return path;
}

static const char *get_fifo_path(void) {
    return get_fifo_path_internal();
}

static int get_fifo_path_buf(char *buf, size_t bufsize) {
    const char *s = get_fifo_path();
    if(!s) return -1;
    snprintf(buf, bufsize, "%s", s);
    return 0;
}

int rtapi_become_master(char **args, int nargs) {
  while (1) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) { perror("socket"); exit(1); }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if(get_fifo_path_buf(addr.sun_path, sizeof(addr.sun_path)) < 0)
       exit(1);
    int result = bind(fd, (struct sockaddr*)&addr, sizeof(addr));

    if(result == 0) {
        int result = listen(fd, 10);
        if(result != 0) { perror("listen"); exit(1); }
        setsid();
        result = master(fd, args, nargs);
        unlink(get_fifo_path());
        return result;
    } else if(errno == EADDRINUSE) {
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);
        gettimeofday(&t1, NULL);
        for(int i = 0; i < 3 || (t1.tv_sec < 3 + t0.tv_sec); i++) {
            result = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
            if(result == 0) break;
            if(i == 0) srand48(t0.tv_sec ^ t0.tv_usec);
            usleep(lrand48() % 100000);
            gettimeofday(&t1, NULL);
        }
        if(result < 0 && errno == ECONNREFUSED) {
            unlink(get_fifo_path());
            fprintf(stderr, "Waited 3 seconds for master. Giving up.\n");
            close(fd);
            continue;
        }
        if(result < 0) { fprintf(stderr, "connect %s: %s\n", addr.sun_path, strerror(errno)); exit(1); }
        return slave(fd, args, nargs);
    } else {
        perror("bind"); exit(1);
    }
  }
}

int main(int argc, char **argv) {
    if(getuid() == 0) {
        char *fallback_uid_str = getenv("RTAPI_UID");
        int fallback_uid = fallback_uid_str ? atoi(fallback_uid_str) : 0;
        if(fallback_uid == 0)
        {
            fprintf(stderr,
                "Refusing to run as root without fallback UID specified\n"
                "To run under a debugger with I/O, use e.g.,\n"
                "    sudo env RTAPI_UID=`id -u` RTAPI_FIFO_PATH=$HOME/.rtapi_fifo gdb " EMC2_BIN_DIR "/rtapi_app\n");
            exit(1);
        }
        if (setreuid(fallback_uid, 0) != 0) { perror("setreuid"); abort(); }
        fprintf(stderr,
            "Running with fallback_uid.  getuid()=%d geteuid()=%d\n",
            getuid(), geteuid());
    }
    ruid = getuid();
    euid = geteuid();
    if (setresuid(euid, euid, ruid) != 0) { perror("setresuid"); abort(); }
#ifdef __linux__
    setfsuid(ruid);
#endif

    char *args[MAX_ARGS];
    int nargs = 0;
    for(int i = 1; i < argc && nargs < MAX_ARGS; i++) {
        args[nargs++] = argv[i];
    }

    return rtapi_become_master(args, nargs);
}


/* Task and RTAPI structures */

struct rtapi_module {
    int magic;
};

#define MODULE_MAGIC  30812
#define SHMEM_MAGIC   25453

#define MAX_MODULES_INT  64
#define MODULE_OFFSET 32768

struct posix_task {
    struct rtapi_task task;
    pthread_t thr;
};

/* Global application state */
static int app_policy = SCHED_FIFO;
static long app_period = 0;
static int do_thread_lock = 0;

static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static pthread_key_t task_key;
static void init_task_key(void) {
    pthread_key_create(&task_key, NULL);
}

static pthread_once_t lock_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t thread_lock;
static void init_thread_lock(void) {
    pthread_mutex_init(&thread_lock, NULL);
}

static void signal_handler(int sig, siginfo_t *si, void *uctx)
{
    (void)si;
    (void)uctx;
    switch (sig) {
    case SIGXCPU:
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "rtapi_app: BUG: SIGXCPU received - exiting\n");
        exit(0);
        break;

    case SIGTERM:
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "rtapi_app: SIGTERM - shutting down\n");
        exit(0);
        break;

    default:
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "rtapi_app: caught signal %d - dumping core\n", sig);
        sleep(1);
        signal(sig, SIG_DFL);
        raise(sig);
        break;
    }
    exit(1);
}

static const size_t PRE_ALLOC_SIZE = 1024*1024*32;
static const struct rlimit unlimited = {RLIM_INFINITY, RLIM_INFINITY};

static void configure_memory(void)
{
    int res = setrlimit(RLIMIT_MEMLOCK, &unlimited);
    if(res < 0) perror("setrlimit");

    res = mlockall(MCL_CURRENT | MCL_FUTURE);
    if(res < 0) perror("mlockall");

#ifdef __linux__
    if (!mallopt(M_TRIM_THRESHOLD, -1)) {
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "mallopt(M_TRIM_THRESHOLD, -1) failed\n");
    }
    if (!mallopt(M_MMAP_MAX, 0)) {
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "mallopt(M_MMAP_MAX, -1) failed\n");
    }
#endif
    char *buf = (char *)malloc(PRE_ALLOC_SIZE);
    if (buf == NULL) {
        rtapi_print_msg(RTAPI_MSG_WARN, "malloc(PRE_ALLOC_SIZE) failed\n");
        return;
    }
    long pagesize = sysconf(_SC_PAGESIZE);
    for (size_t i = 0; i < PRE_ALLOC_SIZE; i += pagesize) {
        buf[i] = 0;
    }
    free(buf);
}

static int harden_rt(void)
{
    if(!rtapi_is_realtime()) return -EINVAL;

    with_root_enter();
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
    if (iopl(3) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "iopl() failed: %s\n"
                        "cannot gain I/O privileges - "
                        "forgot 'sudo make setuid' or using secure boot? -"
                        "parallel port access is not allowed\n",
                        strerror(errno));
    }
#endif

    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(sig_act));
#ifdef __linux__
    if (setrlimit(RLIMIT_RTPRIO, &unlimited) < 0)
    {
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "setrlimit(RTLIMIT_RTPRIO): %s\n",
                  strerror(errno));
        with_root_exit();
        return -errno;
    }

    if (setrlimit(RLIMIT_CORE, &unlimited) < 0)
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "setrlimit: %s - core dumps may be truncated or non-existent\n",
                  strerror(errno));

    if (prctl(PR_SET_DUMPABLE, 1) < 0)
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "prctl(PR_SET_DUMPABLE) failed: no core dumps will be created - %d - %s\n",
                  errno, strerror(errno));
#endif

    configure_memory();

    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_handler = SIG_IGN;
    sig_act.sa_sigaction = NULL;

    sigaction(SIGTSTP, &sig_act, (struct sigaction *) NULL);

    sig_act.sa_sigaction = signal_handler;
    sig_act.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGILL,  &sig_act, (struct sigaction *) NULL);
    sigaction(SIGFPE,  &sig_act, (struct sigaction *) NULL);
    sigaction(SIGTERM, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGINT, &sig_act, (struct sigaction *) NULL);

#ifdef __linux__
    int fd = open("/dev/cpu_dma_latency", O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        rtapi_print_msg(RTAPI_MSG_WARN, "failed to open /dev/cpu_dma_latency: %s\n", strerror(errno));
    } else {
        int r;
        r = write(fd, "\0\0\0\0", 4);
        if (r != 4) {
            rtapi_print_msg(RTAPI_MSG_WARN, "failed to write to /dev/cpu_dma_latency: %s\n", strerror(errno));
        }
    }
#endif
    with_root_exit();
    return 0;
}

static void initialize_app(void)
{
    static int initialized = 0;
    if(initialized) return;
    initialized = 1;
    
    if(euid != 0 || harden_rt() < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using POSIX non-realtime\n");
        app_policy = SCHED_OTHER;
        do_thread_lock = 1;
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using POSIX realtime\n");
        app_policy = SCHED_FIFO;
        do_thread_lock = 0;
    }
    
    pthread_once(&key_once, init_task_key);
    if(do_thread_lock) {
        pthread_once(&lock_once, init_thread_lock);
    }
}

struct rtapi_task *task_array[MAX_TASKS];

static int prio_highest(void)
{
    return sched_get_priority_max(app_policy);
}

static int prio_lowest(void)
{
    return sched_get_priority_min(app_policy);
}

static int prio_higher_delta(void) {
    if(rtapi_prio_highest() > rtapi_prio_lowest()) {
        return 1;
    }
    return -1;
}

static int prio_bound(int prio) {
    if(rtapi_prio_highest() > rtapi_prio_lowest()) {
        if (prio >= rtapi_prio_highest())
            return rtapi_prio_highest();
        if (prio < rtapi_prio_lowest())
            return rtapi_prio_lowest();
    } else {
        if (prio <= rtapi_prio_highest())
            return rtapi_prio_highest();
        if (prio > rtapi_prio_lowest())
            return rtapi_prio_lowest();
    }
    return prio;
}

static int prio_next_higher(int prio)
{
    prio = prio_bound(prio);
    if(prio != rtapi_prio_highest())
        return prio + prio_higher_delta();
    return prio;
}

static int prio_next_lower(int prio)
{
    prio = prio_bound(prio);
    if(prio != rtapi_prio_lowest())
        return prio - prio_higher_delta();
    return prio;
}

static int allocate_task_id(void)
{
    for(int n = 0; n < MAX_TASKS; n++)
    {
        struct rtapi_task **taskptr = &(task_array[n]);
        if(__sync_bool_compare_and_swap(taskptr, (struct rtapi_task*)0, TASK_MAGIC_INIT))
            return n;
    }
    return -ENOSPC;
}

static struct rtapi_task *get_task(int task_id) {
    if(task_id < 0 || task_id >= MAX_TASKS) return NULL;
    struct rtapi_task *task = task_array[task_id];
    if(!task || task == TASK_MAGIC_INIT || task->magic != TASK_MAGIC)
        return NULL;
    return task;
}

static void unexpected_realtime_delay(struct rtapi_task *task, int nperiod) {
    static int printed = 0;
    (void)nperiod;
    if(!printed)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "Unexpected realtime delay on task %d with period %ld\n"
                "This Message will only display once per session.\n"
                "Run the Latency Test and resolve before continuing.\n",
                task->id, task->period);
        printed = 1;
    }
}

static int find_rt_cpu_number(void) {
    if(getenv("RTAPI_CPU_NUMBER")) return atoi(getenv("RTAPI_CPU_NUMBER"));

#ifdef __linux__
    cpu_set_t cpuset_orig;
    int r = sched_getaffinity(getpid(), sizeof(cpuset_orig), &cpuset_orig);
    if(r < 0)
        return 0;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    long top_probe = sysconf(_SC_NPROCESSORS_CONF);
    for(long i = 0; i < top_probe && i < CPU_SETSIZE; i++) CPU_SET(i, &cpuset);

    r = sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
    if(r < 0)
        perror("sched_setaffinity");

    r = sched_getaffinity(getpid(), sizeof(cpuset), &cpuset);
    if(r < 0) {
        perror("sched_getaffinity");
        CPU_AND(&cpuset, &cpuset_orig, &cpuset);
    }

    int top = -1;
    for(int i = 0; i < CPU_SETSIZE; i++) {
        if(CPU_ISSET(i, &cpuset)) top = i;
    }
    return top;
#else
    return -1;
#endif
}

static void *task_wrapper(void *arg);

static int task_start(int task_id, unsigned long int period_nsec)
{
    struct posix_task *task = (struct posix_task*)get_task(task_id);
    if(!task) return -EINVAL;

    if(period_nsec < (unsigned long)app_period) period_nsec = (unsigned long)app_period;
    task->task.period = period_nsec;
    task->task.ratio = period_nsec / app_period;

    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = task->task.prio;

    task->task.pll_correction_limit = period_nsec / 100;
    task->task.pll_correction = 0;

    int nprocs = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_attr_t attr;
    int ret;
    if((ret = pthread_attr_init(&attr)) != 0)
        return -ret;
    if((ret = pthread_attr_setstacksize(&attr, task->task.stacksize)) != 0)
        return -ret;
    if((ret = pthread_attr_setschedpolicy(&attr, app_policy)) != 0)
        return -ret;
    if((ret = pthread_attr_setschedparam(&attr, &param)) != 0)
        return -ret;
    if((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0)
        return -ret;
    if(nprocs > 1) {
        static int rt_cpu_number = -2;  /* -2 means uninitialized, call find_rt_cpu_number() */
        int cpu_num;
        if(rt_cpu_number == -2) {
            rt_cpu_number = find_rt_cpu_number();
        }
        cpu_num = rt_cpu_number;
        if(cpu_num != -1) {
#ifdef __FreeBSD__
            cpuset_t cpuset;
#else
            cpu_set_t cpuset;
#endif
            CPU_ZERO(&cpuset);
            CPU_SET(cpu_num, &cpuset);
            if((ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
                return -ret;
        }
    }
    if((ret = pthread_create(&task->thr, &attr, &task_wrapper, (void*)task)) != 0)
        return -ret;

    return 0;
}

#define RTAPI_CLOCK (CLOCK_MONOTONIC)

static void *task_wrapper(void *arg)
{
    struct posix_task *ptask = (struct posix_task*)arg;
    struct rtapi_task *task = &ptask->task;

    long int period = app_period;
    if(task->period < period) task->period = period;
    task->ratio = task->period / period;
    task->period = task->ratio * period;
    rtapi_print_msg(RTAPI_MSG_INFO, "task %p period = %lu ratio=%u\n",
          (void*)task, task->period, task->ratio);

    pthread_setspecific(task_key, arg);
    set_namef("rtapi_app:T#%d", task->id);

    if(do_thread_lock)
        pthread_mutex_lock(&thread_lock);

    struct timespec now;
    clock_gettime(RTAPI_CLOCK, &now);
    rtapi_timespec_advance(&task->nextstart, &now, task->period + task->pll_correction);

    (task->taskcode)(task->arg);

    rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
    return NULL;
}

static int task_delete(int id)
{
    struct posix_task *task = (struct posix_task*)get_task(id);
    if(!task) return -EINVAL;

    pthread_cancel(task->thr);
    pthread_join(task->thr, 0);
    task->task.magic = 0;
    task_array[id] = 0;
    free(task);
    return 0;
}

static int task_new(void (*taskcode)(void*), void *arg,
        int prio, int owner, unsigned long int stacksize, int uses_fp) {
    if ((prio > rtapi_prio_highest()) || (prio < rtapi_prio_lowest()))
    {
        return -EINVAL;
    }

    int n = allocate_task_id();
    if(n < 0) return n;

    struct posix_task *task = (struct posix_task*)malloc(sizeof(struct posix_task));
    if(!task) {
        task_array[n] = 0;
        return -ENOMEM;
    }
    memset(task, 0, sizeof(*task));
    
    if(stacksize < (1024*1024)) stacksize = (1024*1024);
    task->task.id = n;
    task->task.owner = owner;
    task->task.uses_fp = uses_fp;
    task->task.arg = arg;
    task->task.stacksize = stacksize;
    task->task.taskcode = taskcode;
    task->task.prio = prio;
    task->task.magic = TASK_MAGIC;
    task_array[n] = &task->task;

    return n;
}

static long long task_pll_get_reference(void) {
    struct rtapi_task *task = (struct rtapi_task*)pthread_getspecific(task_key);
    if(!task) return 0;
    return task->nextstart.tv_sec * 1000000000LL + task->nextstart.tv_nsec;
}

static int task_pll_set_correction(long value) {
    struct rtapi_task *task = (struct rtapi_task*)pthread_getspecific(task_key);
    if(!task) return -EINVAL;
    if (value > task->pll_correction_limit) value = task->pll_correction_limit;
    if (value < -(task->pll_correction_limit)) value = -(task->pll_correction_limit);
    task->pll_correction = value;
    return 0;
}

static int task_pause(int task_id) {
    (void)task_id;
    return -ENOSYS;
}

static int task_resume(int task_id) {
    (void)task_id;
    return -ENOSYS;
}

static int task_self(void) {
    struct rtapi_task *task = (struct rtapi_task*)pthread_getspecific(task_key);
    if(!task) return -EINVAL;
    return task->id;
}

static void task_wait(void) {
    if(do_thread_lock)
        pthread_mutex_unlock(&thread_lock);
    pthread_testcancel();
    struct rtapi_task *task = (struct rtapi_task*)pthread_getspecific(task_key);
    if(!task) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_wait called from non-task thread\n");
        if(do_thread_lock)
            pthread_mutex_lock(&thread_lock);
        return;
    }
    rtapi_timespec_advance(&task->nextstart, &task->nextstart, task->period + task->pll_correction);
    struct timespec now;
    clock_gettime(RTAPI_CLOCK, &now);
    if(rtapi_timespec_less(task->nextstart, now))
    {
        if(app_policy == SCHED_FIFO)
            unexpected_realtime_delay(task, 0);
    }
    else
    {
        int res = rtapi_clock_nanosleep(RTAPI_CLOCK, TIMER_ABSTIME, &task->nextstart, NULL, &now);
        if(res < 0) perror("clock_nanosleep");
    }
    if(do_thread_lock)
        pthread_mutex_lock(&thread_lock);
}

static unsigned char do_inb(unsigned int port)
{
#ifdef HAVE_SYS_IO_H
    return inb(port);
#else
    (void)port;
    return 0;
#endif
}

static void do_outb(unsigned char val, unsigned int port)
{
#ifdef HAVE_SYS_IO_H
    outb(val, port);
#else
    (void)val;
    (void)port;
#endif
}

static void do_delay(long ns) {
    struct timespec ts = {0, ns};
    rtapi_clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL, NULL);
}

static long long do_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static long clock_set_period(long nsecs)
{
    if(nsecs == 0) return app_period;
    if(app_period != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
        return -EINVAL;
    }
    app_period = nsecs;
    return app_period;
}

static int run_threads(int fd, int(*callback)(int fd)) {
    while(callback(fd)) { /* nothing */ }
    return 0;
}

int sim_rtapi_run_threads(int fd, int (*callback)(int fd)) {
    return run_threads(fd, callback);
}

/* Public API functions */

int rtapi_prio_highest(void)
{
    initialize_app();
    return prio_highest();
}

int rtapi_prio_lowest(void)
{
    initialize_app();
    return prio_lowest();
}

int rtapi_prio_next_higher(int prio)
{
    initialize_app();
    return prio_next_higher(prio);
}

int rtapi_prio_next_lower(int prio)
{
    initialize_app();
    return prio_next_lower(prio);
}

long rtapi_clock_set_period(long nsecs)
{
    initialize_app();
    return clock_set_period(nsecs);
}

int rtapi_task_new(void (*taskcode)(void*), void *arg,
        int prio, int owner, unsigned long int stacksize, int uses_fp) {
    initialize_app();
    return task_new(taskcode, arg, prio, owner, stacksize, uses_fp);
}

int rtapi_task_delete(int id) {
    return task_delete(id);
}

int rtapi_task_start(int task_id, unsigned long period_nsec)
{
    int ret = task_start(task_id, period_nsec);
    if(ret != 0) {
        errno = -ret;
        perror("rtapi_task_start()");
    }
    return ret;
}

int rtapi_task_pause(int task_id)
{
    return task_pause(task_id);
}

int rtapi_task_resume(int task_id)
{
    return task_resume(task_id);
}

int rtapi_task_self(void)
{
    return task_self();
}

long long rtapi_task_pll_get_reference(void)
{
    return task_pll_get_reference();
}

int rtapi_task_pll_set_correction(long value)
{
    return task_pll_set_correction(value);
}

void rtapi_wait(void)
{
    task_wait();
}

void rtapi_outb(unsigned char byte, unsigned int port)
{
    do_outb(byte, port);
}

unsigned char rtapi_inb(unsigned int port)
{
    return do_inb(port);
}

long int simple_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}

long long rtapi_get_time(void) {
    return do_get_time();
}

void default_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap) {
    if(main_thread && pthread_self() != main_thread) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        msg_queue_push(level, buf);
    } else {
        vfprintf(level == RTAPI_MSG_ALL ? stdout : stderr, fmt, ap);
    }
}

long int rtapi_delay_max(void) { return 10000; }

void rtapi_delay(long ns) {
    if(ns > rtapi_delay_max()) ns = rtapi_delay_max();
    do_delay(ns);
}

const unsigned long ONE_SEC_IN_NS = 1000000000;
void rtapi_timespec_advance(struct timespec *result, const struct timespec *src, unsigned long nsec)
{
    time_t sec = src->tv_sec;
    while(nsec >= ONE_SEC_IN_NS)
    {
        ++sec;
        nsec -= ONE_SEC_IN_NS;
    }
    nsec += src->tv_nsec;
    if(nsec >= ONE_SEC_IN_NS)
    {
        ++sec;
        nsec -= ONE_SEC_IN_NS;
    }
    result->tv_sec = sec;
    result->tv_nsec = nsec;
}

int rtapi_open_as_root(const char *filename, int mode) {
    with_root_enter();
    int r = open(filename, mode);
    with_root_exit();
    if(r < 0) return -errno;
    return r;
}

int rtapi_spawn_as_root(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[], char *const envp[])
{
    return posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

int rtapi_spawnp_as_root(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[], char *const envp[])
{
    return posix_spawnp(pid, path, file_actions, attrp, argv, envp);
}
