/********************************************************************
* Description: rcs.hh
*   This header should be included in each source that uses NML, CMS,
*   or any other libnml function.
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
#ifndef RCS_HH
#define RCS_HH

#ifdef __cplusplus

enum RCS_STATUS {
    UNINITIALIZED_STATUS = -1,
    RCS_DONE = 1,
    RCS_EXEC = 2,
    RCS_ERROR = 3
};

/* Portable Print functions. */
#include "rcs_print.hh"		/* rcs_print_ functions */
/* Linked Lists etc. */
#include "linklist.hh"		/* class RCS_LINKED_LIST */

#include "cms.hh"		/* class CMS */
#include "nml.hh"
#include "nmlmsg.hh"
#include "stat_msg.hh"		// RCS_STAT_MSG, RCS_STAT_CHANNEL

/* System Utilities. */
#include "timer.hh"		/* class RCS_TIMER, etime(),esleep() */
#include "rcs_exit.hh"		// rcs_exit(), attach_rcs_exit_list(),

#endif

/* class INIFILE */
#include "inifile.h"

#include "rcsversion.h"

#endif /* !defined(RCS_HH) */
