#ifndef RCS_HH
#define RCS_HH

/* Include Files */
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
#include "nml_mod.hh"

#include "cmsdiag.hh"		// class CMS_DIAGNOSTICS_INFO
#include "nmldiag.hh"		// class NML_DIAGNOSTICS_INFO

#include "cms_srv.hh"		/* class CMS_SERVER */
#include "nml_srv.hh"		/* class NML_SERVER */

/* System Utilities. */
#include "timer.hh"		/* class RCS_TIMER, etime(),esleep() */
#include "rcs_exit.hh"		// rcs_exit(), attach_rcs_exit_list(),
//#include "inetfile.hh"		// inet_file_open(), inet_file_close()

/* class NML_QR_SERVER, NML_QR_CLIENT, NML_QUERY_MSG */
#include "nmlqr.hh"
#endif
/* Pose/Vector/Matrix Math Classes */
#include "posemath.h"

/* class INIFILE */
#include "inifile.h"

#include "rcsversion.h"

#endif /* !defined(RCS_HH) */
