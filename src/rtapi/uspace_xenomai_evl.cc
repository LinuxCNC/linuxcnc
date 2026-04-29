/* Copyright (C) 2016 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2026 Hannes Diethelm <hannes.diethelm@gmail.com>
 *   Copy uspace_xenomai.cc and adapted to Xenomai4 EVL
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

#include "config.h"
#include "rtapi.h"
#include "uspace_rtapi_app.hh"

#include <sched.h>

#include <evl/thread.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/proxy.h>

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <cstring>
#include <stdexcept>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

namespace {
struct EvlTask : RtapiTask {
    EvlTask() : RtapiTask{}, cancel{}, thr{} {
    }
    std::atomic_int cancel;
    pthread_t thr;
};


struct EvlApp : RtapiApp {
    EvlApp() : RtapiApp(SCHED_FIFO) {
        pthread_once(&key_once, init_key);
    }

    RtapiTask *do_task_new() {
        return new EvlTask;
    }

    int task_delete(int id) {
        auto task = ::rtapi_get_task<EvlTask>(id);
        if (!task)
            return -EINVAL;

        task->cancel = 1;
        pthread_join(task->thr, nullptr);
        task->magic = 0;
        task_array[id] = 0;
        delete task;
        return 0;
    }

    int task_start(int task_id, unsigned long period_nsec) {
        auto task = ::rtapi_get_task<EvlTask>(task_id);
        if (!task)
            return -EINVAL;

        task->period = period_nsec;
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = task->prio;

        // limit PLL correction values to +/-1% of cycle time
        task->pll_correction_limit = period_nsec / 100;
        task->pll_correction = 0;

        int nprocs = sysconf(_SC_NPROCESSORS_ONLN);

        pthread_attr_t attr;
        int ret;
        if ((ret = pthread_attr_init(&attr)) != 0)
            return -ret;
        if ((ret = pthread_attr_setstacksize(&attr, task->stacksize)) != 0)
            return -ret;
        if ((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0)
            return -ret;
        if ((ret = pthread_attr_setschedparam(&attr, &param)) != 0)
            return -ret;
        if ((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0)
            return -ret;
        if (nprocs > 1) {
            const static int rt_cpu_number = find_rt_cpu_number();
            rtapi_print_msg(RTAPI_MSG_INFO, "rt_cpu_number = %i\n", rt_cpu_number);
            if (rt_cpu_number != -1) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(rt_cpu_number, &cpuset);
                if ((ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
                    return -ret;
            }
        }
        if ((ret = pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void *>(task))) != 0)
            return -ret;

        return 0;
    }

    static void *wrapper(void *arg) {
        auto task = reinterpret_cast<EvlTask *>(arg);
        pthread_setspecific(key, arg);

        {
            WithRoot r;
            /* Attach to the core. */
            rtapi_print("linuxcnc-task:%d\n", gettid());
            int tfd = evl_attach_self("linuxcnc-thread:%d", gettid());
            if (tfd < 0) {
                rtapi_print("evl_attach_self() failed ret %i errno %i\n", tfd, errno);
            }
        }

        struct timespec now;
        evl_read_clock(EVL_CLOCK_MONOTONIC, &now);

        // originally, I used pthread_make_periodic_np here, and
        // pthread_wait_np in wait(), but in about 1 run in 50 this led to
        // "xenomai: watchdog triggered" and rtapi_app was killed.
        //
        // encountered on: 3.18.20-xenomai-2.6.5 with a 2-thread SMP system
        rtapi_timespec_advance(task->nextstart, now, task->period + task->pll_correction);

        (task->taskcode)(task->arg);

        rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
        return nullptr;
    }

    int task_pause(int task_id) {
        (void)task_id;
        return -ENOSYS;
    }

    int task_resume(int task_id) {
        (void)task_id;
        return -ENOSYS;
    }

    long long task_pll_get_reference(void) {
        RtapiTask *task = reinterpret_cast<RtapiTask *>(pthread_getspecific(key));
        if (!task)
            return 0;
        return task->nextstart.tv_sec * 1000000000LL + task->nextstart.tv_nsec;
    }

    int task_pll_set_correction(long value) {
        RtapiTask *task = reinterpret_cast<RtapiTask *>(pthread_getspecific(key));
        if (!task)
            return -EINVAL;
        if (value > task->pll_correction_limit)
            value = task->pll_correction_limit;
        if (value < -(task->pll_correction_limit))
            value = -(task->pll_correction_limit);
        task->pll_correction = value;
        return 0;
    }

    void wait() {
        int task_id = task_self();
        auto task = ::rtapi_get_task<EvlTask>(task_id);
        if (task->cancel) {
            pthread_exit(nullptr);
        }
        rtapi_timespec_advance(task->nextstart, task->nextstart, task->period + task->pll_correction);
        struct timespec now;
        evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
        if (rtapi_timespec_less(task->nextstart, now)) {
            if (policy == SCHED_FIFO)
                unexpected_realtime_delay(task);
        } else {
            int res = evl_sleep_until(EVL_CLOCK_MONOTONIC, &task->nextstart);
            if (res < 0)
                perror("evl_sleep_until");
        }
    }

    unsigned char do_inb(unsigned int port) {
#ifdef HAVE_SYS_IO_H
        return inb(port);
#else
        (void)port;
        return 0;
#endif
    }

    void do_outb(unsigned char val, unsigned int port) {
#ifdef HAVE_SYS_IO_H
        return outb(val, port);
#else
        (void)val;
        (void)port;
#endif
    }

    int task_self() {
        RtapiTask *task = reinterpret_cast<RtapiTask *>(pthread_getspecific(key));
        if (!task)
            return -EINVAL;
        return task->id;
    }

    static pthread_once_t key_once;
    static pthread_key_t key;
    static void init_key(void) {
        pthread_key_create(&key, NULL);
    }

    long long do_get_time() {
        struct timespec ts;
        evl_read_clock(EVL_CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    }

    void do_delay(long ns) {
        struct timespec ts;
        evl_read_clock(EVL_CLOCK_MONOTONIC, &ts);
        rtapi_timespec_advance(ts, ts, ns);
        evl_sleep_until(EVL_CLOCK_MONOTONIC, &ts);
    }
};

pthread_once_t EvlApp::key_once;
pthread_key_t EvlApp::key;
} // namespace

extern "C" RtapiApp *make(int policy);

RtapiApp *make(int policy) {
    if (policy != SCHED_FIFO) {
        throw std::invalid_argument("Only SCHED_FIFO allowed");
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using XENOMAI4 EVL realtime\n");
    return new EvlApp();
}
