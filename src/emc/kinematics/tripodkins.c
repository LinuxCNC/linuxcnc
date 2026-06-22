// tripodkins — 3-axis tripod kinematics (cmod version)
//
// Derived from work by Fred Proctor
// Copyright (c) 2004 All rights reserved.
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
// License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

static double sq(double x) { return x * x; }

// ─── HAL pins ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t *bx, *cx, *cy;
};
static struct haldata *haldata;

#define Bx (*(haldata->bx))
#define Cx (*(haldata->cx))
#define Cy (*(haldata->cy))

// ─── Forward kinematics ───

static int32_t tripodkins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)iflags;
    double AD = joints[0], BD = joints[1], CD = joints[2];
    double P, Q, R, s, t, u, Dx, Dy, Dz;

    P = sq(AD);
    Q = sq(BD) - sq(Bx);
    R = sq(CD) - sq(Cx) - sq(Cy);
    s = -2.0 * Bx;
    t = -2.0 * Cx;
    u = -2.0 * Cy;

    if (s == 0.0) { return -1; }
    Dx = (Q - P) / s;

    if (u == 0.0) { return -1; }
    Dy = (R - Q - (t - s) * Dx) / u;
    Dz = P - sq(Dx) - sq(Dy);
    if (Dz < 0.0) { return -1; }
    Dz = sqrt(Dz);
    if (fflags) Dz = -Dz;

    world->x = Dx;
    world->y = Dy;
    world->z = Dz;
    world->a = 0.0; world->b = 0.0; world->c = 0.0;
    world->u = 0.0; world->v = 0.0; world->w = 0.0;

    return 0;
}

// ─── Inverse kinematics ───

static int32_t tripodkins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags;

    joints[0] = sqrt(sq(world->x) + sq(world->y) + sq(world->z));
    joints[1] = sqrt(sq(world->x - Bx) + sq(world->y) + sq(world->z));
    joints[2] = sqrt(sq(world->x - Cx) + sq(world->y - Cy) + sq(world->z));

    if (fflags) {
        *fflags = 0;
        if (world->z < 0.0) *fflags = 1;
    }

    return 0;
}

static kins_kinematics_type_t tripodkins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t tripodkins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t tripodkins_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t tripodkins_callbacks = {
    .ctx = NULL,
    .forward    = tripodkins_forward,
    .inverse    = tripodkins_inverse,
    .type       = tripodkins_type,
    .switchable = tripodkins_switchable,
    .switch_    = tripodkins_switch,
};

// ─── cmod lifecycle ───

static cmod_t tripodkins_cmod;

static void tripodkins_destroy(cmod_t *self) {
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

    int rc;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &haldata->bx,
                                 g_comp_id, "%s.Bx", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &haldata->cx,
                                 g_comp_id, "%s.Cx", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &haldata->cy,
                                 g_comp_id, "%s.Cy", name);
    if (rc < 0) goto fail;

    Bx = Cx = Cy = 1.0;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &tripodkins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    tripodkins_cmod.Destroy = tripodkins_destroy;
    *out = &tripodkins_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
