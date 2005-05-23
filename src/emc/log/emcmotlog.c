/********************************************************************
* Description: emcmotlog.c
*   Definitions for EMC data logging functions
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include "emcmotlog.h"

int emcmotLogInit(emcmot_log_t * log, int type, int size)
{
    log->type = type;
    log->size = size;
    log->start = 0;
    log->end = 0;
    log->howmany = 0;

    return 0;
}

int emcmotLogAdd(emcmot_log_t * log, emcmot_log_struct_t val)
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

int emcmotLogGet(emcmot_log_t * log, emcmot_log_struct_t * val)
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
