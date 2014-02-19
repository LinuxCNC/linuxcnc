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

/** Return 0 if queue is valid, -1 if not */
static inline int tcqCheck(TC_QUEUE_STRUCT const * const tcq)
{
    if ((0 == tcq) || (0 == tcq->queue))
    {
        return -1;
    }
    return 0;
}

/*! tcqCreate() function
 *
 * \brief Creates a new queue for TC elements.
 *
 * This function creates a new queue for TC elements.
 * It gets called by tpCreate()
 *
 * @param    tcq       pointer to the new TC_QUEUE_STRUCT
 * @param	 _size	   size of the new queue
 * @param	 tcSpace   holds the space allocated for the new queue, allocated in motion.c
 *
 * @return	 int	   returns success or failure
 */
int tcqCreate(TC_QUEUE_STRUCT * const tcq, int _size, TC_STRUCT * const tcSpace)
{
    if (_size <= 0 || 0 == tcq) {
	return -1;
    } else {
	tcq->queue = tcSpace;
	tcq->size = _size;
	tcq->_len = 0;
	tcq->start = tcq->end = 0;
	tcq->allFull = 0;

	if (0 == tcq->queue) {
	    return -1;
	}
	return 0;
    }
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
    if (!tcqCheck(tcq)) {
        /* free(tcq->queue); */
        tcq->queue = 0;
    }

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
    tcq->allFull = 0;

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
    if (tcqCheck(tcq) || (n < 0) || (n >= tcq->_len)) return NULL;

    return &(tcq->queue[(tcq->start + n) % tcq->size]);
}

/*!
 * \def TC_QUEUE_MARGIN
 * sets up a margin at the end of the queue, to reduce effects of race conditions
 */
#define TC_QUEUE_MARGIN 20

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
    if (tcq->_len == 0) {
        return NULL;
    }
    //Fix for negative modulus error
    int n = tcq->end-1 + tcq->size;
    return &(tcq->queue[n % tcq->size]);

}

