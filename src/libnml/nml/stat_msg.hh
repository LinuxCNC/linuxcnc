/********************************************************************
* Description: stat_msg.hh
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
#ifndef RCS_STAT_HH
#define RCS_STAT_HH

#include "nml.hh"
#include "nmlmsg.hh"

class RCS_STAT_MSG:public NMLmsg {
  public:
    RCS_STAT_MSG(NMLTYPE t, size_t sz);
    NMLTYPE command_type;
    int echo_serial_number;
    int status;
    int state;
    int line;
    int source_line;
    char source_file[64];

};

extern int RCS_STAT_MSG_format(NMLTYPE, void *, CMS *);

class RCS_STAT_CHANNEL:public NML {
  public:
    RCS_STAT_CHANNEL(NML_FORMAT_PTR, char *, char *, char *,
	int set_to_server = 0);
     ~RCS_STAT_CHANNEL();
    RCS_STAT_MSG *get_address() {
	return ((RCS_STAT_MSG *) NML::get_address());
    };
};

#define RCS_GENERIC_STATUS_TYPE         ((NMLTYPE) 2000000)

class RCS_GENERIC_STATUS:public RCS_STAT_MSG {
  public:
    RCS_GENERIC_STATUS();
    void update(CMS *);
};

#endif
