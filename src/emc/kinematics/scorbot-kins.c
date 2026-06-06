// scorbot-kins — Scorbot ER 3 kinematics (cmod version)
// Copyright (C) 2015-2016 Sebastian Kuzminsky
// License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TO_RAD (M_PI / 180.0)
#define TO_DEG (180.0 / M_PI)

// Linkage constants (mm)
#define L0_HORIZONTAL_DISTANCE 16
#define L0_VERTICAL_DISTANCE   140
#define L1_LENGTH 221
#define L2_LENGTH 221

static void compute_j1_cartesian_location(double j0,
    double *x, double *y, double *z)
{
    *x = L0_HORIZONTAL_DISTANCE * cos(TO_RAD * j0);
    *y = L0_HORIZONTAL_DISTANCE * sin(TO_RAD * j0);
    *z = L0_VERTICAL_DISTANCE;
}

// ─── Forward/Inverse kinematics ───

static int32_t scorbotkins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags,
    uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    double j1x, j1y, j1z;
    double r;

    compute_j1_cartesian_location(joints[0], &j1x, &j1y, &j1z);

    // Link 1: J1 (shoulder) → J2 (elbow)
    r = L1_LENGTH * cos(TO_RAD * joints[1]);
    double j2x = r * cos(TO_RAD * joints[0]);
    double j2y = r * sin(TO_RAD * joints[0]);
    double j2z = L1_LENGTH * sin(TO_RAD * joints[1]);

    // Link 2: J2 (elbow) → J3 (wrist / controlled point)
    r = L2_LENGTH * cos(TO_RAD * joints[2]);
    double j3x = r * cos(TO_RAD * joints[0]);
    double j3y = r * sin(TO_RAD * joints[0]);
    double j3z = L2_LENGTH * sin(TO_RAD * joints[2]);

    world->x = j1x + j2x + j3x;
    world->y = j1y + j2y + j3y;
    world->z = j1z + j2z + j3z;
    world->a = joints[3];
    world->b = joints[4];
    world->c = 0; world->u = 0; world->v = 0; world->w = 0;

    return 0;
}

static int32_t scorbotkins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags,
    uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    double distance_to_cp, distance_to_center;
    double r_j1 = L0_HORIZONTAL_DISTANCE;
    double z_j1 = L0_VERTICAL_DISTANCE;

    joints[0] = TO_DEG * atan2(world->y, world->x);

    double r_cp = sqrt(world->x * world->x + world->y * world->y);
    double z_cp = world->z;

    // translate to J1 origin
    r_cp -= r_j1;
    z_cp -= z_j1;

    distance_to_cp = sqrt(r_cp * r_cp + z_cp * z_cp);
    distance_to_center = distance_to_cp / 2.0;

    double angle_to_cp = TO_DEG * acos(r_cp / distance_to_cp);
    if (z_cp < 0) angle_to_cp *= -1;

    double j1_angle = TO_DEG * acos(distance_to_center / L1_LENGTH);
    joints[1] = angle_to_cp + j1_angle;

    double z_j2 = L1_LENGTH * sin(TO_RAD * joints[1]);
    joints[2] = -1.0 * TO_DEG * asin((z_j2 - z_cp) / L2_LENGTH);

    joints[3] = world->a;
    joints[4] = world->b;

    return 0;
}

static kins_kinematics_type_t scorbotkins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}

static int32_t scorbotkins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t scorbotkins_switch(void *ctx, int32_t t) { (void)ctx; (void)t; return -1; }

static kins_callbacks_t scorbotkins_callbacks = {
    .ctx = NULL,
    .forward    = scorbotkins_forward,
    .inverse    = scorbotkins_inverse,
    .type       = scorbotkins_type,
    .switchable = scorbotkins_switchable,
    .switch_    = scorbotkins_switch,
};

// ─── cmod lifecycle ───

static cmod_t scorbotkins_cmod;

static void scorbotkins_destroy(cmod_t *self) { (void)self; }

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)argc; (void)argv;

    int rc = kins_api_register(env->api, name, &scorbotkins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        return rc;
    }

    scorbotkins_cmod.Destroy = scorbotkins_destroy;
    *out = &scorbotkins_cmod;
    return 0;
}
