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

// KinematicsWORKFRAME and KinematicsTOOLFRAME functions
// (optional, see kinematics.h)
typedef int (*KT)(const double *joint,
                  PmRotationMatrix *rot,
                  const KINEMATICS_FORWARD_FLAGS *fflags);

// KinematicsSETUP functions
typedef int (*KS)(const int   comp_id,     // halpins
                  const char* coordinates, // module parameter
                  kparms*     ksetup_parms //
                 );

//*********************************************************************
// supplied by a module using switchkins_main.c, provides types 0,1,2
extern int switchkinsSetup(kparms* ksetup_parms,
                           KS* kset0, KS* kset1, KS* kset2,
                           KF* kfwd0, KF* kfwd1, KF* kfwd2,
                           KI* kinv0, KI* kinv1, KI* kinv2
                          );

// provide one switchkins-type, before switchkinsInit()
extern int switchkinsRegister(int ktype, KS kset, KF kfwd, KI kinv);

// called from switchkinsSetup() for each type that reports its frames; a type
// that does not simply omits the call.  Both are given, since a machine has a
// work frame whether or not anything turns it.  native is the rotation
// relating the type's own tool frame to the convention, TOOL_FRAME_SPINDLE
// for maths already in it; it is checked once at load and applied by the
// dispatch.
extern int switchkinsRegisterFrames(int ktype, KT kwork, KT ktool,
                                    const PmRotationMatrix *native);

// KinematicsTOOLFRAMEINVERSE function (optional, see kinematics.h)
typedef int (*KTI)(const PmCartesian *axis_in_work,
                   const PmCartesian *x_in_work,
                   const double *seed,
                   unsigned int held,
                   double *solutions,
                   int max_solutions,
                   int *free_directions,
                   double *tool_spin);

// called from switchkinsSetup() only by a type that has a closed form for the
// tool orientation inverse.  A type that does not gets the generic search,
// which needs nothing beyond the frames it already registered.
extern int switchkinsRegisterToolFrameInverse(int ktype, KTI kinv);

// KinematicsJACOBIAN function (optional, see kinematics.h)
typedef int (*KJ)(const double *joint,
                  const EmcPose *world,
                  double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                  const KINEMATICS_INVERSE_FLAGS *iflags);

// called from switchkinsSetup() only by a type with a closed form.  A type
// that does not gets the exact answer if it is an identity type, and
// otherwise the generic differences of its own inverse.
extern int switchkinsRegisterJacobian(int ktype, KJ kjac);

// create the hal pins and start on type 0; the caller owns the hal
// component and does hal_init() before and hal_ready() after
extern int switchkinsInit(const int   comp_id,
                          kparms*     ksetup_parms,
                          const char* coordinates
                         );
#endif // }
