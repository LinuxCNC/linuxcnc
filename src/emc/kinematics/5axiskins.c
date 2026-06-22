// 5axiskins — XYZBC 5-axis bridge mill kinematics (cmod version)
// Copyright (c) 2007 Chris Radek
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
// License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TO_RAD (M_PI / 180.0)

#define REQUIRED_COORDINATES "XYZBCW"
#define DEFAULT_PIVOT_LENGTH 250

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static const gomc_log_t  *g_log;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

struct haldata {
    gomc_hal_float_t *pivot_length;
};
static struct haldata *haldata;

// ─── Spherical-to-rectangular helper ───

typedef struct { double x, y, z; } vec3_t;

static vec3_t s2r(double r, double t, double p) {
    vec3_t c;
    double tr = TO_RAD * t, pr = TO_RAD * p;
    c.x = r * sin(pr) * cos(tr);
    c.y = r * sin(pr) * sin(tr);
    c.z = r * cos(pr);
    return c;
}

// ─── Module-specific forward/inverse ───

static int32_t fiveaxis_forward(const double joints[KINS_MAX_JOINTS],
                            kins_pose_t *world)
{
    int JX = g_map.principal[0], JY = g_map.principal[1];
    int JZ = g_map.principal[2], JA = g_map.principal[3];
    int JB = g_map.principal[4], JC = g_map.principal[5];
    int JU = g_map.principal[6], JV = g_map.principal[7];
    int JW = g_map.principal[8];

    vec3_t r = s2r(*(haldata->pivot_length) + joints[JW],
                   joints[JC], 180.0 - joints[JB]);

    world->x = joints[JX] + r.x;
    world->y = joints[JY] + r.y;
    world->z = joints[JZ] + *(haldata->pivot_length) + r.z;
    world->b = joints[JB];
    world->c = joints[JC];
    world->w = joints[JW];
    world->a = (JA >= 0) ? joints[JA] : 0;
    world->u = (JU >= 0) ? joints[JU] : 0;
    world->v = (JV >= 0) ? joints[JV] : 0;
    return 0;
}

static int32_t fiveaxis_inverse(const kins_pose_t *world,
                            double joints[KINS_MAX_JOINTS])
{
    vec3_t r = s2r(*(haldata->pivot_length) + world->w,
                   world->c, 180.0 - world->b);

    // Computed position
    kins_pose_t P;
    P.x = world->x - r.x;
    P.y = world->y - r.y;
    P.z = world->z - *(haldata->pivot_length) - r.z;
    P.b = world->b;
    P.c = world->c;
    P.w = world->w;
    P.a = (g_map.principal[3] >= 0) ? world->a : 0;
    P.u = (g_map.principal[6] >= 0) ? world->u : 0;
    P.v = (g_map.principal[7] >= 0) ? world->v : 0;

    // Map to joints (handles duplicates)
    sk_pos_to_joints(&g_map, &P, joints);
    return 0;
}

// ─── kins_callbacks_t dispatch ───

static int32_t dispatch_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    int rc;
    switch (g_sw.current_type) {
        case 0:  rc = fiveaxis_forward(joints, world); break;
        default: sk_identity_forward(&g_map, joints, world); rc = 0; break;
    }
    return rc;
}

static int32_t dispatch_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    int rc;
    switch (g_sw.current_type) {
        case 0:  rc = fiveaxis_inverse(world, joints); break;
        default: sk_identity_inverse(&g_map, world, joints); rc = 0; break;
    }
    return rc;
}

static kins_kinematics_type_t dispatch_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t dispatch_switchable(void *ctx) { (void)ctx; return 1; }
static int32_t dispatch_switch(void *ctx, int32_t t)
    { (void)ctx; return sk_switch_to(&g_sw, t); }

static kins_callbacks_t fiveaxis_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t fiveaxis_cmod;

static void fiveaxis_destroy(cmod_t *self) {
    (void)self;
    if (g_hal && g_comp_id > 0)
        g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    g_log = env->log;
    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    // Parse parameters
    const char *coordinates = REQUIRED_COORDINATES;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "coordinates=", 12) == 0)
            coordinates = argv[i] + 12;
    }
    if (env->ini) {
        const char *v = env->ini->get(env->ini->ctx, "KINS", "COORDINATES");
        if (v) coordinates = v;
    }

    // Coordinate mapping (allow duplicates)
    if (sk_map_coordinates(&g_map, coordinates, 1) < 0) {
        gomc_log_errorf(env->log, name, "bad coordinates string: %s",
                      coordinates);
        return -1;
    }

    // Verify required coordinates
    const char *reqd = REQUIRED_COORDINATES;
    for (int i = 0; reqd[i]; i++) {
        int ai = (int)(strchr("XYZABCUVW", reqd[i]) - "XYZABCUVW");
        if (g_map.principal[ai] < 0) {
            gomc_log_errorf(env->log, name,
                "missing required coordinate '%c' in '%s'",
                reqd[i], coordinates);
            return -1;
        }
    }

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
                                 &haldata->pivot_length,
                                 g_comp_id, "%s.pivot-length", name);
    if (rc < 0) goto fail;
    *(haldata->pivot_length) = DEFAULT_PIVOT_LENGTH;

    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &fiveaxis_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    fiveaxis_cmod.Destroy = fiveaxis_destroy;
    *out = &fiveaxis_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
