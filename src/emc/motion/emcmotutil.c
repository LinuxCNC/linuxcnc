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

#include "emcmotcfg.h"		/* EMCMOT_ERROR_NUM,LEN */
#include "motion.h"		/* these decls */

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

int emcmotErrorPut(emcmot_error_t * errlog, const char *error)
{
    char *p1;
    const char *p2;
    int i;

    if (errlog == 0 || errlog->num == EMCMOT_ERROR_NUM) {
	/* full */
	return -1;
    }

    errlog->head++;

    // strncpy(errlog->error[errlog->end], error, EMCMOT_ERROR_LEN);
    // replaced strncpy with manual copy
    p1 = errlog->error[errlog->end];
    p2 = error;
    i = 0;
    while (*p2 && i < EMCMOT_ERROR_LEN) {
	*p1 = *p2;
	p1++;
	p2++;
	i++;
    }
    *p1 = 0;

    errlog->end = (errlog->end + 1) % EMCMOT_ERROR_NUM;
    errlog->num++;

    errlog->tail = errlog->head;

    return 0;
}

int emcmotErrorGet(emcmot_error_t * errlog, char *error)
{
    char *p1;
    const char *p2;
    int i;
    if (errlog == 0 || errlog->num == 0) {
	/* empty */
	return -1;
    }

    errlog->head++;

    // strncpy(error, errlog->error[errlog->start], EMCMOT_ERROR_LEN); 
    // replaced strncpy with manual copy
    p1 = error;
    p2 = errlog->error[errlog->start];
    i = 0;
    while (*p2 && i < EMCMOT_ERROR_LEN) {
	*p1 = *p2;
	p1++;
	p2++;
	i++;
    }
    *p1 = 0;

    errlog->start = (errlog->start + 1) % EMCMOT_ERROR_NUM;
    errlog->num--;

    errlog->tail = errlog->head;

    return 0;
}
