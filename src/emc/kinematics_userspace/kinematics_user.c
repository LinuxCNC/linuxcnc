/********************************************************************
 * Description: kinematics_user.c
 *   Userspace kinematics loader for trajectory planning
 *
 * Loads the RT kinematics .so via dlopen and resolves nonrt_attach()
 * exported by each kinematics module. This allows the userspace planner
 * to call kinematics without RT dependencies.
 *
 * The RT kinematics module registers a kinematics_params_t blob in HAL
 * shmem via hal_struct_newf() and updates it every servo cycle.
 * Userspace calls hal_struct_attach() to map the same memory directly,
 * eliminating the per-call HAL pin list walk.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include "../motion/kinematics_params.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"  /* EMC2_HOME */
#include "rtapi.h"
#include "hal.h"

/* ========================================================================
 * Internal context definition
 * ======================================================================== */

typedef int  (*nonrt_forward_fn)(const double *joints, EmcPose *pos,
                                 const KINEMATICS_FORWARD_FLAGS *ff, KINEMATICS_INVERSE_FLAGS *if_);
typedef int  (*nonrt_inverse_fn)(const EmcPose *pos, double *joints,
                                 const KINEMATICS_INVERSE_FLAGS *if_, KINEMATICS_FORWARD_FLAGS *ff);
typedef void (*nonrt_attach_fn)(nonrt_ops_t *ops);

struct KinematicsUserContext {
    int initialized;
    int rt_only;               /* 1 if <module>.params HAL struct not found */
    int is_identity;           /* 1 for trivkins/identity — no dlopen needed */
    KINEMATICS_TYPE kins_type;
    void *rt_handle;           /* dlopen handle to RT .so */
    nonrt_forward_fn forward;
    nonrt_inverse_fn inverse;
    kinematics_params_t *shmem_params; /* ptr into HAL shmem (read-only) */
    int num_joints;
    int joint_to_axis[KINEMATICS_USER_MAX_JOINTS]; /* joint→axis for identity path */
    char module_name[HAL_NAME_LEN + 1];
};

/* ========================================================================
 * Identity joint mapping (for trivkins and is_identity modules)
 * ======================================================================== */

static void fill_identity_joint_map(KinematicsUserContext *ctx, const char *coords)
{
    int i, j = 0;
    for (i = 0; i < KINEMATICS_USER_MAX_JOINTS; i++) ctx->joint_to_axis[i] = -1;
    if (!coords) return;
    for (; *coords && j < ctx->num_joints; coords++) {
        int axis;
        switch (tolower((unsigned char)*coords)) {
            case 'x': axis = 0; break; case 'y': axis = 1; break;
            case 'z': axis = 2; break; case 'a': axis = 3; break;
            case 'b': axis = 4; break; case 'c': axis = 5; break;
            case 'u': axis = 6; break; case 'v': axis = 7; break;
            case 'w': axis = 8; break; default:  continue;
        }
        ctx->joint_to_axis[j++] = axis;
    }
}

/* ========================================================================
 * RT module loading via dlopen
 * ======================================================================== */

/*
 * Load an RT kinematics .so and resolve nonrt_* symbols.
 * Returns 0 on success, -1 on failure.
 */
static int load_rt_module(KinematicsUserContext *ctx, const char *module_name)
{
    char module_path[512];
    void *handle;

    snprintf(module_path, sizeof(module_path),
             "%s/rtlib/%s.so", EMC2_HOME, module_name);

    handle = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "kinematicsUserInit: dlopen '%s': %s\n",
                module_path, dlerror());
        return -1;
    }

    ctx->rt_handle = handle;

    {
        nonrt_attach_fn attach = (nonrt_attach_fn)dlsym(handle, "nonrt_attach");
        if (!attach) {
            fprintf(stderr, "kinematicsUserInit: missing nonrt_attach in '%s'\n",
                    module_path);
            dlclose(handle);
            ctx->rt_handle = NULL;
            return -1;
        }
        nonrt_ops_t ops = {NULL, NULL};
        attach(&ops);
        ctx->forward = ops.forward;
        ctx->inverse = ops.inverse;
    }

    if (!ctx->forward || !ctx->inverse) {
        fprintf(stderr, "kinematicsUserInit: nonrt_attach did not set fwd/inv in '%s'\n",
                module_path);
        dlclose(handle);
        ctx->rt_handle = NULL;
        return -1;
    }

    return 0;
}

/* ========================================================================
 * HAL shmem parameter mapping
 * ======================================================================== */

/*
 * Attach to the kinematics_params_t in HAL shmem via hal_struct_attach().
 * The RT module publishes "<module_name>.params" via hal_struct_newf().
 *
 * Returns 0 and sets ctx->shmem_params on success.
 * Returns -1 and sets ctx->rt_only=1 if not found (old / external module).
 */
static int find_shmem_params(KinematicsUserContext *ctx, const char *module_name)
{
    char param_name[HAL_NAME_LEN + 1];

    snprintf(param_name, sizeof(param_name), "%s.params", module_name);

    if (hal_struct_attach(param_name, (void **)&ctx->shmem_params) != 0) {
        fprintf(stderr, "kinematicsUserInit: '%s' not found — planner 2 disabled for '%s'\n",
                param_name, module_name);
        ctx->rt_only = 1;
        return -1;
    }

    /* Sanity: check the shmem block looks valid (skip for identity — num_joints not populated) */
    if (!ctx->shmem_params->is_identity &&
        (ctx->shmem_params->num_joints < 1 ||
        ctx->shmem_params->num_joints > KINEMATICS_USER_MAX_JOINTS)) {
        fprintf(stderr, "kinematicsUserInit: shmem_params->num_joints=%d invalid — planner 2 disabled\n",
                ctx->shmem_params->num_joints);
        ctx->shmem_params = NULL;
        ctx->rt_only = 1;
        return -1;
    }

    return 0;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

KinematicsUserContext* kinematicsUserInit(const char* kins_type,
                                          int num_joints,
                                          const char* coordinates)
{
    KinematicsUserContext *ctx;

    fprintf(stderr, "kinematicsUserInit: kins_type='%s', num_joints=%d, coordinates='%s'\n",
            kins_type ? kins_type : "(null)",
            num_joints,
            coordinates ? coordinates : "(null)");

    if (!kins_type || num_joints < 1 || num_joints > KINEMATICS_USER_MAX_JOINTS) {
        fprintf(stderr, "kinematicsUserInit: invalid arguments\n");
        return NULL;
    }

    ctx = (KinematicsUserContext *)calloc(1, sizeof(KinematicsUserContext));
    if (!ctx) {
        fprintf(stderr, "kinematicsUserInit: memory allocation failed\n");
        return NULL;
    }

    ctx->num_joints = num_joints;
    strncpy(ctx->module_name, kins_type, sizeof(ctx->module_name) - 1);

    /* Map HAL shmem params first — detects identity kinematics before dlopen */
    find_shmem_params(ctx, kins_type);

    if (!ctx->rt_only && ctx->shmem_params->is_identity) {
        /* Identity kinematics (trivkins): handle entirely in userspace — no dlopen needed */
        ctx->is_identity = 1;
        fill_identity_joint_map(ctx, coordinates);
        ctx->kins_type = KINEMATICS_IDENTITY;
        fprintf(stderr, "kinematicsUserInit: identity kinematics '%s' — direct joint mapping\n",
                kins_type);
    } else {
        /* Non-identity: load RT module and call nonrt_attach() */
        if (load_rt_module(ctx, kins_type) != 0) {
            fprintf(stderr, "kinematicsUserInit: failed to load RT module '%s'\n", kins_type);
            free(ctx);
            return NULL;
        }
        if (!ctx->rt_only) {
            ctx->kins_type = KINEMATICS_BOTH;
            fprintf(stderr, "kinematicsUserInit: shmem mapped for '%s', joints=%d\n",
                    kins_type, ctx->shmem_params->num_joints);
        } else {
            ctx->kins_type = KINEMATICS_BOTH; /* conservative default */
        }
    }

    ctx->initialized = 1;
    return ctx;
}

int kinematicsUserInverse(KinematicsUserContext* ctx,
                          const EmcPose* world,
                          double* joints)
{
    if (!ctx || !ctx->initialized || !world || !joints) return -1;

    if (ctx->is_identity) {
        int i;
        for (i = 0; i < ctx->num_joints; i++) {
            int ax = ctx->joint_to_axis[i];
            joints[i] = (ax >= 0) ? emcPoseGetAxis(world, ax) : 0.0;
        }
        return 0;
    }

    if (ctx->rt_only || !ctx->shmem_params) return -1;
    return ctx->inverse(world, joints, NULL, NULL);
}

int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world)
{
    if (!ctx || !ctx->initialized || !joints || !world) return -1;

    if (ctx->is_identity) {
        int i;
        memset(world, 0, sizeof(*world));
        for (i = 0; i < ctx->num_joints; i++) {
            int ax = ctx->joint_to_axis[i];
            if (ax >= 0) emcPoseSetAxis(world, ax, joints[i]);
        }
        return 0;
    }

    if (ctx->rt_only || !ctx->shmem_params) return -1;
    return ctx->forward(joints, world, NULL, NULL);
}

int kinematicsUserIsIdentity(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 0;
    if (ctx->is_identity) return 1;
    if (ctx->rt_only || !ctx->shmem_params) return 0;
    return ctx->shmem_params->is_identity;
}

int kinematicsUserGetNumJoints(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 0;
    return ctx->num_joints;
}

KINEMATICS_TYPE kinematicsUserGetType(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return KINEMATICS_IDENTITY;
    return ctx->kins_type;
}

const char* kinematicsUserGetModuleName(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return "unknown";
    return ctx->module_name;
}

int kinematicsUserRefreshParams(KinematicsUserContext* ctx)
{
    (void)ctx;
    return 0; /* no-op: RT module pushes params into shmem every servo cycle */
}

int kinematicsUserIsRtOnly(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) return 1;
    return ctx->rt_only;
}

void kinematicsUserFree(KinematicsUserContext* ctx)
{
    if (ctx) {
        if (ctx->rt_handle) {
            dlclose(ctx->rt_handle);
        }
        free(ctx);
    }
}
