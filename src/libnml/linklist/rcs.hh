#ifndef RCS_HH
#define RCS_HH

/* Include Files */

/* Portable Print functions. */
#include "rcs_prnt.hh"		/* rcs_print_ functions */

/* Linked Lists etc. */
#include "linklist.hh"		/* RCS_LINKED_LIST */

/* Neutral Manufacturing Language (NML) */
#include "nmlmsg.hh"		/* NMLmsg */
#include "nml.hh"		/* NML */
#include "cms.hh"		/* CMS */
#include "cmd_msg.hh"		/* RCS_CMD_MSG, RCS_CMD_CHANNEL */
#include "stat_msg.hh"		/* RCS_STAT_MSG, RCS_STAT_CHANNEL */
#include "wm_msg.hh"		/* RCS_WM_MSG, RCS_WM_CHANNEL */
#include "nml_oi.hh"		/* nmlErrorFormat(), NML_TEXT, NML_ERROR */

#include "cmsdiag.hh"		/* CMS_DIAGNOSTICS_INFO */
#include "nmldiag.hh"		/* NML_DIAGNOSTICS_INFO */
#include "cms_srv.hh"		/* CMS_SERVER */
#include "nml_srv.hh"		/* NML_SERVER */
#include "nmlqr.hh"		/* NML_QR_SERVER, NML_QR_CLIENT,
				   NML_QUERY_MSG */

/* System Utilities. */
#include "timer.hh"		/* RCS_TIMER, etime(),esleep() */
#include "rcs_exit.hh"		/* rcs_exit(), attach_rcs_exit_list(), */
#include "inifile.h"		/* INIFILE */
#include "nml_mod.hh"		/* NML_MODULE */

/* Library version info */
#define LIB_VERSION "0.1"
#define LIB_MAJOR_VERSION (0)
#define LIB_MINOR_VERSION (1)
const int lib_major_version = LIB_MAJOR_VERSION;
const int lib_minor_version = LIB_MINOR_VERSION;


#endif

/* defined(RCS_HH) */
