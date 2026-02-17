/********************************************************************
 * 5axiskins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "kinematics_user.h"
#include "hal_pin_reader.h"
#include "5axiskins_math.h"
#include <stdio.h>

static inline void build_jmap(const kinematics_params_t *kp, fiveaxis_joints_t *jmap)
{
    jmap->jx = kp->axis_to_joint[0];
    jmap->jy = kp->axis_to_joint[1];
    jmap->jz = kp->axis_to_joint[2];
    jmap->ja = kp->axis_to_joint[3];
    jmap->jb = kp->axis_to_joint[4];
    jmap->jc = kp->axis_to_joint[5];
    jmap->ju = kp->axis_to_joint[6];
    jmap->jv = kp->axis_to_joint[7];
    jmap->jw = kp->axis_to_joint[8];
}

static int fiveaxis_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    fiveaxis_params_t params;
    fiveaxis_joints_t jmap;
    EmcPose axis_values;

    params.pivot_length = kp->params.fiveaxis.pivot_length;
    build_jmap(kp, &jmap);
    fiveaxis_inverse_math(&params, world, &axis_values);
    fiveaxis_axis_to_joints(&jmap, &axis_values, joints);
    return 0;
}

static int fiveaxis_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    fiveaxis_params_t params;
    fiveaxis_joints_t jmap;

    params.pivot_length = kp->params.fiveaxis.pivot_length;
    build_jmap(kp, &jmap);
    return fiveaxis_forward_math(&params, &jmap, joints, world);
}

static int fiveaxis_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("5axiskins.pivot-length", &fval) == 0) {
        ctx->params.params.fiveaxis.pivot_length = fval;
        return 0;
    }
    fprintf(stderr, "5axiskins_userspace: could not read 5axiskins.pivot-length\n");
    return -1;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = fiveaxis_inverse;
    ctx->forward = fiveaxis_forward;
    ctx->refresh = fiveaxis_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
