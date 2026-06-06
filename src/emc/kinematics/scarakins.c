// scarakins — SCARA robot kinematics (cmod version)
// Copyright (c) 2003 Sagar Behere. License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define REQUIRED_COORDINATES "XYZABC"

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

struct scara_data {
    gomc_hal_float_t *d1, *d2, *d3, *d4, *d5, *d6;
};
static struct scara_data *haldata;

#define D1 (*(haldata->d1))
#define D2 (*(haldata->d2))
#define D3 (*(haldata->d3))
#define D4 (*(haldata->d4))
#define D5 (*(haldata->d5))
#define D6 (*(haldata->d6))

#define DEFAULT_D1 490
#define DEFAULT_D2 340
#define DEFAULT_D3  50
#define DEFAULT_D4 250
#define DEFAULT_D5  50
#define DEFAULT_D6  50

// ─── SCARA forward ───

static int32_t scara_forward(const double joints[KINS_MAX_JOINTS],
                         kins_pose_t *world)
{
    double a0 = joints[0] * (M_PI / 180.0);
    double a1 = joints[1] * (M_PI / 180.0);
    double a3 = joints[3] * (M_PI / 180.0);

    a1 = a1 + a0;
    a3 = a3 + a1;

    world->x = D2 * cos(a0) + D4 * cos(a1) + D6 * cos(a3);
    world->y = D2 * sin(a0) + D4 * sin(a1) + D6 * sin(a3);
    world->z = D1 + D3 - joints[2] - D5;
    world->c = a3 * 180.0 / M_PI;
    world->a = joints[4];
    world->b = joints[5];
    world->u = 0; world->v = 0; world->w = 0;
    return 0;
}

// ─── SCARA inverse ───

static int32_t scara_inverse(const kins_pose_t *world,
                         double joints[KINS_MAX_JOINTS])
{
    double a3 = world->c * (M_PI / 180.0);
    double xt = world->x - D6 * cos(a3);
    double yt = world->y - D6 * sin(a3);

    double rsq = xt * xt + yt * yt;
    double cc = (rsq - D2 * D2 - D4 * D4) / (2.0 * D2 * D4);
    if (cc < -1) cc = -1;
    if (cc >  1) cc =  1;
    double q1 = acos(cc);

    // Use current inverse flags convention: stored in world state
    // In the legacy code, *iflags selects elbow up/down.
    // Here we use positive q1 by default (elbow up).

    double q0 = atan2(yt, xt);
    double x2 = D2 + D4 * cos(q1);
    double y2 = D4 * sin(q1);
    q0 = q0 - atan2(y2, x2);

    q0 = q0 * (180.0 / M_PI);
    q1 = q1 * (180.0 / M_PI);

    joints[0] = q0;
    joints[1] = q1;
    joints[2] = D1 + D3 - D5 - world->z;
    joints[3] = world->c - (q0 + q1);
    joints[4] = world->a;
    joints[5] = world->b;
    return 0;
}

// ─── Dispatch ───

static int32_t dispatch_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS], kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    switch (g_sw.current_type) {
        case 0:  return scara_forward(joints, world);
        default: sk_identity_forward(&g_map, joints, world);
                 return 0;
    }
}

static int32_t dispatch_inverse(
    void *ctx,
    const kins_pose_t *world, double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    switch (g_sw.current_type) {
        case 0:  return scara_inverse(world, joints);
        default: sk_identity_inverse(&g_map, world, joints);
                 return 0;
    }
}

static kins_kinematics_type_t dispatch_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t dispatch_switchable(void *ctx) { (void)ctx; return 1; }
static int32_t dispatch_switch(void *ctx, int32_t t)
    { (void)ctx; return sk_switch_to(&g_sw, t); }

static kins_callbacks_t scara_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t scara_cmod;

static void scara_destroy(cmod_t *self) {
    (void)self;
    if (g_hal && g_comp_id > 0) g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    const char *coordinates = REQUIRED_COORDINATES;
    for (int i = 0; i < argc; i++)
        if (strncmp(argv[i], "coordinates=", 12) == 0)
            coordinates = argv[i] + 12;
    if (env->ini) {
        const char *v = env->ini->get(env->ini->ctx, "KINS", "COORDINATES");
        if (v) coordinates = v;
    }

    if (sk_map_coordinates(&g_map, coordinates, 0) < 0) {
        gomc_log_errorf(env->log, name, "bad coordinates: %s", coordinates);
        return -1;
    }

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct scara_data));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc = 0;
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d1,
                                  g_comp_id, "%s.D1", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d2,
                                  g_comp_id, "%s.D2", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d3,
                                  g_comp_id, "%s.D3", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d4,
                                  g_comp_id, "%s.D4", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d5,
                                  g_comp_id, "%s.D5", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d6,
                                  g_comp_id, "%s.D6", name);
    if (rc < 0) goto fail;

    D1 = DEFAULT_D1; D2 = DEFAULT_D2; D3 = DEFAULT_D3;
    D4 = DEFAULT_D4; D5 = DEFAULT_D5; D6 = DEFAULT_D6;

    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &scara_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "kins_api_register failed: %d", rc);
        goto fail;
    }

    scara_cmod.Destroy = scara_destroy;
    *out = &scara_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
