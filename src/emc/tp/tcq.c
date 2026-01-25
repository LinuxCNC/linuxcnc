/*!
 ********************************************************************
 * Description: tcq.c
 *\brief queue handling functions for trajectory planner
 * These following functions implement the motion queue that
 * is fed by tpAddLine/tpAddCircle and consumed by tpRunCycle.
 * They have been fully working for a long time and a wise programmer
 * won't mess with them.
 *
 *\author Derived from a work by Fred Proctor & Will Shackleford
 *\author rewritten by Chris Radek
 *
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2004 All rights reserved.
 *
 ********************************************************************/

#include "tcq.h"
#include <stddef.h>
#include "motion.h"
#include "atomic_9d.h"
#include "rtapi.h"

/* Access to global emcmotStatus for planner_type checking */
extern emcmot_status_t *emcmotStatus;

#ifndef GET_TRAJ_PLANNER_TYPE
#define GET_TRAJ_PLANNER_TYPE() (emcmotStatus->planner_type)
#endif

/* Queue margin constants */
#define TCQ_REVERSE_MARGIN 200
#define TC_QUEUE_MARGIN (TCQ_REVERSE_MARGIN+20)

/** Return 0 if queue is valid, -1 if not */
static inline int tcqCheck(TC_QUEUE_STRUCT const * const tcq)
{
    if (0 == tcq)
    {
        return -1;
    }
    /* queue is embedded in struct, no need to check for NULL */
    return 0;
}

/*! tcqCreate() function
 *
 * \brief Creates a new queue for TC elements.
 *
 * This function initializes a queue for TC elements.
 * The queue array is embedded in the TC_QUEUE_STRUCT.
 * It gets called by tpCreate()
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 * @param	 _size	   size of the queue
 *
 * @return	 int	   returns success or failure
 */
int tcqCreate(TC_QUEUE_STRUCT * const tcq, int _size)
{
    if (!tcq || _size < 1) {
        return -1;
    }
    /* queue array is now embedded in struct, no pointer assignment needed */
	tcq->size = _size;
    tcqInit(tcq);

	return 0;
}

/*! tcqDelete() function
 *
 * \brief Deletes a queue holding TC elements.
 *
 * This function creates deletes a queue. It doesn't free the space
 * only throws the pointer away.
 * It gets called by tpDelete()
 * \todo FIXME, it seems tpDelete() is gone, and this function isn't used.
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success
 */
int tcqDelete(TC_QUEUE_STRUCT * const tcq)
{
    /* Queue is embedded in struct, nothing to free or clear */
    (void)tcq;  /* suppress unused parameter warning */
    return 0;
}

/*! tcqInit() function
 *
 * \brief Initializes a queue with TC elements.
 *
 * This function initializes a queue with TC elements.
 * It gets called by tpClear() and
 * 	  	   		  by tpRunCycle() when we are aborting
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success or failure (if no tcq found)
 */
int tcqInit(TC_QUEUE_STRUCT * const tcq)
{
    if (tcqCheck(tcq)) return -1;

    tcq->_len = 0;
    tcq->start = tcq->end = 0;
    tcq->rend = 0;
    tcq->_rlen = 0;
    tcq->allFull = 0;

    /* Initialize atomic indices for planner_type 2 */
    __atomic_store_n(&tcq->start_atomic, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&tcq->end_atomic, 0, __ATOMIC_SEQ_CST);

    return 0;
}

/*! tcqPut() function
 *
 * \brief puts a TC element at the end of the queue
 *
 * This function adds a tc element at the end of the queue.
 * It gets called by tpAddLine() and tpAddCircle()
 *
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 tc        the new TC element to be added
 *
 * @return	 int	   returns success or failure
 */
int tcqPut(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc)
{
    /* check for initialized */
    if (tcqCheck(tcq)) return -1;

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load current indices atomically */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Calculate next end index */
        int next_end = (current_end + 1) % tcq->size;

        /* Check if queue is full (with margin for reverse history) */
        int available_space = (current_start - next_end + tcq->size) % tcq->size;
        if (available_space < TCQ_REVERSE_MARGIN + 20) {
            return -1;  /* Queue full */
        }

        /* Copy TC element to queue */
        tcq->queue[current_end] = *tc;

        /* Atomically update end index (producer) - use exchange for stronger visibility */
        __atomic_exchange_n(&tcq->end_atomic, next_end, __ATOMIC_ACQ_REL);

        return 0;
    }

    /* Original mutex-based implementation for planner_type 0/1 */

    /* check for allFull, so we don't overflow the queue */
    if (tcq->allFull) {
	    return -1;
    }

    /* add it */
    tcq->queue[tcq->end] = *tc;
    tcq->_len++;

    /* update end ptr, modulo size of queue */
    tcq->end = (tcq->end + 1) % tcq->size;

    /* set allFull flag if we're really full */
    if (tcq->end == tcq->start) {
	tcq->allFull = 1;
    }

    return 0;
}


/*! tcqPopBack() function
 *
 * \brief removes the newest TC element (converse of tcqRemove)
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns success or failure
 */
int tcqPopBack(TC_QUEUE_STRUCT * const tcq)
{
    /* check for initialized */
    if (tcqCheck(tcq)) return -1;

    /* Too short to pop! */
    if (tcq->_len < 1) {
        return -1;
    }

    int n = tcq->end - 1 + tcq->size;
    tcq->end = n % tcq->size;
    tcq->_len--;

    return 0;
}

int tcqPop(TC_QUEUE_STRUCT * const tcq)
{

    if (tcqCheck(tcq)) {
        return -1;
    }

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load indices atomically */
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);

        /* Check if queue is empty */
        if (current_start == current_end) {
            return -1;
        }

        /* Atomically increment start index (RT consumer) - use exchange for stronger visibility */
        int new_start = (current_start + 1) % tcq->size;
        __atomic_exchange_n(&tcq->start_atomic, new_start, __ATOMIC_ACQ_REL);

        /* Note: For planner_type 2, reverse history is managed differently
         * by the userspace planning layer, so we don't update rend/rlen here */

        return 0;
    }

    /* Original implementation for planner_type 0/1 */

    if (tcq->_len < 1 && !tcq->allFull) {
        return -1;
    }

    /* update start ptr and reset allFull flag and len */
    tcq->start = (tcq->start + 1) % tcq->size;
    tcq->allFull = 0;
    tcq->_len--;

    if (tcq->_rlen < TCQ_REVERSE_MARGIN) {
        //If we're not overwriting the history yet, then we have another segment added to the reverse history
        tcq->_rlen++;
    } else {
        //If we're run out of spare reverse history, then advance rend
        tcq->rend = (tcq->rend + 1) % tcq->size;
    }

    return 0;
}

/*! tcqRemove() function
 *
 * \brief removes n items from the queue
 *
 * This function removes the first n items from the queue,
 * after checking that they can be removed
 * (queue initialized, queue not empty, enough elements in it)
 * Function gets called by tpRunCycle() with n=1
 * \todo FIXME: Optimize the code to remove only 1 element, might speed it up
 *
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 n         the number of TC elements to be removed
 *
 * @return	 int	   returns success or failure
 */
int tcqRemove(TC_QUEUE_STRUCT * const tcq, int n)
{

    if (n <= 0) {
	    return 0;		/* okay to remove 0 or fewer */
    }

    if (tcqCheck(tcq) || ((tcq->start == tcq->end) && !tcq->allFull) ||
            (n > tcq->_len)) {	/* too many requested */
	    return -1;
    }

    /* update start ptr and reset allFull flag and len */
    tcq->start = (tcq->start + n) % tcq->size;
    tcq->allFull = 0;
    tcq->_len -= n;

    return 0;
}


/**
 * Step backward into the reverse history.
 */
int tcqBackStep(TC_QUEUE_STRUCT * const tcq)
{

    if (tcqCheck(tcq)) {
        return -1;
    }

    // start == end means that queue is empty

    if ( tcq->start == tcq->rend) {	
        return -1;
    }
    /* update start ptr and reset allFull flag and len */
    tcq->start = (tcq->start - 1 + tcq->size) % tcq->size;
    tcq->_len++;
    tcq->_rlen--;

    return 0;
}

/*! tcqLen() function
 *
 * \brief returns the number of elements in the queue
 *
 * Function gets called by tpSetVScale(), tpAddLine(), tpAddCircle()
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int	   returns number of elements
 */
int tcqLen(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) return -1;

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Read producer index first (end), then consumer index (start) */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Calculate length with circular buffer wraparound */
        int len = (current_end - current_start + tcq->size) % tcq->size;
        return len;
    }

    /* Original implementation for planner_type 0/1 */
    return tcq->_len;
}

/*! tcqItem() function
 *
 * \brief gets the n-th TC element in the queue, without removing it
 *
 * Function gets called by tpSetVScale(), tpRunCycle(), tpIsPaused()
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 TC_STRUCT returns the TC elements
 */
TC_STRUCT * tcqItem(TC_QUEUE_STRUCT const * const tcq, int n)
{
    if (tcqCheck(tcq) || (n < 0)) return NULL;

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load indices atomically */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Calculate length and check bounds */
        int len = (current_end - current_start + tcq->size) % tcq->size;
        if (n >= len) return NULL;

        /* Return item at offset n from start */
        return &(tcq->queue[(current_start + n) % tcq->size]);
    }

    /* Original implementation for planner_type 0/1 */
    if (n >= tcq->_len) return NULL;
    return &(tcq->queue[(tcq->start + n) % tcq->size]);
}

/*! tcqFull() function
 *
 * \brief get the full status of the queue
 * Function returns full if the count is closer to the end of the queue than TC_QUEUE_MARGIN
 *
 * Function called by update_status() in control.c
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 int       returns status (0==not full, 1==full)
 */
int tcqFull(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) {
	   return 1;		/* null queue is full, for safety */
    }

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load indices atomically */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Calculate length */
        int len = (current_end - current_start + tcq->size) % tcq->size;

        /* Check if queue is into the margin */
        if (tcq->size <= TC_QUEUE_MARGIN) {
            /* No margin available, so full means really all full */
            return (len >= tcq->size - 1);
        }

        if (len >= tcq->size - TC_QUEUE_MARGIN) {
            /* We're into the margin, so call it full */
            return 1;
        }

        /* We're not into the margin */
        return 0;
    }

    /* Original implementation for planner_type 0/1 */

    /* call the queue full if the length is into the margin, so reduce the
       effect of a race condition where the appending process may not see the
       full status immediately and send another motion */

    if (tcq->size <= TC_QUEUE_MARGIN) {
	/* no margin available, so full means really all full */
	    return tcq->allFull;
    }

    if (tcq->_len >= tcq->size - TC_QUEUE_MARGIN) {
	/* we're into the margin, so call it full */
	    return 1;
    }

    /* we're not into the margin */
    return 0;
}

/*! tcqLast() function
 *
 * \brief gets the last TC element in the queue, without removing it
 *
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 *
 * @return	 TC_STRUCT returns the TC element
 */
TC_STRUCT *tcqLast(TC_QUEUE_STRUCT const * const tcq)
{
    if (tcqCheck(tcq)) {
        return NULL;
    }

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load indices atomically */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Check if queue is empty */
        if (current_end == current_start) {
            return NULL;
        }

        /* Get last element (end - 1), fix for negative modulus error */
        int n = current_end - 1 + tcq->size;
        return &(tcq->queue[n % tcq->size]);
    }

    /* Original implementation for planner_type 0/1 */
    if (tcq->_len == 0) {
        return NULL;
    }
    //Fix for negative modulus error
    int n = tcq->end-1 + tcq->size;
    return &(tcq->queue[n % tcq->size]);

}

/*! tcqBack() function
 *
 * \brief gets the n-th item from the back of the queue
 *
 * This function allows backward iteration from the end of the queue.
 * n=0 is the most recently added item (same as tcqLast)
 * n=-1 is the second-to-last item
 * n=-2 is the third-to-last item, etc.
 *
 * Used by the backward velocity pass optimization in planner_type 2.
 *
 * @param    tcq       pointer to the TC_QUEUE_STRUCT
 * @param    n         negative offset from end (0, -1, -2, ...)
 *
 * @return	 TC_STRUCT returns the TC element, or NULL if invalid
 */
TC_STRUCT * tcqBack(TC_QUEUE_STRUCT * const tcq, int n)
{
    if (tcqCheck(tcq)) {
        return NULL;
    }

    /* For planner_type 2, use atomic lock-free queue */
    if (GET_TRAJ_PLANNER_TYPE() == 2) {
        /* Load producer index first (end), then consumer index (start) */
        int current_end = __atomic_load_n(&tcq->end_atomic, __ATOMIC_ACQUIRE);
        int current_start = __atomic_load_n(&tcq->start_atomic, __ATOMIC_ACQUIRE);

        /* Calculate length */
        int len = (current_end - current_start + tcq->size) % tcq->size;

        /* Check for empty queue */
        if (0 == len) {
            return NULL;
        }
        /* Only allow negative indices (from back) */
        else if (n > 0) {
            return NULL;
        }
        /* Check if index is within valid range */
        else if (n > -len) {
            /* Calculate index from end: end + n - 1
             * Fix for negative modulus error by adding tcq->size */
            int k = current_end + n - 1 + tcq->size;
            int idx = k % tcq->size;
            return &(tcq->queue[idx]);
        }
        else {
            return NULL;
        }
    }

    /* Original implementation for planner_type 0/1 */
    /* Uses same logic as atomic version but with _len and end */
    if (0 == tcq->_len) {
        return NULL;
    } else if (n > 0) {
        return NULL;
    } else if (n > -tcq->_len) {
        int k = tcq->end + n - 1 + tcq->size;
        int idx = k % tcq->size;
        return &(tcq->queue[idx]);
    } else {
        return NULL;
    }
}

/*! tcqBackConst() function
 *
 * \brief const-correct version of tcqBack
 */
TC_STRUCT const * tcqBackConst(TC_QUEUE_STRUCT const * const tcq, int n)
{
    /* Cast away const for the pointer, not the data */
    return (TC_STRUCT const *)tcqBack((TC_QUEUE_STRUCT *)tcq, n);
}

