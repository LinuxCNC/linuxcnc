/********************************************************************
* xyzac-trt-kins.c employing switchkins.[ch]
* License: GPL Version 2
*
* NOTEs:
*  1) specify all kparms items
*  2) the 0th switchkins_type is the startup default
*  3) sparm is a module string parameter for configuration
*  4) The directions of the rotational axes are the opposite of the
*     conventional axis directions.
*  5) the maths and the geometry table are in trtfuncs.c, written as
*     pure functions of the parameter block (see kinematics.h)
*/

#include <rtapi.h>
#include <rtapi_string.h>
#include <emcmotcfg.h>

#include <switchkins.h>

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    (void)kset0; (void)kset1; (void)kset2;
    (void)kfwd0; (void)kfwd1; (void)kfwd2;
    (void)kinv0; (void)kinv1; (void)kinv2;
    kp->kinsname    = "xyzac-trt-kins"; // !!! must agree with filename
    kp->halprefix   = "xyzac-trt-kins"; // hal pin names
    kp->required_coordinates = "xyzac";
    kp->allow_duplicates     = 1;
    kp->max_joints           = EMCMOT_MAX_JOINTS;
    kp->params               = TRT_PARAMS;
    kp->nparams              = TRT_NPARAMS;

    if (kp->sparm && strstr(kp->sparm,"identityfirst")) {
        rtapi_print("\n!!! switchkins-type 0 is IDENTITY\n");
        switchkinsRegisterOps(0, &KINS_IDENTITY_OPS);
        switchkinsRegisterOps(1, &XYZAC_OPS);
    } else {
        rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
        switchkinsRegisterOps(0, &XYZAC_OPS);
        switchkinsRegisterOps(1, &KINS_IDENTITY_OPS);
    }

    switchkinsRegisterOps(2, &USERK_OPS);

    return 0;
}
