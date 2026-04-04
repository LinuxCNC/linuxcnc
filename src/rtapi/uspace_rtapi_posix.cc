#include "uspace_rtapi_posix.hh"
#include "rtapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

struct PosixTask : rtapi_task
{
    PosixTask() : rtapi_task{}, thr{}
    {}

    pthread_t thr;                /* thread's context */
};

Posix::Posix(int policy) : RtapiApp(policy), do_thread_lock(policy != SCHED_FIFO) {
    pthread_once(&key_once, init_key);
    if(do_thread_lock) {
        pthread_once(&lock_once, init_lock);
    }
}

int Posix::task_delete(int id)
{
  auto task = ::rtapi_get_task<PosixTask>(id);
  if(!task) return -EINVAL;

  pthread_cancel(task->thr);
  pthread_join(task->thr, 0);
  task->magic = 0;
  task_array[id] = 0;
  delete task;
  return 0;
}

int Posix::task_start(int task_id, unsigned long int period_nsec)
{
  auto task = ::rtapi_get_task<PosixTask>(task_id);
  if(!task) return -EINVAL;

  if(period_nsec < (unsigned long)period) period_nsec = (unsigned long)period;
  task->period = period_nsec;
  task->ratio = period_nsec / period;

  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = task->prio;

  // limit PLL correction values to +/-1% of cycle time
  task->pll_correction_limit = period_nsec / 100;
  task->pll_correction = 0;

  int nprocs = sysconf( _SC_NPROCESSORS_ONLN );

  pthread_attr_t attr;
  int ret;
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
  if(nprocs > 1) {
      const static int rt_cpu_number = find_rt_cpu_number();
      rtapi_print_msg(RTAPI_MSG_INFO, "rt_cpu_number = %i\n", rt_cpu_number);
      if(rt_cpu_number != -1) {
#ifdef __FreeBSD__
          cpuset_t cpuset;
#else
          cpu_set_t cpuset;
#endif
          CPU_ZERO(&cpuset);
          CPU_SET(rt_cpu_number, &cpuset);
          if((ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset)) != 0)
               return -ret;
      }
  }
  if((ret = pthread_create(&task->thr, &attr, &wrapper, reinterpret_cast<void*>(task))) != 0)
      return -ret;

  return 0;
}

#define RTAPI_CLOCK (CLOCK_MONOTONIC)

pthread_once_t Posix::key_once = PTHREAD_ONCE_INIT;
pthread_once_t Posix::lock_once = PTHREAD_ONCE_INIT;
pthread_key_t Posix::key;
pthread_mutex_t Posix::thread_lock;

extern RtapiApp &App(); //ToDo: Nicer

void *Posix::wrapper(void *arg)
{
  struct rtapi_task *task;

  /* use the argument to point to the task data */
  task = (struct rtapi_task*)arg;
  long int period = App().period;
  if(task->period < period) task->period = period;
  task->ratio = task->period / period;
  task->period = task->ratio * period;
  rtapi_print_msg(RTAPI_MSG_INFO, "task %p period = %lu ratio=%u\n",
	  task, task->period, task->ratio);

  pthread_setspecific(key, arg);
  set_namef("rtapi_app:T#%d", task->id);

  Posix &papp = reinterpret_cast<Posix&>(App());
  if(papp.do_thread_lock)
      pthread_mutex_lock(&papp.thread_lock);

  struct timespec now;
  clock_gettime(RTAPI_CLOCK, &now);
  rtapi_timespec_advance(task->nextstart, now, task->period + task->pll_correction);

  /* call the task function with the task argument */
  (task->taskcode) (task->arg);

  rtapi_print("ERROR: reached end of wrapper for task %d\n", task->id);
  return NULL;
}

long long Posix::task_pll_get_reference(void) {
    struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
    if(!task) return 0;
    return task->nextstart.tv_sec * 1000000000LL + task->nextstart.tv_nsec;
}

int Posix::task_pll_set_correction(long value) {
    struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
    if(!task) return -EINVAL;
    if (value > task->pll_correction_limit) value = task->pll_correction_limit;
    if (value < -(task->pll_correction_limit)) value = -(task->pll_correction_limit);
    task->pll_correction = value;
    return 0;
}

int Posix::task_pause(int) {
    return -ENOSYS;
}

int Posix::task_resume(int) {
    return -ENOSYS;
}

int Posix::task_self() {
    struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
    if(!task) return -EINVAL;
    return task->id;
}

void Posix::wait() {
    if(do_thread_lock)
        pthread_mutex_unlock(&thread_lock);
    pthread_testcancel();
    struct rtapi_task *task = reinterpret_cast<rtapi_task*>(pthread_getspecific(key));
    rtapi_timespec_advance(task->nextstart, task->nextstart, task->period + task->pll_correction);
    struct timespec now;
    clock_gettime(RTAPI_CLOCK, &now);
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
    if(do_thread_lock)
        pthread_mutex_lock(&thread_lock);
}

struct rtapi_task *Posix::do_task_new() {
    return new PosixTask;
}

unsigned char Posix::do_inb(unsigned int port)
{
#ifdef HAVE_SYS_IO_H
    return inb(port);
#else
    (void)port;
    return 0;
#endif
}

void Posix::do_outb(unsigned char val, unsigned int port)
{
#ifdef HAVE_SYS_IO_H
    return outb(val, port);
#else
    (void)val;
    (void)port;
#endif
}

void Posix::do_delay(long ns) {
    struct timespec ts = {0, ns};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
}

int Posix::run_threads(int fd, int(*callback)(int fd)) {
    while(callback(fd)) { /* nothing */ }
    return 0;
}