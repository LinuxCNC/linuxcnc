/* Copyright (C) 2006-2014 Jeff Epler <jepler@unpythonic.net>
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

#include "rtapi.h"
#include "uspace_rtapi_app.hh"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

std::atomic_int WithRoot::level;
uid_t euid, ruid;

WithRoot::WithRoot() {
    if (!level++) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

WithRoot::~WithRoot() {
    if (!--level) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

rtapi_task::rtapi_task()
    : magic{}, id{}, owner{}, uses_fp{}, stacksize{}, prio{}, period{}, nextstart{}, pll_correction{},
      pll_correction_limit{}, arg{}, taskcode{}

{
}

struct rtapi_task *RtapiApp::task_array[MAX_TASKS];

/* Priority functions.  Uspace uses POSIX task priorities. */

int RtapiApp::prio_highest() const {
    return sched_get_priority_max(policy);
}

int RtapiApp::prio_lowest() const {
    return sched_get_priority_min(policy);
}

int RtapiApp::prio_higher_delta() const {
    if (rtapi_prio_highest() > rtapi_prio_lowest()) {
        return 1;
    }
    return -1;
}

int RtapiApp::prio_bound(int prio) const {
    if (rtapi_prio_highest() > rtapi_prio_lowest()) {
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
    if (rtapi_prio_highest() > rtapi_prio_lowest()) {
        return (prio <= rtapi_prio_highest()) && (prio >= rtapi_prio_lowest());
    } else {
        return (prio <= rtapi_prio_lowest()) && (prio >= rtapi_prio_highest());
    }
}

int RtapiApp::prio_next_higher(int prio) const {
    prio = prio_bound(prio);
    if (prio != rtapi_prio_highest())
        return prio + prio_higher_delta();
    return prio;
}

int RtapiApp::prio_next_lower(int prio) const {
    prio = prio_bound(prio);
    if (prio != rtapi_prio_lowest())
        return prio - prio_higher_delta();
    return prio;
}

int RtapiApp::allocate_task_id() {
    for (int n = 0; n < MAX_TASKS; n++) {
        rtapi_task **taskptr = &(task_array[n]);
        if (__sync_bool_compare_and_swap(taskptr, (rtapi_task *)0, TASK_MAGIC_INIT))
            return n;
    }
    return -ENOSPC;
}

int RtapiApp::task_new(
    void (*taskcode)(void *), void *arg, int prio, int owner, unsigned long int stacksize, int /*uses_fp*/
) {
    /* check requested priority */
    if (!prio_check(prio)) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "rtapi:task_new prio is not in bound lowest %i prio %i highest %i\n",
            rtapi_prio_lowest(),
            prio,
            rtapi_prio_highest()
        );
        return -EINVAL;
    }

    /* label as a valid task structure */
    int n = allocate_task_id();
    if (n < 0)
        return n;

    struct rtapi_task *task = do_task_new();
    if (stacksize < (1024 * 1024))
        stacksize = (1024 * 1024);
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
    if (task_id < 0 || task_id >= MAX_TASKS)
        return NULL;
    /* validate task handle */
    rtapi_task *task = task_array[task_id];
    if (!task || task == TASK_MAGIC_INIT || task->magic != TASK_MAGIC)
        return NULL;

    return task;
}

void RtapiApp::unexpected_realtime_delay(rtapi_task *task, int /*nperiod*/) {
    static int printed = 0;
    if (!printed) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "Unexpected realtime delay on task %d with period %ld\n"
            "This Message will only display once per session.\n"
            "Run the Latency Test and resolve before continuing.\n",
            task->id,
            task->period
        );
        printed = 1;
    }
}

long RtapiApp::clock_set_period(long nsecs) {
    if (nsecs == 0)
        return period;
    if (period != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "attempt to set period twice\n");
        return -EINVAL;
    }
    period = nsecs;
    return period;
}

//parse_cpu_list from https://gitlab.com/Xenomai/xenomai4/libevl/-/blob/11e6a1fb183a315ae861762e7650fd5e10d83ff5/tests/helpers.c
//License: MIT
static void parse_cpu_list(const char *path, cpu_set_t *cpuset) {
    char *p, *range, *range_p = NULL, *id, *id_r;
    int start, end, cpu;
    char buf[BUFSIZ];
    FILE *fp;

    CPU_ZERO(cpuset);

    fp = fopen(path, "r");
    if (fp == NULL)
        return;

    if (!fgets(buf, sizeof(buf), fp))
        goto out;

    p = buf;
    while ((range = strtok_r(p, ",", &range_p)) != NULL) {
        if (*range == '\0' || *range == '\n')
            goto next;
        end = -1;
        id = strtok_r(range, "-", &id_r);
        if (id) {
            start = atoi(id);
            id = strtok_r(NULL, "-", &id_r);
            if (id)
                end = atoi(id);
            else if (end < 0)
                end = start;
            for (cpu = start; cpu <= end; cpu++)
                CPU_SET(cpu, cpuset);
        }
    next:
        p = NULL;
    }
out:
    fclose(fp);
}

int find_rt_cpu_number() {
    if (getenv("RTAPI_CPU_NUMBER"))
        return atoi(getenv("RTAPI_CPU_NUMBER"));

#ifdef __linux__
    const char *isolated_file = "/sys/devices/system/cpu/isolated";
    cpu_set_t cpuset;

    parse_cpu_list(isolated_file, &cpuset);

    //Print list
    rtapi_print_msg(RTAPI_MSG_INFO, "cpuset isolated ");
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            rtapi_print_msg(RTAPI_MSG_INFO, "%i ", i);
        }
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "\n");

    int top = -1;
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset))
            top = i;
    }
    if (top == -1) {
        rtapi_print_msg(
            RTAPI_MSG_ERR, "No isolated CPU's found, expect some latency or set RTAPI_CPU_NUMBER to select CPU\n"
        );
    }
    return top;
#else
    return (-1);
#endif
}

void set_namef(const char *fmt, ...) {
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