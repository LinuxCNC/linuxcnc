/********************************************************************
 * tripodkins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "tripodkins_math.h"
#include <stddef.h>

static int tripod_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    tripod_params_t params;
    params.bx = ctx->params.params.tripod.bx;
    params.cx = ctx->params.params.tripod.cx;
    params.cy = ctx->params.params.tripod.cy;
    return tripod_inverse_math(&params, world, joints, NULL, NULL);
}

static int tripod_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    tripod_params_t params;
    params.bx = ctx->params.params.tripod.bx;
    params.cx = ctx->params.params.tripod.cx;
    params.cy = ctx->params.params.tripod.cy;
    return tripod_forward_math(&params, joints, world, NULL, NULL);
}

static int tripod_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("tripodkins.Bx", &fval) == 0)
        ctx->params.params.tripod.bx = fval;
    if (hal_pin_reader_read_float("tripodkins.Cx", &fval) == 0)
        ctx->params.params.tripod.cx = fval;
    if (hal_pin_reader_read_float("tripodkins.Cy", &fval) == 0)
        ctx->params.params.tripod.cy = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = tripod_inv;
    ctx->forward = tripod_fwd;
    ctx->refresh = tripod_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    /* Default values */
    ctx->params.params.tripod.bx = 1.0;
    ctx->params.params.tripod.cx = 1.0;
    ctx->params.params.tripod.cy = 1.0;
    return 0;
}
