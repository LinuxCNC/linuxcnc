/********************************************************************
 * Description: kins_plugin.h
 *   Plugin API for userspace kinematics modules
 *
 * Each kinematics module that supports planner type 2 provides a
 * userspace plugin (.so) that exports kins_userspace_setup().
 * The planner loads plugins via dlopen at init time.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef KINS_PLUGIN_H
#define KINS_PLUGIN_H

#include "emcpos.h"
#include "kinematics.h"        /* KINEMATICS_TYPE */
#include "kinematics_params.h" /* kinematics_params_t, per-kins param structs */

#define KINEMATICS_USER_MAX_JOINTS 9

/* Forward declaration */
struct KinematicsUserContext;

/* Function pointer typedefs for plugin dispatch */
typedef int (*kins_inverse_fn)(struct KinematicsUserContext *ctx,
                               const EmcPose *world, double *joints);
typedef int (*kins_forward_fn)(struct KinematicsUserContext *ctx,
                               const double *joints, EmcPose *world);
typedef int (*kins_refresh_fn)(struct KinematicsUserContext *ctx);

/*
 * Plugin context structure - visible to plugins.
 * The loader allocates and populates module_name, coordinates,
 * num_joints, and joint mappings before calling kins_userspace_setup().
 * The plugin sets function pointers, flags, and default params.
 */
typedef struct KinematicsUserContext {
    kinematics_params_t params;     /* Kinematics parameters */
    int initialized;

    /* Function pointers - set by plugin setup */
    kins_inverse_fn inverse;
    kins_forward_fn forward;
    kins_refresh_fn refresh;

    /* Identity flag - set by plugin (1 = trivkins/identity, 0 = non-trivial) */
    int is_identity;

    /* Kinematics type - set by plugin (KINEMATICS_IDENTITY or KINEMATICS_BOTH) */
    KINEMATICS_TYPE kins_type;

    /* dlopen handle (managed by loader, not plugin) */
    void *plugin_handle;
} KinematicsUserContext;

/*
 * Plugin setup function prototype.
 * Each plugin .so must export this symbol.
 *
 * When called, ctx has been pre-populated with:
 *   - params.module_name
 *   - params.coordinates
 *   - params.num_joints
 *   - params.joint_to_axis[] / axis_to_joint[] mappings
 *
 * The plugin must:
 *   1. Set ctx->inverse, ctx->forward, ctx->refresh
 *   2. Set ctx->is_identity (1 for identity kins, 0 otherwise)
 *   3. Set ctx->kins_type (KINEMATICS_IDENTITY or KINEMATICS_BOTH)
 *   4. Optionally set default parameter values in ctx->params.params.*
 *
 * Returns 0 on success, -1 on failure.
 */
typedef int (*kins_userspace_setup_fn)(struct KinematicsUserContext *ctx);

#define KINS_PLUGIN_SETUP_SYMBOL "kins_userspace_setup"

#endif /* KINS_PLUGIN_H */
