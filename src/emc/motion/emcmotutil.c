/********************************************************************
* Description: emcmotutil.c
*   Utility functions shared between motion and other systems
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include <emcmotcfg.h>		/* EMCMOT_ERROR_NUM,LEN */
#include "motion.h"		/* these decls */
#include "dbuf.h"
#include "stashf.h"

/*
 * Lockfree multi-producer single-consumer (MPSC) ring buffer.
 *
 * Producers (real-time threads invoked via the rtapi message handler) may
 * race; the consumer (the userspace task) is single-threaded.
 *
 * Producer:
 *   1. CAS-loop on write_reserve to claim seq S; bail if buffer full.
 *   2. Write payload into slot S % EMCMOT_ERROR_NUM.
 *   3. fetch_add(write_commit, 1) with release ordering.
 * Consumer:
 *   1. If write_reserve != write_commit, some producer is mid-write; skip
 *      (return -1) so the RT producer is never blocked. Next consumer call
 *      will retry.
 *   2. Otherwise drain one slot at read_seq, increment read_seq.
 */

int emcmotErrorInit(emcmot_error_t * errlog)
{
    if (errlog == NULL) {
	return -1;
    }

    errlog->write_reserve = 0;
    errlog->write_commit  = 0;
    errlog->read_seq      = 0;
    atomic_thread_fence(memory_order_release);

    return 0;
}

int emcmotErrorPutfv(emcmot_error_t * errlog, const char *fmt, va_list ap)
{
    struct dbuf errbuf;
    struct dbuf_iter it;
    unsigned long long my_seq;

    if (errlog == NULL) {
	return -1;
    }

    {
	unsigned long long w = atomic_load_explicit(&errlog->write_reserve,
	                                            memory_order_relaxed);
	for (;;) {
	    unsigned long long r = atomic_load_explicit(&errlog->read_seq,
	                                                memory_order_acquire);
	    if (w - r >= (unsigned long long)EMCMOT_ERROR_NUM) {
		return -1;
	    }
	    if (atomic_compare_exchange_weak_explicit(
	            &errlog->write_reserve, &w, w + 1,
	            memory_order_acquire, memory_order_relaxed)) {
		my_seq = w;
		break;
	    }
	}
    }

    dbuf_init(&errbuf,
              (unsigned char*)errlog->error[my_seq % EMCMOT_ERROR_NUM],
              EMCMOT_ERROR_LEN);
    dbuf_iter_init(&it, &errbuf);
    vstashf(&it, fmt, ap);

    atomic_fetch_add_explicit(&errlog->write_commit, 1ULL,
                              memory_order_release);

    return 0;
}

int emcmotErrorPutf(emcmot_error_t *errlog, const char *fmt, ...)
{
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = emcmotErrorPutfv(errlog, fmt, ap);
    va_end(ap);
    return result;
}

int emcmotErrorPut(emcmot_error_t *errlog, const char *error)
{
    return emcmotErrorPutf(errlog, "%s", error);
}

int emcmotErrorGet(emcmot_error_t * errlog, char *error)
{
    unsigned long long r;
    unsigned long long w_res;
    unsigned long long w_com;

    if (errlog == NULL) {
	return -1;
    }

    w_com = atomic_load_explicit(&errlog->write_commit, memory_order_acquire);
    w_res = atomic_load_explicit(&errlog->write_reserve,
                                 memory_order_acquire);
    if (w_res != w_com) {
	/* a producer is mid-write; try again next cycle */
	return -1;
    }

    r = atomic_load_explicit(&errlog->read_seq, memory_order_relaxed);
    if (r >= w_com) {
	return -1;
    }

    memcpy(error, errlog->error[r % EMCMOT_ERROR_NUM], EMCMOT_ERROR_LEN);
    atomic_store_explicit(&errlog->read_seq, r + 1, memory_order_release);

    return 0;
}
