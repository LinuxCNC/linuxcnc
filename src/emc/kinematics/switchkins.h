/*
** License GPL Version 2
*/
#ifndef SWITCHKINS_H // {
#define SWITCHKINS_H

#include "kinematics.h"

//hardcoded number of switchkins types (KS,KF,KI):
#define SWITCHKINS_MAX_TYPES 3

// KinematicsFORWARD functions
typedef int (*KF)(const double *joint,
                  EmcPose * pos,
                  const KINEMATICS_FORWARD_FLAGS * fflags,
                  KINEMATICS_INVERSE_FLAGS * iflags);

// KinematicsINVERSE functions
typedef int (*KI)(const struct EmcPose * world,
                  double *joint,
                  const KINEMATICS_INVERSE_FLAGS * iflags,
                  KINEMATICS_FORWARD_FLAGS * fflags);

// KinematicsSETUP functions
typedef int (*KS)(const int   comp_id,     // halpins
                  const char* coordinates, // module parameter
                  kparms*     ksetup_parms //
                 );

//*********************************************************************
extern int switchkinsSetup(kparms* ksetup_parms,
                           KS* kset0, KS* kset1, KS* kset2,
                           KF* kfwd0, KF* kfwd1, KF* kfwd2,
                           KI* kinv0, KI* kinv1, KI* kinv2
                          );
#endif // }
