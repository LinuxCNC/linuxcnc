// rosekins — rose engine / polar kinematics (cmod version)
// Copyright 2016 Dewey Garrett. License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TO_RAD (M_PI / 180.0)
#define TO_DEG (180.0 / M_PI)
#define PM_2_PI (2.0 * M_PI)

static double my_hypot(double a, double b) { return sqrt(a * a + b * b); }

// ─── HAL pins ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t *revolutions;
    gomc_hal_float_t *theta_degrees;
    gomc_hal_float_t *bigtheta_degrees;
};
static struct haldata *haldata;

// ─── Inverse state ───

static int  oldquad;
static int  revolutions;

// ─── Forward kinematics ───

static int32_t rosekins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;

    double radius = joints[0];
    double z      = joints[1];
    double theta  = TO_RAD * joints[2];

    world->x = radius * cos(theta);
    world->y = radius * sin(theta);
    world->z = z;
    world->a = 0;  world->b = 0;  world->c = 0;
    world->u = 0;  world->v = 0;  world->w = 0;

    return 0;
}

// ─── Inverse kinematics ───

static int32_t rosekins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;

    int nowquad = 0;
    double x = world->x, y = world->y, z = world->z;

    if      (x >= 0 && y >= 0) nowquad = 1;
    else if (x <  0 && y >= 0) nowquad = 2;
    else if (x <  0 && y <  0) nowquad = 3;
    else if (x >= 0 && y <  0) nowquad = 4;

    if (oldquad == 2 && nowquad == 3) revolutions += 1;
    if (oldquad == 3 && nowquad == 2) revolutions -= 1;

    double theta    = atan2(y, x);
    double bigtheta = theta + PM_2_PI * revolutions;

    *(haldata->revolutions)      = revolutions;
    *(haldata->theta_degrees)    = theta * TO_DEG;
    *(haldata->bigtheta_degrees) = bigtheta * TO_DEG;

    joints[0] = my_hypot(x, y);
    joints[1] = z;
    joints[2] = TO_DEG * bigtheta;
    joints[3] = 0; joints[4] = 0; joints[5] = 0;
    joints[6] = 0; joints[7] = 0; joints[8] = 0;

    oldquad = nowquad;
    return 0;
}

static kins_kinematics_type_t rosekins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t rosekins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t rosekins_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t rosekins_callbacks = {
    .ctx = NULL,
    .forward    = rosekins_forward,
    .inverse    = rosekins_inverse,
    .type       = rosekins_type,
    .switchable = rosekins_switchable,
    .switch_    = rosekins_switch,
};

// ─── cmod lifecycle ───

static cmod_t rosekins_cmod;

static void rosekins_destroy(cmod_t *self) {
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
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &haldata->revolutions,
                                 g_comp_id, "%s.revolutions", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &haldata->theta_degrees,
                                 g_comp_id, "%s.theta_degrees", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &haldata->bigtheta_degrees,
                                 g_comp_id, "%s.bigtheta_degrees", name);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &rosekins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    rosekins_cmod.Destroy = rosekins_destroy;
    *out = &rosekins_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
