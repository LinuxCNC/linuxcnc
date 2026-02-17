/********************************************************************
 * pumakins userspace plugin (needs libposemath)
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include <stddef.h>
#include "pumakins_math.h"

static int puma_inv(KinematicsUserContext *ctx,
                    const EmcPose *world, double *joints)
{
    puma_params_t params;
    params.a2 = ctx->params.params.puma.a2;
    params.a3 = ctx->params.params.puma.a3;
    params.d3 = ctx->params.params.puma.d3;
    params.d4 = ctx->params.params.puma.d4;
    params.d6 = ctx->params.params.puma.d6;
    return puma_inverse_math(&params, world, joints, NULL, 0, NULL);
}

static int puma_fwd(KinematicsUserContext *ctx,
                    const double *joints, EmcPose *world)
{
    puma_params_t params;
    params.a2 = ctx->params.params.puma.a2;
    params.a3 = ctx->params.params.puma.a3;
    params.d3 = ctx->params.params.puma.d3;
    params.d4 = ctx->params.params.puma.d4;
    params.d6 = ctx->params.params.puma.d6;
    return puma_forward_math(&params, joints, world, NULL);
}

static int puma_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("pumakins.A2", &fval) == 0)
        ctx->params.params.puma.a2 = fval;
    if (hal_pin_reader_read_float("pumakins.A3", &fval) == 0)
        ctx->params.params.puma.a3 = fval;
    if (hal_pin_reader_read_float("pumakins.D3", &fval) == 0)
        ctx->params.params.puma.d3 = fval;
    if (hal_pin_reader_read_float("pumakins.D4", &fval) == 0)
        ctx->params.params.puma.d4 = fval;
    if (hal_pin_reader_read_float("pumakins.D6", &fval) == 0)
        ctx->params.params.puma.d6 = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = puma_inv;
    ctx->forward = puma_fwd;
    ctx->refresh = puma_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    ctx->params.params.puma.a2 = 300.0;
    ctx->params.params.puma.a3 = 50.0;
    ctx->params.params.puma.d3 = 70.0;
    ctx->params.params.puma.d4 = 400.0;
    ctx->params.params.puma.d6 = 70.0;
    return 0;
}
