// pumakins — PUMA robot kinematics (cmod version)
// Derived from work by Fred Proctor. License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"
#include "posemath.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define REQUIRED_COORDINATES "XYZABC"

// ─── Constants ───

#define DEFAULT_PUMA560_A2 300.0
#define DEFAULT_PUMA560_A3  50.0
#define DEFAULT_PUMA560_D3  70.0
#define DEFAULT_PUMA560_D4 400.0
#define DEFAULT_PUMA560_D6  70.0

#define SINGULAR_FUZZ 0.000001
#define FLAG_FUZZ     0.000001

#define PUMA_SHOULDER_RIGHT 0x01
#define PUMA_ELBOW_DOWN     0x02
#define PUMA_WRIST_FLIP     0x04
#define PUMA_SINGULAR       0x08
#define PUMA_REACH          0x01

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

struct haldata {
    gomc_hal_float_t *a2, *a3, *d3, *d4, *d6;
};
static struct haldata *haldata;

#define PUMA_A2 (*(haldata->a2))
#define PUMA_A3 (*(haldata->a3))
#define PUMA_D3 (*(haldata->d3))
#define PUMA_D4 (*(haldata->d4))
#define PUMA_D6 (*(haldata->d6))

// ─── PUMA forward ───

static int32_t puma_forward(const double joints[KINS_MAX_JOINTS],
                        kins_pose_t *world)
{
    double s1, s2, s3, s4, s5, s6;
    double c1, c2, c3, c4, c5, c6;
    double s23, c23;
    double t1, t2, t3, t4;
    PmRotationMatrix R;
    PmRpy rpy;
    double tx, ty, tz;

    s1 = sin(joints[0]*M_PI/180); c1 = cos(joints[0]*M_PI/180);
    s2 = sin(joints[1]*M_PI/180); c2 = cos(joints[1]*M_PI/180);
    s3 = sin(joints[2]*M_PI/180); c3 = cos(joints[2]*M_PI/180);
    s4 = sin(joints[3]*M_PI/180); c4 = cos(joints[3]*M_PI/180);
    s5 = sin(joints[4]*M_PI/180); c5 = cos(joints[4]*M_PI/180);
    s6 = sin(joints[5]*M_PI/180); c6 = cos(joints[5]*M_PI/180);

    s23 = c2*s3 + s2*c3;
    c23 = c2*c3 - s2*s3;

    // First column of rotation matrix
    t1 = c4*c5*c6 - s4*s6;
    t2 = s23*s5*c6;
    t3 = s4*c5*c6 + c4*s6;
    t4 = c23*t1 - t2;
    R.x.x = c1*t4 + s1*t3;
    R.x.y = s1*t4 - c1*t3;
    R.x.z = -s23*t1 - c23*s5*c6;

    // Second column
    t1 = -c4*c5*s6 - s4*c6;
    t2 = s23*s5*s6;
    t3 = c4*c6 - s4*c5*s6;
    t4 = c23*t1 + t2;
    R.y.x = c1*t4 + s1*t3;
    R.y.y = s1*t4 - c1*t3;
    R.y.z = -s23*t1 + c23*s5*s6;

    // Third column
    t1 = c23*c4*s5 + s23*c5;
    R.z.x = -c1*t1 - s1*s4*s5;
    R.z.y = -s1*t1 + c1*s4*s5;
    R.z.z = s23*c4*s5 - c23*c5;

    // Position
    t1 = PUMA_A2*c2 + PUMA_A3*c23 - PUMA_D4*s23;
    tx = c1*t1 - PUMA_D3*s1;
    ty = s1*t1 + PUMA_D3*c1;
    tz = -PUMA_A3*s23 - PUMA_A2*s2 - PUMA_D4*c23;

    // D6 effect
    tx += R.z.x * PUMA_D6;
    ty += R.z.y * PUMA_D6;
    tz += R.z.z * PUMA_D6;

    world->x = tx;
    world->y = ty;
    world->z = tz;

    pmMatRpyConvert(&R, &rpy);
    world->a = rpy.r * 180.0 / M_PI;
    world->b = rpy.p * 180.0 / M_PI;
    world->c = rpy.y * 180.0 / M_PI;

    world->u = 0; world->v = 0; world->w = 0;
    return 0;
}

// ─── PUMA inverse ───

static int32_t puma_inverse(const kins_pose_t *world,
                        double joints[KINS_MAX_JOINTS])
{
    PmRotationMatrix R;
    PmRpy rpy;
    double t1, t2, t3, k, sumSq;
    double th1, th2, th3, th23, th4, th5, th6;
    double s1, c1, s3, c3, s23, c23, s4, c4, s5, c5, s6, c6;
    double px, py, pz;

    // RPY → rotation matrix
    rpy.r = world->a * M_PI / 180.0;
    rpy.p = world->b * M_PI / 180.0;
    rpy.y = world->c * M_PI / 180.0;
    pmRpyMatConvert(&rpy, &R);

    // Remove D6 effect
    px = world->x - PUMA_D6 * R.z.x;
    py = world->y - PUMA_D6 * R.z.y;
    pz = world->z - PUMA_D6 * R.z.z;

    // Joint 1
    sumSq = px*px + py*py - PUMA_D3*PUMA_D3;
    // Default: shoulder left
    th1 = atan2(py, px) - atan2(PUMA_D3, sqrt(fabs(sumSq)));

    s1 = sin(th1); c1 = cos(th1);

    // Joint 3
    k = (sumSq + pz*pz - PUMA_A2*PUMA_A2 - PUMA_A3*PUMA_A3 -
         PUMA_D4*PUMA_D4) / (2.0 * PUMA_A2);
    double d34sq = PUMA_A3*PUMA_A3 + PUMA_D4*PUMA_D4 - k*k;
    if (d34sq < 0) d34sq = 0;
    // Default: elbow up
    th3 = atan2(PUMA_A3, PUMA_D4) - atan2(k, sqrt(d34sq));

    s3 = sin(th3); c3 = cos(th3);

    // Joint 2
    t1 = (-PUMA_A3 - PUMA_A2*c3)*pz +
         (c1*px + s1*py)*(PUMA_A2*s3 - PUMA_D4);
    t2 = (PUMA_A2*s3 - PUMA_D4)*pz +
         (PUMA_A3 + PUMA_A2*c3)*(c1*px + s1*py);
    t3 = pz*pz + (c1*px + s1*py)*(c1*px + s1*py);

    th23 = atan2(t1, t2);
    th2 = th23 - th3;
    s23 = t1/t3; c23 = t2/t3;

    // Joint 4
    t1 = -R.z.x*s1 + R.z.y*c1;
    t2 = -R.z.x*c1*c23 - R.z.y*s1*c23 + R.z.z*s23;
    if (fabs(t1) < SINGULAR_FUZZ && fabs(t2) < SINGULAR_FUZZ)
        th4 = joints[3]*M_PI/180; // singular: keep current
    else
        th4 = atan2(t1, t2);

    s4 = sin(th4); c4 = cos(th4);

    // Joint 5
    s5 = R.z.z*(s23*c4) - R.z.x*(c1*c23*c4 + s1*s4)
                        - R.z.y*(s1*c23*c4 - c1*s4);
    c5 = -R.z.x*(c1*s23) - R.z.y*(s1*s23) - R.z.z*c23;
    th5 = atan2(s5, c5);

    // Joint 6
    s6 = R.x.z*(s23*s4) - R.x.x*(c1*c23*s4 - s1*c4)
                        - R.x.y*(s1*c23*s4 + c1*c4);
    c6 = R.x.x*((c1*c23*c4 + s1*s4)*c5 - c1*s23*s5) +
         R.x.y*((s1*c23*c4 - c1*s4)*c5 - s1*s23*s5) -
         R.x.z*(s23*c4*c5 + c23*s5);
    th6 = atan2(s6, c6);

    joints[0] = th1*180.0/M_PI;
    joints[1] = th2*180.0/M_PI;
    joints[2] = th3*180.0/M_PI;
    joints[3] = th4*180.0/M_PI;
    joints[4] = th5*180.0/M_PI;
    joints[5] = th6*180.0/M_PI;
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
        case 0:  return puma_forward(joints, world);
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
        case 0:  return puma_inverse(world, joints);
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

static kins_callbacks_t puma_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t puma_cmod;

static void puma_destroy(cmod_t *self) {
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

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc = 0;
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->a2,
                                  g_comp_id, "%s.A2", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->a3,
                                  g_comp_id, "%s.A3", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d3,
                                  g_comp_id, "%s.D3", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d4,
                                  g_comp_id, "%s.D4", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->d6,
                                  g_comp_id, "%s.D6", name);
    if (rc < 0) goto fail;

    PUMA_A2 = DEFAULT_PUMA560_A2;
    PUMA_A3 = DEFAULT_PUMA560_A3;
    PUMA_D3 = DEFAULT_PUMA560_D3;
    PUMA_D4 = DEFAULT_PUMA560_D4;
    PUMA_D6 = DEFAULT_PUMA560_D6;

    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &puma_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "kins_api_register failed: %d", rc);
        goto fail;
    }

    puma_cmod.Destroy = puma_destroy;
    *out = &puma_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
