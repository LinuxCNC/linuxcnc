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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef RTAPI_USPACE_HH
#define RTAPI_USPACE_HH
#include <sys/fsuid.h>
#include <unistd.h>
#include <pthread.h>
#include <atomic>

struct WithRoot
{
    WithRoot();
    ~WithRoot();
    static std::atomic<int> level;
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
    static int allocate_task();
    static struct rtapi_task *get_task(int task_id);
    virtual int task_delete(int id) = 0;
    virtual int task_start(int task_id, unsigned long period_nsec) = 0;
    virtual int task_pause(int task_id) = 0;
    virtual int task_resume(int task_id) = 0;
    virtual int task_self() = 0;
    virtual void wait() = 0;
    virtual unsigned char do_inb(unsigned int port) = 0;
    virtual void do_outb(unsigned char value, unsigned int port) = 0;
    virtual int run_threads(int fd, int (*callback)(int fd)) = 0;
    int policy;
    long period;
};

#define MAX_TASKS  64
#define TASK_MAGIC    21979	/* random numbers used as signatures */
#define TASK_MAGIC_INIT   ~21979

struct rtapi_task {
  int magic;			/* to check for valid handle */
  int owner;
  union {
      pthread_t thr;                /* thread's context */
      void *appspecific;          /* app-specific storage */
  };
  size_t stacksize;
  int prio;
  long period;
  struct timespec nextstart;
  unsigned ratio;
  void *arg;
  void (*taskcode) (void*);	/* pointer to task function */
};

extern struct rtapi_task task_array[MAX_TASKS];

#define WITH_ROOT WithRoot root
#endif
