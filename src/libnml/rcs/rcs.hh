/********************************************************************
* Description: rcs.hh
*   This header should be included in each source that uses NML, CMS,
*   or any other libnml function.
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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef RCS_HH
#define RCS_HH

#ifdef __cplusplus

/* Portable Print functions. */
#include "rcs_print.hh"		/* rcs_print_ functions */
/* Linked Lists etc. */
#include "linklist.hh"		/* class RCS_LINKED_LIST */

/* Neutral Manufacturing Language (NML) */
#include "nmlmsg.hh"		/* class NMLmsg */
#include "nml.hh"		/* class NML */
#include "cms.hh"		/* class CMS */

#include "cmd_msg.hh"		// RCS_CMD_MSG, RCS_CMD_CHANNEL
#include "stat_msg.hh"		// RCS_STAT_MSG, RCS_STAT_CHANNEL
//#include "wm_msg.hh"          // RCS_WM_MSG, RCS_WM_CHANNEL
#include "nml_oi.hh"		// nmlErrorFormat(), NML_TEXT, NML_ERROR
//#include "nml_mod.hh"

enum RCS_STATUS {               /* Originally from nml_mod.hh */
    UNINITIALIZED_STATUS = -1,
    RCS_DONE = 1,
    RCS_EXEC = 2,
    RCS_ERROR = 3
};

#include "cmsdiag.hh"		// class CMS_DIAGNOSTICS_INFO
#include "nmldiag.hh"		// class NML_DIAGNOSTICS_INFO

#include "cms_srv.hh"		/* class CMS_SERVER */
#include "nml_srv.hh"		/* class NML_SERVER */

/* System Utilities. */
#include "timer.hh"		/* class RCS_TIMER, etime(),esleep() */
#include "rcs_exit.hh"		// rcs_exit(), attach_rcs_exit_list(),
//#include "inetfile.hh"		// inet_file_open(), inet_file_close()

/* class NML_QR_SERVER, NML_QR_CLIENT, NML_QUERY_MSG */
//#include "nmlqr.hh"
#endif
/* Pose/Vector/Matrix Math Classes */
#include "posemath.h"

/* class INIFILE */
#include "inifile.h"
#include "inifile.hh"

#include "rcsversion.h"

#endif /* !defined(RCS_HH) */
