#ifndef RCS_HH
#define RCS_HH

/* Include Files */

/* Portable Print functions. */
#include "rcs_print.hh"		/* rcs_print_ functions */

/* Linked Lists etc. */
#include "linklist.hh"		/* class LinkedList */

/* Neutral Manufacturing Language (NML) */
#include "nmlmsg.hh"		/* class NMLmsg */
#include "nml.hh"		/* class NML */
#include "cms.hh"		/* class CMS */
#include "cmd_msg.hh"		// RCS_CMD_MSG, RCS_CMD_CHANNEL
#include "stat_msg.hh"		// RCS_STAT_MSG, RCS_STAT_CHANNEL
#include "nml_oi.hh"		// nmlErrorFormat(), NML_TEXT, NML_ERROR

#include "cmsdiag.hh"		// class CMS_DIAGNOSTICS_INFO
#include "nmldiag.hh"		// class NML_DIAGNOSTICS_INFO
#include "cms_srv.hh"		/* class CMS_SERVER */
#include "nml_srv.hh"		/* class NML_SERVER */

/* System Utilities. */
#include "timer.hh"		/* class RCS_TIMER, etime(),esleep() */
#include "rcs_exit.hh"		// rcs_exit(), attach_rcs_exit_list(),

/* class INIFILE */
#include "inifile.hh"

/* class NML_MODULE */
#include "nml_mod.hh"

#include "rcsversion.h"

#endif /* !defined(RCS_HH) */
