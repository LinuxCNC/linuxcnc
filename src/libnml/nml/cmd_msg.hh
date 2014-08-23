/********************************************************************
* Description: cmd_msg.hh
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
#ifndef RCS_CMD_HH
#define RCS_CMD_HH

#include "nml.hh"
#include "nmlmsg.hh"

class RCS_CMD_MSG:public NMLmsg {
  public:
    RCS_CMD_MSG(NMLTYPE t, long sz);
    int serial_number;
};

extern int RCS_CMD_MSG_format(NMLTYPE, void *, CMS *);

class RCS_CMD_CHANNEL:public NML {
  public:
    RCS_CMD_CHANNEL(NML_FORMAT_PTR, const char *, const char *, const char *,
	int set_to_server = 0);
     ~RCS_CMD_CHANNEL();
    RCS_CMD_MSG *get_address() {
	return ((RCS_CMD_MSG *) NML::get_address());
    };
};

enum RCS_GENERIC_CMD_ID {
    GENERIC_INIT,
    GENERIC_HALT
};

#define RCS_GENERIC_CMD_TYPE    ((NMLTYPE) 1000000)

class RCS_GENERIC_CMD:public RCS_CMD_MSG {
  public:
    RCS_GENERIC_CMD();
    void update(CMS *);
    int gen_id;
};

#endif
