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

#include <sys/types.h>
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
#include <link.h>
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
#include "rtapi_task.h"
#include "hal.h"
#include "hal/hal_priv.h"

/* Declarations for compatibility with uspace_common.h */
static uid_t euid = 0, ruid = 0;

/* Helper function for rtapi_timespec_less */
static int rtapi_timespec_less(const struct timespec ta, const struct timespec tb) {
    if(ta.tv_sec < tb.tv_sec) return 1;
    if(ta.tv_sec > tb.tv_sec) return 0;
    return ta.tv_nsec < tb.tv_nsec;
}

/* Forward declaration of rtapi_timespec_advance */
void rtapi_timespec_advance(struct timespec *result, const struct timespec *src, unsigned long nsec);

/* No-op stubs: under capabilities (cap_sys_nice, cap_ipc_lock, cap_sys_rawio),
 * euid == ruid so the old setfsuid() toggling is unnecessary. */
static void with_root_enter(void) {}
static void with_root_exit(void) {}

#include "rtapi/uspace_common.h"

void rtapi_set_namef(const char *fmt, ...) {
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
                        "BUG: SIGXCPU received - exiting\n");
        exit(0);
        break;

    default:
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "caught signal %d - dumping core\n", sig);
        sleep(1);
        signal(sig, SIG_DFL);
        raise(sig);
        break;
    }
    exit(1);
}

static const struct rlimit unlimited = {RLIM_INFINITY, RLIM_INFINITY};

/* Allocate memory suitable for realtime use: pre-fault + mlock. */
void *rtapi_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) return NULL;

    /* Pre-fault and lock all pages (read+write) */
    rtapi_lock_mem(p, malloc_usable_size(p), 1);

    return p;
}

void *rtapi_calloc(size_t size) {
    void *p = rtapi_malloc(size);
    if (!p) return NULL;

    memset(p, 0, size);

    return p;
}

void *rtapi_realloc(void *ptr, size_t size) {
    /* unlock old area */
    if (ptr) {
        rtapi_unlock_mem(ptr, malloc_usable_size(ptr));
    }

    void *p = realloc(ptr, size);
    if (!p) return NULL;

    /* Pre-fault and lock all pages (read+write) */
    rtapi_lock_mem(p, malloc_usable_size(p), 1);

    return p;
}

/* Free realtime-locked memory. */
void rtapi_free(void *p) {
    if (!p) return;
    rtapi_unlock_mem(p, malloc_usable_size(p));
    free(p);
}

/* pre-fault + mlock RT memory. */
int rtapi_lock_mem(void *p, size_t size, int prefault_rw) {
    int ret;

    /* Pre-fault all pages */
    long pagesize = sysconf(_SC_PAGESIZE);
    volatile char *c = (volatile char *)p;
    volatile char dummy;
    for (size_t i = 0; i < size; i += pagesize) {
        dummy = c[i];
        if (prefault_rw) {
          c[i] = dummy;
        }
    }
    /* Touch the last byte if size is not a multiple of pagesize */
    if (size > 0 && (size % (size_t)pagesize) != 0) {
        dummy = c[size - 1];
        if (prefault_rw) {
          c[size - 1] = dummy;
        }
    }
    (void)dummy;

    /* Lock into physical RAM */
    ret = mlock(p, size);
    if (ret < 0) {
        rtapi_print_msg(RTAPI_MSG_WARN,
            "rtapi_lock_mem: mlock(%zu) failed: %s\n", size, strerror(errno));
    }
    return ret;
}

/* unlock RT memory. */
void rtapi_unlock_mem(void *p, size_t size) {
    if (!p) return;
    munlock(p, size);
}

// Get the resolved path from a dlopen handle
static const char *dl_resolve_name(void *handle) {
    struct link_map *lm = NULL;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &lm) != 0 || !lm) {
        rtapi_print_msg(RTAPI_MSG_WARN,
            "rtapi: dlinfo failed: %s\n", dlerror());
        return NULL;
    }
    return lm->l_name;  // resolved path, e.g. "/usr/lib/libexample.so"
}

static int dl_mlock_callback(struct dl_phdr_info *info, size_t size, void *data) {
    const char *target = (const char *)data;

    if (!info->dlpi_name || strcmp(info->dlpi_name, target) != 0)
        return 0;

    // First pass: find the RELRO range (if any)
    uintptr_t relro_start = 0, relro_end = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (phdr->p_type == PT_GNU_RELRO) {
            relro_start = info->dlpi_addr + phdr->p_vaddr;
            relro_end   = relro_start + phdr->p_memsz;
            break;
        }
    }

    // Second pass: lock PT_LOAD segments
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (phdr->p_type != PT_LOAD)
            continue;

        uintptr_t seg_start = info->dlpi_addr + phdr->p_vaddr;
        size_t    seg_size  = phdr->p_memsz;
        uintptr_t seg_end   = seg_start + seg_size;

        int writable = (phdr->p_flags & PF_W) != 0;

        if (writable && relro_start) {
            // This segment may be partially or fully covered by RELRO.
            // Split into: RELRO part (read-only) and non-RELRO part (writable).

            // RELRO portion (made read-only by dynamic linker)
            if (relro_start < seg_end && relro_end > seg_start) {
                uintptr_t ro_start = relro_start > seg_start ? relro_start : seg_start;
                uintptr_t ro_end   = relro_end < seg_end ? relro_end : seg_end;
                rtapi_lock_mem((void *)ro_start, ro_end - ro_start, 0);
            }

            // Before RELRO (still writable)
            if (seg_start < relro_start) {
                uintptr_t end = relro_start < seg_end ? relro_start : seg_end;
                rtapi_lock_mem((void *)seg_start, end - seg_start, 1);
            }

            // After RELRO (still writable — .data, .bss)
            if (seg_end > relro_end) {
                uintptr_t start = relro_end > seg_start ? relro_end : seg_start;
                rtapi_lock_mem((void *)start, seg_end - start, 1);
            }
        } else {
            // No RELRO overlap — use ELF flags as-is
            rtapi_lock_mem((void *)seg_start, seg_size, writable);
        }
    }
    return 1;
}

static int dl_munlock_callback(struct dl_phdr_info *info, size_t size, void *data) {
    const char *target = (const char *)data;

    if (!info->dlpi_name || strcmp(info->dlpi_name, target) != 0)
        return 0;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (phdr->p_type != PT_LOAD)
            continue;

        void  *seg_addr = (void *)(info->dlpi_addr + phdr->p_vaddr);
        size_t seg_size = phdr->p_memsz;
        rtapi_unlock_mem(seg_addr, seg_size);
    }
    return 1;
}

void *rtapi_dlopen(const char *path, int flags) {
    void *handle = dlopen(path, flags);
    if (!handle) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "rtapi_dlopen: %s\n", dlerror());
        return NULL;
    }

    rtapi_lock_dl_handle(handle);

    return handle;
}

int rtapi_dlclose(void *handle) {
    if (handle) {
        rtapi_unlock_dl_handle(handle);
    }

    return dlclose(handle);
}

void rtapi_lock_dl_handle(void *handle) {
    if (!handle) return;
    const char *resolved = dl_resolve_name(handle);
    if (resolved) {
        dl_iterate_phdr(dl_mlock_callback, (void *)resolved);
    }
}

void rtapi_unlock_dl_handle(void *handle) {
    if (!handle) return;
    const char *resolved = dl_resolve_name(handle);
    if (resolved) {
        dl_iterate_phdr(dl_munlock_callback, (void *)resolved);
    }
}

static void configure_memory(void)
{
    /* Raise memlock rlimit — needed for mlockall(), per-region mlock() and SHM_LOCK */
    int res = setrlimit(RLIMIT_MEMLOCK, &unlimited);
    if(res < 0) perror("setrlimit");

    /* Memory locking strategy (Go-safe — no MCL_FUTURE):
     *   - Pre-loaded libs (libc, librtapi, vdso): mlockall(MCL_CURRENT) in rtapi_initialize_app()
     *   - HAL component .so files: rtapi_dlopen() locks PT_LOAD segments
     *   - SysV shmem segments: SHM_LOCK in rtapi_shmem_new()
     *   - Task structs: mlock() in rtapi_malloc()
     *   - Thread stacks: mlock() in task_wrapper()
     */

#ifdef __linux__
    /* Prevent glibc from returning C-side malloc pages to OS
     * (avoids page faults on reuse). Does not affect Go allocator. */
    if (!mallopt(M_TRIM_THRESHOLD, -1)) {
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "mallopt(M_TRIM_THRESHOLD, -1) failed\n");
    }
    if (!mallopt(M_MMAP_MAX, 0)) {
        rtapi_print_msg(RTAPI_MSG_WARN,
                  "mallopt(M_MMAP_MAX, 0) failed\n");
    }
#endif

    /* Lock all currently-mapped pages: libc, librtapi, ld-linux, vdso,
     * and initial Go runtime pages (~5-15 MB). MCL_CURRENT is a one-shot
     * snapshot — it does NOT affect future allocations (no MCL_FUTURE),
     * so the Go heap can still grow freely. HAL component .so files
     * loaded later are covered by rtapi_dlopen(). */
    if (mlockall(MCL_CURRENT) < 0) {
        rtapi_print_msg(RTAPI_MSG_WARN,
            "mlockall(MCL_CURRENT) failed: %s\n", strerror(errno));
    }
}

static int harden_rt(void)
{
    /* Initialize euid/ruid here; used by uspace_common.h for shmem ownership. */
    euid = geteuid();
    ruid = getuid();

    /* With setcap-based privileges (cap_sys_nice, cap_ipc_lock, cap_sys_rawio)
     * we no longer need setuid or root.  Capabilities are inherited by the
     * process, so iopl/mlockall/SCHED_FIFO work without uid juggling. */

#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
    if (iopl(3) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "iopl() failed: %s\n"
                        "cannot gain I/O privileges - "
                        "missing cap_sys_rawio capability or using secure boot? -"
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
    /* SIGTERM and SIGINT are handled by the Go runtime / launcher;
     * do not override them here to avoid conflicting handlers. */

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
    return 0;
}

void rtapi_initialize_app(void)
{
    static int initialized = 0;
    if(initialized) return;
    initialized = 1;
    
    if(harden_rt() < 0) {
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

#ifdef __linux__
/* Parse /sys/devices/system/cpu/isolated (cpulist format) and populate
 * the cpu_set_t pointed to by 'isolated'.  Returns the highest CPU number
 * found, or -1 if the file does not exist, is empty, or contains no valid
 * entries.  The cpulist format is comma-separated tokens where each token
 * is either a single number "N" or a range "N-M". */
static int parse_isolated_cpus(cpu_set_t *isolated) {
    CPU_ZERO(isolated);

    FILE *f = fopen("/sys/devices/system/cpu/isolated", "r");
    if(!f) return -1;

    char buf[4096];
    if(!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    int top = -1;
    char *p = buf;
    while(*p) {
        /* skip whitespace */
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if(!*p) break;

        /* parse first number */
        char *end;
        long a = strtol(p, &end, 10);
        if(end == p) break; /* no digits found */
        p = end;

        if(*p == '-') {
            /* range N-M: mark every CPU in [a, b] */
            p++;
            long b = strtol(p, &end, 10);
            if(end == p) break;
            p = end;
            /* skip range entirely if start is out of bounds */
            if(a < CPU_SETSIZE) {
                if(b >= CPU_SETSIZE) b = CPU_SETSIZE - 1;
                for(long j = a; j <= b; j++) CPU_SET(j, isolated);
                if(b > top) top = (int)b;
            }
        } else {
            if(a < CPU_SETSIZE) {
                CPU_SET(a, isolated);
                if(a > top) top = (int)a;
            }
        }

        /* skip comma separator */
        if(*p == ',') p++;
    }
    return top;
}
#endif

static int find_rt_cpu_number(void) {
    if(getenv("RTAPI_CPU_NUMBER")) return atoi(getenv("RTAPI_CPU_NUMBER"));

#ifdef __linux__
    /* Read the current process affinity mask — read-only, no sched_setaffinity. */
    cpu_set_t cpuset;
    int r = sched_getaffinity(getpid(), sizeof(cpuset), &cpuset);
    if(r < 0)
        return -1;

    /* Prefer isolated CPUs: find the highest isolated CPU that is also
     * present in our affinity mask. */
    cpu_set_t isolated;
    int isolated_top = parse_isolated_cpus(&isolated);
    if(isolated_top >= 0) {
        for(int i = isolated_top; i >= 0; i--) {
            if(CPU_ISSET(i, &isolated) && CPU_ISSET(i, &cpuset)) return i;
        }
    }

    /* Fallback: return the highest CPU in the process affinity mask. */
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
    void *stack_lockaddr = NULL;
    size_t stack_locksize = 0;

    /* Lock our own stack into RAM — must happen before any RT work.
     * Uses pthread_self() so there is no race with the parent thread. */
#ifdef __linux__
    {
        pthread_attr_t self_attr;
        void *stackaddr;
        size_t stacksize, guardsize;
        if (pthread_getattr_np(pthread_self(), &self_attr) == 0) {
            if (pthread_attr_getstack(&self_attr, &stackaddr, &stacksize) == 0
                && pthread_attr_getguardsize(&self_attr, &guardsize) == 0) {
                /* Skip guard page(s) at the bottom — they are PROT_NONE,
                 * mlock() on them would fail with ENOMEM. */
                stack_lockaddr = (char*)stackaddr + guardsize;
                stack_locksize = stacksize - guardsize;

                /* Pre-fault and lock all pages (read+write) */
                rtapi_lock_mem(stack_lockaddr, stack_locksize, 1);
            }
            pthread_attr_destroy(&self_attr);
        }
    }
#endif

    long int period = app_period;
    if(task->period < period) task->period = period;
    task->ratio = task->period / period;
    task->period = task->ratio * period;
    rtapi_print_msg(RTAPI_MSG_INFO, "task %p period = %lu ratio=%u\n",
          (void*)task, task->period, task->ratio);

    pthread_setspecific(task_key, arg);
    rtapi_set_namef("rtapi:T#%d", task->id);

    if(do_thread_lock)
        pthread_mutex_lock(&thread_lock);

    struct timespec now;
    clock_gettime(RTAPI_CLOCK, &now);
    rtapi_timespec_advance(&task->nextstart, &now, task->period + task->pll_correction);

    (task->taskcode)(task->arg);

#ifdef __linux__
    /* Pre-fault and lock all pages (read+write) */
    rtapi_unlock_mem(stack_lockaddr, stack_locksize);
#endif

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
    rtapi_free(task);
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

    struct posix_task *task = (struct posix_task*)rtapi_malloc(sizeof(struct posix_task));
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

/* Public API functions */

int rtapi_prio_highest(void)
{
    return prio_highest();
}

int rtapi_prio_lowest(void)
{
    return prio_lowest();
}

int rtapi_prio_next_higher(int prio)
{
    return prio_next_higher(prio);
}

int rtapi_prio_next_lower(int prio)
{
    return prio_next_lower(prio);
}

long rtapi_clock_set_period(long nsecs)
{
    return clock_set_period(nsecs);
}

int rtapi_task_new(void (*taskcode)(void*), void *arg,
        int prio, int owner, unsigned long int stacksize, int uses_fp) {
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
    if(level == RTAPI_MSG_ALL) {
	vfprintf(stdout, fmt, ap);
        fflush(stdout);
    } else {
	vfprintf(stderr, fmt, ap);
        fflush(stderr);
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
    int r = open(filename, mode);
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
