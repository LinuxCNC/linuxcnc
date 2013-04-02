#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>


#include "ring.h"
typedef struct {
    int serial;
    int realsize;
    char fluff[10000];
} elem_t;

static size_t rb_size = 16384;
static size_t e_size = sizeof(int);
static int iterations;
int underruns, overruns;

int ringops;
clock_t start,end;

void *producer(void *arg)
{
    int i = 0;
    elem_t e = { .serial = 0};
    ringbuffer_t *r = (ringbuffer_t *)arg;

    e.realsize = e_size;
    for (i = 0; i <= iterations; ++i) {
	if (ringops)
	    while (ring_write(r, (void *)&e, e_size)) {
		overruns++;
	    };
	e.serial += 1;
    }
    return NULL;
}

void *consumer(void *arg)
{
    int i = 0;
    ring_size_t size = e_size;
    elem_t *ep;
    int expect = 0;
    ringbuffer_t *r = (ringbuffer_t *)arg;

    for (i = 0; i <= iterations; ++i) {
	if (ringops) {
	    while ((size = ring_next_size(r)) < 0)
		underruns++;
	    ep = ring_next(r);
	    assert(expect == ep->serial);
	}
	assert(size == e_size);
	if (ringops) {
	    expect++;
	    ring_shift(r);
	}
    }
    return NULL;
}

int main(int argc, char **argv)
{
    ringbuffer_t ring1;
    ringbuffer_t *rb = &ring1;
    pthread_t c, p;
    double time1, time2;

    if (argc != 4) {
	fprintf(stderr, "%s: <iterations> <buffer size> <elemnt size>\n", argv[0]);
	return 1;
    }
    iterations = strtoul(argv[1],NULL,0);
    rb_size = strtoul(argv[2],NULL,0);
    e_size = strtoul(argv[3],NULL,0);

    assert(rb_size >= 16);
    assert(e_size <= sizeof(elem_t));

    assert(ring_init(rb, rb_size, 0) == 0);
    start = clock();
    assert(!pthread_create(&c, NULL, consumer, rb));
    assert(!pthread_create(&p, NULL, producer, rb));
    assert(!pthread_join(p, NULL));
    assert(!pthread_join(c, NULL));
    end = clock();
    time1 = ((double) (end - start)) / CLOCKS_PER_SEC;

    ringops = 1;

    start = clock();
    assert(!pthread_create(&c, NULL, consumer, rb));
    assert(!pthread_create(&p, NULL, producer, rb));
    assert(!pthread_join(p, NULL));
    assert(!pthread_join(c, NULL));
    end = clock();
    time2 = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("dt=%fs, nsecs per msg: %g\n", time2-time1, (time2-time1)*1e9/iterations);
    printf("overruns %d underruns %d\n", overruns, underruns);
    exit(0);
}
