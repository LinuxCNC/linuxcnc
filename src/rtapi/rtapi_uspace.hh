/*    This is a component of LinuxCNC
 *    Copyright 2014 Jeff Epler <jepler@unpythonic.net>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef RTAPI_USPACE_HH
#define RTAPI_USPACE_HH
#ifdef __linux__
#include <sys/fsuid.h>
#endif
#include <unistd.h>
#include <pthread.h>
#include <atomic>

inline void rtapi_timespec_add(timespec &result, const timespec &ta, const timespec &tb) {
    result.tv_sec = ta.tv_sec + tb.tv_sec;
    result.tv_nsec = ta.tv_nsec + tb.tv_nsec;
    if (result.tv_nsec >= 1000000000) {
        result.tv_sec++;
        result.tv_nsec -= 1000000000;
    }
}

inline bool rtapi_timespec_less(const struct timespec &ta, const struct timespec &tb) {
    if(ta.tv_sec < tb.tv_sec) return 1;
    if(ta.tv_sec > tb.tv_sec) return 0;
    return ta.tv_nsec < tb.tv_nsec;
}

void rtapi_timespec_advance(struct timespec &result, const struct timespec &src, unsigned long nsec);

struct WithRoot
{
    WithRoot();
    ~WithRoot();
    static std::atomic<int> level;
};

struct rtapi_task {
  rtapi_task();

  int magic;			/* to check for valid handle */
  int id;
  int owner;
  int uses_fp;
  size_t stacksize;
  int prio;
  long period;
  struct timespec nextstart;
  unsigned ratio;
  void *arg;
  void (*taskcode) (void*);	/* pointer to task function */
};

struct RtapiApp
{

    RtapiApp(int policy = SCHED_OTHER) : policy(policy), period(0) {}

    int prio_highest();
    int prio_lowest();
    int prio_next_higher(int prio);
    int prio_next_lower(int prio);
    long clock_set_period(long int period_nsec);
    int task_new(void (*taskcode)(void*), void *arg,
            int prio, int owner, unsigned long int stacksize, int uses_fp);
    virtual rtapi_task *do_task_new() = 0;
    static int allocate_task_id();
    static struct rtapi_task *get_task(int task_id);
    void unexpected_realtime_delay(rtapi_task *task, int nperiod=1);
    virtual int task_delete(int id) = 0;
    virtual int task_start(int task_id, unsigned long period_nsec) = 0;
    virtual int task_pause(int task_id) = 0;
    virtual int task_resume(int task_id) = 0;
    virtual int task_self() = 0;
    virtual void wait() = 0;
    virtual unsigned char do_inb(unsigned int port) = 0;
    virtual void do_outb(unsigned char value, unsigned int port) = 0;
    virtual int run_threads(int fd, int (*callback)(int fd)) = 0;
    virtual long long do_get_time(void) = 0;
    virtual void do_delay(long ns) = 0;
    int policy;
    long period;
};

template<class T=rtapi_task>
T *rtapi_get_task(int task_id) {
    return static_cast<T*>(RtapiApp::get_task(task_id));
}

#define MAX_TASKS  64
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define TASK_MAGIC_INIT   ((rtapi_task*)(-1))

extern struct rtapi_task *task_array[MAX_TASKS];

#define WITH_ROOT WithRoot root
#endif
