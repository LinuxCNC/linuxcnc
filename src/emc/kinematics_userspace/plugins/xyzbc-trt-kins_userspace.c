/********************************************************************
 * xyzbc-trt-kins userspace plugin - tilting rotary table (B,C axes)
 ********************************************************************/
#include "kins_plugin.h"
#include "kinematics_user.h"
#include "hal_pin_reader.h"
#include "trtfuncs_math.h"

static inline void build_jmap(const kinematics_params_t *kp, trt_joints_t *jmap)
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

static inline void build_params(const kins_trt_params_t *trt, trt_params_t *params)
{
    params->x_rot_point = trt->x_rot_point;
    params->y_rot_point = trt->y_rot_point;
    params->z_rot_point = trt->z_rot_point;
    params->x_offset = trt->x_offset;
    params->y_offset = trt->y_offset;
    params->z_offset = trt->z_offset;
    params->tool_offset = trt->tool_offset;
    params->conventional_directions = trt->conventional_directions;
}

static int xyzbc_inverse(KinematicsUserContext *ctx,
                         const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;
    EmcPose axis_values;

    build_params(&kp->params.trt, &params);
    build_jmap(kp, &jmap);
    xyzbc_inverse_math(&params, &jmap, world, &axis_values);
    trt_axis_to_joints(&jmap, &axis_values, joints);
    return 0;
}

static int xyzbc_forward(KinematicsUserContext *ctx,
                         const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;

    build_params(&kp->params.trt, &params);
    build_jmap(kp, &jmap);
    return xyzbc_forward_math(&params, &jmap, joints, world);
}

static int xyzbc_refresh(KinematicsUserContext *ctx)
{
    double fval;
    int ival;

    if (hal_pin_reader_read_float("xyzbc-trt-kins.x-rot-point", &fval) == 0)
        ctx->params.params.trt.x_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.y-rot-point", &fval) == 0)
        ctx->params.params.trt.y_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.z-rot-point", &fval) == 0)
        ctx->params.params.trt.z_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.x-offset", &fval) == 0)
        ctx->params.params.trt.x_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.y-offset", &fval) == 0)
        ctx->params.params.trt.y_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.z-offset", &fval) == 0)
        ctx->params.params.trt.z_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.tool-offset", &fval) == 0)
        ctx->params.params.trt.tool_offset = fval;
    if (hal_pin_reader_read_bit("xyzbc-trt-kins.conventional-directions", &ival) == 0)
        ctx->params.params.trt.conventional_directions = ival;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = xyzbc_inverse;
    ctx->forward = xyzbc_forward;
    ctx->refresh = xyzbc_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
