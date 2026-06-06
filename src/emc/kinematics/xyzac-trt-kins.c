// xyzac-trt-kins — 5-axis AC tilting rotary table kinematics (cmod version)
// Copyright 2016 Rudy du Preez. License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TO_RAD (M_PI / 180.0)

#define REQUIRED_COORDINATES "XYZAC"

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

struct haldata {
    gomc_hal_float_t *x_rot_point;
    gomc_hal_float_t *y_rot_point;
    gomc_hal_float_t *z_rot_point;
    gomc_hal_float_t *x_offset;
    gomc_hal_float_t *y_offset;
    gomc_hal_float_t *z_offset;
    gomc_hal_float_t *tool_offset;
};
static struct haldata *haldata;

// ─── AC forward ───

static int32_t xyzac_forward(const double joints[KINS_MAX_JOINTS],
                         kins_pose_t *world)
{
    int JX = g_map.principal[0], JY = g_map.principal[1];
    int JZ = g_map.principal[2], JA = g_map.principal[3];
    int JB = g_map.principal[4], JC = g_map.principal[5];
    int JU = g_map.principal[6], JV = g_map.principal[7];
    int JW = g_map.principal[8];

    double x_rp = *haldata->x_rot_point;
    double y_rp = *haldata->y_rot_point;
    double z_rp = *haldata->z_rot_point;
    double dt   = *haldata->tool_offset;
    double dy   = *haldata->y_offset;
    double dz   = *haldata->z_offset + dt;
    double a_r  = joints[JA] * TO_RAD;
    double c_r  = joints[JC] * TO_RAD;

    world->x = + cos(c_r)             * (joints[JX]      - x_rp)
               + sin(c_r) * cos(a_r) * (joints[JY] - dy - y_rp)
               + sin(c_r) * sin(a_r) * (joints[JZ] - dz - z_rp)
               + sin(c_r) * dy + x_rp;

    world->y = - sin(c_r)             * (joints[JX]      - x_rp)
               + cos(c_r) * cos(a_r) * (joints[JY] - dy - y_rp)
               + cos(c_r) * sin(a_r) * (joints[JZ] - dz - z_rp)
               + cos(c_r) * dy + y_rp;

    world->z = - sin(a_r) * (joints[JY] - dy - y_rp)
               + cos(a_r) * (joints[JZ] - dz - z_rp)
               + dz + z_rp;

    world->a = joints[JA];
    world->c = joints[JC];
    world->b = (JB >= 0) ? joints[JB] : 0;
    world->u = (JU >= 0) ? joints[JU] : 0;
    world->v = (JV >= 0) ? joints[JV] : 0;
    world->w = (JW >= 0) ? joints[JW] : 0;
    return 0;
}

// ─── AC inverse ───

static int32_t xyzac_inverse(const kins_pose_t *world,
                         double joints[KINS_MAX_JOINTS])
{
    int JB = g_map.principal[4];
    int JU = g_map.principal[6], JV = g_map.principal[7];
    int JW = g_map.principal[8];

    double x_rp = *haldata->x_rot_point;
    double y_rp = *haldata->y_rot_point;
    double z_rp = *haldata->z_rot_point;
    double dy   = *haldata->y_offset;
    double dz   = *haldata->z_offset + *haldata->tool_offset;
    double a_r  = world->a * TO_RAD;
    double c_r  = world->c * TO_RAD;

    kins_pose_t P;
    P.x = + cos(c_r)             * (world->x - x_rp)
          - sin(c_r)             * (world->y - y_rp)
          + x_rp;
    P.y = + sin(c_r) * cos(a_r) * (world->x - x_rp)
          + cos(c_r) * cos(a_r) * (world->y - y_rp)
          -            sin(a_r) * (world->z - z_rp)
          -            cos(a_r) * dy + sin(a_r) * dz
          + dy + y_rp;
    P.z = + sin(c_r) * sin(a_r) * (world->x - x_rp)
          + cos(c_r) * sin(a_r) * (world->y - y_rp)
          +            cos(a_r) * (world->z - z_rp)
          -            sin(a_r) * dy - cos(a_r) * dz
          + dz + z_rp;

    P.a = world->a;
    P.c = world->c;
    P.b = (JB >= 0) ? world->b : 0;
    P.u = (JU >= 0) ? world->u : 0;
    P.v = (JV >= 0) ? world->v : 0;
    P.w = (JW >= 0) ? world->w : 0;

    sk_pos_to_joints(&g_map, &P, joints);
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
        case 0:  return xyzac_forward(joints, world);
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
        case 0:  return xyzac_inverse(world, joints);
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

static kins_callbacks_t xyzac_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t xyzac_cmod;

static void xyzac_destroy(cmod_t *self) {
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

    if (sk_map_coordinates(&g_map, coordinates, 1) < 0) {
        gomc_log_errorf(env->log, name, "bad coordinates: %s", coordinates);
        return -1;
    }

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc = 0;
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->x_rot_point,
                                  g_comp_id, "%s.x-rot-point", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->y_rot_point,
                                  g_comp_id, "%s.y-rot-point", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->z_rot_point,
                                  g_comp_id, "%s.z-rot-point", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->x_offset,
                                  g_comp_id, "%s.x-offset", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->y_offset,
                                  g_comp_id, "%s.y-offset", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->z_offset,
                                  g_comp_id, "%s.z-offset", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->tool_offset,
                                  g_comp_id, "%s.tool-offset", name);
    if (rc < 0) goto fail;

    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &xyzac_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "kins_api_register failed: %d", rc);
        goto fail;
    }

    xyzac_cmod.Destroy = xyzac_destroy;
    *out = &xyzac_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
