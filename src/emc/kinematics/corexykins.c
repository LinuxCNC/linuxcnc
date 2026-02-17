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

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    (void)params;
    return corexy_forward_math(joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    (void)params;
    return corexy_inverse_math(pos, joints);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    (void)params; (void)read_float; (void)read_bit; (void)read_s32;
    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
