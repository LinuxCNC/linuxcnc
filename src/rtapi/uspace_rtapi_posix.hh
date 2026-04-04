#include "uspace_rtapi.hh"

struct Posix : RtapiApp
{
    Posix(int policy = SCHED_FIFO);
    int task_delete(int id);
    int task_start(int task_id, unsigned long period_nsec);
    int task_pause(int task_id);
    int task_resume(int task_id);
    int task_self();
    long long task_pll_get_reference(void);
    int task_pll_set_correction(long value);
    void wait();
    struct rtapi_task *do_task_new();
    unsigned char do_inb(unsigned int port);
    void do_outb(unsigned char value, unsigned int port);
    int run_threads(int fd, int (*callback)(int fd));
    static void *wrapper(void *arg);
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

    long long do_get_time(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    }

    void do_delay(long ns);
};