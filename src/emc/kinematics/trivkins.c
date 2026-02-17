/********************************************************************
* Description: trivkins.c
*   general trivkins for 3 axis Cartesian machine
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
*
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/

#include "motion.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi.h"      /* RTAPI realtime OS API */
#include "rtapi_app.h"  /* RTAPI realtime module decls */
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "kinematics.h"


#define SET(f) pos->f = joints[i]

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    return identityKinematicsForward(joints, pos, fflags, iflags);
}

int kinematicsInverse(const EmcPose * pos,
                      double *joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    return identityKinematicsInverse(pos, joints, iflags, fflags);
}

static KINEMATICS_TYPE ktype = -1;

KINEMATICS_TYPE kinematicsType()
{
    return ktype;
}

#define TRIVKINS_DEFAULT_COORDINATES "XYZABCUVW"
static char *coordinates = TRIVKINS_DEFAULT_COORDINATES;
RTAPI_MP_STRING(coordinates, "Existing Axes");

static char *kinstype = "1"; // use KINEMATICS_IDENTITY
RTAPI_MP_STRING(kinstype, "Kinematics Type (Identity,Both)");

const char* kinematicsGetName(void)
{
    return "trivkins";
}

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

static int comp_id;

int rtapi_app_main(void) {
    kparms ksetup;

    switch (*kinstype) {
      case 'b': case 'B': ktype = KINEMATICS_BOTH;         break;
      case 'f': case 'F': ktype = KINEMATICS_FORWARD_ONLY; break;
      case 'i': case 'I': ktype = KINEMATICS_INVERSE_ONLY; break;
      case '1': default:  ktype = KINEMATICS_IDENTITY;
    }

    comp_id = hal_init("trivkins");
    if(comp_id < 0) return comp_id;

    // see typedef for KS KinematicsSETUP:
    ksetup.max_joints       = EMCMOT_MAX_JOINTS;
    ksetup.allow_duplicates = 1;
    if (identityKinematicsSetup(comp_id, coordinates, &ksetup)) {
       return -1; //setup failed
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"
#include <string.h>

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    int i;
    double axes[9] = {0};

    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        if (axis >= 0 && axis < 9)
            axes[axis] = joints[i];
    }
    pos->tran.x = axes[0]; pos->tran.y = axes[1]; pos->tran.z = axes[2];
    pos->a = axes[3]; pos->b = axes[4]; pos->c = axes[5];
    pos->u = axes[6]; pos->v = axes[7]; pos->w = axes[8];
    return 0;
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    int i;
    double axes[9];

    axes[0] = pos->tran.x; axes[1] = pos->tran.y; axes[2] = pos->tran.z;
    axes[3] = pos->a; axes[4] = pos->b; axes[5] = pos->c;
    axes[6] = pos->u; axes[7] = pos->v; axes[8] = pos->w;

    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        joints[i] = (axis >= 0 && axis < 9) ? axes[axis] : 0.0;
    }
    return 0;
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    (void)params; (void)read_float; (void)read_bit; (void)read_s32;
    return 0;
}

int nonrt_is_identity(void) { return 1; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
