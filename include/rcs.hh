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
********************************************************************/
#ifndef RCS_HH
#define RCS_HH

class RCS_LINKED_LIST;
class NMLmsg;
class NML;
class CMS;
class CMS_DIAGNOSTICS_INFO;
class NML_DIAGNOSTICS_INFO;
class CMS_SERVER;
class NML_SERVER;
class RCS_TIMER;
class RCS_CMD_MSG;
class RCS_STAT_MSG;

enum RCS_STATUS {               /* Originally from nml_mod.hh */
    UNINITIALIZED_STATUS = -1,
    RCS_DONE = 1,
    RCS_EXEC = 2,
    RCS_ERROR = 3
};

#include "nml_type.hh"
#endif /* !defined(RCS_HH) */
