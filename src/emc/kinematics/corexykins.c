/********************************************************************
* Description: kinematics for corexy
* Adapted from trivkins.c
* ref: http://corexy.com/theory.html
********************************************************************/

#include "motion.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "kinematics.h"
#include "corexykins_math.h"

static struct data {
    hal_s32_t joints[EMCMOT_MAX_JOINTS];
} *data;

int kinematicsForward(const double *joints
                     ,EmcPose *pos
                     ,const KINEMATICS_FORWARD_FLAGS *fflags
                     ,KINEMATICS_INVERSE_FLAGS *iflags
                     ) {
    (void)fflags;
    (void)iflags;
    return corexy_forward_math(joints, pos);
}

int kinematicsInverse(const EmcPose *pos
                     ,double *joints
                     ,const KINEMATICS_INVERSE_FLAGS *iflags
                     ,KINEMATICS_FORWARD_FLAGS *fflags
                     ) {
    (void)iflags;
    (void)fflags;
    return corexy_inverse_math(pos, joints);
}

int kinematicsHome(EmcPose *world
                  ,double *joint
                  ,KINEMATICS_FORWARD_FLAGS *fflags
                  ,KINEMATICS_INVERSE_FLAGS *iflags
                  ) {
    *fflags = 0;
    *iflags = 0;
    return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType() { return KINEMATICS_BOTH; }

const char* kinematicsGetName(void) { return "corexykins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

static int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("corexykins");
    if(comp_id < 0) return comp_id;

    data = hal_malloc(sizeof(struct data));

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
