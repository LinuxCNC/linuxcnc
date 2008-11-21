/********************************************************************
* Description: memsem.cc
*   Uses a block of memory to implement a mutual exclusion semaphore.
*   With LynxOs and SunOs using semop is very inefficient if the semaphore 
*   will usually be available. Other platforms may give you no semaphore
*   operations.
*
*   Example: You are communicating with another process via shared memory.
*   You need a mutual exclusion semaphore because you don't want the reader
*   to occasionally read the buffer while the writer has written only part
*   of the message, but most of the time when the writer won't be using
*   buffer and the reader should get access immediately. semop will take
*   100 to 150 microseconds to return access, while mem_get_access should
*   take less than a microsecond with less than 10 total_connections.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif

#include <stddef.h>		/* NULL */

#ifdef  __cplusplus
};
#endif

#include "timer.hh"		/* etime(), esleep() */
#include "memsem.hh"		/* struct mem_access_object */
#include "rcs_print.hh"		// rcs_print_error()

#define TIMEOUT_MIN ((double) 1.0E-6)

/******************************************************************
* Take the mutual exclusion semaphore.
*
* Parameters:
* data should point to an area of memory accesable to all processes that
*   is at least "total_connections" bytes long,
* connection_number is a unique identifier for this process it should
*  be between 0 and ("total_connections"-1)
* total_connections is the maximum number of processes that can use this
*  block of memory as a semaphore.
* timeout - is the time allowed for getting the semaphore.
*    (If timeout is negative then wait forever.)
* semdelay - is the time to wait between tries for the semaphore.
* read_only - Will the access to the buffer only to read the memory.
* split_buffer - The buffer will be split so that one area may be read
* while the other area is written to.
* toggle_bit - If the buffer is split the toggle bit determines which area
* will be read from and which may be written to. (O.K. its really a byte but
* think of it as 1 bit since it should only be 0 or 1)
*
* Returns: 0 for success: -1 for invalid parameters: or -2 if it timed out.
***********************************************************************/
/*! \todo Another #if 0 */
#if 0
int mem_get_access(void *data, long connection_number,
    long total_connections, double timeout, double semdelay,
    int read_only, int split_buffer, char &toggle_bit)
#endif
     int mem_get_access(struct mem_access_object *mo)
{
    register char *mylock;
    register char current_lock;
    register char *plock;
    char *lastlock;
    int semaphores_clear;
    double start_time, time;
    int split_buffer = 0;
    int read_only;
    int total_connections;
    int connection_number;
    double timeout;
#ifdef DEBUG
    rcs_print("mem_get_access: - Time = %lf\n", etime());
    static int count = 0;
    count++;
#endif

    /* Check Parameters. */
    if ((total_connections = mo->total_connections) <=
	(connection_number = mo->connection_number) || connection_number < 0)
    {
	return -1;
    }
    if (NULL == mo->data) {
	return -1;
    }

    /* Check for a request for me to sleep */
    int wait_requested = 1;
    lastlock = ((char *) mo->data) + total_connections;
    mylock = ((char *) mo->data) + connection_number;
    time = start_time = etime();	/* get start time */
    while (wait_requested
	&& (time - start_time < mo->timeout / 2 || mo->timeout < 0)) {
	wait_requested = 0;
	for (plock = (char *) mo->data; plock < lastlock; plock++) {
	    if (5 == (current_lock = *plock) && plock != mylock) {
		wait_requested = 1;
	    }
	}
	if (wait_requested) {
	    esleep(mo->sem_delay);
	}
    }

    /* Try the locks one time before checking time because checking the locks 
       generally takes much less time than checking the time. */
    *mylock = 4;
    mo->toggle_bit = ((char *) mo->data)[total_connections];
    read_only = mo->read_only;
#ifdef DEBUG
    if (read_only) {
	rcs_print("added sleep: %d - Time = %lf\n", __LINE__, etime());
	esleep(0.02);
    }
#endif
    if (read_only) {
	split_buffer = mo->split_buffer;
	if (split_buffer) {
	    *mylock = 2 + mo->toggle_bit;
	    return 0;
	}
	*mylock = 2;
    } else {
	*mylock = 1;
    }

#ifdef debug
    if (read_only) {
	rcs_print("added sleep: %d - time = %lf\n", __line__, etime());
	esleep(0.01);
    }
#endif
    semaphores_clear = 1;
    lastlock = ((char *) mo->data) + total_connections;
    mo->toggle_bit = ((char *) mo->data)[total_connections];
    for (plock = (char *) mo->data; plock < lastlock; plock++) {
	if (0 != (current_lock = *plock)) {
	    if (plock != mylock) {
		if (!(read_only && current_lock > 1) &&
		    !(split_buffer && current_lock == 2 + mo->toggle_bit)
		    && (current_lock != 5)) {
		    semaphores_clear = 0;
		}
	    }
	}
    }
#ifdef debug
    if (0)			// read_only && 0 == count % 2)
    {
	rcs_print("added sleep: %d - time = %lf\n", __line__, etime());
	esleep(0.01);
    }
#endif
    if (semaphores_clear) {
	return 0;
    }
    timeout = mo->timeout;
    if (timeout < TIMEOUT_MIN && timeout > 0) {
	*mylock = 0;
	return (-2);
    }
    /* release the lock before going to sleep. */
    *mylock = 5;

    if (NULL != mo->sem) {
	if (-1 == mo->sem->wait()) {
	    *mylock = 0;
	    return -1;
	}
    } else {
	esleep(mo->sem_delay);	/* sleep for 100 microseconds */
    }
    if (read_only) {
	*mylock = 2;
    } else {
	*mylock = 1;
    }

#ifdef debug
    if (0)			// read_only && 0 == count % 7)
    {
	rcs_print("added sleep: %d - time = %lf\n", __line__, etime());
	esleep(0.01);
    }
#endif
    while ((timeout >= 0) ? ((time - start_time) < timeout) : 1) {
	if (split_buffer) {
	    mo->toggle_bit = ((char *) mo->data)[total_connections];
	}
	semaphores_clear = 1;
	plock = (char *) mo->data;
	mo->toggle_bit = ((char *) mo->data)[total_connections];
	for (; plock < lastlock; plock++) {
	    current_lock = *plock;
	    if (0 != current_lock) {
		if (plock != mylock &&
		    !(read_only && current_lock > 1) &&
		    !(split_buffer && current_lock == 2 + mo->toggle_bit)
		    && (current_lock != 5)) {
		    semaphores_clear = 0;
		}
	    }
	}
	if (semaphores_clear) {
	    return 0;
	}
	if (NULL != mo->sem) {
	    *mylock = 5;
	    mo->sem->wait();
	} else {
	    *mylock = 5;
	    esleep(mo->sem_delay);	/* sleep for 100 microseconds */
	}
	if (read_only) {
	    *mylock = 2;
	} else {
	    *mylock = 1;
	}
#ifdef DEBUG
	if (0)			// read_only && 0 == count % 4)
	{
	    rcs_print("added sleep: %d - Time = %lf\n", __LINE__, etime());
	    esleep(0.01);
	}
#endif
	time = etime();
    }
    *mylock = 0;
    return (-2);
}

/******************************************************************
* Give up the mutual exclusion semaphore.
*
* Parameters:
* data should point to an area of memory accesable to all processes that
*   is at least "total_connections" bytes long,
* connection_number is a unique identifier for this process it should
*  be between 0 and ("total_connections"-1)
*
* Returns: 0 for success: -1 for invalid parameters
***********************************************************************/
int mem_release_access(struct mem_access_object *mo)
{
    int i;
    int process_waiting = 0;
#ifdef DEBUG
    rcs_print("mem_release_access: - Time = %lf\n", etime());
    static int count = 0;
    count++;
#endif
    if (NULL == mo) {
	rcs_print_error("mem_release_access: Invalid memory object.\n");
	return -1;
    }
    if (NULL == mo->data || mo->connection_number < 0) {
	rcs_print_error("mem_release_access: Invalid memory object.\n");
	return -1;
    }

    if (mo->sem != NULL) {
	process_waiting = 0;
	for (i = 0; i < mo->total_connections; i++) {
	    if (((char *) mo->data)[i] == 5) {
		process_waiting = 1;
		break;
	    }
	}
    }
#ifdef DEBUG
    if (0)			// (0 == count % 5)
    {
	rcs_print("added sleep: %d - Time = %lf\n", __LINE__, etime());
	esleep(0.01);
    }
#endif

    /* If were using a split buffer and this is the end of a write toggle the 
       toggle bit. */
    if (mo->split_buffer && ((char *) mo->data)[mo->connection_number] == 1) {
	((char *) mo->data)[mo->total_connections] = !(mo->toggle_bit);
    }
#ifdef DEBUG
    if (0)			// 0 == count % 7)
    {
	rcs_print("added sleep: %d - Time = %lf\n", __LINE__, etime());
	esleep(0.01);
    }
#endif

    ((char *) mo->data)[mo->connection_number] = 0;
#ifdef DEBUG
    if (0)			// 0 == count % 11)
    {
	rcs_print("added sleep: %d - Time = %lf\n", __LINE__, etime());
	esleep(0.01);
    }
#endif

    if (mo->sem != NULL) {
	if (process_waiting) {
	    mo->sem->post();
	}
    }
    return (0);
}
