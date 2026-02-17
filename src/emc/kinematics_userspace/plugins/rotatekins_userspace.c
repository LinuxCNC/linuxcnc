/********************************************************************
 * rotatekins userspace plugin
 ********************************************************************/
#include "kins_plugin.h"
#include "rotatekins_math.h"

static int rotate_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    (void)ctx;
    return rotate_inverse_math(world, joints);
}

static int rotate_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    (void)ctx;
    return rotate_forward_math(joints, world);
}

static int rotate_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = rotate_inv;
    ctx->forward = rotate_fwd;
    ctx->refresh = rotate_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
