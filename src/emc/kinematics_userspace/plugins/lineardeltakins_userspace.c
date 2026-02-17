/********************************************************************
 * lineardeltakins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "lineardeltakins_math.h"

static int ldelta_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    lineardelta_params_t params;
    params.radius = ctx->params.params.lineardelta.jointradius;
    params.rod_length = ctx->params.params.lineardelta.radius;
    return lineardelta_inverse_math(&params, world, joints);
}

static int ldelta_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    lineardelta_params_t params;
    params.radius = ctx->params.params.lineardelta.jointradius;
    params.rod_length = ctx->params.params.lineardelta.radius;
    return lineardelta_forward_math(&params, joints, world);
}

static int ldelta_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("lineardeltakins.R", &fval) == 0)
        ctx->params.params.lineardelta.jointradius = fval;
    if (hal_pin_reader_read_float("lineardeltakins.L", &fval) == 0)
        ctx->params.params.lineardelta.radius = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = ldelta_inv;
    ctx->forward = ldelta_fwd;
    ctx->refresh = ldelta_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    ctx->params.params.lineardelta.radius = 269.0;
    ctx->params.params.lineardelta.jointradius = 130.25;
    return 0;
}
