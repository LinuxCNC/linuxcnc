/********************************************************************
* genserkins.c employing switchkins.[ch]
* License: GPL Version 2
*
* NOTEs:
*  1) specify all kparms items
*  2) specify 3 KS,KF,KI functions (setup,forward,inverse)
*/

/********************************************************************
TEST: switchable kinematics: identity or genserkins

1) genser kinematics provided by genserfuncs.c (shared with genserkins.c
2) uses same pin names as genserkins
3) tesing mm configs: increase GO_REAL_EPSILON from 1e-7 to 1e-6

NOTE:
a) requires *exactly* 6 joints
b) identity assigments can use any of xyzabcuvw
   but should agree with [TRAJ]COORDINATES
   and may be confusing

www refs:

frame-larger-than:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg03790.html

angles:
https://www.mail-archive.com/emc-developers@lists.sourceforge.net/msg15285.html
*/

//----------------------------------------------------------------------
// genserKinematicsInverse() is 5104 with buster amd64 gcc 8.3.0-6
//#pragma GCC diagnostic error   "-Wframe-larger-than=6000"
  #pragma GCC diagnostic warning "-Wframe-larger-than=6000"

#include "rtapi.h"
#include <rtapi_string.h>
#include "genserkins.h"
#include "motion.h"
#include "switchkins.h"

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
    kp->kinsname    = "genserkins"; // !!! must agree with filename
    kp->halprefix   = "genserkins"; // hal pin names
    kp->required_coordinates = "xyzabc";
    kp->max_joints  = strlen(kp->required_coordinates);
    kp->allow_duplicates  = 0;
     
    *kset0 = genserKinematicsSetup;
    *kfwd0 = genserKinematicsForward;
    *kinv0 = genserKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
}
