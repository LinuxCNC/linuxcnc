/********************************************************************
 * rosekins userspace plugin - cylindrical coordinates
 ********************************************************************/
#include "kins_plugin.h"
#include <stddef.h>
#include "rosekins_math.h"

static rosekins_state_t userspace_state = {0, 0};

static int rose_inverse(KinematicsUserContext *ctx,
                        const EmcPose *world, double *joints)
{
    (void)ctx;
    return rosekins_inverse_math(world, joints, &userspace_state, NULL);
}

static int rose_forward(KinematicsUserContext *ctx,
                        const double *joints, EmcPose *world)
{
    (void)ctx;
    return rosekins_forward_math(joints, world);
}

static int rose_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    ctx->inverse = rose_inverse;
    ctx->forward = rose_forward;
    ctx->refresh = rose_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;
    return 0;
}
