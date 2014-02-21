/********************************************************************
 * Description: tcq.c
 *\brief queue handling functions for trajectory planner
 * These following functions implement the motion queue that
 * is fed by tpAddLine/tpAddCircle and consumed by tpRunCycle.
 * They have been fully working for a long time and a wise programmer
 * won't mess with them.
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

/* queue of TC_STRUCT elements*/
#ifndef TCQ_H
#define TCQ_H

#include "tc_types.h"

typedef struct {
    TC_STRUCT *queue;	/* ptr to the tcs */
    int size;			/* size of queue */
    int _len;			/* number of tcs now in queue */
    int start, end;		/* indices to next to get, next to put */
    int allFull;		/* flag meaning it's actually full */
} TC_QUEUE_STRUCT;

/* TC_QUEUE_STRUCT functions */

/* create queue of _size */
extern int tcqCreate(TC_QUEUE_STRUCT * const tcq, int _size,
		     TC_STRUCT * const tcSpace);

/* free up queue */
extern int tcqDelete(TC_QUEUE_STRUCT * const tcq);

/* reset queue to empty */
extern int tcqInit(TC_QUEUE_STRUCT * const tcq);

/* put tc on end */
extern int tcqPut(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc);

/* remove a single tc from the back of the queue */
extern int tcqPopBack(TC_QUEUE_STRUCT * const tcq);

/* remove n tcs from front */
extern int tcqRemove(TC_QUEUE_STRUCT * const tcq, int n);

/* how many tcs on queue */
extern int tcqLen(TC_QUEUE_STRUCT const * const tcq);

/* look at nth item, first is 0 */
extern TC_STRUCT * tcqItem(TC_QUEUE_STRUCT const * const tcq, int n);

/**
 * Get the "end" of the queue, the most recently added item.
 */
extern TC_STRUCT * tcqLast(TC_QUEUE_STRUCT const * const tcq);

/* get full status */
extern int tcqFull(TC_QUEUE_STRUCT const * const tcq);

#endif
