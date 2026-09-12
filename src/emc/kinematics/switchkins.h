/*
** License GPL Version 2
*/
#ifndef SWITCHKINS_H // {
#define SWITCHKINS_H

#include <kinematics.h>

//SWITCHKINS_MAX_TYPES (max number of types a module may provide)
//is in kinematics.h: motion and the NML status channel need it too

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

// optionally called from switchkinsSetup() to declare what a type IS
// (KINSTYPE_IDENTITY, KINSTYPE_PRIMARY, kinematics.h).  A module that
// never calls it leaves its types numeric-only: G12.1 P<n> still works,
// G13.1 refuses to guess which type is identity.
extern int switchkinsDeclare(int ktype, int flags);
#endif // }
