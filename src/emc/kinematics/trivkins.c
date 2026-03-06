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

#include <rtapi.h>
#include <rtapi.h>      /* RTAPI realtime OS API */
#include <rtapi_app.h>  /* RTAPI realtime module decls */
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <kinematics.h>
#include "hal_priv.h"
#include "kinematics_params.h"


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

/* Userspace-accessible params in HAL shmem */
static kinematics_params_t *uspace_params;

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

    /* Publish identity flag to HAL shmem for userspace planner.
     * Userspace reads is_identity=1 and builds joint mapping from the
     * coordinates string — no need to duplicate the mapping here. */
    uspace_params = (kinematics_params_t *)hal_malloc(sizeof(kinematics_params_t));
    if (!uspace_params) {
        hal_exit(comp_id);
        return -1;
    }
    memset(uspace_params, 0, sizeof(*uspace_params));
    if (hal_param_s32_newf(HAL_RO, &uspace_params->self_offset, comp_id,
                           "trivkins.uspace-params-offset") < 0) {
        hal_exit(comp_id);
        return -1;
    }
    uspace_params->is_identity = 1;
    uspace_params->valid       = 1;
    uspace_params->head = uspace_params->tail = 1;
    uspace_params->self_offset = (int)SHMOFF(uspace_params);

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
