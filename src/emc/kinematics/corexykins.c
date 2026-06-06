// corexykins — CoreXY kinematics module (cmod version)
// ref: http://corexy.com/theory.html
// License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

// ─── Forward/Inverse kinematics ───

static int32_t corexykins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags,
    uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    world->x = 0.5 * (joints[0] + joints[1]);
    world->y = 0.5 * (joints[0] - joints[1]);
    world->z = joints[2];
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];
    return 0;
}

static int32_t corexykins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags,
    uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    joints[0] = world->x + world->y;
    joints[1] = world->x - world->y;
    joints[2] = world->z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;
    return 0;
}

static kins_kinematics_type_t corexykins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}

static int32_t corexykins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t corexykins_switch(void *ctx, int32_t t) { (void)ctx; (void)t; return -1; }

static kins_callbacks_t corexykins_callbacks = {
    .ctx = NULL,
    .forward    = corexykins_forward,
    .inverse    = corexykins_inverse,
    .type       = corexykins_type,
    .switchable = corexykins_switchable,
    .switch_    = corexykins_switch,
};

// ─── cmod lifecycle ───

static cmod_t corexykins_cmod;

static void corexykins_destroy(cmod_t *self) { (void)self; }

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)argc; (void)argv;

    int rc = kins_api_register(env->api, name, &corexykins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        return rc;
    }

    corexykins_cmod.Destroy = corexykins_destroy;
    *out = &corexykins_cmod;
    return 0;
}
