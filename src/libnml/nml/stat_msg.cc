/********************************************************************
* Description: stat_msg.cc
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

#include "nml.hh"
#include "nmlmsg.hh"
#include "cms.hh"
#include "physmem.hh"
#include "linklist.hh"

#include "stat_msg.hh"

#include <string.h>		// memset()

RCS_STAT_MSG::RCS_STAT_MSG(NMLTYPE t, size_t sz):NMLmsg(t, sz)
{
// just avoiding an inline function.
    command_type = -1;
    echo_serial_number = -1;
    status = -1;
    state = -1;
    line = -1;
    source_line = -1;
    memset(source_file, 0, 64);

}

int RCS_STAT_MSG_format(NMLTYPE t, void *buf, CMS * cms)
{
    cms->update(((RCS_STAT_MSG *) buf)->command_type);
    cms->update(((RCS_STAT_MSG *) buf)->echo_serial_number);
    cms->update(((RCS_STAT_MSG *) buf)->status);
    cms->update(((RCS_STAT_MSG *) buf)->state);
    cms->update(((RCS_STAT_MSG *) buf)->line);
    cms->update(((RCS_STAT_MSG *) buf)->source_line);
    cms->update(((RCS_STAT_MSG *) buf)->source_file, 64);

    switch (t) {
    case RCS_GENERIC_STATUS_TYPE:
	((RCS_GENERIC_STATUS *) buf)->update(cms);
	return (1);

    default:
	return (0);
    }
    return (0);
}

RCS_GENERIC_STATUS::RCS_GENERIC_STATUS():
RCS_STAT_MSG(RCS_GENERIC_STATUS_TYPE, sizeof(RCS_GENERIC_STATUS))
{
// Just avoiding an inline function.
}

void
  RCS_GENERIC_STATUS::update(CMS *)
{
// Just avoiding an inline function.
}

RCS_STAT_CHANNEL::RCS_STAT_CHANNEL(NML_FORMAT_PTR f_ptr, const char *name,
    const char *process, const char *file,
    int set_to_server):NML(name, process, file, set_to_server)
{
    format_chain = new LinkedList;
    prefix_format_chain(f_ptr);
    prefix_format_chain(RCS_STAT_MSG_format);

    channel_type = RCS_STAT_CHANNEL_TYPE;

    register_with_server();
}

RCS_STAT_CHANNEL::~RCS_STAT_CHANNEL()
{
    // Something funny happens to gdb without this being explicitly defined.
}
