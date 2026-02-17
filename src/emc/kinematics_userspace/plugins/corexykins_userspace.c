/********************************************************************
 * corexykins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "corexykins_math.h"

static int corexy_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    (void)ctx;
    return corexy_inverse_math(world, joints);
}

static int corexy_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    (void)ctx;
    return corexy_forward_math(joints, world);
}

static int corexy_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = corexy_inv;
    ctx->forward = corexy_fwd;
    ctx->refresh = corexy_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
