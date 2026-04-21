/* Copyright (C) 2016 Jeff Epler <jepler@unpythonic.net>
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#include <rtai_lxrt.h>
#pragma GCC diagnostic pop
#include <stdexcept>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

namespace {
RtapiApp *app;

struct RtaiTask : RtapiTask {
    RtaiTask() : RtapiTask{}, cancel{}, rt_task{}, thr{} {
    }
    std::atomic_int cancel;
    RT_TASK *rt_task;
    pthread_t thr;
};

template <class T = RtapiTask> T *get_task(int task_id) {
    return static_cast<T *>(RtapiApp::get_task(task_id));
}


struct RtaiApp : RtapiApp {
    RtaiApp() : RtapiApp(SCHED_FIFO) {
        pthread_once(&key_once, init_key);
    }

    RtapiTask *do_task_new() {
        return new RtaiTask;
    }

    int task_delete(int id) {
        auto task = ::get_task<RtaiTask>(id);
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
        auto task = ::get_task<RtaiTask>(task_id);
        if (!task)
            return -EINVAL;

        task->period = period_nsec;
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = task->prio;

        // PLL functions not supported
        task->pll_correction_limit = 0;
        task->pll_correction = 0;

        int ret;
        pthread_attr_t attr;
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
        if ((ret = pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void *>(task))) != 0)
            return -ret;

        return 0;
    }

    static void *wrapper(void *arg) {
        auto task = reinterpret_cast<RtaiTask *>(arg);
        pthread_setspecific(key, arg);

        int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        int cpus_allowed = 1 << (nprocs - 1); //Use last CPU as default
        const static int rt_cpu_number = find_rt_cpu_number();
        if (rt_cpu_number != -1) {
            rtapi_print_msg(RTAPI_MSG_INFO, "rt_cpu_number = %i\n", rt_cpu_number);
            cpus_allowed = 1 << rt_cpu_number;
        }
        task->rt_task = rt_task_init_schmod(task->id, task->prio, 0, 0, SCHED_FIFO, cpus_allowed);
        rt_set_periodic_mode();
        start_rt_timer(nano2count(task->period));
        /* uses_fp is deprecated and ignored; always save FPU state */
        rt_task_use_fpu(task->rt_task, 1);
        rt_make_hard_real_time();
        rt_task_make_periodic_relative_ns(task->rt_task, task->period, task->period);
        (task->taskcode)(task->arg);

        rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
        rt_make_soft_real_time();
        return nullptr;
    }

    int task_pause(int task_id) {
        auto task = ::get_task<RtaiTask>(task_id);
        if (!task)
            return -EINVAL;
        return rt_task_suspend(task->rt_task);
    }

    int task_resume(int task_id) {
        auto task = ::get_task<RtaiTask>(task_id);
        if (!task)
            return -EINVAL;
        return rt_task_resume(task->rt_task);
    }

    long long task_pll_get_reference(void) {
        // PLL functions not supported
        return 0;
    }

    int task_pll_set_correction(long value) {
        (void)value;
        // PLL functions not supported
        return -EINVAL;
    }

    void wait() {
        int task_id = task_self();
        auto task = ::get_task<RtaiTask>(task_id);
        if (task->cancel) {
            rt_make_soft_real_time();
            pthread_exit(nullptr);
        }
        if (rt_task_wait_period() < 0)
            unexpected_realtime_delay(task);
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
        return rt_get_cpu_time_ns();
    }

    void do_delay(long ns) {
        rt_sleep(nano2count(ns));
    }

    int prio_highest() const {
        return RT_SCHED_HIGHEST_PRIORITY;
    }

    int prio_lowest() const {
        return RT_SCHED_LOWEST_PRIORITY;
    }
};

pthread_once_t RtaiApp::key_once;
pthread_key_t RtaiApp::key;
} // namespace

extern "C" RtapiApp *make(int policy);

RtapiApp *make(int policy) {
    if (policy != SCHED_FIFO) {
        throw std::invalid_argument("Only SCHED_FIFO allowed");
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using LXRT realtime\n");
    return app = new RtaiApp();
}
