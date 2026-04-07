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

#include "config.h"
#include "rtapi.h"
#include "uspace_rtapi_app.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

namespace {
struct PosixTask : rtapi_task {
    PosixTask() : rtapi_task{}, thr{} {
    }

    pthread_t thr; /* thread's context */
};

struct PosixApp : RtapiApp {
    PosixApp(int policy = SCHED_FIFO) : RtapiApp(policy), do_thread_lock(policy != SCHED_FIFO) {
        pthread_once(&key_once, init_key);
        if (do_thread_lock) {
            pthread_once(&lock_once, init_lock);
        }
    }

    struct rtapi_task *do_task_new() {
        return new PosixTask;
    }

    int task_delete(int id) {
        auto task = ::rtapi_get_task<PosixTask>(id);
        if (!task)
            return -EINVAL;

        pthread_cancel(task->thr);
        pthread_join(task->thr, 0);
        task->magic = 0;
        task_array[id] = 0;
        delete task;
        return 0;
    }

    int task_start(int task_id, unsigned long period_nsec) {
        auto task = ::rtapi_get_task<PosixTask>(task_id);
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
#ifdef __FreeBSD__
                cpuset_t cpuset;
#else
                cpu_set_t cpuset;
#endif
                CPU_ZERO(&cpuset);
                CPU_SET(rt_cpu_number, &cpuset);
                if ((ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
                    return -ret;
            }
        }
        if (do_thread_lock)
            pthread_mutex_lock(&thread_lock);
        if ((ret = pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void *>(task))) != 0)
            return -ret;

        return 0;
    }

    static void *wrapper(void *arg) {
        auto task = reinterpret_cast<PosixTask *>(arg);

        pthread_setspecific(key, arg);
        set_namef("rtapi_app:T#%d", task->id);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        rtapi_timespec_advance(task->nextstart, now, task->period + task->pll_correction);

        /* call the task function with the task argument */
        (task->taskcode)(task->arg);

        rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
        return NULL;
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
        struct rtapi_task *task = reinterpret_cast<rtapi_task *>(pthread_getspecific(key));
        if (!task)
            return 0;
        return task->nextstart.tv_sec * 1000000000LL + task->nextstart.tv_nsec;
    }

    int task_pll_set_correction(long value) {
        struct rtapi_task *task = reinterpret_cast<rtapi_task *>(pthread_getspecific(key));
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
        if (do_thread_lock)
            pthread_mutex_unlock(&thread_lock);
        pthread_testcancel();
        struct rtapi_task *task = reinterpret_cast<rtapi_task *>(pthread_getspecific(key));
        rtapi_timespec_advance(task->nextstart, task->nextstart, task->period + task->pll_correction);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (rtapi_timespec_less(task->nextstart, now)) {
            if (policy == SCHED_FIFO)
                unexpected_realtime_delay(task);
        } else {
            int res = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task->nextstart, nullptr);
            if (res < 0)
                perror("clock_nanosleep");
        }
        if (do_thread_lock)
            pthread_mutex_lock(&thread_lock);
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

    int run_threads(int fd, int (*callback)(int fd)) {
        while (callback(fd)) {
            /* nothing */
        }
        return 0;
    }

    int task_self() {
        struct rtapi_task *task = reinterpret_cast<rtapi_task *>(pthread_getspecific(key));
        if (!task)
            return -EINVAL;
        return task->id;
    }

    bool do_thread_lock;

    static pthread_once_t key_once;
    static pthread_key_t key;
    static void init_key(void) {
        pthread_key_create(&key, NULL);
    }

    static pthread_once_t lock_once;
    static pthread_mutex_t thread_lock;
    static void init_lock(void) {
        pthread_mutex_init(&thread_lock, NULL);
    }

    long long do_get_time() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    }

    void do_delay(long ns) {
        struct timespec ts = {0, ns};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
    }
};

pthread_once_t PosixApp::key_once = PTHREAD_ONCE_INIT;
pthread_once_t PosixApp::lock_once = PTHREAD_ONCE_INIT;
pthread_key_t PosixApp::key;
pthread_mutex_t PosixApp::thread_lock;

} // namespace

extern "C" RtapiApp *make(int policy);

RtapiApp *make(int policy) {
    if (policy == SCHED_OTHER) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using POSIX non-realtime\n");
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using POSIX realtime\n");
    }
    return new PosixApp(policy);
}
