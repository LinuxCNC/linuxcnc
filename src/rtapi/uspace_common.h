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
#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <rtapi_errno.h>
#include <rtapi_mutex.h>

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

#include "config.h"

typedef struct {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int count;                    /* count of maps in this process */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
} rtapi_shmem_handle;

#define MAX_SHM 64

#define SHMEM_MAGIC   25453	/* random numbers used as signatures */

static rtapi_shmem_handle shmem_array[MAX_SHM] = {{0},};

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
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

  /* allocate RT-hardened memory (mlocked, page-faulted) */
  shmem->mem = rtapi_calloc(size);
  if (!shmem->mem) {
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new failed due to rtapi_calloc(%lu)\n", size);
    return -ENOMEM;
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
  rtapi_shmem_handle *shmem;

  if(handle < 0 || handle >= MAX_SHM)
    return -EINVAL;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return -EINVAL;

  shmem->count --;
  if(shmem->count) return 0;

  /* free the RT-hardened memory */
  rtapi_free(shmem->mem);
  shmem->mem = NULL;

  /* free the shmem structure */
  shmem->magic = 0;

  return 0;
}




// Internal message handler function pointer — set once by gomc-server to
// connect rtapi_print/rtapi_print_msg to the lock-free log ring.
// Before the ring is connected, messages are silently discarded.
typedef void(*rtapi_msg_handler_t)(msg_level_t level, const char *fmt, va_list ap);
static rtapi_msg_handler_t rtapi_msg_handler = NULL;

void rtapi_set_msg_handler(rtapi_msg_handler_t handler) {
    rtapi_msg_handler = handler;
}


void rtapi_print(const char *fmt, ...)
{
    va_list args;

    if (rtapi_msg_handler) {
        va_start(args, fmt);
        rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
        va_end(args);
    }
}


void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
{
    va_list args;

    if (rtapi_msg_handler) {
        va_start(args, fmt);
        rtapi_msg_handler(level, fmt, args);
        va_end(args);
    }
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
    static uuid_data_t* uuid_data   = 0;
    const static   int  uuid_id     = 0;

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
static int _rtapi_is_realtime = -1;
#ifdef __linux__
static int detect_preempt_rt() {
    struct utsname u;
    int crit1, crit2 = 0;
    FILE *fd;

    uname(&u);
    crit1 = strcasestr (u.version, "PREEMPT RT") != 0;

    //"PREEMPT_RT" is used in the version string instead of "PREEMPT RT" starting with kernel version 5.4
    crit1 = crit1 || (strcasestr(u.version, "PREEMPT_RT") != 0);

    if ((fd = fopen("/sys/kernel/realtime","r")) != NULL) {
        int flag;
        crit2 = ((fscanf(fd, "%d", &flag) == 1) && (flag == 1));
        fclose(fd);
    }

    return crit1 || crit2;
}
#else
static int detect_preempt_rt() {
    return 0;
}
#endif
static int detect_env_override() {
    char *p = getenv("LINUXCNC_FORCE_REALTIME");
    return p != NULL && atoi(p) != 0;
}

static int detect_realtime() {
    return detect_env_override() || detect_preempt_rt();
}

int rtapi_is_realtime() {
    if(_rtapi_is_realtime == -1) _rtapi_is_realtime = detect_realtime();
    return _rtapi_is_realtime;
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
#if defined(HAVE_CLOCK_NANOSLEEP)
    return clock_nanosleep(clock_id, flags, prequest, remain);
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
