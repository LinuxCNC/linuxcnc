/********************************************************************
* Description: userkfuncs.h
*   The user-defined kinematics template, userkfuncs.c, shared by the
*   switchkins modules as their third type.
*
* License: GPL Version 2
********************************************************************/
#ifndef __LINUXCNC_USERKFUNCS_H
#define __LINUXCNC_USERKFUNCS_H

#include "kins_module.h"


extern const kins_ops USERK_OPS;

extern int userkKinematicsSetup(const int   comp_id,
                                const char* coordinates,
                                kparms*     ksetup_parms);

extern int userkKinematicsForward(const double *joint,
                                  struct EmcPose * world,
                                  const KINEMATICS_FORWARD_FLAGS * fflags,
                                  KINEMATICS_INVERSE_FLAGS * iflags);

extern int userkKinematicsInverse(const struct EmcPose * world,
                                  double *joint,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags);

#endif
