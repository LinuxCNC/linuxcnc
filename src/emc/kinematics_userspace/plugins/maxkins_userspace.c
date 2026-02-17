/********************************************************************
 * maxkins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "maxkins_math.h"
#include <stdio.h>

static int maxkins_inv(KinematicsUserContext *ctx,
                       const EmcPose *world, double *joints)
{
    maxkins_params_t params;
    params.pivot_length = ctx->params.params.maxkins.pivot_length;
    params.conventional_directions = ctx->params.params.maxkins.conventional_directions;
    return maxkins_inverse_math(&params, world, joints);
}

static int maxkins_fwd(KinematicsUserContext *ctx,
                       const double *joints, EmcPose *world)
{
    maxkins_params_t params;
    params.pivot_length = ctx->params.params.maxkins.pivot_length;
    params.conventional_directions = ctx->params.params.maxkins.conventional_directions;
    return maxkins_forward_math(&params, joints, world);
}

static int maxkins_refresh(KinematicsUserContext *ctx)
{
    double fval;
    int ival;

    if (hal_pin_reader_read_float("maxkins.pivot-length", &fval) == 0)
        ctx->params.params.maxkins.pivot_length = fval;
    else {
        fprintf(stderr, "maxkins_userspace: could not read maxkins.pivot-length\n");
        return -1;
    }
    if (hal_pin_reader_read_bit("maxkins.conventional-directions", &ival) == 0)
        ctx->params.params.maxkins.conventional_directions = ival;
    else {
        fprintf(stderr, "maxkins_userspace: could not read maxkins.conventional-directions\n");
        return -1;
    }
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = maxkins_inv;
    ctx->forward = maxkins_fwd;
    ctx->refresh = maxkins_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
