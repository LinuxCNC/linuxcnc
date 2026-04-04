#include "rtapi.h"
#include "uspace_rtapi.hh"

std::atomic_int WithRoot::level;
uid_t euid, ruid;

WithRoot::WithRoot() {
    if(!level++) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

WithRoot::~WithRoot() {
    if(!--level) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

rtapi_task::rtapi_task()
    : magic{}, id{}, owner{}, uses_fp{}, stacksize{}, prio{},
      period{}, nextstart{},
      ratio{}, pll_correction{}, pll_correction_limit{},
      arg{}, taskcode{}

{}

/* Priority functions.  Uspace uses POSIX task priorities. */

int RtapiApp::prio_highest() const
{
    return sched_get_priority_max(policy);
}

int RtapiApp::prio_lowest() const
{
  return sched_get_priority_min(policy);
}

int RtapiApp::prio_higher_delta() const {
    if(rtapi_prio_highest() > rtapi_prio_lowest()) {
        return 1;
    }
    return -1;
}

int RtapiApp::prio_bound(int prio) const {
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

bool RtapiApp::prio_check(int prio) const {
    if(rtapi_prio_highest() > rtapi_prio_lowest()) {
        return (prio <= rtapi_prio_highest()) && (prio >= rtapi_prio_lowest());
    } else {
        return (prio <= rtapi_prio_lowest()) && (prio >= rtapi_prio_highest());
    }
}

int RtapiApp::prio_next_higher(int prio) const
{
    prio = prio_bound(prio);
    if(prio != rtapi_prio_highest())
        return prio + prio_higher_delta();
    return prio;
}

int RtapiApp::prio_next_lower(int prio) const
{
    prio = prio_bound(prio);
    if(prio != rtapi_prio_lowest())
        return prio - prio_higher_delta();
    return prio;
}

int RtapiApp::allocate_task_id()
{
    for(int n=0; n<MAX_TASKS; n++)
    {
        rtapi_task **taskptr = &(task_array[n]);
        if(__sync_bool_compare_and_swap(taskptr, (rtapi_task*)0, TASK_MAGIC_INIT))
            return n;
    }
    return -ENOSPC;
}

int RtapiApp::task_new(void (*taskcode) (void*), void *arg,
        int prio, int owner, unsigned long int stacksize, int /*uses_fp*/) {
  /* check requested priority */
  if (!prio_check(prio))
  {
    rtapi_print_msg(RTAPI_MSG_ERR,"rtapi:task_new prio is not in bound lowest %i prio %i highest %i\n",
        rtapi_prio_lowest(), prio, rtapi_prio_highest());
    return -EINVAL;
  }

  /* label as a valid task structure */
  int n = allocate_task_id();
  if(n < 0) return n;

  struct rtapi_task *task = do_task_new();
  if(stacksize < (1024*1024)) stacksize = (1024*1024);
  task->id = n;
  task->owner = owner;
  /* uses_fp is deprecated and ignored; always save FPU state */
  task->uses_fp = 1;
  task->arg = arg;
  task->stacksize = stacksize;
  task->taskcode = taskcode;
  task->prio = prio;
  task->magic = TASK_MAGIC;
  task_array[n] = task;

  /* and return handle to the caller */

  return n;
}

rtapi_task *RtapiApp::get_task(int task_id) {
    if(task_id < 0 || task_id >= MAX_TASKS) return NULL;
    /* validate task handle */
    rtapi_task *task = task_array[task_id];
    if(!task || task == TASK_MAGIC_INIT || task->magic != TASK_MAGIC)
        return NULL;

    return task;
}

void RtapiApp::unexpected_realtime_delay(rtapi_task *task, int /*nperiod*/) {
    static int printed = 0;
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

long RtapiApp::clock_set_period(long nsecs)
{
  if(nsecs == 0) return period;
  if(period != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
      return -EINVAL;
  }
  period = nsecs;
  return period;
}