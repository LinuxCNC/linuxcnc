// rotatekins — Rotary table kinematics (cmod version)
// Software rotation of XY by the C axis angle.
// Author: Chris Radek
// License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Forward/Inverse kinematics ───

static int32_t rotatekins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags,
    uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    double c_rad = -joints[5] * M_PI / 180.0;
    world->x = joints[0] * cos(c_rad) - joints[1] * sin(c_rad);
    world->y = joints[0] * sin(c_rad) + joints[1] * cos(c_rad);
    world->z = joints[2];
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];
    return 0;
}

static int32_t rotatekins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags,
    uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    double c_rad = world->c * M_PI / 180.0;
    joints[0] = world->x * cos(c_rad) - world->y * sin(c_rad);
    joints[1] = world->x * sin(c_rad) + world->y * cos(c_rad);
    joints[2] = world->z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;
    return 0;
}

static kins_kinematics_type_t rotatekins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}

static int32_t rotatekins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t rotatekins_switch(void *ctx, int32_t t) { (void)ctx; (void)t; return -1; }

static kins_callbacks_t rotatekins_callbacks = {
    .ctx = NULL,
    .forward    = rotatekins_forward,
    .inverse    = rotatekins_inverse,
    .type       = rotatekins_type,
    .switchable = rotatekins_switchable,
    .switch_    = rotatekins_switch,
};

// ─── cmod lifecycle ───

static cmod_t rotatekins_cmod;

static void rotatekins_destroy(cmod_t *self) { (void)self; }

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)argc; (void)argv;

    int rc = kins_api_register(env->api, name, &rotatekins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        return rc;
    }

    rotatekins_cmod.Destroy = rotatekins_destroy;
    *out = &rotatekins_cmod;
    return 0;
}
