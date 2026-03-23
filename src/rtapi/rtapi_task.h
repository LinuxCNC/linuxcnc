#ifndef RTAPI_TASK_H
#define RTAPI_TASK_H

#include <stddef.h>
#include <time.h>

/* rtapi_task structure — single definition shared by the launcher CGo shims
   and uspace_rtapi_lib.c.  Converted from the C++ class hierarchy that
   lived in the now-deleted rtapi_uspace.hh. */
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

#endif /* RTAPI_TASK_H */
