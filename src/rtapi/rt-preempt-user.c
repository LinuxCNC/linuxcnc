/********************************************************************
* Description:  rt-preempt-user.c
*
*               This file, 'rt-preempt-user.c', implements the unique
*               functions for the RT_PREEMPT thread system.
********************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/

#include <unistd.h>		// getpid(), syscall()
#include <time.h>               // clock_nanosleep()

#ifdef RTAPI
#include <stdlib.h>		// malloc(), sizeof(), free()
#include <string.h>		// memset()
#include <signal.h>		// sigaction(), sigemptyset(), SIGXCPU...
#include <sys/resource.h>	// rusage, getrusage(), RUSAGE_SELF

/* Lock for task_array and module_array allocations */
static pthread_key_t task_key;
static pthread_once_t task_key_once = PTHREAD_ONCE_INIT;

#  ifndef RTAPI_POSIX
static int error_printed;
#  endif
#endif  /* RTAPI */

#define MODULE_OFFSET		32768

#ifdef ULAPI

int _rtapi_init(const char *modname) {
	/* do nothing for ULAPI */
	return getpid();
}

int _rtapi_exit(int module_id) {
	/* do nothing for ULAPI */
	return 0;
}

#else /* RTAPI */

int _rtapi_init(const char *modname) {
    int n, module_id = -ENOMEM;
    module_data *module;

    /* say hello */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: initing module %s\n", modname);

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* find empty spot in module array */
    n = 1;
    while ((n <= RTAPI_MAX_MODULES) && (module_array[n].state != NO_MODULE)) {
	n++;
    }
    if (n > RTAPI_MAX_MODULES) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: reached module limit %d\n",
			n);
	return -EMFILE;
    }
    /* we have space for the module */
    module_id = n + MODULE_OFFSET;
    module = &(module_array[n]);
    /* update module data */
    module->state = REALTIME;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "%s", modname);
    } else {
	/* make up a name */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "ULMOD%03d", module_id);
    }
    rtapi_data->ul_module_count++;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module '%s' loaded, ID: %d\n",
	module->name, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return module_id;
}

int _rtapi_exit(int module_id) {
    module_id -= MODULE_OFFSET;

    if (module_id < 0 || module_id >= RTAPI_MAX_MODULES)
	return -1;
    /* Remove the module from the module_array. */
    rtapi_mutex_get(&(rtapi_data->mutex));
    module_array[module_id].state = NO_MODULE;
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_exit: freed module slot %d, was %s\n",
		    module_id, module_array[module_id].name);
    rtapi_mutex_give(&(rtapi_data->mutex));

    return 0;
}

static inline int task_id(task_data *task) {
    return (int)(task - task_array);
}


#ifndef RTAPI_POSIX
static unsigned long rtapi_get_pagefault_count(task_data *task) {
    struct rusage rusage;
    unsigned long minor, major;

    getrusage(RUSAGE_SELF, &rusage);
    minor = rusage.ru_minflt;
    major = rusage.ru_majflt;
    if (minor < task->minfault_base || major < task->majfault_base) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi task %d %s: Got invalid fault counts.\n",
			task_id(task), task->name);
	return 0;
    }
    minor -= task->minfault_base;
    major -= task->majfault_base;

    return minor + major;
}

static void rtapi_reset_pagefault_count(task_data *task) {
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    if (task->minfault_base != rusage.ru_minflt ||
	task->majfault_base != rusage.ru_majflt) {
	task->minfault_base = rusage.ru_minflt;
	task->majfault_base = rusage.ru_majflt;
	rtapi_print_msg(RTAPI_MSG_DBG,
			"rtapi task %d %s: Reset pagefault counter\n",
			task_id(task), task->name);
    }
}
#endif


static void rtapi_advance_time(struct timespec *tv, unsigned long ns,
			       unsigned long s) {
    ns += tv->tv_nsec;
    while (ns > 1000000000) {
	s++;
	ns -= 1000000000;
    }
    tv->tv_nsec = ns;
    tv->tv_sec += s;
}

static void rtapi_key_alloc() {
    pthread_key_create(&task_key, NULL);
}

static void rtapi_set_task(task_data *t) {
    pthread_once(&task_key_once, rtapi_key_alloc);
    pthread_setspecific(task_key, (void *)t);
}

static task_data *rtapi_this_task() {
    pthread_once(&task_key_once, rtapi_key_alloc);
    return (task_data *)pthread_getspecific(task_key);
}

int rtapi_task_new_hook(task_data *task, int task_id) {
    void *stackaddr;

    stackaddr = malloc(task->stacksize);
    if (!stackaddr) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"Failed to allocate realtime thread stack\n");
	return -ENOMEM;
    }
    memset(stackaddr, 0, task->stacksize);
    task->stackaddr = stackaddr;
    task->destroyed = 0;
    return task_id;
}


void rtapi_task_delete_hook(task_data *task, int task_id) {
    int err;
    void *returncode;

    /* Signal thread termination and wait for the thread to exit. */
    if (!task->deleted) {
	task->deleted = 1;
	err = pthread_join(task->thread, &returncode);
	if (err)
	    rtapi_print_msg
		(RTAPI_MSG_ERR, "pthread_join() on realtime thread failed\n");
    }
    /* Free the thread stack. */
    free(task->stackaddr);
    task->stackaddr = NULL;
}

static int realtime_set_affinity(task_data *task) {
    cpu_set_t set;
    int err, cpu_nr, use_cpu = -1;

    pthread_getaffinity_np(task->thread, sizeof(set), &set);
    if (task->cpu > -1) { // CPU set explicitly
	if (!CPU_ISSET(task->cpu, &set)) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "RTAPI: ERROR: realtime_set_affinity(%s): "
			    "CPU %d not available\n",
			    task->name, task->cpu);
	    return -EINVAL;
	}
	use_cpu = task->cpu;
    } else {
	// select last CPU as default
	for (cpu_nr = CPU_SETSIZE - 1; cpu_nr >= 0; cpu_nr--) {
	    if (CPU_ISSET(cpu_nr, &set)) {
		use_cpu = cpu_nr;
		break;
	    }
	}
	if (use_cpu < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "Unable to get ID of the last CPU\n");
	    return -EINVAL;
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "task %s: using default CPU %d\n",
			task->name, use_cpu);
    }
    CPU_ZERO(&set);
    CPU_SET(use_cpu, &set);

    err = pthread_setaffinity_np(task->thread, sizeof(set), &set);
    if (err) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%d %s: Failed to set CPU affinity to CPU %d (%s)\n",
			task_id(task), task->name, use_cpu, strerror(errno));
	return -EINVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "realtime_set_affinity(): task %s assigned to CPU %d\n", 
		    task->name, use_cpu);
    return 0;
}


#define ENABLE_SCHED_DEADLINE	0 /*XXX set to 1 to enable deadline scheduling. */

#ifndef __NR_sched_setscheduler_ex
# if defined(__x86_64__)
#  define __NR_sched_setscheduler_ex	299
#  define __NR_sched_wait_interval	302
# elif defined(__i386__)
#  define __NR_sched_setscheduler_ex	337
#  define __NR_sched_wait_interval	340
# else
#  warning "SCHED_DEADLINE syscall numbers unknown"
# endif
#endif

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE		6

struct sched_param_ex {
    int sched_priority;
    struct timespec sched_runtime;
    struct timespec sched_deadline;
    struct timespec sched_period;
    int sched_flags;
};

#define SCHED_SIG_RORUN		0x80000000
#define SCHED_SIG_DMISS		0x40000000

static inline int sched_setscheduler_ex(pid_t pid, int policy, unsigned len,
					struct sched_param_ex *param) {
#ifdef __NR_sched_setscheduler_ex
    return syscall(__NR_sched_setscheduler_ex, pid, policy, len, param);
#endif
    return -ENOSYS;
}
static inline int sched_wait_interval(int flags, const struct timespec *rqtp,
				      struct timespec *rmtp) {
#ifdef __NR_sched_wait_interval
    return syscall(__NR_sched_wait_interval, flags, rqtp, rmtp);
#endif
    return -ENOSYS;
}
#endif /* SCHED_DEADLINE */


#ifndef RTAPI_POSIX
static void deadline_exception(int signr) {
    if (signr != SIGXCPU) {
	rtapi_print_msg(RTAPI_MSG_ERR, "Received unknown signal %d\n", signr);
	return;
    }
    if (!error_printed++)
	rtapi_print_msg(RTAPI_MSG_ERR,
			"Missed scheduling deadline or overran "
			"scheduling runtime!\n");
}


static int realtime_set_priority(task_data *task) {
    struct sched_param schedp;
    struct sched_param_ex ex;
    struct sigaction sa;

    task->deadline_scheduling = 0;
    if (ENABLE_SCHED_DEADLINE) {
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = deadline_exception;
	if (sigaction(SIGXCPU, &sa, NULL)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "Unable to register SIGXCPU handler.\n");
	    return -1;
	}

	memset(&ex, 0, sizeof(ex));
	ex.sched_deadline.tv_nsec = period;
	ex.sched_runtime.tv_nsec = 8000; //FIXME
	ex.sched_flags = SCHED_SIG_RORUN | SCHED_SIG_DMISS;
	rtapi_print_msg(RTAPI_MSG_DBG,
			"Setting deadline scheduler for %d\n", task_id(task));
	if (sched_setscheduler_ex(0, SCHED_DEADLINE, sizeof(ex), &ex)) {
	    rtapi_print_msg
		(RTAPI_MSG_INFO,
		 "Unable to set DEADLINE scheduling policy (%s). "
		 "Trying FIFO.\n",
		 strerror(errno));
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
			    "Running DEADLINE scheduling policy.\n");
	    task->deadline_scheduling = 1;
	    return 0;
	}
    }

    memset(&schedp, 0, sizeof(schedp));
    schedp.sched_priority = task->prio;
    if (sched_setscheduler(0, SCHED_FIFO, &schedp)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"Unable to set FIFO scheduling policy: %s",
			strerror(errno));
	return 1;
    }

    return 0;
}
#endif


static void *realtime_thread(void *arg) {
    task_data *task = arg;

    rtapi_set_task(task);

    /* The task should not pagefault at all. So reset the counter now.

     * Note that currently we _do_ receive a few pagefaults in the
     * taskcode init. This is noncritical and probably not worth
     * fixing. */
#ifndef RTAPI_POSIX
    rtapi_reset_pagefault_count(task);
#endif

    if (task->period < period)
	task->period = period;
    task->ratio = task->period / period;
    rtapi_print_msg(RTAPI_MSG_DBG, "task %p period = %d ratio=%d\n",
		    task, task->period, task->ratio);

    if (realtime_set_affinity(task))
	goto error;
#ifndef RTAPI_POSIX // This requires privs; skip it in simulator build
    if (realtime_set_priority(task))
	goto error;
#endif

    /* We're done initializing. Open the barrier. */
    pthread_barrier_wait(&task->thread_init_barrier);

    clock_gettime(CLOCK_MONOTONIC, &task->next_time);
    rtapi_advance_time(&task->next_time, task->period, 0);

    /* call the task function with the task argument */
    task->taskcode(task->arg);

    rtapi_print_msg
	(RTAPI_MSG_ERR,
	 "ERROR: reached end of realtime thread for task %d\n",
	 task_id(task));
    task->deleted = 1;

    return NULL;
 error:
    /* Signal that we're dead and open the barrier. */
    task->deleted = 1;
    pthread_barrier_wait(&task->thread_init_barrier);
    return NULL;
}

int rtapi_task_start_hook(task_data *task, int task_id) {
    pthread_attr_t attr;
    int retval;

    task->deleted = 0;

    pthread_barrier_init(&task->thread_init_barrier, NULL, 2);
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, task->stackaddr, task->stacksize);
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "About to pthread_create task %d\n", task_id);
    retval = pthread_create(&task->thread, &attr, realtime_thread,
			    (void *)task);
    rtapi_print_msg(RTAPI_MSG_DBG,"Created task %d\n", task_id);
    pthread_attr_destroy(&attr);
    if (retval) {
	pthread_barrier_destroy(&task->thread_init_barrier);
	rtapi_print_msg(RTAPI_MSG_ERR, "Failed to create realtime thread\n");
	return -ENOMEM;
    }
    /* Wait for the thread to do basic initialization. */
    pthread_barrier_wait(&task->thread_init_barrier);
    pthread_barrier_destroy(&task->thread_init_barrier);
    if (task->deleted) { /* The thread died in the init phase. */
	rtapi_print_msg(RTAPI_MSG_ERR,
			"Realtime thread initialization failed\n");
	return -ENOMEM;
    }
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "Task %d finished its basic init\n", task_id);

    return 0;
}

void rtapi_task_stop_hook(task_data *task, int task_id) {
    task->destroyed = 1;
}

int rtapi_wait_hook(void) {
    struct timespec ts;
    task_data *task = rtapi_this_task();
#ifndef RTAPI_POSIX
    int msg_level = RTAPI_MSG_NONE;
#endif

    if (task->deleted)
	pthread_exit(0);

    if (task->deadline_scheduling)
	sched_wait_interval(TIMER_ABSTIME, &task->next_time, NULL);
    else
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task->next_time,
			NULL);
    rtapi_advance_time(&task->next_time, task->period, 0);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ts.tv_sec > task->next_time.tv_sec
	|| (ts.tv_sec == task->next_time.tv_sec
	    && ts.tv_nsec > task->next_time.tv_nsec)) {
	task->failures++;
#ifndef RTAPI_POSIX // don't care about scheduling deadlines in sim mode
	if (task->failures == 1)
	    msg_level = RTAPI_MSG_ERR;
	/* else if (task->failures < 10 ||		\
	       (task->failures % 10000 == 0))  */
	else if (task->failures < 10)
	    msg_level = RTAPI_MSG_WARN;

	if (msg_level != RTAPI_MSG_NONE) {
	    rtapi_print_msg
		(msg_level,
		 "ERROR: Missed scheduling deadline for task %d [%d times]\n"
		 "Now is %ld.%09ld, deadline was %ld.%09ld\n"
		 "Absolute number of pagefaults in realtime context: %lu\n",
		 task_id(task), task->failures,
		 (long)ts.tv_sec, (long)ts.tv_nsec,
		 (long)task->next_time.tv_sec,
		 (long)task->next_time.tv_nsec,
		 rtapi_get_pagefault_count(task));
	}
#endif
    }

    return 0;
}

void rtapi_delay_hook(long int nsec)
{
    struct timespec t;

    t.tv_nsec = nsec;
    t.tv_sec = 0;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &t, NULL);
}

#endif /* ULAPI/RTAPI */
