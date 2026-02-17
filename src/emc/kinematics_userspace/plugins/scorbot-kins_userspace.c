/********************************************************************
 * scorbot-kins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "scorbotkins_math.h"

static int scorbot_inv(KinematicsUserContext *ctx,
                       const EmcPose *world, double *joints)
{
    scorbot_params_t params;
    params.l0_horizontal = ctx->params.params.scorbot.l0_horizontal;
    params.l0_vertical = ctx->params.params.scorbot.l0_vertical;
    params.l1_length = ctx->params.params.scorbot.l1_length;
    params.l2_length = ctx->params.params.scorbot.l2_length;
    return scorbot_inverse_math(&params, world, joints);
}

static int scorbot_fwd(KinematicsUserContext *ctx,
                       const double *joints, EmcPose *world)
{
    scorbot_params_t params;
    params.l0_horizontal = ctx->params.params.scorbot.l0_horizontal;
    params.l0_vertical = ctx->params.params.scorbot.l0_vertical;
    params.l1_length = ctx->params.params.scorbot.l1_length;
    params.l2_length = ctx->params.params.scorbot.l2_length;
    return scorbot_forward_math(&params, joints, world);
}

static int scorbot_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = scorbot_inv;
    ctx->forward = scorbot_fwd;
    ctx->refresh = scorbot_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    ctx->params.params.scorbot.l0_horizontal = 16.0;
    ctx->params.params.scorbot.l0_vertical = 140.0;
    ctx->params.params.scorbot.l1_length = 221.0;
    ctx->params.params.scorbot.l2_length = 221.0;
    return 0;
}
