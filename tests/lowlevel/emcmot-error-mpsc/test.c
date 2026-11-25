/*
 * Concurrency / correctness test for the emcmotError MPSC ring buffer.
 *
 * Spawns NPROD producer threads pumping NMSG messages each, plus one
 * consumer that drains. Verifies every message arrives exactly once
 * and that messages from each individual producer arrive in producer-
 * local sequence order. Across producers any interleaving is allowed.
 */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <emcmotcfg.h>
#include "motion.h"
#include "dbuf.h"
#include "stashf.h"
#include <rtapi_atomic.h>

#define NPROD 8
#define NMSG  100000

static emcmot_error_t errlog;
static rtapi_atomic_int producers_done = 0;

static void *producer_fn(void *arg) {
    long id = (long)arg;
    char buf[64];
    for (int i = 0; i < NMSG; i++) {
        snprintf(buf, sizeof(buf), "P%ld:%010d", id, i);
        while (emcmotErrorPut(&errlog, buf) < 0) {
            sched_yield();
        }
    }
    atomic_fetch_add(&producers_done, 1);
    return NULL;
}

static void *consumer_fn(void *arg) {
    (void)arg;
    long total = (long)NPROD * NMSG;
    long received = 0;
    int per_producer[NPROD] = {0};
    char raw[EMCMOT_ERROR_LEN];
    char msg[EMCMOT_ERROR_LEN];
    int errors = 0;

    while (received < total) {
        struct dbuf d;
        struct dbuf_iter di;
        /* dbuf_init zero-fills the buffer, so call it BEFORE
           emcmotErrorGet, which then writes the received bytes in. */
        dbuf_init(&d, (unsigned char *)raw, EMCMOT_ERROR_LEN);
        if (emcmotErrorGet(&errlog, raw) == 0) {
            dbuf_iter_init(&di, &d);
            if (snprintdbuf(msg, sizeof(msg), &di) < 0) {
                fprintf(stderr, "snprintdbuf failed\n");
                errors++;
                received++;
                continue;
            }
            long pid;
            int seq;
            if (sscanf(msg, "P%ld:%d", &pid, &seq) != 2
                || pid < 0 || pid >= NPROD) {
                fprintf(stderr, "bad msg: '%s'\n", msg);
                errors++;
            } else if (seq != per_producer[pid]) {
                fprintf(stderr, "out-of-order P%ld: got %d, want %d\n",
                        pid, seq, per_producer[pid]);
                errors++;
            } else {
                per_producer[pid]++;
            }
            received++;
        } else {
            if (atomic_load(&producers_done) >= NPROD) {
                struct timespec ts = {0, 1000000};
                nanosleep(&ts, NULL);
            } else {
                sched_yield();
            }
        }
    }

    if (errors == 0) {
        printf("test passed\n");
    } else {
        printf("test FAILED with %d errors\n", errors);
    }
    return errors == 0 ? (void *)0 : (void *)1;
}

int main(void) {
    emcmotErrorInit(&errlog);

    pthread_t prods[NPROD];
    pthread_t cons;
    pthread_create(&cons, NULL, consumer_fn, NULL);
    for (long i = 0; i < NPROD; i++) {
        pthread_create(&prods[i], NULL, producer_fn, (void *)i);
    }
    for (long i = 0; i < NPROD; i++) {
        pthread_join(prods[i], NULL);
    }
    void *cret;
    pthread_join(cons, &cret);
    return cret ? 1 : 0;
}
