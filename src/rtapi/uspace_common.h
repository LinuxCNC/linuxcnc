// Description:  uspace_common.h
//              Shared methods used by various uspace modules.  Only
//              included once in any module to avoid conflicting
//              definitions.
//
//    Copyright 2006-2021, various authors
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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <stdlib.h>

#include <rtapi_errno.h>
#include <rtapi_mutex.h>
static msg_level_t msg_level = RTAPI_MSG_ERR;	/* message printing level */

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

#include "config.h"

#ifdef RTAPI
#include "uspace_rtapi_app.hh"
#endif

typedef struct {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int id;			/* OS identifier for shmem */
  int count;                    /* count of maps in this process */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
} rtapi_shmem_handle;

#define MAX_SHM 64

#define SHMEM_MAGIC   25453	/* random numbers used as signatures */

static rtapi_shmem_handle shmem_array[MAX_SHM];

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
#ifdef RTAPI
  WITH_ROOT;
#endif
  (void)module_id;
  rtapi_shmem_handle *shmem;
  int i;

  for (i=0,shmem=0 ; i < MAX_SHM; i++) {
    if(shmem_array[i].magic == SHMEM_MAGIC) {
      if (shmem_array[i].key == key) {
        shmem_array[i].count ++;
        return i;
      }
    }
    else if (!shmem) {
      shmem = &shmem_array[i];
    }
  }
  if (!shmem) {
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new failed due to MAX_SHM\n");
    return -ENOMEM;
  }

  /* now get shared memory block from OS */
  int shmget_retries = 5;
shmget_again:
  shmem->id = shmget((key_t) key, (int) size, IPC_CREAT | 0600);
  if (shmem->id == -1) {
      // See below for explanation of why retry against -EPERM here
      if(shmget_retries-- && errno == -EPERM) {
          sched_yield();
          goto shmget_again;
      }
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new failed due to shmget(key=0x%08x): %s\n", key, strerror(errno));
    return -errno;
  }

  struct shmid_ds stat;
  int res = shmctl(shmem->id, IPC_STAT, &stat);
  if(res < 0) perror("shmctl IPC_STAT");

#ifdef RTAPI
  /* At present, setuid rtapi_app runs with geteuid() == 0 at all times but the
   * fsuid is ruid except when WITH_ROOT when it's 0.
   *
   * Filesystem operations such as creat() respect the fsuid, but as shmget is
   * not a filesystem operation, it does not respect the fsuid. So, if
   * rtapi_app has created the segment in question, its owning uid is root.
   * Changing the permission here is racy, but it is the best alternative
   * currently available.
   *
   * The race causes a low probability (<1/1000 in testing in a VM) chance of
   * linuxcnc/halrun to fail to start
   */
  /* ensure the segment is owned by user, not root */
  if(geteuid() == 0) {
    stat.shm_perm.uid = WithRoot::getRuid();
    res = shmctl(shmem->id, IPC_SET, &stat);
    if(res < 0) perror("shmctl IPC_SET");
  }

#ifndef __FreeBSD__ // FreeBSD doesn't implement SHM_LOCK
  if(rtapi_is_realtime())
  {
    /* ensure the segment is locked */
    res = shmctl(shmem->id, SHM_LOCK, NULL);
    if(res < 0) perror("shmctl IPC_LOCK");

    res = shmctl(shmem->id, IPC_STAT, &stat);
    if(res < 0) perror("shmctl IPC_STAT");
    if((stat.shm_perm.mode & SHM_LOCKED) != SHM_LOCKED)
      rtapi_print_msg(RTAPI_MSG_ERR,
          "shared memory segment not locked as requested\n");
  }
#endif
#endif

  /* and map it into process space */
  shmem->mem = shmat(shmem->id, 0, 0);
  if ((ssize_t) (shmem->mem) == -1) {
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new failed due to shmat()\n");
    return -errno;
  }

  long pagesize = sysconf(_SC_PAGESIZE);
  /* touch every page */
  for(size_t off = 0; off < size; off += pagesize)
  {
      volatile char i = ((char*)shmem->mem)[off];
      (void)i;
  }

  /* label as a valid shmem structure */
  shmem->magic = SHMEM_MAGIC;
  /* fill in the other fields */
  shmem->size = size;
  shmem->key = key;
  shmem->count = 1;

  /* return handle to the caller */
  return shmem - shmem_array;
}


int rtapi_shmem_getptr(int handle, void **ptr)
{
  rtapi_shmem_handle *shmem;
  if(handle < 0 || handle >= MAX_SHM)
    return -EINVAL;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return -EINVAL;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return 0;
}


int rtapi_shmem_delete(int handle, int module_id)
{
  struct shmid_ds d;
  int r1, r2;
  rtapi_shmem_handle *shmem;
  (void)module_id;

  if(handle < 0 || handle >= MAX_SHM)
    return -EINVAL;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return -EINVAL;

  shmem->count --;
  if(shmem->count) return 0;

  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);

  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_STAT, &d);
  if (r2 != 0)
      rtapi_print_msg(RTAPI_MSG_ERR, "shmctl(%d, IPC_STAT, ...): %s\n", shmem->id, strerror(errno));

  if(r2 == 0 && d.shm_nattch == 0) {
      r2 = shmctl(shmem->id, IPC_RMID, &d);
      if (r2 != 0)
	      rtapi_print_msg(RTAPI_MSG_ERR, "shmctl(%d, IPC_RMID, ...): %s\n", shmem->id, strerror(errno));
  }

  /* free the shmem structure */
  shmem->magic = 0;

  if ((r1 != 0) || (r2 != 0))
    return -EINVAL;
  return 0;
}




void default_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap);

static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

rtapi_msg_handler_t rtapi_get_msg_handler(void) {
    return rtapi_msg_handler;
}

void rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
    if(handler == NULL) rtapi_msg_handler = default_rtapi_msg_handler;
    else rtapi_msg_handler = handler;
}


void rtapi_print(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
    va_end(args);
}


void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
{
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	rtapi_msg_handler(level, fmt, args);
	va_end(args);
    }
}

int rtapi_snprintf(char *buffer, unsigned long int size, const char *msg, ...) {
    va_list args;
    int result;

    va_start(args, msg);
    /* call the normal library vnsprintf() */
    result = vsnprintf(buffer, size, msg, args);
    va_end(args);
    return result;
}

int rtapi_vsnprintf(char *buffer, unsigned long int size, const char *fmt,
	va_list args) {
    return vsnprintf(buffer, size, fmt, args);
}

int rtapi_set_msg_level(int level) {
    msg_level = (msg_level_t)level;
    return 0;
}

int rtapi_get_msg_level() {
    return msg_level;
}

#if defined(__i386) || defined(__amd64)
#define rdtscll(val) ((val) = __builtin_ia32_rdtsc())
#else
#define rdtscll(val) ((val) = rtapi_get_time())
#endif

long long rtapi_get_clocks(void)
{
    long long int retval;

    rdtscll(retval);
    return retval;
}

typedef struct {
    rtapi_mutex_t mutex;
    int           uuid;
} uuid_data_t;

#define UUID_KEY  0x48484c34 /* key for UUID for simulator */

static         int  uuid_mem_id = 0;
int rtapi_init(const char *modname)
{
    (void)modname;
    static uuid_data_t* uuid_data   = 0;
    static const   int  uuid_id     = 0;

    static char* uuid_shmem_base = 0;
    int retval,id;
    void *uuid_mem;

    uuid_mem_id = rtapi_shmem_new(UUID_KEY,uuid_id,sizeof(uuid_data_t));
    if (uuid_mem_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "rtapi_init: could not open shared memory for uuid\n");
        rtapi_exit(uuid_id);
        return -EINVAL;
    }
    retval = rtapi_shmem_getptr(uuid_mem_id,&uuid_mem);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "rtapi_init: could not access shared memory for uuid\n");
        rtapi_exit(uuid_id);
        return -EINVAL;
    }
    if (uuid_shmem_base == 0) {
        uuid_shmem_base =        (char *) uuid_mem;
        uuid_data       = (uuid_data_t *) uuid_mem;
    }
    rtapi_mutex_get(&uuid_data->mutex);
        uuid_data->uuid++;
        id = uuid_data->uuid;
    rtapi_mutex_give(&uuid_data->mutex);

    return id;
}

int rtapi_exit(int module_id)
{
  rtapi_shmem_delete(uuid_mem_id, module_id);
  return 0;
}

int rtapi_is_kernelspace() { return 0; }

#ifdef __linux__
// detect_preempt_rt() inspects uname for the PREEMPT_RT marker.  Used only
// for diagnostic warning at startup; callers must not gate behavior on
// the kernel string, since SCHED_FIFO on a PREEMPT_DYNAMIC kernel is still
// useful (better than SCHED_OTHER, worse than PREEMPT_RT).
static inline int detect_preempt_rt() {
    struct utsname u;
    if(uname(&u) < 0) return 0;
    return strcasestr(u.version, "PREEMPT RT") != 0
        || strcasestr(u.version, "PREEMPT_RT") != 0;
}
#else
static inline int detect_preempt_rt() {
    return 0;
}
#endif

// FIXME: detect_rtai/detect_xenomai/detect_xenomai_evl currently gate on
// setuid because the RTAI/Xenomai backends still need root for iopl()
// (RTAI) or RTDM device access (Xenomai/EVL).  Long-term these should
// probe the actual capability the way can_set_sched_fifo() does, paired
// with udev rules + a 'xenomai'/'evl' group; @hdiethelm has a follow-up
// planned.  Until then, an unprivileged user on a Xenomai kernel cannot
// claim the Xenomai backend, and falls back to the SCHED_FIFO probe.
static inline int has_setuid_root() {
    return geteuid() == 0;
}

#ifdef USPACE_RTAI
static int detect_rtai() {
    if(!has_setuid_root()) return 0;
    struct utsname u;
    uname(&u);
    return strcasestr (u.release, "-rtai") != 0;
}
#else
static int detect_rtai() {
    return 0;
}
#endif
#ifdef USPACE_XENOMAI
static int detect_xenomai() {
    if(!has_setuid_root()) return 0;
    struct stat sb;
    //Running xenomai has /proc/xenomai
    return stat("/proc/xenomai", &sb) == 0;
}
#else
static int detect_xenomai() {
    return 0;
}
#endif
#ifdef USPACE_XENOMAI_EVL
static int detect_xenomai_evl() {
    if(!has_setuid_root()) return 0;
    struct stat sb;
    //Running xenomai evl has /dev/evl but no /proc/xenomai
    return stat("/dev/evl", &sb) == 0;
}
#else
static int detect_xenomai_evl() {
    return 0;
}
#endif

// errno from the most recent sched_setscheduler(SCHED_FIFO) probe.  Zero
// when the probe succeeded or has not run yet.  Read via
// rtapi_sched_fifo_errno() from diagnostic code.
static int rtapi_sched_fifo_last_errno = 0;

// Success-probe for realtime scheduling: briefly try to set SCHED_FIFO on
// the calling thread and restore the previous policy.  Succeeds when the
// process holds CAP_SYS_NICE (file caps or setuid root) or has a matching
// RLIMIT_RTPRIO.  Works on any kernel, so the probe also covers the
// PREEMPT_RT-vs-stock distinction implicitly: if we can actually get
// SCHED_FIFO, the platform can deliver realtime, regardless of how.
static int can_set_sched_fifo(void) {
    struct sched_param old_param, probe_param;
    int old_policy = sched_getscheduler(0);
    if(old_policy < 0) {
        rtapi_sched_fifo_last_errno = errno;
        return 0;
    }
    if(sched_getparam(0, &old_param) < 0) {
        rtapi_sched_fifo_last_errno = errno;
        return 0;
    }

    memset(&probe_param, 0, sizeof(probe_param));
    probe_param.sched_priority = sched_get_priority_min(SCHED_FIFO);
    if(sched_setscheduler(0, SCHED_FIFO, &probe_param) < 0) {
        rtapi_sched_fifo_last_errno = errno;
        return 0;
    }

    // Best-effort restore; if this fails we are still on SCHED_FIFO at
    // minimum priority, which is no worse than where we started.
    sched_setscheduler(0, old_policy, &old_param);
    rtapi_sched_fifo_last_errno = 0;
    return 1;
}

static inline int rtapi_sched_fifo_errno(void) { return rtapi_sched_fifo_last_errno; }

// rtapi_is_realtime() reports whether this process can actually run
// realtime code.  This matches the convention used by JACK, PipeWire,
// rtkit, Xenomai, and Klipper: surface the observed capability, not
// kernel metadata.  The old setuid-root stat check has been removed; it
// stat()ed EMC2_BIN_DIR/rtapi_app rather than the running binary (breaking
// wrapper-based installs like NixOS /run/wrappers) and silently masked
// LINUXCNC_FORCE_REALTIME (see issue #3928).
int rtapi_is_realtime() {
    static int cached = -1;
    if(cached != -1) return cached;

    const char *force = getenv("LINUXCNC_FORCE_REALTIME");
    if(force != NULL && atoi(force) != 0)
        return (cached = 1);

    if(detect_rtai() || detect_xenomai() || detect_xenomai_evl())
        return (cached = 1);

    return (cached = can_set_sched_fifo());
}

/* Like clock_nanosleep, except that an optional 'estimate of now' parameter may
 * optionally be passed in.  This is a very slight optimization for platforms
 * where rtapi_clock_nanosleep is implemented in terms of nanosleep, because it
 * can avoid an additional clock_gettime syscall.
 */
static int rtapi_clock_nanosleep(clockid_t clock_id, int flags,
        const struct timespec *prequest, struct timespec *remain,
        const struct timespec *pnow)
{
    (void)pnow;
#if defined(HAVE_CLOCK_NANOSLEEP)
    return TEMP_FAILURE_RETRY(clock_nanosleep(clock_id, flags, prequest, remain));
#else
    if(flags == 0)
        return nanosleep(prequest, remain);
    if(flags != TIMER_ABSTIME)
    {
        errno = EINVAL;
        return -1;
    }
    struct timespec now;
    if(!pnow)
    {
        int res = clock_gettime(clock_id, &now);
        if(res < 0) return res;
        pnow = &now;
    }
#undef timespecsub
#define	timespecsub(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_nsec = (tvp)->tv_nsec - (uvp)->tv_nsec;	\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)
    struct timespec request;
    timespecsub(prequest, pnow, &request);
    return nanosleep(&request, remain);
#endif
}
