/*
  trivkins.c

  Trivial kinematics for 3 axis Cartesian machine

  Modification history:

  13-Mar-2000 WPS added unused attribute to ident to avoid 'defined but not used' compiler warning.
  11-Aug-1999  FMP added kinematicsType()
  9-Aug-1999  FMP added kinematicsHome(), changed naming from invK to kinI
  18-Dec-1997  FMP changed to PmPose
  16-Oct-1997  FMP created
  */

#include "motion.h"		/* these decls */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) ident[] =
    "$Id$";

int kinematicsForward(const double *joints,
    EmcPose * pos,
    const KINEMATICS_FORWARD_FLAGS * fflags,
    KINEMATICS_INVERSE_FLAGS * iflags)
{
    pos->tran.x = joints[0];
    pos->tran.y = joints[1];
    pos->tran.z = joints[2];
    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
    double *joints,
    const KINEMATICS_INVERSE_FLAGS * iflags,
    KINEMATICS_FORWARD_FLAGS * fflags)
{
    joints[0] = pos->tran.x;
    joints[1] = pos->tran.y;
    joints[2] = pos->tran.z;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;

    return 0;
}

/* implemented for these kinematics as giving joints preference */
int kinematicsHome(EmcPose * world,
    double *joint,
    KINEMATICS_FORWARD_FLAGS * fflags, KINEMATICS_INVERSE_FLAGS * iflags)
{
    *fflags = 0;
    *iflags = 0;

    return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_IDENTITY;
}
