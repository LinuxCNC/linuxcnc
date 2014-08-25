#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private API decls */
#include "hal_ring.h"	        /* ringbuffer declarations */

volatile int go, done, rdone = 0;
static int comp_id;		/* component ID */
struct timeval t1, t2;

typedef struct {
    int val;
    int tid;
    char extra[0];
} value_t;

typedef struct {
    pthread_t self;
    int id;
    ringbuffer_t *r;
    int ctr;
    int wfail;
    int wlocked;
} prodinfo_t;

typedef struct {
    pthread_t self;
    int id;
    ringbuffer_t *r;
    int rfail;
    int rcnt;
    int rlocked;
} consinfo_t;

volatile int *ep;


static char *option_string = "p:c:r:t:dhmRSs:ve:";
static struct option long_options[] = {
    {"num-producers", no_argument, 0, 'p'},
    {"num-consumers", no_argument, 0, 'c'},
    {"runtime", no_argument, 0, 't'},
    {"rtapi-msg-level", no_argument, 0, 'r'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"size", required_argument, 0, 's'},
    {"extra", required_argument, 0, 'e'},
    {"help", no_argument, 0, 'h'},
    {"use-mutex", no_argument, 0, 'm'},
    {"use-rtapi-shm", no_argument, 0, 'R'},
    {"stream-mode", no_argument, 0, 'S'},
    {0,0,0,0}
};

struct conf {
    int msglevel;
    int debug;
    int verbose;
    int n_producers;
    int n_consumers;
    int runtime;
    int use_mutex;
    int alloc;
    int mode;
    int size;
    int extra;
} conf = {
    .verbose = 0,
    .debug = 0,
    .runtime = 10,
    .n_producers = 1,
    .n_consumers = 1,
    .msglevel = -1,
    .use_mutex = 0,
    .alloc =  0, // default HAL memory
    .mode = 0, // default record mode
    .size = 16384,
    .extra = 0,
};


void usage(int argc, char **argv) {
    printf("Usage:  %s [options]\n", argv[0]);
    printf("This is a userspace HAL program, typically loaded using the halcmd \"loadusr\" command:\n"
	   "    loadusr halreport [options]\n"
	   "Options are:\n"
	   "-I or --ini <inifile>\n"
	   "    Use <inifile> (default: take ini filename from environment variable INI_FILE_NAME)\n"
	   "-S or --section <section-name> (default 8)\n"
	   "    Read parameters from <section_name> (default 'VFS11')\n"
	   "-r or --rtapi-msg-level <level>\n"
	   "    set the RTAPI message level.\n"
	   "-d or --debug\n"
	   "    Turn on event debugging messages.\n");
}

void *timer(void *arg)
{
    int seconds = *((int *) arg);
    if (conf.verbose)
	printf("timer start\n");
    gettimeofday(&t1, NULL);

    go = 1;
    sleep(seconds);
    done = 1;
    if (conf.verbose)
	printf("timer expired\n");
    return 0;
}

void *producer(void *arg)
{
    prodinfo_t *p = arg;
    value_t v;
    int retval;

    v.tid = p->id;
    v.val = p->ctr;

    if (conf.verbose)
	printf("producer %d start\n",p->id);

    while (!go);

    if (conf.verbose)
	printf("producer %d go\n",p->id);

    while (!done) {
	if (p->r->header->use_wmutex && rtapi_mutex_try(&p->r->header->wmutex)) {
	    p->wlocked++;
	    sched_yield();
	    continue;
	}
	if (conf.mode == MODE_STREAM) {
	    if (stream_write(p->r, (const char *)&v, sizeof(v)) != sizeof(v)) {
		p->wfail++;
	    } else {
		if (conf.verbose)
		    printf("producer %d write %d\n",p->id,v.val);
		p->ctr++;
		v.val = p->ctr;
	    }
	    if (p->r->header->use_wmutex)
		rtapi_mutex_give(&p->r->header->wmutex);
	    p->ctr++;
	} else {

	    retval = record_write(p->r, &v, sizeof(v));
	    if (retval)
		p->wfail++;
	    else {
		if (conf.verbose)
		    printf("producer %d write %d\n",p->id,v.val);
		p->ctr++;
		v.val = p->ctr;
	    }
	    if (p->r->header->use_wmutex)
		rtapi_mutex_give(&p->r->header->wmutex);

	}
    }
    return 0;
}

void *consumer(void *arg)
{
    consinfo_t *c = arg;
    const value_t *vp;
    ring_size_t size;

    if (conf.verbose)
	printf("consumer %d start\n",c->id);

    while (!go);

    if (conf.verbose)
	printf("consumer %d go\n",c->id);

    while (1) {
	if (c->r->header->use_rmutex && rtapi_mutex_try(&c->r->header->rmutex)) {
	    sched_yield();
	    c->rlocked++;
	    continue;
	}
	if (conf.mode == MODE_STREAM) {
	} else {
	    size = record_next_size(c->r);
	    if (size < 0) {
		if (rdone) {
		    if (c->r->header->use_rmutex)
			rtapi_mutex_give(&c->r->header->rmutex);
		    break;
		}
		c->rfail++;
	    } else {
		assert(size == sizeof(value_t));
		vp = record_next(c->r);
		assert(vp->val == __sync_fetch_and_add (&ep[vp->tid],1));
		record_shift(c->r);
		c->rcnt++;
	    }
	    if (c->r->header->use_rmutex)
		rtapi_mutex_give(&c->r->header->rmutex);
	}
    }
    return 0;
}

const char *ringname = "testring";

int main(int argc, char **argv)
{
    int retval;
    pthread_t t;
    int opt,i;
    ringbuffer_t rb;
    prodinfo_t *pi;
    consinfo_t *ci;
    int swfail,srfail;
    int swlock, srlock;
    int stx,srx;
    double elapsedTime;

    while ((opt = getopt_long(argc, argv, option_string,
			      long_options, NULL)) != -1) {
	switch(opt) {
	case 'd':
	    conf.debug++;
	    break;
	case 'v':
	    conf.verbose++;
	    break;
	case 'm':
	    conf.use_mutex = 1;
	    break;
	case 's':
	    conf.size = atoi(optarg);
	    break;
	case 'e':
	    conf.extra = atoi(optarg);
	    break;
	case 'p':
	    conf.n_producers = atoi(optarg);
	    break;
	case 'c':
	    conf.n_consumers = atoi(optarg);
	    break;
	case 't':
	    conf.runtime = atoi(optarg);
	    break;
	case 'r':
	    conf.msglevel = atoi(optarg);
	    break;
	case 'S':
	    conf.mode = MODE_STREAM;
	    break;
	case 'h':
	default:
	    usage(argc, argv);
	    exit(0);
	}
    }
    if (conf.msglevel > -1)
	rtapi_set_msg_level(conf.msglevel);

    comp_id = hal_init("ringbench");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ringbench: ERROR: hal_init() failed: %d\n", comp_id);
	return -1;
    }
    assert((ci = calloc(sizeof(consinfo_t),conf.n_consumers)) != NULL);
    assert((pi = calloc(sizeof(prodinfo_t),conf.n_producers)) != NULL);
    assert((ep = calloc(sizeof(int),conf.n_producers)) != NULL);

    if ((retval = hal_ring_new(ringname, conf.size, 0, comp_id, conf.mode | conf.alloc))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ringbench: failed to create new ring %s: %d\n",
			ringname, retval);
    }
    if ((retval = hal_ring_attach(ringname, &rb, comp_id, NULL))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ringbench: failed to attach to ring %s: %d\n",
			ringname, retval);
    }

    hal_ready(comp_id);

    rb.header->use_wmutex = (conf.n_producers > 1);
    rb.header->use_rmutex = (conf.n_consumers > 1);

    for(i = 0; i < conf.n_producers; i++) {
	pi[i].id = i;
	pi[i].r = &rb;
	assert(!pthread_create(&pi[i].self, NULL, producer, (void *) &pi[i]));
    }
    for(i = 0; i < conf.n_consumers; i++) {
	ci[i].id = i;
	ci[i].r = &rb;
	assert(!pthread_create(&ci[i].self, NULL, consumer, (void *) &ci[i]));
    }
    assert(!pthread_create(&t, NULL, timer, (void *) &conf.runtime));
    assert(!pthread_join(t, NULL));
    if (conf.verbose)
	printf("timer joined\n");

    swfail = 0;
    stx = 0;
    swlock = 0;
    for(i = 0; i < conf.n_producers; i++) {
	assert(!pthread_join(pi[i].self, NULL));
	if (conf.verbose)
	    printf("producer %d joined wfail=%d ctr=%d\n",pi[i].id,pi[i].wfail,pi[i].ctr);
	swfail += pi[i].wfail;
	stx += pi[i].ctr;
	swlock += pi[i].wlocked;
    }
    rdone = 1;
    srfail = 0;
    srx = 0;
    srlock = 0;
    for(i = 0; i < conf.n_consumers; i++) {
	assert(!pthread_join(ci[i].self, NULL));
	srfail += ci[i].rfail;
	srx += ci[i].rcnt;
	srlock += ci[i].rlocked;
	if (conf.verbose)
	    printf("consumer %d joined rfail=%d ctr=%d\n",ci[i].id,ci[i].rfail,ci[i].rcnt);
    }
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;

    printf("tx=%d rx=%d txfail=%d rxfail=%d wlock=%d rlock=%d\n",stx,srx,swfail,srfail,swlock,srlock);
    printf("dt=%fs, nsecs per msg: %g\n", elapsedTime/1000.0, (elapsedTime)*1e6/(srx));

    for(i = 0; i < conf.n_producers; i++) {
	if (pi[i].ctr != ep[i])
	    printf("p %d: ctr=%d ep=%d\n",i,pi[i].ctr,ep[i]);
	assert(pi[i].ctr == ep[i]);
    }
    hal_ring_detach(ringname, &rb);
    hal_exit(comp_id);
    exit(0);
}
