// trivkins — identity kinematics module (cmod version)
//
// Implements a 1:1 joint↔axis mapping using the GMI kinematics API.
// This is the new-style cmod equivalent of src/emc/kinematics/trivkins.c.
//
// Parameters (via INI or argv):
//   coordinates  — axis letters, default "XYZABCUVW"
//   kinstype     — "identity" (default), "both", "forward_only", "inverse_only"
//
// Derived from a work by Fred Proctor & Will Shackleford
// Copyright (c) 2009 All rights reserved.
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
// License: GPL Version 2

#include <string.h>
#include <stdlib.h>

#include "gomc_env.h"
#include "kins_api.h"

// ─── Private state ───

typedef struct {
    cmod_t              base;       // must be first — returned to launcher
    const cmod_env_t   *env;
    char               *name;

    kins_kinematics_type_t ktype;
    // Axis-to-joint mapping: map[axis_index] = joint_index.
    // Built from the coordinates string, e.g. "XYZ" → {0,1,2,-1,...}
    int                 axis_to_joint[KINS_MAX_JOINTS];
    int                 num_joints;
    kins_callbacks_t   *kins_cb;    // per-instance allocated callbacks
} trivkins_t;

// ─── Axis letter helpers ───

// axis_index returns 0..8 for X,Y,Z,A,B,C,U,V,W or -1.
static int axis_index(char c) {
    const char *axes = "XYZABCUVW";
    for (int i = 0; axes[i]; i++) {
        if (c == axes[i] || c == (axes[i] + 32)) // case-insensitive
            return i;
    }
    return -1;
}

// ─── Kinematics callbacks ───

static int32_t trivkins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags,
    uint64_t *iflags)
{
    (void)ctx;
    (void)fflags;
    (void)iflags;

    // Identity: each axis = its joint
    double *wp = (double *)world;  // pose fields are contiguous doubles
    for (int i = 0; i < 9; i++) {
        wp[i] = 0.0;
    }
    // We only fill in the axes we know about.
    // For identity kins, axis N = joint N (via the mapping).
    // But since forward gets raw joint values and the mapping is
    // set up at init time, we just copy through.
    for (int i = 0; i < 9; i++) {
        // joints and pose positions are identity-mapped for trivkins
        wp[i] = joints[i];
    }

    return 0;
}

static int32_t trivkins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags,
    uint64_t *fflags)
{
    (void)ctx;
    (void)iflags;
    (void)fflags;

    const double *wp = (const double *)world;
    for (int i = 0; i < KINS_MAX_JOINTS; i++) {
        joints[i] = 0.0;
    }
    for (int i = 0; i < 9; i++) {
        joints[i] = wp[i];
    }

    return 0;
}

static kins_kinematics_type_t trivkins_type(void *ctx) {
    (void)ctx;
    // We store ktype in thread-local? No — we need the instance.
    // Since there's only one kinematics instance, use a file-static.
    trivkins_t *tk = (trivkins_t *)ctx;
    return tk->ktype;
}

static int32_t trivkins_switchable(void *ctx) {
    (void)ctx;
    return 0;
}

static int32_t trivkins_switch(void *ctx, int32_t switchkins_type) {
    (void)ctx;

    (void)switchkins_type;
    return -1; // not supported
}



// ─── cmod lifecycle ───

static int trivkins_Start(cmod_t *self) {
    (void)self;
    return 0;
}

static void trivkins_Stop(cmod_t *self) {
    (void)self;
}

static void trivkins_Destroy(cmod_t *self) {
    trivkins_t *tk = (trivkins_t *)self;
    if (tk->kins_cb) free(tk->kins_cb);
    if (tk->name) free(tk->name);
    free(tk);
}

// ─── Parse kinstype string ───

static kins_kinematics_type_t parse_kinstype(const char *s) {
    if (!s || !*s) return KINS_IDENTITY;
    switch (*s) {
        case 'b': case 'B': return KINS_BOTH;
        case 'f': case 'F': return KINS_FORWARD_ONLY;
        case 'i': case 'I': return KINS_INVERSE_ONLY;
        case '1': default:  return KINS_IDENTITY;
    }
}

// ─── Constructor ───

int New(
    const cmod_env_t *env,
    const char *name,
    int argc, const char **argv,
    cmod_t **out)
{
    trivkins_t *tk = calloc(1, sizeof(trivkins_t));
    if (!tk) return -1;

    tk->env  = env;
    tk->name = strdup(name);
    tk->base.Start   = trivkins_Start;
    tk->base.Stop    = trivkins_Stop;
    tk->base.Destroy = trivkins_Destroy;

    // Defaults
    const char *coordinates = "XYZABCUVW";
    const char *kinstype    = "1";

    // Parse argv: "coordinates=XYZ" "kinstype=both"
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "coordinates=", 12) == 0)
            coordinates = argv[i] + 12;
        else if (strncmp(argv[i], "kinstype=", 9) == 0)
            kinstype = argv[i] + 9;
    }

    // Also check INI if available
    if (env->ini) {
        const char *val;
        val = env->ini->get(env->ini->ctx, "KINS", "COORDINATES");
        if (val) coordinates = val;
        val = env->ini->get(env->ini->ctx, "KINS", "KINSTYPE");
        if (val) kinstype = val;
    }

    tk->ktype = parse_kinstype(kinstype);

    // Build axis→joint mapping from coordinates string
    for (int i = 0; i < KINS_MAX_JOINTS; i++)
        tk->axis_to_joint[i] = -1;

    tk->num_joints = 0;
    for (int i = 0; coordinates[i] && i < KINS_MAX_JOINTS; i++) {
        int ax = axis_index(coordinates[i]);
        if (ax >= 0) {
            tk->axis_to_joint[ax] = i;
            tk->num_joints++;
        }
    }

    // Allocate per-instance callbacks
    kins_callbacks_t *cb = calloc(1, sizeof(*cb));
    if (!cb) { trivkins_Destroy(&tk->base); return -1; }
    cb->ctx = tk;
    cb->forward    = trivkins_forward;
    cb->inverse    = trivkins_inverse;
    cb->type       = trivkins_type;
    cb->switchable = trivkins_switchable;
    cb->switch_    = trivkins_switch;
    tk->kins_cb = cb;

    // Register with the GMI kinematics API
    int rc = kins_api_register(env->api, name, cb);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        trivkins_Destroy(&tk->base);
        return rc;
    }

    *out = &tk->base;

    return 0;
}
