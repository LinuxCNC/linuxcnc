/********************************************************************
* genserkins.c employing switchkins.[ch]
* License: GPL Version 2
*
* NOTEs:
*  1) specify all kparms items
*  2) the maths and the geometry table are in genserfuncs.c, written as
*     pure functions of the parameter block (see kinematics.h)
*/

/********************************************************************
TEST: switchable kinematics: identity or genserkins

1) genser kinematics provided by genserfuncs.c (shared with genserkins.c
2) uses same pin names as genserkins
3) tesing mm configs: increase GO_REAL_EPSILON from 1e-7 to 1e-6

NOTE:
a) requires *exactly* 6 joints
b) identity assignments can use any of xyzabcuvw
   but should agree with [TRAJ]COORDINATES
   and may be confusing

www refs:

frame-larger-than:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg03790.html

angles:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg15285.html
*/

//----------------------------------------------------------------------
// Only gcc/g++ supports the #pragma
#if __GNUC__ && !defined(__clang__)
// genserKinematicsInverse() is 5104 with buster amd64 gcc 8.3.0-6
//#pragma GCC diagnostic error   "-Wframe-larger-than=6000"
  #pragma GCC diagnostic warning "-Wframe-larger-than=6000"
#endif

#include <rtapi.h>
#include <rtapi_string.h>
#include <emcmotcfg.h>

#include "genserkins.h"
#include <switchkins.h>

//-7 is system defined -3 ok, -4 ok, -5 ok,-6 ok (mm system)
#undef  GO_REAL_EPSILON
#define GO_REAL_EPSILON (1e-6)

//*********************************************************************


int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    (void)kset0; (void)kset1; (void)kset2;
    (void)kfwd0; (void)kfwd1; (void)kfwd2;
    (void)kinv0; (void)kinv1; (void)kinv2;
    kp->kinsname    = "genserkins"; // !!! must agree with filename
    kp->halprefix   = "genserkins"; // hal pin names
    kp->required_coordinates = "xyzabcuvw"; // u,v,w are joints 6,7,8
    kp->max_joints  = strlen(kp->required_coordinates);
    kp->allow_duplicates  = 0;
    kp->params      = GENSER_PARAMS;
    kp->nparams     = GENSER_NPARAMS;

    switchkinsRegisterOps(0, &GENSER_OPS);
    switchkinsRegisterOps(1, &KINS_IDENTITY_OPS);
    switchkinsRegisterOps(2, &USERK_OPS);

    return 0;
}
