/********************************************************************
* Description: nmlmsg.cc
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

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>		// memset

#ifdef __cplusplus
}
#endif
#include "cms.hh"
#include "nmlmsg.hh"
#include "rcs_print.hh"		/* rcs_error_print() */
/* NMLmsg Functions. */ int NMLmsg::automatically_clear = 1;

/* Constructor */
NMLmsg::NMLmsg(NMLTYPE t, long s)
{
    type = t;
    size = s;
    if (automatically_clear) {
	clear();
    }
    if (size < ((long) sizeof(NMLmsg))) {
	rcs_print_error("NMLmsg: size(=%ld) must be atleast %zu\n", size,
	    sizeof(NMLmsg));
	size = sizeof(NMLmsg);
    }
    if (type <= 0) {
	rcs_print_error("NMLmsg: type(=%d) should be greater than zero.\n",
	    (int)type);
    }
};

NMLmsg::NMLmsg(NMLTYPE t, size_t s)
{
    type = t;
    size = s;
    if (automatically_clear) {
	clear();
    }
    if (size < ((long) sizeof(NMLmsg))) {
	rcs_print_error("NMLmsg: size(=%ld) must be atleast %zu\n", size,
	    sizeof(NMLmsg));
	size = sizeof(NMLmsg);
    }
    if (type <= 0) {
	rcs_print_error("NMLmsg: type(=%d) should be greater than zero.\n",
	    (int)type);
    }
}

NMLmsg::NMLmsg(NMLTYPE t, long s, int noclear)
{
    if (automatically_clear && !noclear) {
	clear();
    }
    type = t;
    size = s;
    if (size < ((long) sizeof(NMLmsg))) {
	rcs_print_error("NMLmsg: size(=%ld) must be atleast %zu\n", size,
	    sizeof(NMLmsg));
	size = sizeof(NMLmsg);
    }
    if (type <= 0) {
	rcs_print_error("NMLmsg: type(=%d) should be greater than zero.\n",
	    (int)type);
    }
};

void NMLmsg::clear()
{
    long temp_size;
    NMLTYPE temp_type;
    temp_size = size;
    temp_type = type;
    memset((void *) this, 0, size);
    size = temp_size;
    type = temp_type;
    if (size < ((long) sizeof(NMLmsg))) {
	rcs_print_error("NMLmsg: size(=%ld) must be atleast %zu\n", size,
	    sizeof(NMLmsg));
	size = sizeof(NMLmsg);
    }
}

/* Error message stub for base class. */
 /* update should only be called with derived classes. */
void NMLmsg::update(CMS * cms)
{
    rcs_print_error("update called for NMLmsg base class.");
    rcs_print_error("(This is an error.)\n");
    cms->status = CMS_MISC_ERROR;
}
