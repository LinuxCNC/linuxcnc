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
*
* Last change:
********************************************************************/

#include "emcmotcfg.h"		/* EMCMOT_ERROR_NUM,LEN */
#include "motion.h"		/* these decls */
#include "dbuf.h"
#include "stashf.h"

int emcmotErrorInit(emcmot_error_t * errlog)
{
    if (errlog == 0) {
	return -1;
    }

    errlog->head = 0;
    errlog->start = 0;
    errlog->end = 0;
    errlog->num = 0;
    errlog->tail = 0;

    return 0;
}

int emcmotErrorPutfv(emcmot_error_t * errlog, const char *fmt, va_list ap)
{
    struct dbuf errbuf;
    struct dbuf_iter it;

    if (errlog == 0 || errlog->num == EMCMOT_ERROR_NUM) {
	/* full */
	return -1;
    }

    errlog->head++;

    dbuf_init(&errbuf, (unsigned char*)errlog->error[errlog->end], EMCMOT_ERROR_LEN);
    dbuf_iter_init(&it, &errbuf);
    vstashf(&it, fmt, ap);

    errlog->end = (errlog->end + 1) % EMCMOT_ERROR_NUM;
    errlog->num++;

    errlog->tail = errlog->head;

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
    if (errlog == 0 || errlog->num == 0) {
	/* empty */
	return -1;
    }

    errlog->head++;
    memcpy(error, errlog->error[errlog->start], EMCMOT_ERROR_LEN);
    errlog->start = (errlog->start + 1) % EMCMOT_ERROR_NUM;
    errlog->num--;
    errlog->tail = errlog->head;

    return 0;
}
