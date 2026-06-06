// maxkins — tabletop 5-axis mill kinematics (cmod version)
// Copyright (c) 2007 Chris Radek. License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define d2r(d) ((d) * M_PI / 180.0)

static double my_hypot(double a, double b) { return sqrt(a * a + b * b); }

// ─── HAL pins ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t *pivot_length;
};
static struct haldata *haldata;

// ─── Forward kinematics ───

static int32_t maxkins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;

    // B correction
    double zb = (*(haldata->pivot_length) + joints[8]) * cos(d2r(joints[4]));
    double xb = (*(haldata->pivot_length) + joints[8]) * sin(d2r(joints[4]));

    // C correction
    double xyr = my_hypot(joints[0], joints[1]);
    double xytheta = atan2(joints[1], joints[0]) + d2r(joints[5]);

    // U correction
    double zv = joints[6] * sin(d2r(joints[4]));
    double xv = joints[6] * cos(d2r(joints[4]));

    world->x = xyr * cos(xytheta) + xb - xv;
    world->y = xyr * sin(xytheta) - joints[7];
    world->z = joints[2] - zb + zv + *(haldata->pivot_length);

    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

// ─── Inverse kinematics ───

static int32_t maxkins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;

    // B correction
    double zb = (*(haldata->pivot_length) + world->w) * cos(d2r(world->b));
    double xb = (*(haldata->pivot_length) + world->w) * sin(d2r(world->b));

    // C correction
    double xyr = my_hypot(world->x, world->y);
    double xytheta = atan2(world->y, world->x) - d2r(world->c);

    // U correction
    double zv = world->u * sin(d2r(world->b));
    double xv = world->u * cos(d2r(world->b));

    joints[0] = xyr * cos(xytheta) - xb + xv;
    joints[1] = xyr * sin(xytheta) + world->v;
    joints[2] = world->z + zb + zv - *(haldata->pivot_length);

    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

static kins_kinematics_type_t maxkins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t maxkins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t maxkins_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t maxkins_callbacks = {
    .ctx = NULL,
    .forward    = maxkins_forward,
    .inverse    = maxkins_inverse,
    .type       = maxkins_type,
    .switchable = maxkins_switchable,
    .switch_    = maxkins_switch,
};

// ─── cmod lifecycle ───

static cmod_t maxkins_cmod;

static void maxkins_destroy(cmod_t *self) {
    (void)self;
    if (g_hal && g_comp_id > 0)
        g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)argc; (void)argv;

    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO,
                                     &haldata->pivot_length,
                                     g_comp_id, "%s.pivot-length", name);
    if (rc < 0) goto fail;

    *(haldata->pivot_length) = 0.666;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &maxkins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    maxkins_cmod.Destroy = maxkins_destroy;
    *out = &maxkins_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
