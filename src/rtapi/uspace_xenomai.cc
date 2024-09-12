#include "config.h"
#include "rtapi.h"
#include "rtapi_uspace.hh"
#include <pthread.h>
#include  <errno.h>
#include <stdio.h>
#include <cstring>
#include <atomic>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

namespace
{
struct RtaiTask : rtapi_task {
    RtaiTask() : rtapi_task{}, cancel{}, thr{} {}
    std::atomic<int> cancel;
    pthread_t thr;
};


struct XenomaiApp : RtapiApp {
    XenomaiApp() : RtapiApp(SCHED_FIFO) {
        pthread_once(&key_once, init_key);
    }

    RtaiTask *do_task_new() {
        return new RtaiTask;
    }

    int task_delete(int id) {
        auto task = ::rtapi_get_task<RtaiTask>(id);
        if(!task) return -EINVAL;

        task->cancel = 1;
        pthread_join(task->thr, nullptr);
        task->magic = 0;
        task_array[id] = 0;
        delete task;
        return 0;
    }

    int task_start(int task_id, unsigned long period_nsec) {
        auto task = ::rtapi_get_task<RtaiTask>(task_id);
        if(!task) return -EINVAL;

        task->period = period_nsec;
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        param.sched_priority = task->prio;

        // limit PLL correction values to +/-1% of cycle time
        task->pll_correction_limit = period_nsec / 100;
        task->pll_correction = 0;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        int nprocs = sysconf( _SC_NPROCESSORS_ONLN );
        CPU_SET(nprocs-1, &cpuset); // assumes processor numbers are contiguous

        int ret;
        pthread_attr_t attr;
        if((ret = pthread_attr_init(&attr)) != 0)
            return -ret;
        if((ret = pthread_attr_setstacksize(&attr, task->stacksize)) != 0)
            return -ret;
        if((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0)
            return -ret;
        if((ret = pthread_attr_setschedparam(&attr, &param)) != 0)
            return -ret;
        if((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0)
            return -ret;
        if(nprocs > 1)
            if((ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
                return -ret;
        if((ret = pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void*>(task))) != 0)
            return -ret;

        return 0;
    }

    static void *wrapper(void *arg) {
        auto task = reinterpret_cast<RtaiTask*>(arg);
        pthread_setspecific(key, arg);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // originally, I used pthread_make_periodic_np here, and
        // pthread_wait_np in wait(), but in about 1 run in 50 this led to
        // "xenomai: watchdog triggered" and rtapi_app was killed.
        //
        // encountered on: 3.18.20-xenomai-2.6.5 with a 2-thread SMP system
        rtapi_timespec_advance(task->nextstart, now, task->period + task->pll_correction);

        (task->taskcode) (task->arg);

        rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
        return nullptr;
    }

    int task_pause(int task_id) {
        return -ENOSYS;
    }

    int task_resume(int task_id) {
        return -ENOSYS;
    }

    long long task_pll_get_reference(void) {
        struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
        if(!task) return 0;
        return task->nextstart.tv_sec * 1000000000LL + task->nextstart.tv_nsec;
    }

    int task_pll_set_correction(long value) {
        struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
        if(!task) return -EINVAL;
        if (value > task->pll_correction_limit) value = task->pll_correction_limit;
        if (value < -(task->pll_correction_limit)) value = -(task->pll_correction_limit);
        task->pll_correction = value;
        return 0;
    }

    void wait() {
        int task_id = task_self();
        auto task = ::rtapi_get_task<RtaiTask>(task_id);
        if(task->cancel) {
            pthread_exit(nullptr);
        }
        rtapi_timespec_advance(task->nextstart, task->nextstart, task->period + task->pll_correction);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(rtapi_timespec_less(task->nextstart, now))
        {
            if(policy == SCHED_FIFO)
                unexpected_realtime_delay(task);
        }
        else
        {
            int res = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task->nextstart, nullptr);
            if(res < 0) perror("clock_nanosleep");
        }
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
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    }

    void do_delay(long ns) {
        struct timespec ts = {0, ns};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
    }
};

pthread_once_t XenomaiApp::key_once;
pthread_key_t XenomaiApp::key;
}

extern "C" RtapiApp *make();

RtapiApp *make() {
    rtapi_print_msg(RTAPI_MSG_ERR, "Note: Using XENOMAI (posix-skin) realtime\n");
    return new XenomaiApp;
}
