/********************************************************************
 * trivkins userspace plugin - identity kinematics
 ********************************************************************/
#include "kins_plugin.h"
#include "kinematics_user.h"  /* emcPoseGetAxis, emcPoseSetAxis */
#include <string.h>

static int trivkins_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        if (axis >= 0 && axis < 9)
            joints[i] = emcPoseGetAxis(world, axis);
        else
            joints[i] = 0.0;
    }
    return 0;
}

static int trivkins_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    memset(world, 0, sizeof(EmcPose));
    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        if (axis >= 0 && axis < 9)
            emcPoseSetAxis(world, axis, joints[i]);
    }
    return 0;
}

static int trivkins_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = trivkins_inverse;
    ctx->forward = trivkins_forward;
    ctx->refresh = trivkins_refresh;
    ctx->is_identity = 1;
    ctx->kins_type = KINEMATICS_IDENTITY;
    return 0;
}
