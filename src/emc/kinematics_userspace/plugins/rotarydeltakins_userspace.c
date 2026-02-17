/********************************************************************
 * rotarydeltakins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "rotarydeltakins_math.h"

static int rdelta_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    rotarydelta_params_t params;
    params.platformradius = ctx->params.params.rotarydelta.platformradius;
    params.thighlength = ctx->params.params.rotarydelta.thighlength;
    params.shinlength = ctx->params.params.rotarydelta.shinlength;
    params.footradius = ctx->params.params.rotarydelta.footradius;
    return rotarydelta_inverse_math(&params, world, joints);
}

static int rdelta_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    rotarydelta_params_t params;
    params.platformradius = ctx->params.params.rotarydelta.platformradius;
    params.thighlength = ctx->params.params.rotarydelta.thighlength;
    params.shinlength = ctx->params.params.rotarydelta.shinlength;
    params.footradius = ctx->params.params.rotarydelta.footradius;
    return rotarydelta_forward_math(&params, joints, world);
}

static int rdelta_refresh(KinematicsUserContext *ctx)
{
    double fval;
    if (hal_pin_reader_read_float("rotarydeltakins.platformradius", &fval) == 0)
        ctx->params.params.rotarydelta.platformradius = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.thighlength", &fval) == 0)
        ctx->params.params.rotarydelta.thighlength = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.shinlength", &fval) == 0)
        ctx->params.params.rotarydelta.shinlength = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.footradius", &fval) == 0)
        ctx->params.params.rotarydelta.footradius = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = rdelta_inv;
    ctx->forward = rdelta_fwd;
    ctx->refresh = rdelta_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    ctx->params.params.rotarydelta.platformradius = ROTARYDELTA_DEFAULT_PLATFORMRADIUS;
    ctx->params.params.rotarydelta.thighlength = ROTARYDELTA_DEFAULT_THIGHLENGTH;
    ctx->params.params.rotarydelta.shinlength = ROTARYDELTA_DEFAULT_SHINLENGTH;
    ctx->params.params.rotarydelta.footradius = ROTARYDELTA_DEFAULT_FOOTRADIUS;
    return 0;
}
