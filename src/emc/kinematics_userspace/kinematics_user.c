/********************************************************************
 * Description: kinematics_user.c
 *   Userspace kinematics loader for trajectory planning
 *
 * Loads kinematics plugin .so files via dlopen. Each kinematics
 * module provides a <name>_userspace.so that exports
 * kins_userspace_setup() to register forward/inverse/refresh
 * function pointers.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include "kins_plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"  /* EMC2_HOME */

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
 * Plugin loading via dlopen
 * ======================================================================== */

/*
 * Load a kinematics plugin and call its setup function.
 * Returns 0 on success, -1 on failure.
 */
static int load_plugin(KinematicsUserContext *ctx, const char *module_name)
{
    char plugin_path[512];
    void *handle;
    kins_userspace_setup_fn setup_fn;

    snprintf(plugin_path, sizeof(plugin_path),
             "%s/lib/kinematics/%s_userspace.so",
             EMC2_HOME, module_name);

    /* Make symbols from the main process (and its loaded libraries like
     * libposemath, hal_pin_reader) available to plugins */
    dlopen(NULL, RTLD_GLOBAL);

    handle = dlopen(plugin_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "kinematicsUserInit: dlopen '%s': %s\n",
                plugin_path, dlerror());
        return -1;
    }

    setup_fn = (kins_userspace_setup_fn)dlsym(handle, KINS_PLUGIN_SETUP_SYMBOL);
    if (!setup_fn) {
        fprintf(stderr, "kinematicsUserInit: dlsym '%s' in '%s': %s\n",
                KINS_PLUGIN_SETUP_SYMBOL, plugin_path, dlerror());
        dlclose(handle);
        return -1;
    }

    ctx->plugin_handle = handle;

    if (setup_fn(ctx) != 0) {
        fprintf(stderr, "kinematicsUserInit: setup failed for '%s'\n", module_name);
        dlclose(handle);
        ctx->plugin_handle = NULL;
        return -1;
    }

    /* Verify plugin set required function pointers */
    if (!ctx->inverse || !ctx->forward || !ctx->refresh) {
        fprintf(stderr, "kinematicsUserInit: plugin '%s' did not set all function pointers\n",
                module_name);
        dlclose(handle);
        ctx->plugin_handle = NULL;
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

    /* Load plugin via dlopen */
    if (load_plugin(ctx, kins_type) != 0) {
        fprintf(stderr, "kinematicsUserInit: no plugin for '%s'\n",
                kins_type ? kins_type : "(null)");
        fprintf(stderr, "  searched: %s/lib/kinematics/%s_userspace.so\n",
                EMC2_HOME, kins_type ? kins_type : "(null)");
        free(ctx);
        return NULL;
    }

    kp->valid = 1;
    ctx->initialized = 1;

    /* Read initial HAL pin values */
    if (ctx->refresh(ctx) != 0) {
        fprintf(stderr, "kinematicsUserInit: warning - could not read initial HAL pins\n");
        /* Don't fail - HAL pins may not exist yet */
    }

    fprintf(stderr, "kinematicsUserInit: loaded plugin for '%s' with %d joints, coords='%s'\n",
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
    return ctx->inverse(ctx, world, joints);
}

int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world)
{
    if (!ctx || !ctx->initialized || !joints || !world) {
        return -1;
    }
    return ctx->forward(ctx, joints, world);
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
    return ctx->refresh(ctx);
}

void kinematicsUserFree(KinematicsUserContext* ctx)
{
    if (ctx) {
        if (ctx->plugin_handle) {
            dlclose(ctx->plugin_handle);
        }
        free(ctx);
    }
}
