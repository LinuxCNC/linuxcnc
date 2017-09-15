/********************************************************************
* Description:  threads.c
*               This file, 'threads.c', is a HAL component that 
*               provides a way to create realtime threads but 
*               contains no other functionality.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/
/** This file, 'threads.c', is a HAL component that provides a way to 
    create realtime threads but contains no other functionality.
    It will mostly be used for testing - when EMC is run normally,
    the motion module creates all the neccessary threads.
    
    The module has three pairs of parameters, "name1, period1", etc.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "rtapi_string.h"

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Thread Module for HAL");
MODULE_LICENSE("GPL");
static char *name1 = "thread1";	/* name of thread */
RTAPI_MP_STRING(name1, "name of thread 1");
static int fp1 = 1;		/* use floating point? default = yes */
RTAPI_MP_INT(fp1, "thread1 uses floating point");
static long period1 = 1000000;	/* thread period - default = 1ms thread */
RTAPI_MP_LONG(period1,  "thread1 period (nsecs)");
static char *name2 = NULL;	/* name of thread */
RTAPI_MP_STRING(name2, "name of thread 2");
static int fp2 = 1;		/* use floating point? default = yes */
RTAPI_MP_INT(fp2, "thread2 uses floating point");
static long period2 = 0;	/* thread period - default = no thread */
RTAPI_MP_LONG(period2, "thread2 period (nsecs)");
static char *name3 = NULL;	/* name of thread */
RTAPI_MP_STRING(name3, "name of thread 3");
static int fp3 = 1;		/* use floating point? default = yes */
RTAPI_MP_INT(fp3, "thread1 uses floating point");
static long period3 = 0;	/* thread period - default = no thread */
RTAPI_MP_LONG(period3, "thread3 period (nsecs)");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/


int rtapi_app_main(void)
{
    int retval;

    /* have good config info, connect to the HAL */
    comp_id = hal_init("threads");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "THREADS: ERROR: hal_init() failed\n");
	return -1;
    }
    /* was 'period' specified in the insmod command? */
    if ((period1 > 0) && (name1 != NULL) && (*name1 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(name1, period1, fp1);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name1);
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period1 / 1000);
	}
    }
    if ((period2 > 0) && (name2 != NULL) && (*name2 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(name2, period2, fp2);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name2);
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period2 / 1000);
	}
    }
    if ((period3 > 0) && (name3 != NULL) && (*name3 != '\0')) {
	/* create a thread */
	retval = hal_create_thread(name3, period3, fp3);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"THREADS: ERROR: could not create thread '%s'\n", name3);
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "THREADS: created %ld uS thread\n", period3 / 1000);
	}
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

