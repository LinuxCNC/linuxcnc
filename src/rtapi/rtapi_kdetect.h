// support for thread style autodetection based on digging in the kernel
// and userland libraries

#include "rtapi_bitops.h"

// utsname.release matches telltale strings (soft)
#define UTSNAME_REL_RTAI     _BIT(0)
#define UTSNAME_REL_RT       _BIT(1)
#define UTSNAME_REL_XENOMAI  _BIT(2)

// utsname.version matches "PREEMPT RT" (hard)
#define UTSNAME_VER_RT_PREEMPT _BIT(3)

// utsname.version matches "#rtai" (soft)
#define UTSNAME_VER_RTAI     _BIT(4)

#define XENO_RTHEAP_FOUND     _BIT(5) // /dev/rtheap seen (hard)
#define XENO_PROCENTRY_FOUND  _BIT(6) // /proc/xenomai seen (hard)

#define HAS_HIRES_TIMERS      _BIT(7) // sanitary - assume vanilla if false.
#define SYS_KERNEL_REALTIME_FOUND _BIT(8) // RT_PREEMPT (hard)

#define DEV_RTAI_SHM_FOUND _BIT(9) // RTAI (hard)

// verify the Xenomai userland libraries are present and make sense (sanitary for xenomai)
#define XENO_LIBXENOMAI       _BIT(10) // can dlopen("libxenomai.so")
#define XENO_LIBNATIVE        _BIT(11) // can dlopen("libnative.so")
#define XENO_LIBSYMBOL        _BIT(12) // can resolve symbol in "libnative.so"

// hard evidence for an ipipe patch in place (RTAI and Xenomai):
#define HAS_PROC_IPIPE        _BIT(13) // /proc/ipipe exists and is a directory

#define PROC_IPIPE "/proc/ipipe"

// really in nucleus/heap.h but we rather get away with minimum include files
#ifndef XNHEAP_DEV_NAME
#define XNHEAP_DEV_NAME  "/dev/rtheap"
#endif

// test for "/proc/xenomai" existance and a directory
#define PROC_XENOMAI  "/proc/xenomai"

// dlopen() these shared libraries
#define LIBXENOMAI "libxenomai.so"
#define LIBNATIVE "libnative.so"
// and dig for LIBNATIVE_SYM
#define LIBNATIVE_SYM "rt_task_create"

// if this exists, and contents is '1', it's RT_PREEMPT
#define PREEMPT_RT_SYSFS "/sys/kernel/realtime"

// dev/rtai_shm visible only after 'realtime start'
#define DEV_RTAI_SHM "/dev/rtai_shm"


int rtapi_kdetect(unsigned long *feat);
