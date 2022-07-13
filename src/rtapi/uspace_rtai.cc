#include "config.h"
#include "rtapi.h"
#include "rtapi_uspace.hh"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#include <rtai_lxrt.h>
#pragma GCC diagnostic pop
#include <atomic>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

namespace
{
RtapiApp *app;

struct RtaiTask : rtapi_task {
    RtaiTask() : rtapi_task{}, cancel{}, rt_task{}, thr{} {}
    std::atomic<int> cancel;
    RT_TASK *rt_task;
    pthread_t thr;
};

template<class T=rtapi_task>
T *get_task(int task_id) {
    return static_cast<T*>(RtapiApp::get_task(task_id));
}


struct RtaiApp : RtapiApp {
    RtaiApp() : RtapiApp(SCHED_FIFO) {
        pthread_once(&key_once, init_key);
    }

    RtaiTask *do_task_new() {
        return new RtaiTask;
    }

    int task_delete(int id) {
        auto task = ::get_task<RtaiTask>(id);
        if(!task) return -EINVAL;

        task->cancel = 1;
        pthread_join(task->thr, nullptr);
        task->magic = 0;
        task_array[id] = 0;
        delete task;
        return 0;
    }

    int task_start(int task_id, unsigned long period_nsec) {
        auto task = ::get_task<RtaiTask>(task_id);
        if(!task) return -EINVAL;

        task->period = period_nsec;
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = task->prio;

        // PLL functions not supported
        task->pll_correction_limit = 0;
        task->pll_correction = 0;

        pthread_attr_t attr;
        if(pthread_attr_init(&attr) < 0)
            return -errno;
        if(pthread_attr_setstacksize(&attr, task->stacksize) < 0)
            return -errno;
        if(pthread_attr_setschedpolicy(&attr, policy) < 0)
            return -errno;
        if(pthread_attr_setschedparam(&attr, &param) < 0)
            return -errno;
        if(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) < 0)
            return -errno;
        if(pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void*>(task)) < 0)
            return -errno;

        return 0;
    }

    static void *wrapper(void *arg) {
        auto task = reinterpret_cast<RtaiTask*>(arg);
        pthread_setspecific(key, arg);
        task->rt_task = rt_task_init(task->id, task->prio, 0, 0);
        rt_set_periodic_mode();
        start_rt_timer(nano2count(task->period));
        if(task->uses_fp) rt_task_use_fpu(task->rt_task, 1);
        // assumes processor numbers are contiguous
        int nprocs = sysconf( _SC_NPROCESSORS_ONLN );
        rt_set_runnable_on_cpus(task->rt_task, 1u << (nprocs - 1));
        rt_make_hard_real_time();
        rt_task_make_periodic_relative_ns(task->rt_task, task->period, task->period);
        (task->taskcode) (task->arg);

        rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
        rt_make_soft_real_time();
        return nullptr;
    }

    int task_pause(int task_id) {
        auto task = ::get_task<RtaiTask>(task_id);
        if(!task) return -EINVAL;
        return rt_task_suspend(task->rt_task);
    }

    int task_resume(int task_id) {
        auto task = ::get_task<RtaiTask>(task_id);
        if(!task) return -EINVAL;
        return rt_task_resume(task->rt_task);
    }

    long long task_pll_get_reference(void) {
        // PLL functions not supported
        return 0;
    }

    int task_pll_set_correction(long value) {
        // PLL functions not supported
        return -EINVAL;
    }

    void wait() {
        int task_id = task_self();
        auto task = ::get_task<RtaiTask>(task_id);
        if(task->cancel) {
            rt_make_soft_real_time();
            pthread_exit(nullptr);
        }
        if(rt_task_wait_period() < 0) unexpected_realtime_delay(task);
    }

    unsigned char do_inb(unsigned int port) {
#ifdef HAVE_SYS_IO_H
        return inb(port);
#endif
    }

    void do_outb(unsigned char val, unsigned int port) {
#ifdef HAVE_SYS_IO_H
        return outb(val, port);
#endif
    }

    int run_threads(int fd, int (*callback)(int fd)) {
        while(callback(fd)) { /* nothing */ }
        return 0;
    }

    int task_self() {
        struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
        if(!task) return -EINVAL;
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
};

pthread_once_t RtaiApp::key_once;
pthread_key_t RtaiApp::key;
}

extern "C" RtapiApp *make();

RtapiApp *make() {
    rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using LXRT realtime\n");
    return app = new RtaiApp;
}
