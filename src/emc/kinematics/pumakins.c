/*****************************************************************
* Description: pumakins.c
*   Kinematics for puma typed robots
*   Set the params using HAL to fit your robot
*
*   Derived from a work by Fred Proctor
*
*   modified by rdp to add effect of D6 parameter (see pumagui)
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*******************************************************************
*/
#include "pumakins.h"
#include "pumakins_math.h"
#include "posemath.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "hal.h"
#include "kinematics.h"
#include "switchkins.h"

struct haldata {
    hal_float_t *a2, *a3, *d3, *d4, *d6;
} *haldata = 0;

#define PUMA_A2 (*(haldata->a2))
#define PUMA_A3 (*(haldata->a3))
#define PUMA_D3 (*(haldata->d3))
#define PUMA_D4 (*(haldata->d4))
#define PUMA_D6 (*(haldata->d6))

static int pumaKinematicsForward(const double * joint,
                                 EmcPose * world,
                                 const KINEMATICS_FORWARD_FLAGS * fflags,
                                 KINEMATICS_INVERSE_FLAGS * iflags)
{
   (void)fflags;
   puma_params_t params;
   int flags_out = 0;

   params.a2 = PUMA_A2;
   params.a3 = PUMA_A3;
   params.d3 = PUMA_D3;
   params.d4 = PUMA_D4;
   params.d6 = PUMA_D6;

   puma_forward_math(&params, joint, world, &flags_out);

   *iflags = flags_out;
   return 0;
}

static int pumaKinematicsInverse(const EmcPose * world,
                                 double * joint,
                                 const KINEMATICS_INVERSE_FLAGS * iflags,
                                 KINEMATICS_FORWARD_FLAGS * fflags)
{
   puma_params_t params;
   int flags_out = 0;

   params.a2 = PUMA_A2;
   params.a3 = PUMA_A3;
   params.d3 = PUMA_D3;
   params.d4 = PUMA_D4;
   params.d6 = PUMA_D6;

   *fflags = 0;

   puma_inverse_math(&params, world, joint, joint, *iflags, &flags_out);

   *fflags = flags_out;
   return 0;
}

int pumaKinematicsSetup(const  int   comp_id,
                        const  char* coordinates,
                        kparms*      kp)
{
    (void)coordinates;
    int res=0;

    haldata = hal_malloc(sizeof(*haldata));
    if (!haldata) goto error;


    res += hal_pin_float_newf(HAL_IN, &(haldata->a2), comp_id,"%s.A2",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->a3), comp_id,"%s.A3",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d3), comp_id,"%s.D3",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d4), comp_id,"%s.D4",kp->halprefix);
    res += hal_pin_float_newf(HAL_IN, &(haldata->d6), comp_id,"%s.D6",kp->halprefix);
    if (res) { goto error; }

    PUMA_A2 = DEFAULT_PUMA560_A2;
    PUMA_A3 = DEFAULT_PUMA560_A3;
    PUMA_D3 = DEFAULT_PUMA560_D3;
    PUMA_D4 = DEFAULT_PUMA560_D4;
    PUMA_D6 = DEFAULT_PUMA560_D6;

    return 0;

error:
    return -1;
} // pumaKinematicsSetup()

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "pumakins"; // !!! must agree with filename
    kp->halprefix   = "pumakins"; // hal pin names
    kp->required_coordinates = "xyzabc";
    kp->allow_duplicates     = 0;
    kp->max_joints = strlen(kp->required_coordinates);

    rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
    *kset0 = pumaKinematicsSetup;
    *kfwd0 = pumaKinematicsForward;
    *kinv0 = pumaKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
} // switchkinsSetup()
