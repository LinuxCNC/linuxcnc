/*****************************************************************
* Description: scarakins.c
*   Kinematics for scara typed robots
*   Set the params using HAL to fit your robot
*
*   Derived from a work by Sagar Behere
*
* Author: Sagar Behere
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2003 All rights reserved.
*
* Last change:
*******************************************************************
*/

#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "posemath.h"
#include "hal.h"
#include "kinematics.h"
#include "switchkins.h"
#include "scarakins_math.h"

struct scara_data {
    hal_float_t *d1, *d2, *d3, *d4, *d5, *d6;
} *haldata = 0;

/* key dimensions

   joint[0] = Entire arm rotates around a vertical axis at its inner end
                which is attached to the earth.  A value of zero means the
                inner arm is pointing along the X axis.
   D1 = Vertical distance from the ground plane to the center of the inner
                arm.
   D2 = Horizontal distance between joint[0] axis and joint[1] axis, ie.
                the length of the inner arm.
   joint[1] = Outer arm rotates around a vertical axis at its inner end
                which is attached to the outer end of the inner arm.  A
                value of zero means the outer arm is parallel to the
                inner arm (and extending outward).
   D3 = Vertical distance from the center of the inner arm to the center
                of the outer arm.  May be positive or negative depending
                on the structure of the robot.
   joint[2] = End effector slides along a vertical axis at the outer end
                of the outer arm.  A value of zero means the end effector
                is at the same height as the center of the outer arm, and
                positive values mean downward movement.
   D4 = Horizontal distance between joint[1] axis and joint[2] axis, ie.
                the length of the outer arm
   joint[3] = End effector rotates around the same vertical axis that it
                slides along.  A value of zero means that the tooltip (if
                offset from the axis) is pointing in the same direction
                as the centerline of the outer arm.
   D5 = Vertical distance from the end effector to the tooltip.  Positive
                means the tooltip is lower than the end effector, and is
                the normal case.
   D6 = Horizontal distance from the centerline of the end effector (and
                the joints 2 and 3 axis) and the tooltip.  Zero means the
                tooltip is on the centerline.  Non-zero values should be
                positive, if negative they introduce a 180 degree offset
                on the value of joint[3].
*/

#define D1 (*(haldata->d1))
#define D2 (*(haldata->d2))
#define D3 (*(haldata->d3))
#define D4 (*(haldata->d4))
#define D5 (*(haldata->d5))
#define D6 (*(haldata->d6))

/* joint[0], joint[1] and joint[3] are in degrees and joint[2] is in length units */
static
int scaraKinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    scara_params_t params;
    int flags_out = 0;

    params.d1 = D1;
    params.d2 = D2;
    params.d3 = D3;
    params.d4 = D4;
    params.d5 = D5;
    params.d6 = D6;

    scara_forward_math(&params, joint, world, &flags_out);

    *iflags = flags_out;
    return 0;
} //scaraKinematicsForward()

static int scaraKinematicsInverse(const EmcPose * world,
                                  double * joint,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags)
{
    scara_params_t params;
    int flags_out = 0;

    params.d1 = D1;
    params.d2 = D2;
    params.d3 = D3;
    params.d4 = D4;
    params.d5 = D5;
    params.d6 = D6;

    scara_inverse_math(&params, world, joint, *iflags, &flags_out);

    *fflags = flags_out;
    return 0;
} // scaraKinematicsInverse()

#define DEFAULT_D1 490
#define DEFAULT_D2 340
#define DEFAULT_D3  50
#define DEFAULT_D4 250
#define DEFAULT_D5  50
#define DEFAULT_D6  50

static int scaraKinematicsSetup(const  int   comp_id,
                                const  char* coordinates,
                                kparms*      kp)
{
    (void)coordinates;
    int res=0;

    haldata = hal_malloc(sizeof(*haldata));
    if (!haldata) goto error;

    res += hal_pin_float_newf(HAL_IN, &(haldata->d1), comp_id,"%s.D1",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d2), comp_id,"%s.D2",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d3), comp_id,"%s.D3",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d4), comp_id,"%s.D4",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d5), comp_id,"%s.D5",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d6), comp_id,"%s.D6",kp->halprefix);
    if (res) { goto error; }

    D1 = DEFAULT_D1;
    D2 = DEFAULT_D2;
    D3 = DEFAULT_D3;
    D4 = DEFAULT_D4;
    D5 = DEFAULT_D5;
    D6 = DEFAULT_D6;

    return 0;

error:
    return -1;
} // scaraKinematicsSetup()

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "scarakins"; // !!! must agree with filename
    kp->halprefix   = "scarakins"; // hal pin names
    kp->required_coordinates = "xyzabc"; // ab are scaragui table tilts
    kp->allow_duplicates     = 0;
    kp->max_joints = strlen(kp->required_coordinates);

    rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
    *kset0 = scaraKinematicsSetup;
    *kfwd0 = scaraKinematicsForward;
    *kinv0 = scaraKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
} // switchkinsSetup()
