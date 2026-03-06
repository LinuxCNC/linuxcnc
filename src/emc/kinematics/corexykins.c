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
#include "hal_priv.h"
#include "kinematics_params.h"

/*
 * CoreXY kinematics - pure linear transformation, no parameters
 *
 * Forward:  X = 0.5*(J0+J1),  Y = 0.5*(J0-J1)
 * Inverse:  J0 = X+Y,         J1 = X-Y
 */
static int corexy_forward_math(const double *joints, EmcPose *world)
{
    world->tran.x = 0.5 * (joints[0] + joints[1]);
    world->tran.y = 0.5 * (joints[0] - joints[1]);
    world->tran.z = joints[2];
    world->a      = joints[3];
    world->b      = joints[4];
    world->c      = joints[5];
    world->u      = joints[6];
    world->v      = joints[7];
    world->w      = joints[8];

    return 0;
}

static int corexy_inverse_math(const EmcPose *world, double *joints)
{
    joints[0] = world->tran.x + world->tran.y;
    joints[1] = world->tran.x - world->tran.y;
    joints[2] = world->tran.z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

static struct data {
    hal_s32_t joints[EMCMOT_MAX_JOINTS];
} *data;

static kinematics_params_t *uspace_params;

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

    uspace_params = (kinematics_params_t *)hal_malloc(sizeof(kinematics_params_t));
    if (!uspace_params) { hal_exit(comp_id); return -1; }
    if (hal_param_s32_newf(HAL_RO, &uspace_params->self_offset, comp_id,
                         "corexykins.uspace-params-offset") < 0) {
        hal_exit(comp_id); return -1;
    }
    memset(uspace_params, 0, sizeof(*uspace_params));
    uspace_params->num_joints = 9;
    uspace_params->valid       = 1;
    uspace_params->is_identity = 0;
    uspace_params->head = 1;
    uspace_params->tail = 1;
    uspace_params->self_offset = (int)SHMOFF(uspace_params);

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */

void nonrt_attach(char *shmem_base, int offset, nonrt_ops_t *ops)
{
    (void)shmem_base; (void)offset;
    ops->forward = kinematicsForward;
    ops->inverse = kinematicsInverse;
}

EXPORT_SYMBOL(nonrt_attach);
