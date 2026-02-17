/********************************************************************
 * scarakins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include <stddef.h>
#include "scarakins_math.h"

static int scara_inv(KinematicsUserContext *ctx,
                     const EmcPose *world, double *joints)
{
    scara_params_t params;
    params.d1 = ctx->params.params.scara.d1;
    params.d2 = ctx->params.params.scara.d2;
    params.d3 = ctx->params.params.scara.d3;
    params.d4 = ctx->params.params.scara.d4;
    params.d5 = ctx->params.params.scara.d5;
    params.d6 = ctx->params.params.scara.d6;
    return scara_inverse_math(&params, world, joints, 0, NULL);
}

static int scara_fwd(KinematicsUserContext *ctx,
                     const double *joints, EmcPose *world)
{
    scara_params_t params;
    params.d1 = ctx->params.params.scara.d1;
    params.d2 = ctx->params.params.scara.d2;
    params.d3 = ctx->params.params.scara.d3;
    params.d4 = ctx->params.params.scara.d4;
    params.d5 = ctx->params.params.scara.d5;
    params.d6 = ctx->params.params.scara.d6;
    return scara_forward_math(&params, joints, world, NULL);
}

static int scara_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("scarakins.D1", &fval) == 0)
        ctx->params.params.scara.d1 = fval;
    if (hal_pin_reader_read_float("scarakins.D2", &fval) == 0)
        ctx->params.params.scara.d2 = fval;
    if (hal_pin_reader_read_float("scarakins.D3", &fval) == 0)
        ctx->params.params.scara.d3 = fval;
    if (hal_pin_reader_read_float("scarakins.D4", &fval) == 0)
        ctx->params.params.scara.d4 = fval;
    if (hal_pin_reader_read_float("scarakins.D5", &fval) == 0)
        ctx->params.params.scara.d5 = fval;
    if (hal_pin_reader_read_float("scarakins.D6", &fval) == 0)
        ctx->params.params.scara.d6 = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = scara_inv;
    ctx->forward = scara_fwd;
    ctx->refresh = scara_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    ctx->params.params.scara.d1 = 490.0;
    ctx->params.params.scara.d2 = 340.0;
    ctx->params.params.scara.d3 = 50.0;
    ctx->params.params.scara.d4 = 250.0;
    ctx->params.params.scara.d5 = 50.0;
    ctx->params.params.scara.d6 = 50.0;
    return 0;
}
