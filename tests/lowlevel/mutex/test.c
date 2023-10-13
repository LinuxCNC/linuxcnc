#include <rtapi_mutex.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

// to make this test fail, you can...
#if 0
#define rtapi_mutex_give(x) (void)0
#define rtapi_mutex_get(x) (void)0
#endif

const unsigned int N = 100000;
unsigned long mut;
unsigned long count;

void *thread_fun(void *unused) {
    for(unsigned long i=0; i<N; i++) {
        rtapi_mutex_get(&mut);
            unsigned long tmp = count;
            tmp++;
            sched_yield();
            count = tmp;
        rtapi_mutex_give(&mut);
    }
    return unused;
}

int main() {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_fun, NULL);
    pthread_create(&t2, NULL, thread_fun, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("count=%lu\n", count);
    assert(count == 2*N);
    printf("test passed\n");
    return 0;
}
