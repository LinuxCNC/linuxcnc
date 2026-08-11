/*
** License GPL Version 2
*/
#ifndef SWITCHKINS_H // {
#define SWITCHKINS_H

#include <kinematics.h>

//max number of switchkins types (KS,KF,KI) a module may provide:
#define SWITCHKINS_MAX_TYPES 9

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
// supplied by the using module, provides types 0,1,2
extern int switchkinsSetup(kparms* ksetup_parms,
                           KS* kset0, KS* kset1, KS* kset2,
                           KF* kfwd0, KF* kfwd1, KF* kfwd2,
                           KI* kinv0, KI* kinv1, KI* kinv2
                          );

// called from switchkinsSetup(), once per type it does not provide itself
extern int switchkinsRegister(int ktype, KS kset, KF kfwd, KI kinv);
#endif // }
