/*
  emcmotlog.c

  Definitions for EMC data logging functions

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

#include "emcmotlog.h"

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

/* ident tag */
static char __attribute__ ((unused)) ident[] =
    "$Id$";

int emcmotLogInit(EMCMOT_LOG * log, int type, int size)
{
    log->type = type;
    log->size = size;
    log->start = 0;
    log->end = 0;
    log->howmany = 0;

    return 0;
}

int emcmotLogAdd(EMCMOT_LOG * log, EMCMOT_LOG_STRUCT val)
{
    log->log[log->end] = val;

    log->end++;
    if (log->end >= log->size) {
	log->end = 0;
    }

    log->howmany++;
    if (log->howmany > log->size) {
	log->howmany = log->size;
	log->start++;
	if (log->start >= log->size) {
	    log->start = 0;
	}
    }

    return 0;
}

int emcmotLogGet(EMCMOT_LOG * log, EMCMOT_LOG_STRUCT * val)
{
    if (log->howmany == 0) {
	return -1;
    }

    *val = log->log[log->start];
    log->start++;
    if (log->start >= log->size) {
	log->start = 0;
    }

    log->howmany--;

    return 0;
}
