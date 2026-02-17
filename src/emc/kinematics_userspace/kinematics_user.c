/********************************************************************
 * Description: kinematics_user.c
 *   Userspace kinematics loader for trajectory planning
 *
 * Loads the RT kinematics .so via dlopen and resolves nonrt_*
 * functions exported by each kinematics module. This allows the
 * userspace planner to call kinematics without RT dependencies.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include "hal_pin_reader.h"
#include "kinematics_params.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"  /* EMC2_HOME */

/* ========================================================================
 * Internal context definition
 * ======================================================================== */

typedef int (*nonrt_forward_fn)(const void *params, const double *joints, EmcPose *pos);
typedef int (*nonrt_inverse_fn)(const void *params, const EmcPose *pos, double *joints);
typedef int (*nonrt_refresh_fn)(void *params,
                                int (*read_float)(const char *, double *),
                                int (*read_bit)(const char *, int *),
                                int (*read_s32)(const char *, int *));
typedef int (*nonrt_is_identity_fn)(void);

struct KinematicsUserContext {
    kinematics_params_t params;
    int initialized;
    int is_identity;
    KINEMATICS_TYPE kins_type;
    void *rt_handle;            /* dlopen handle to RT .so */
    nonrt_forward_fn forward;
    nonrt_inverse_fn inverse;
    nonrt_refresh_fn refresh;
};

/* ========================================================================
 * Helper functions
 * ======================================================================== */

/*
 * Build joint mapping from coordinates string
 */
static int build_joint_mapping(kinematics_params_t *kp, const char *coordinates)
{
    int joint = 0;
    const char *c;
    int i;

    if (!kp) return -1;

    /* Initialize all mappings to -1 */
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        kp->joint_to_axis[i] = -1;
    }
    for (i = 0; i < 9; i++) {
        kp->axis_to_joint[i] = -1;
    }

    /* Use default coordinates if none provided */
    if (!coordinates || !*coordinates) {
        coordinates = "XYZABCUVW";
    }

    /* Parse coordinates string */
    for (c = coordinates; *c && joint < EMCMOT_MAX_JOINTS && joint < kp->num_joints; c++) {
        int axis = -1;
        switch (toupper((unsigned char)*c)) {
            case 'X': axis = 0; break;
            case 'Y': axis = 1; break;
            case 'Z': axis = 2; break;
            case 'A': axis = 3; break;
            case 'B': axis = 4; break;
            case 'C': axis = 5; break;
            case 'U': axis = 6; break;
            case 'V': axis = 7; break;
            case 'W': axis = 8; break;
            default: continue;  /* Skip invalid characters */
        }
        kp->joint_to_axis[joint] = axis;
        /* Build axis-to-joint mapping (first joint for each axis) */
        if (kp->axis_to_joint[axis] == -1) {
            kp->axis_to_joint[axis] = joint;
        }
        joint++;
    }

    return joint;
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
    nonrt_is_identity_fn is_identity_fn;

    snprintf(module_path, sizeof(module_path),
             "%s/rtlib/%s.so", EMC2_HOME, module_name);

    handle = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "kinematicsUserInit: dlopen '%s': %s\n",
                module_path, dlerror());
        return -1;
    }

    ctx->rt_handle = handle;

    /* Resolve nonrt function symbols */
    ctx->forward = (nonrt_forward_fn)dlsym(handle, "nonrt_kinematicsForward");
    ctx->inverse = (nonrt_inverse_fn)dlsym(handle, "nonrt_kinematicsInverse");
    ctx->refresh = (nonrt_refresh_fn)dlsym(handle, "nonrt_refresh");
    is_identity_fn = (nonrt_is_identity_fn)dlsym(handle, "nonrt_is_identity");

    if (!ctx->forward || !ctx->inverse || !ctx->refresh || !is_identity_fn) {
        fprintf(stderr, "kinematicsUserInit: missing nonrt symbols in '%s'\n",
                module_path);
        if (!ctx->forward)  fprintf(stderr, "  missing: nonrt_kinematicsForward\n");
        if (!ctx->inverse)  fprintf(stderr, "  missing: nonrt_kinematicsInverse\n");
        if (!ctx->refresh)  fprintf(stderr, "  missing: nonrt_refresh\n");
        if (!is_identity_fn) fprintf(stderr, "  missing: nonrt_is_identity\n");
        dlclose(handle);
        ctx->rt_handle = NULL;
        return -1;
    }

    ctx->is_identity = is_identity_fn();
    ctx->kins_type = ctx->is_identity ? KINEMATICS_IDENTITY : KINEMATICS_BOTH;

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
    kinematics_params_t *kp;

    /* Debug: print incoming parameters */
    fprintf(stderr, "kinematicsUserInit ENTRY: kins_type='%s', num_joints=%d, coordinates='%s'\n",
            kins_type ? kins_type : "(null)",
            num_joints,
            coordinates ? coordinates : "(null)");

    if (num_joints < 1 || num_joints > KINEMATICS_USER_MAX_JOINTS) {
        fprintf(stderr, "kinematicsUserInit: invalid num_joints %d\n", num_joints);
        return NULL;
    }

    /* Allocate context */
    ctx = (KinematicsUserContext *)calloc(1, sizeof(KinematicsUserContext));
    if (!ctx) {
        fprintf(stderr, "kinematicsUserInit: memory allocation failed\n");
        return NULL;
    }

    kp = &ctx->params;
    kp->num_joints = num_joints;

    /* Copy module name and coordinates */
    if (kins_type) {
        strncpy(kp->module_name, kins_type, sizeof(kp->module_name) - 1);
        kp->module_name[sizeof(kp->module_name) - 1] = '\0';
    }
    if (coordinates) {
        strncpy(kp->coordinates, coordinates, sizeof(kp->coordinates) - 1);
        kp->coordinates[sizeof(kp->coordinates) - 1] = '\0';
    }

    /* Build joint-axis mapping */
    build_joint_mapping(kp, coordinates);

    /* Load RT module via dlopen */
    if (load_rt_module(ctx, kins_type) != 0) {
        fprintf(stderr, "kinematicsUserInit: failed to load RT module for '%s'\n",
                kins_type ? kins_type : "(null)");
        fprintf(stderr, "  searched: %s/rtlib/%s.so\n",
                EMC2_HOME, kins_type ? kins_type : "(null)");
        free(ctx);
        return NULL;
    }

    kp->valid = 1;
    ctx->initialized = 1;

    /* Read initial HAL pin values */
    if (ctx->refresh(kp, hal_pin_reader_read_float,
                     hal_pin_reader_read_bit,
                     hal_pin_reader_read_s32) != 0) {
        fprintf(stderr, "kinematicsUserInit: warning - could not read initial HAL pins\n");
        /* Don't fail - HAL pins may not exist yet */
    }

    fprintf(stderr, "kinematicsUserInit: loaded RT module for '%s' with %d joints, coords='%s'\n",
            kp->module_name, kp->num_joints, kp->coordinates);

    return ctx;
}

int kinematicsUserInverse(KinematicsUserContext* ctx,
                          const EmcPose* world,
                          double* joints)
{
    if (!ctx || !ctx->initialized || !world || !joints) {
        return -1;
    }
    return ctx->inverse(&ctx->params, world, joints);
}

int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world)
{
    if (!ctx || !ctx->initialized || !joints || !world) {
        return -1;
    }
    return ctx->forward(&ctx->params, joints, world);
}

int kinematicsUserIsIdentity(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return 0;
    }
    return ctx->is_identity;
}

int kinematicsUserGetNumJoints(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return 0;
    }
    return ctx->params.num_joints;
}

KINEMATICS_TYPE kinematicsUserGetType(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return KINEMATICS_IDENTITY;
    }
    return ctx->kins_type;
}

const char* kinematicsUserGetModuleName(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return "unknown";
    }
    return ctx->params.module_name;
}

int kinematicsUserRefreshParams(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return -1;
    }
    return ctx->refresh(&ctx->params, hal_pin_reader_read_float,
                        hal_pin_reader_read_bit,
                        hal_pin_reader_read_s32);
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
