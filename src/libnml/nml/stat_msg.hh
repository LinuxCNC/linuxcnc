/********************************************************************
* Description: stat_msg.hh
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
#ifndef RCS_STAT_HH
#define RCS_STAT_HH

#include "nml.hh"
#include "nmlmsg.hh"
#include "rcs.hh"

class RCS_STAT_MSG:public NMLmsg {
  public:
    RCS_STAT_MSG(NMLTYPE t, size_t sz);
    NMLTYPE command_type;
    int echo_serial_number;
    // The anonymous union is provided to allow for RCS_STAT_MSG_format() in
    // nml/stat_msg.cc to call cms->update() with an integer reference. Member
    // 'status' is normally accessed. Casting the 'status' member to int& in
    // the cms->update() call will give a 'type-punned pointer dereference'
    // warning. We sidestep this problem with the union where we can address
    // either field and get the same value.
    // The union is necessary because the update() call implementation checks
    // the address of its argument to be in a specific memory region, which
    // excludes using a temporary. Now, with the union, both members 'status'
    // and 'status_int' share the same memory location. The 'RCS_STATUS' enum
    // in rcs/rcs.hh explicitly declares the enum's underlying type as int.
    // Therefore, both union members are of same underlying type.
    union {
        RCS_STATUS status;
        int status_int;
    };
    int _state;
};

extern int RCS_STAT_MSG_format(NMLTYPE, void *, CMS *);

class RCS_STAT_CHANNEL:public NML {
  public:
    RCS_STAT_CHANNEL(NML_FORMAT_PTR, const char *, const char *, const char *,
	int set_to_server = 0);
     ~RCS_STAT_CHANNEL();
    // Sub-class calls base-class
    // cppcheck-suppress duplInheritedMember
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

enum RCS_STATE
{
  UNINITIALIZED_STATE = -1,
  NEW_COMMAND = -2,
  NOP_STATE = -3,
  SE0 = -10,
  SE1 = -11,
  SE2 = -12,
  SE3 = -13,
  SE4 = -14,
  SE5 = -15,
  SE6 = -16,
  SE7 = -17,
  SE8 = -18,
  SE9 = -19,
  S0 = 0,
  S1 = 1,
  S2 = 2,
  S3 = 3,
  S4 = 4,
  S5 = 5,
  S6 = 6,
  S7 = 7,
  S8 = 8,
  S9 = 9,
  S10 = 10,
  S11 = 11,
  S12 = 12,
  S13 = 13,
  S14 = 14,
  S15 = 15,
  S16 = 16,
  S17 = 17,
  S18 = 18,
  S19 = 19,
  S20 = 20,
  S21 = 21,
  S22 = 22,
  S23 = 23,
  S24 = 24,
  S25 = 25,
  S26 = 26,
  S27 = 27,
  S28 = 28,
  S29 = 29,
  S30 = 30,
  S31 = 31,
  S32 = 32,
  S33 = 33,
  S34 = 34,
  S35 = 35,
  S36 = 36,
  S37 = 37,
  S38 = 38,
  S39 = 39
};

#endif
