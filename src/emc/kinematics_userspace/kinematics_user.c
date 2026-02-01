/********************************************************************
 * Description: kinematics_user.c
 *   Userspace kinematics implementation for trajectory planning
 *
 * This provides the same Forward/Inverse kinematics calculations as
 * the RT modules, but runs in userspace. Parameters like pivot_length
 * are read directly from HAL pins via hal_pin_reader.
 *
 * Uses function pointers for dispatch - no switch statements needed
 * when calling forward/inverse kinematics.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "kinematics_user.h"
#include "hal_pin_reader.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Include shared math headers for kinematics calculations */
#include "5axiskins_math.h"
#include "tripodkins_math.h"
#include "maxkins_math.h"
#include "rosekins_math.h"
#include "trtfuncs_math.h"
#include "pumakins_math.h"
#include "scarakins_math.h"
#include "corexykins_math.h"
#include "rotatekins_math.h"
#include "scorbotkins_math.h"
#include "pentakins_math.h"
#include "genhexkins_math.h"
#include "genserkins_math.h"
#include "lineardeltakins_math.h"
#include "rotarydeltakins_math.h"

/* Forward declarations for function pointer types */
typedef int (*kins_inverse_fn)(struct KinematicsUserContext *ctx,
                               const EmcPose *world, double *joints);
typedef int (*kins_forward_fn)(struct KinematicsUserContext *ctx,
                               const double *joints, EmcPose *world);
typedef int (*kins_refresh_fn)(struct KinematicsUserContext *ctx);

/*
 * Internal context structure with function pointers for dispatch
 */
struct KinematicsUserContext {
    kinematics_params_t params;     /* Kinematics parameters */
    int initialized;

    /* Function pointers - set once at init, eliminates switch dispatch */
    kins_inverse_fn inverse;
    kins_forward_fn forward;
    kins_refresh_fn refresh;
};

/* ========================================================================
 * Helper functions
 * ======================================================================== */

/*
 * Map module name to kinematics type ID
 */
static kinematics_type_id_t module_name_to_type_id(const char *name)
{
    if (!name) return KINS_TYPE_UNKNOWN;

    if (strstr(name, "trivkins")) return KINS_TYPE_TRIVKINS;
    if (strstr(name, "5axiskins")) return KINS_TYPE_5AXISKINS;
    if (strstr(name, "xyzac-trt")) return KINS_TYPE_XYZAC_TRT;
    if (strstr(name, "xyzbc-trt")) return KINS_TYPE_XYZBC_TRT;
    if (strstr(name, "maxkins")) return KINS_TYPE_MAXKINS;
    if (strstr(name, "pumakins")) return KINS_TYPE_PUMAKINS;
    if (strstr(name, "scarakins")) return KINS_TYPE_SCARAKINS;
    if (strstr(name, "tripodkins")) return KINS_TYPE_TRIPODKINS;
    if (strstr(name, "genhexkins")) return KINS_TYPE_GENHEXKINS;
    if (strstr(name, "genserkins")) return KINS_TYPE_GENSERFUNCS;
    if (strstr(name, "pentakins")) return KINS_TYPE_PENTAKINS;
    if (strstr(name, "lineardeltakins")) return KINS_TYPE_LINEARDELTA;
    if (strstr(name, "rotarydeltakins")) return KINS_TYPE_ROTARYDELTA;
    if (strstr(name, "rosekins")) return KINS_TYPE_ROSEKINS;
    if (strstr(name, "corexykins")) return KINS_TYPE_COREXYKINS;
    if (strstr(name, "rotatekins")) return KINS_TYPE_ROTATEKINS;
    if (strstr(name, "scorbot-kins")) return KINS_TYPE_SCORBOTKINS;

    return KINS_TYPE_UNKNOWN;
}

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
 * Trivkins implementation
 * ======================================================================== */

static int trivkins_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    int i;

    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        if (axis >= 0 && axis < 9) {
            joints[i] = emcPoseGetAxis(world, axis);
        } else {
            joints[i] = 0.0;
        }
    }
    return 0;
}

static int trivkins_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    int i;

    memset(world, 0, sizeof(EmcPose));
    for (i = 0; i < kp->num_joints; i++) {
        int axis = kp->joint_to_axis[i];
        if (axis >= 0 && axis < 9) {
            emcPoseSetAxis(world, axis, joints[i]);
        }
    }
    return 0;
}

static int trivkins_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    /* trivkins has no HAL pins to refresh */
    return 0;
}

/* ========================================================================
 * 5axiskins implementation - uses shared 5axiskins_math.h
 * ======================================================================== */

/* Helper to build joint map from kinematics_params */
static inline void fiveaxis_build_jmap(const kinematics_params_t *kp, fiveaxis_joints_t *jmap)
{
    jmap->jx = kp->axis_to_joint[0];
    jmap->jy = kp->axis_to_joint[1];
    jmap->jz = kp->axis_to_joint[2];
    jmap->ja = kp->axis_to_joint[3];
    jmap->jb = kp->axis_to_joint[4];
    jmap->jc = kp->axis_to_joint[5];
    jmap->ju = kp->axis_to_joint[6];
    jmap->jv = kp->axis_to_joint[7];
    jmap->jw = kp->axis_to_joint[8];
}

static int fiveaxis_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    fiveaxis_params_t params;
    fiveaxis_joints_t jmap;
    EmcPose axis_values;

    params.pivot_length = kp->params.fiveaxis.pivot_length;
    fiveaxis_build_jmap(kp, &jmap);

    fiveaxis_inverse_math(&params, world, &axis_values);
    fiveaxis_axis_to_joints(&jmap, &axis_values, joints);

    return 0;
}

static int fiveaxis_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    fiveaxis_params_t params;
    fiveaxis_joints_t jmap;

    params.pivot_length = kp->params.fiveaxis.pivot_length;
    fiveaxis_build_jmap(kp, &jmap);

    return fiveaxis_forward_math(&params, &jmap, joints, world);
}

static int fiveaxis_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("5axiskins.pivot-length", &fval) == 0) {
        ctx->params.params.fiveaxis.pivot_length = fval;
        return 0;
    }
    fprintf(stderr, "kinematicsUserRefreshParams: could not read 5axiskins.pivot-length\n");
    return -1;
}

/* ========================================================================
 * XYZAC-TRT (tilting rotary table with A and C axes) implementation
 * Uses shared trtfuncs_math.h
 * ======================================================================== */

/* Helper to build TRT joint map from kinematics_params */
static inline void trt_build_jmap(const kinematics_params_t *kp, trt_joints_t *jmap)
{
    jmap->jx = kp->axis_to_joint[0];
    jmap->jy = kp->axis_to_joint[1];
    jmap->jz = kp->axis_to_joint[2];
    jmap->ja = kp->axis_to_joint[3];
    jmap->jb = kp->axis_to_joint[4];
    jmap->jc = kp->axis_to_joint[5];
    jmap->ju = kp->axis_to_joint[6];
    jmap->jv = kp->axis_to_joint[7];
    jmap->jw = kp->axis_to_joint[8];
}

/* Helper to build TRT params from kins_trt_params_t */
static inline void trt_build_params(const kins_trt_params_t *trt, trt_params_t *params)
{
    params->x_rot_point = trt->x_rot_point;
    params->y_rot_point = trt->y_rot_point;
    params->z_rot_point = trt->z_rot_point;
    params->x_offset = trt->x_offset;
    params->y_offset = trt->y_offset;
    params->z_offset = trt->z_offset;
    params->tool_offset = trt->tool_offset;
    params->conventional_directions = trt->conventional_directions;
}

static int xyzac_trt_inverse(KinematicsUserContext *ctx,
                             const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;
    EmcPose axis_values;

    trt_build_params(&kp->params.trt, &params);
    trt_build_jmap(kp, &jmap);

    xyzac_inverse_math(&params, &jmap, world, &axis_values);
    trt_axis_to_joints(&jmap, &axis_values, joints);

    return 0;
}

static int xyzac_trt_forward(KinematicsUserContext *ctx,
                             const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;

    trt_build_params(&kp->params.trt, &params);
    trt_build_jmap(kp, &jmap);

    return xyzac_forward_math(&params, &jmap, joints, world);
}

static int xyzac_trt_refresh(KinematicsUserContext *ctx)
{
    double fval;
    int ival;

    if (hal_pin_reader_read_float("xyzac-trt-kins.x-rot-point", &fval) == 0)
        ctx->params.params.trt.x_rot_point = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.y-rot-point", &fval) == 0)
        ctx->params.params.trt.y_rot_point = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.z-rot-point", &fval) == 0)
        ctx->params.params.trt.z_rot_point = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.x-offset", &fval) == 0)
        ctx->params.params.trt.x_offset = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.y-offset", &fval) == 0)
        ctx->params.params.trt.y_offset = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.z-offset", &fval) == 0)
        ctx->params.params.trt.z_offset = fval;
    if (hal_pin_reader_read_float("xyzac-trt-kins.tool-offset", &fval) == 0)
        ctx->params.params.trt.tool_offset = fval;
    if (hal_pin_reader_read_bit("xyzac-trt-kins.conventional-directions", &ival) == 0)
        ctx->params.params.trt.conventional_directions = ival;

    return 0;
}

/* ========================================================================
 * XYZBC-TRT (tilting rotary table with B and C axes) implementation
 * Uses shared trtfuncs_math.h
 * ======================================================================== */

static int xyzbc_trt_inverse(KinematicsUserContext *ctx,
                             const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;
    EmcPose axis_values;

    trt_build_params(&kp->params.trt, &params);
    trt_build_jmap(kp, &jmap);

    xyzbc_inverse_math(&params, &jmap, world, &axis_values);
    trt_axis_to_joints(&jmap, &axis_values, joints);

    return 0;
}

static int xyzbc_trt_forward(KinematicsUserContext *ctx,
                             const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    trt_params_t params;
    trt_joints_t jmap;

    trt_build_params(&kp->params.trt, &params);
    trt_build_jmap(kp, &jmap);

    return xyzbc_forward_math(&params, &jmap, joints, world);
}

static int xyzbc_trt_refresh(KinematicsUserContext *ctx)
{
    double fval;
    int ival;

    if (hal_pin_reader_read_float("xyzbc-trt-kins.x-rot-point", &fval) == 0)
        ctx->params.params.trt.x_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.y-rot-point", &fval) == 0)
        ctx->params.params.trt.y_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.z-rot-point", &fval) == 0)
        ctx->params.params.trt.z_rot_point = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.x-offset", &fval) == 0)
        ctx->params.params.trt.x_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.y-offset", &fval) == 0)
        ctx->params.params.trt.y_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.z-offset", &fval) == 0)
        ctx->params.params.trt.z_offset = fval;
    if (hal_pin_reader_read_float("xyzbc-trt-kins.tool-offset", &fval) == 0)
        ctx->params.params.trt.tool_offset = fval;
    if (hal_pin_reader_read_bit("xyzbc-trt-kins.conventional-directions", &ival) == 0)
        ctx->params.params.trt.conventional_directions = ival;

    return 0;
}

/* ========================================================================
 * Tripodkins implementation - uses shared tripodkins_math.h
 * ======================================================================== */

/* Helper to build tripod params from kinematics_params */
static inline void tripod_build_params(const kinematics_params_t *kp, tripod_params_t *params)
{
    params->bx = kp->params.tripod.bx;
    params->cx = kp->params.tripod.cx;
    params->cy = kp->params.tripod.cy;
}

static int tripodkins_inverse(KinematicsUserContext *ctx,
                              const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    tripod_params_t params;

    tripod_build_params(kp, &params);
    return tripod_inverse_math(&params, world, joints, NULL, NULL);
}

static int tripodkins_forward(KinematicsUserContext *ctx,
                              const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    tripod_params_t params;

    tripod_build_params(kp, &params);
    /* Pass NULL for fflags - defaults to D above xy plane */
    return tripod_forward_math(&params, joints, world, NULL, NULL);
}

static int tripodkins_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("tripodkins.Bx", &fval) == 0)
        ctx->params.params.tripod.bx = fval;
    if (hal_pin_reader_read_float("tripodkins.Cx", &fval) == 0)
        ctx->params.params.tripod.cx = fval;
    if (hal_pin_reader_read_float("tripodkins.Cy", &fval) == 0)
        ctx->params.params.tripod.cy = fval;

    return 0;
}

/* ========================================================================
 * Linear delta kinematics implementation - uses shared lineardeltakins_math.h
 * ======================================================================== */

/* Helper to build lineardelta params from kinematics_params */
static inline void lineardelta_build_params(const kinematics_params_t *kp,
                                             lineardelta_params_t *params)
{
    params->radius = kp->params.lineardelta.jointradius;
    params->rod_length = kp->params.lineardelta.radius;
}

static int lineardelta_inverse(KinematicsUserContext *ctx,
                               const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    lineardelta_params_t params;

    lineardelta_build_params(kp, &params);
    return lineardelta_inverse_math(&params, world, joints);
}

static int lineardelta_forward(KinematicsUserContext *ctx,
                               const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    lineardelta_params_t params;

    lineardelta_build_params(kp, &params);
    return lineardelta_forward_math(&params, joints, world);
}

static int lineardelta_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("lineardeltakins.R", &fval) == 0)
        ctx->params.params.lineardelta.jointradius = fval;
    if (hal_pin_reader_read_float("lineardeltakins.L", &fval) == 0)
        ctx->params.params.lineardelta.radius = fval;

    return 0;
}

/* ========================================================================
 * Rotary delta kinematics implementation - uses shared rotarydeltakins_math.h
 * ======================================================================== */

/* Helper to build rotarydelta params from kinematics_params */
static inline void rotarydelta_build_params(const kinematics_params_t *kp,
                                             rotarydelta_params_t *params)
{
    params->platformradius = kp->params.rotarydelta.platformradius;
    params->thighlength = kp->params.rotarydelta.thighlength;
    params->shinlength = kp->params.rotarydelta.shinlength;
    params->footradius = kp->params.rotarydelta.footradius;
}

static int rotarydelta_inverse(KinematicsUserContext *ctx,
                               const EmcPose *world, double *joints)
{
    rotarydelta_params_t params;
    kinematics_params_t *kp = &ctx->params;
    rotarydelta_build_params(kp, &params);
    return rotarydelta_inverse_math(&params, world, joints);
}

static int rotarydelta_forward(KinematicsUserContext *ctx,
                               const double *joints, EmcPose *world)
{
    rotarydelta_params_t params;
    kinematics_params_t *kp = &ctx->params;
    rotarydelta_build_params(kp, &params);
    return rotarydelta_forward_math(&params, joints, world);
}

static int rotarydelta_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("rotarydeltakins.platformradius", &fval) == 0)
        ctx->params.params.rotarydelta.platformradius = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.thighlength", &fval) == 0)
        ctx->params.params.rotarydelta.thighlength = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.shinlength", &fval) == 0)
        ctx->params.params.rotarydelta.shinlength = fval;
    if (hal_pin_reader_read_float("rotarydeltakins.footradius", &fval) == 0)
        ctx->params.params.rotarydelta.footradius = fval;

    return 0;
}

/* ========================================================================
 * Rose kinematics implementation (cylindrical coordinates)
 * Uses shared rosekins_math.h
 * ======================================================================== */

/* State for userspace revolution tracking - each context gets its own state */
static rosekins_state_t userspace_rosekins_state = {0, 0};

static int rosekins_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    (void)ctx;
    /* Use shared math with revolution tracking state */
    return rosekins_inverse_math(world, joints, &userspace_rosekins_state, NULL);
}

static int rosekins_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    (void)ctx;
    return rosekins_forward_math(joints, world);
}

static int rosekins_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;
    /* Rosekins has output pins, not input pins for parameters */
    return 0;
}

/* ========================================================================
 * Maxkins implementation - uses shared maxkins_math.h
 * ======================================================================== */

/* Helper to build maxkins params from kinematics_params */
static inline void maxkins_build_params(const kinematics_params_t *kp, maxkins_params_t *params)
{
    params->pivot_length = kp->params.maxkins.pivot_length;
    params->conventional_directions = kp->params.maxkins.conventional_directions;
}

static int maxkins_forward(KinematicsUserContext *ctx,
                           const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    maxkins_params_t params;

    maxkins_build_params(kp, &params);
    return maxkins_forward_math(&params, joints, world);
}

static int maxkins_inverse(KinematicsUserContext *ctx,
                           const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    maxkins_params_t params;

    maxkins_build_params(kp, &params);
    return maxkins_inverse_math(&params, world, joints);
}

static int maxkins_refresh(KinematicsUserContext *ctx)
{
    double fval;
    int ival;

    if (hal_pin_reader_read_float("maxkins.pivot-length", &fval) == 0) {
        ctx->params.params.maxkins.pivot_length = fval;
    } else {
        fprintf(stderr, "kinematicsUserRefreshParams: could not read maxkins.pivot-length\n");
        return -1;
    }

    if (hal_pin_reader_read_bit("maxkins.conventional-directions", &ival) == 0) {
        ctx->params.params.maxkins.conventional_directions = ival;
    } else {
        fprintf(stderr, "kinematicsUserRefreshParams: could not read maxkins.conventional-directions\n");
        return -1;
    }

    return 0;
}

/* ========================================================================
 * Pumakins implementation - uses shared pumakins_math.h
 * ======================================================================== */

/* Helper to build puma params from kinematics_params */
static inline void puma_build_params(const kinematics_params_t *kp, puma_params_t *params)
{
    params->a2 = kp->params.puma.a2;
    params->a3 = kp->params.puma.a3;
    params->d3 = kp->params.puma.d3;
    params->d4 = kp->params.puma.d4;
    params->d6 = kp->params.puma.d6;
}

static int pumakins_forward(KinematicsUserContext *ctx,
                            const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    puma_params_t params;

    puma_build_params(kp, &params);
    return puma_forward_math(&params, joints, world, NULL);
}

static int pumakins_inverse(KinematicsUserContext *ctx,
                            const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    puma_params_t params;

    puma_build_params(kp, &params);
    /* Pass NULL for current_joints and fflags - use default behavior */
    return puma_inverse_math(&params, world, joints, NULL, 0, NULL);
}

static int pumakins_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("pumakins.A2", &fval) == 0)
        ctx->params.params.puma.a2 = fval;
    if (hal_pin_reader_read_float("pumakins.A3", &fval) == 0)
        ctx->params.params.puma.a3 = fval;
    if (hal_pin_reader_read_float("pumakins.D3", &fval) == 0)
        ctx->params.params.puma.d3 = fval;
    if (hal_pin_reader_read_float("pumakins.D4", &fval) == 0)
        ctx->params.params.puma.d4 = fval;
    if (hal_pin_reader_read_float("pumakins.D6", &fval) == 0)
        ctx->params.params.puma.d6 = fval;

    return 0;
}

/* ========================================================================
 * Scarakins implementation - uses shared scarakins_math.h
 * ======================================================================== */

/* Helper to build scara params from kinematics_params */
static inline void scara_build_params(const kinematics_params_t *kp, scara_params_t *params)
{
    params->d1 = kp->params.scara.d1;
    params->d2 = kp->params.scara.d2;
    params->d3 = kp->params.scara.d3;
    params->d4 = kp->params.scara.d4;
    params->d5 = kp->params.scara.d5;
    params->d6 = kp->params.scara.d6;
}

static int scarakins_forward(KinematicsUserContext *ctx,
                             const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    scara_params_t params;

    scara_build_params(kp, &params);
    return scara_forward_math(&params, joints, world, NULL);
}

static int scarakins_inverse(KinematicsUserContext *ctx,
                             const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    scara_params_t params;

    scara_build_params(kp, &params);
    /* Pass 0 for iflags - use default elbow configuration */
    return scara_inverse_math(&params, world, joints, 0, NULL);
}

static int scarakins_refresh(KinematicsUserContext *ctx)
{
    double fval;

    if (hal_pin_reader_read_float("scarakins.D1", &fval) == 0)
        ctx->params.params.scara.d1 = fval;
    if (hal_pin_reader_read_float("scarakins.D2", &fval) == 0)
        ctx->params.params.scara.d2 = fval;
    if (hal_pin_reader_read_float("scarakins.D3", &fval) == 0)
        ctx->params.params.scara.d3 = fval;
    if (hal_pin_reader_read_float("scarakins.D4", &fval) == 0)
        ctx->params.params.scara.d4 = fval;
    if (hal_pin_reader_read_float("scarakins.D5", &fval) == 0)
        ctx->params.params.scara.d5 = fval;
    if (hal_pin_reader_read_float("scarakins.D6", &fval) == 0)
        ctx->params.params.scara.d6 = fval;

    return 0;
}

/* ========================================================================
 * Corexykins implementation - uses shared corexykins_math.h
 * ======================================================================== */

static int corexykins_forward(KinematicsUserContext *ctx,
                              const double *joints, EmcPose *world)
{
    (void)ctx;  /* No parameters needed */
    return corexy_forward_math(joints, world);
}

static int corexykins_inverse(KinematicsUserContext *ctx,
                              const EmcPose *world, double *joints)
{
    (void)ctx;  /* No parameters needed */
    return corexy_inverse_math(world, joints);
}

static int corexykins_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;  /* No parameters to refresh */
    return 0;
}

/* ========================================================================
 * Rotatekins implementation - uses shared rotatekins_math.h
 * ======================================================================== */

static int rotatekins_forward(KinematicsUserContext *ctx,
                              const double *joints, EmcPose *world)
{
    (void)ctx;  /* No parameters needed */
    return rotate_forward_math(joints, world);
}

static int rotatekins_inverse(KinematicsUserContext *ctx,
                              const EmcPose *world, double *joints)
{
    (void)ctx;  /* No parameters needed */
    return rotate_inverse_math(world, joints);
}

static int rotatekins_refresh(KinematicsUserContext *ctx)
{
    (void)ctx;  /* No parameters to refresh */
    return 0;
}

/* ========================================================================
 * Scorbotkins implementation - uses shared scorbotkins_math.h
 * ======================================================================== */

/* Helper to build scorbot params from kinematics_params */
static inline void scorbot_build_params(const kinematics_params_t *kp, scorbot_params_t *params)
{
    params->l0_horizontal = kp->params.scorbot.l0_horizontal;
    params->l0_vertical = kp->params.scorbot.l0_vertical;
    params->l1_length = kp->params.scorbot.l1_length;
    params->l2_length = kp->params.scorbot.l2_length;
}

static int scorbotkins_forward(KinematicsUserContext *ctx,
                               const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    scorbot_params_t params;

    scorbot_build_params(kp, &params);
    return scorbot_forward_math(&params, joints, world);
}

/* ========================================================================
 * Genhexkins (6-DOF hexapod/Stewart platform)
 * ======================================================================== */

static void genhexkins_build_params(kinematics_params_t *kp, genhex_params_t *gparams)
{
    int i;

    /* Initialize with default values first */
    genhex_init_params(gparams);

    /* Copy base coordinates */
    for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
        gparams->base[i].x = kp->params.genhex.basex[i];
        gparams->base[i].y = kp->params.genhex.basey[i];
        gparams->base[i].z = kp->params.genhex.basez[i];
        gparams->platform[i].x = kp->params.genhex.platformx[i];
        gparams->platform[i].y = kp->params.genhex.platformy[i];
        gparams->platform[i].z = kp->params.genhex.platformz[i];
        gparams->base_n[i].x = kp->params.genhex.basenx[i];
        gparams->base_n[i].y = kp->params.genhex.baseny[i];
        gparams->base_n[i].z = kp->params.genhex.basenz[i];
        gparams->platform_n[i].x = kp->params.genhex.platformnx[i];
        gparams->platform_n[i].y = kp->params.genhex.platformny[i];
        gparams->platform_n[i].z = kp->params.genhex.platformnz[i];
    }

    /* Copy iteration and other parameters */
    gparams->conv_criterion = kp->params.genhex.conv_criterion;
    gparams->iter_limit = kp->params.genhex.iter_limit;
    gparams->max_error = kp->params.genhex.max_error;
    gparams->tool_offset = kp->params.genhex.tool_offset;
    gparams->spindle_offset = kp->params.genhex.spindle_offset;
    gparams->screw_lead = kp->params.genhex.screw_lead;
}

static int genhexkins_inverse(KinematicsUserContext *ctx,
                              const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    genhex_params_t gparams;

    genhexkins_build_params(kp, &gparams);
    return genhex_inv(&gparams, world, joints);
}

static int genhexkins_forward(KinematicsUserContext *ctx,
                              const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    genhex_params_t gparams;
    int result;

    genhexkins_build_params(kp, &gparams);
    result = genhex_fwd(&gparams, joints, world);

    /* Update iteration statistics back to context */
    kp->params.genhex.max_iter = gparams.max_iterations;

    return result;
}

static int genhexkins_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    /* Read base coordinates from HAL pins */
    for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basex[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basey[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basez[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformx[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformy[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformz[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basenx[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.baseny[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basenz[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformnx[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformny[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformnz[i] = fval;
    }

    /* Read iteration control parameters */
    if (hal_pin_reader_read_float("genhexkins.convergence-criterion", &fval) == 0)
        kp->params.genhex.conv_criterion = fval;
    if (hal_pin_reader_read_s32("genhexkins.limit-iterations", &ival) == 0)
        kp->params.genhex.iter_limit = (unsigned int)ival;
    if (hal_pin_reader_read_float("genhexkins.max-error", &fval) == 0)
        kp->params.genhex.max_error = fval;
    if (hal_pin_reader_read_float("genhexkins.tool-offset", &fval) == 0)
        kp->params.genhex.tool_offset = fval;
    if (hal_pin_reader_read_float("genhexkins.spindle-offset", &fval) == 0)
        kp->params.genhex.spindle_offset = fval;
    if (hal_pin_reader_read_float("genhexkins.screw-lead", &fval) == 0)
        kp->params.genhex.screw_lead = fval;

    return 0;
}

/* ========================================================================
 * Pentakins (5-strut parallel kinematics)
 * ======================================================================== */

static void pentakins_build_params(kinematics_params_t *kp, pentakins_params_t *pparams)
{
    int i;

    /* Initialize with default values first */
    pentakins_init_params(pparams);

    /* Copy base coordinates */
    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        pparams->base[i].x = kp->params.penta.basex[i];
        pparams->base[i].y = kp->params.penta.basey[i];
        pparams->base[i].z = kp->params.penta.basez[i];
        pparams->effector_r[i] = kp->params.penta.effectorr[i];
        pparams->effector_z[i] = kp->params.penta.effectorz[i];
    }

    /* Copy iteration parameters */
    pparams->conv_criterion = kp->params.penta.conv_criterion;
    pparams->iter_limit = kp->params.penta.iter_limit;
}

static int pentakins_inverse(KinematicsUserContext *ctx,
                             const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    pentakins_params_t pparams;

    pentakins_build_params(kp, &pparams);
    return pentakins_inv(&pparams, world, joints);
}

static int pentakins_forward(KinematicsUserContext *ctx,
                             const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    pentakins_params_t pparams;
    int result;

    pentakins_build_params(kp, &pparams);
    result = pentakins_fwd(&pparams, joints, world);

    /* Update iteration statistics back to context */
    kp->params.penta.iter_limit = pparams.last_iterations;

    return result;
}

static int pentakins_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    /* Read base coordinates from HAL pins */
    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basex[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basey[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basez[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.r", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.effectorr[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.effectorz[i] = fval;
    }

    /* Read iteration control parameters */
    if (hal_pin_reader_read_float("pentakins.convergence-criterion", &fval) == 0)
        kp->params.penta.conv_criterion = fval;
    if (hal_pin_reader_read_s32("pentakins.limit-iterations", &ival) == 0)
        kp->params.penta.iter_limit = (unsigned int)ival;

    return 0;
}

/* ========================================================================
 * Genserkins (Generic serial robot with DH parameters)
 * ======================================================================== */

static void genserkins_build_params(kinematics_params_t *kp, genserkins_params_t *gparams)
{
    int i;

    /* Initialize with default values first */
    genserkins_init_params(gparams);

    /* Copy parameters from shared memory */
    gparams->link_num = kp->params.genser.link_num;
    gparams->max_iterations = kp->params.genser.max_iterations;

    /* Copy DH parameters */
    for (i = 0; i < GENSERKINS_MAX_JOINTS && i < KINS_GENSER_MAX_JOINTS; i++) {
        gparams->a[i] = kp->params.genser.a[i];
        gparams->alpha[i] = kp->params.genser.alpha[i];
        gparams->d[i] = kp->params.genser.d[i];
        gparams->unrotate[i] = kp->params.genser.unrotate[i];
    }
}

static int genserkins_inverse(KinematicsUserContext *ctx,
                              const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    genserkins_params_t gparams;
    int result;

    genserkins_build_params(kp, &gparams);
    result = genserkins_inv(&gparams, world, joints);

    /* Update iteration count back to context */
    kp->params.genser.last_iterations = gparams.last_iterations;

    return result;
}

static int genserkins_forward(KinematicsUserContext *ctx,
                              const double *joints, EmcPose *world)
{
    kinematics_params_t *kp = &ctx->params;
    genserkins_params_t gparams;

    genserkins_build_params(kp, &gparams);
    return genserkins_fwd(&gparams, joints, world);
}

static int genserkins_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    /* Read DH parameters from HAL pins */
    for (i = 0; i < KINS_GENSER_MAX_JOINTS && i < 6; i++) {
        snprintf(pin_name, sizeof(pin_name), "genserkins.A-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.a[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genserkins.ALPHA-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.alpha[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genserkins.D-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.d[i] = fval;

        snprintf(pin_name, sizeof(pin_name), "genserkins.unrotate-%d", i);
        if (hal_pin_reader_read_s32(pin_name, &ival) == 0)
            kp->params.genser.unrotate[i] = ival;
    }

    /* Read max iterations (s32 cast to unsigned) */
    if (hal_pin_reader_read_s32("genserkins.max-iterations", &ival) == 0)
        kp->params.genser.max_iterations = (unsigned int)ival;

    return 0;
}

/* ========================================================================
 * Scorbotkins
 * ======================================================================== */

static int scorbotkins_inverse(KinematicsUserContext *ctx,
                               const EmcPose *world, double *joints)
{
    kinematics_params_t *kp = &ctx->params;
    scorbot_params_t params;

    scorbot_build_params(kp, &params);
    return scorbot_inverse_math(&params, world, joints);
}

static int scorbotkins_refresh(KinematicsUserContext *ctx)
{
    /* Scorbot has fixed link lengths - no HAL pins to read */
    /* Could add HAL pins if needed for different Scorbot models */
    (void)ctx;
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
    kinematics_type_id_t type_id;

    /* Debug: print incoming parameters */
    fprintf(stderr, "kinematicsUserInit ENTRY: kins_type='%s', num_joints=%d, coordinates='%s'\n",
            kins_type ? kins_type : "(null)",
            num_joints,
            coordinates ? coordinates : "(null)");

    if (num_joints < 1 || num_joints > KINEMATICS_USER_MAX_JOINTS) {
        fprintf(stderr, "kinematicsUserInit: invalid num_joints %d\n", num_joints);
        return NULL;
    }

    /* Determine kinematics type */
    type_id = module_name_to_type_id(kins_type);
    if (type_id == KINS_TYPE_UNKNOWN) {
        fprintf(stderr, "kinematicsUserInit: unknown kinematics type '%s'\n",
                kins_type ? kins_type : "(null)");
        return NULL;
    }

    /* Allocate context */
    ctx = (KinematicsUserContext *)calloc(1, sizeof(KinematicsUserContext));
    if (!ctx) {
        fprintf(stderr, "kinematicsUserInit: memory allocation failed\n");
        return NULL;
    }

    kp = &ctx->params;
    kp->type_id = type_id;
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

    /* Set function pointers based on kinematics type */
    switch (type_id) {
        case KINS_TYPE_TRIVKINS:
            ctx->inverse = trivkins_inverse;
            ctx->forward = trivkins_forward;
            ctx->refresh = trivkins_refresh;
            break;

        case KINS_TYPE_5AXISKINS:
            ctx->inverse = fiveaxis_inverse;
            ctx->forward = fiveaxis_forward;
            ctx->refresh = fiveaxis_refresh;
            break;

        case KINS_TYPE_XYZAC_TRT:
            ctx->inverse = xyzac_trt_inverse;
            ctx->forward = xyzac_trt_forward;
            ctx->refresh = xyzac_trt_refresh;
            break;

        case KINS_TYPE_XYZBC_TRT:
            ctx->inverse = xyzbc_trt_inverse;
            ctx->forward = xyzbc_trt_forward;
            ctx->refresh = xyzbc_trt_refresh;
            break;

        case KINS_TYPE_MAXKINS:
            ctx->inverse = maxkins_inverse;
            ctx->forward = maxkins_forward;
            ctx->refresh = maxkins_refresh;
            break;

        case KINS_TYPE_TRIPODKINS:
            ctx->inverse = tripodkins_inverse;
            ctx->forward = tripodkins_forward;
            ctx->refresh = tripodkins_refresh;
            /* Set default values */
            ctx->params.params.tripod.bx = 1.0;
            ctx->params.params.tripod.cx = 1.0;
            ctx->params.params.tripod.cy = 1.0;
            break;

        case KINS_TYPE_LINEARDELTA:
            ctx->inverse = lineardelta_inverse;
            ctx->forward = lineardelta_forward;
            ctx->refresh = lineardelta_refresh;
            /* Set default values from lineardeltakins-common.h */
            ctx->params.params.lineardelta.radius = 269.0;  /* DELTA_DIAGONAL_ROD */
            ctx->params.params.lineardelta.jointradius = 130.25;  /* DELTA_RADIUS */
            break;

        case KINS_TYPE_ROTARYDELTA:
            ctx->inverse = rotarydelta_inverse;
            ctx->forward = rotarydelta_forward;
            ctx->refresh = rotarydelta_refresh;
            /* Set default values from rotarydeltakins_math.h */
            ctx->params.params.rotarydelta.platformradius = ROTARYDELTA_DEFAULT_PLATFORMRADIUS;
            ctx->params.params.rotarydelta.thighlength = ROTARYDELTA_DEFAULT_THIGHLENGTH;
            ctx->params.params.rotarydelta.shinlength = ROTARYDELTA_DEFAULT_SHINLENGTH;
            ctx->params.params.rotarydelta.footradius = ROTARYDELTA_DEFAULT_FOOTRADIUS;
            break;

        case KINS_TYPE_ROSEKINS:
            ctx->inverse = rosekins_inverse;
            ctx->forward = rosekins_forward;
            ctx->refresh = rosekins_refresh;
            break;

        case KINS_TYPE_PUMAKINS:
            ctx->inverse = pumakins_inverse;
            ctx->forward = pumakins_forward;
            ctx->refresh = pumakins_refresh;
            /* Set default PUMA560 values */
            ctx->params.params.puma.a2 = 300.0;
            ctx->params.params.puma.a3 = 50.0;
            ctx->params.params.puma.d3 = 70.0;
            ctx->params.params.puma.d4 = 400.0;
            ctx->params.params.puma.d6 = 70.0;
            break;

        case KINS_TYPE_SCARAKINS:
            ctx->inverse = scarakins_inverse;
            ctx->forward = scarakins_forward;
            ctx->refresh = scarakins_refresh;
            /* Set default SCARA values */
            ctx->params.params.scara.d1 = 490.0;
            ctx->params.params.scara.d2 = 340.0;
            ctx->params.params.scara.d3 = 50.0;
            ctx->params.params.scara.d4 = 250.0;
            ctx->params.params.scara.d5 = 50.0;
            ctx->params.params.scara.d6 = 50.0;
            break;

        case KINS_TYPE_COREXYKINS:
            ctx->inverse = corexykins_inverse;
            ctx->forward = corexykins_forward;
            ctx->refresh = corexykins_refresh;
            /* No parameters needed for CoreXY */
            break;

        case KINS_TYPE_ROTATEKINS:
            ctx->inverse = rotatekins_inverse;
            ctx->forward = rotatekins_forward;
            ctx->refresh = rotatekins_refresh;
            /* No parameters needed for rotatekins */
            break;

        case KINS_TYPE_GENHEXKINS:
            ctx->inverse = genhexkins_inverse;
            ctx->forward = genhexkins_forward;
            ctx->refresh = genhexkins_refresh;
            /* Set default hexapod values from genhexkins.h */
            kp->params.genhex.basex[0] = -22.950; kp->params.genhex.basey[0] =  13.250; kp->params.genhex.basez[0] = 0.000;
            kp->params.genhex.basex[1] =  22.950; kp->params.genhex.basey[1] =  13.250; kp->params.genhex.basez[1] = 0.000;
            kp->params.genhex.basex[2] =  22.950; kp->params.genhex.basey[2] =  13.250; kp->params.genhex.basez[2] = 0.000;
            kp->params.genhex.basex[3] =   0.000; kp->params.genhex.basey[3] = -26.500; kp->params.genhex.basez[3] = 0.000;
            kp->params.genhex.basex[4] =   0.000; kp->params.genhex.basey[4] = -26.500; kp->params.genhex.basez[4] = 0.000;
            kp->params.genhex.basex[5] = -22.950; kp->params.genhex.basey[5] =  13.250; kp->params.genhex.basez[5] = 0.000;
            kp->params.genhex.platformx[0] =  -1.000; kp->params.genhex.platformy[0] =  11.500; kp->params.genhex.platformz[0] = 0.000;
            kp->params.genhex.platformx[1] =   1.000; kp->params.genhex.platformy[1] =  11.500; kp->params.genhex.platformz[1] = 0.000;
            kp->params.genhex.platformx[2] =  10.459; kp->params.genhex.platformy[2] =  -4.884; kp->params.genhex.platformz[2] = 0.000;
            kp->params.genhex.platformx[3] =   9.459; kp->params.genhex.platformy[3] =  -6.616; kp->params.genhex.platformz[3] = 0.000;
            kp->params.genhex.platformx[4] =  -9.459; kp->params.genhex.platformy[4] =  -6.616; kp->params.genhex.platformz[4] = 0.000;
            kp->params.genhex.platformx[5] = -10.459; kp->params.genhex.platformy[5] =  -4.884; kp->params.genhex.platformz[5] = 0.000;
            kp->params.genhex.basenx[0] =  0.707107; kp->params.genhex.baseny[0] =  0.0;      kp->params.genhex.basenz[0] = 0.707107;
            kp->params.genhex.basenx[1] =  0.0;      kp->params.genhex.baseny[1] = -0.707107; kp->params.genhex.basenz[1] = 0.707107;
            kp->params.genhex.basenx[2] = -0.707107; kp->params.genhex.baseny[2] =  0.0;      kp->params.genhex.basenz[2] = 0.707107;
            kp->params.genhex.basenx[3] = -0.707107; kp->params.genhex.baseny[3] =  0.0;      kp->params.genhex.basenz[3] = 0.707107;
            kp->params.genhex.basenx[4] =  0.0;      kp->params.genhex.baseny[4] =  0.707107; kp->params.genhex.basenz[4] = 0.707107;
            kp->params.genhex.basenx[5] =  0.707107; kp->params.genhex.baseny[5] =  0.0;      kp->params.genhex.basenz[5] = 0.707107;
            kp->params.genhex.platformnx[0] = -1.0;     kp->params.genhex.platformny[0] =  0.0; kp->params.genhex.platformnz[0] = 0.0;
            kp->params.genhex.platformnx[1] =  0.866025; kp->params.genhex.platformny[1] =  0.5; kp->params.genhex.platformnz[1] = 0.0;
            kp->params.genhex.platformnx[2] =  0.866025; kp->params.genhex.platformny[2] =  0.5; kp->params.genhex.platformnz[2] = 0.0;
            kp->params.genhex.platformnx[3] =  0.866025; kp->params.genhex.platformny[3] = -0.5; kp->params.genhex.platformnz[3] = 0.0;
            kp->params.genhex.platformnx[4] =  0.866025; kp->params.genhex.platformny[4] = -0.5; kp->params.genhex.platformnz[4] = 0.0;
            kp->params.genhex.platformnx[5] = -1.0;     kp->params.genhex.platformny[5] =  0.0; kp->params.genhex.platformnz[5] = 0.0;
            kp->params.genhex.conv_criterion = 1e-9;
            kp->params.genhex.iter_limit = 120;
            kp->params.genhex.max_error = 500.0;
            kp->params.genhex.tool_offset = 0.0;
            kp->params.genhex.spindle_offset = 0.0;
            kp->params.genhex.screw_lead = 0.0;
            break;

        case KINS_TYPE_PENTAKINS:
            ctx->inverse = pentakins_inverse;
            ctx->forward = pentakins_forward;
            ctx->refresh = pentakins_refresh;
            /* Set default pentapod values */
            kp->params.penta.basex[0] = -418.03; kp->params.penta.basey[0] =  324.56; kp->params.penta.basez[0] = 895.56;
            kp->params.penta.basex[1] =  417.96; kp->params.penta.basey[1] =  324.56; kp->params.penta.basez[1] = 895.56;
            kp->params.penta.basex[2] = -418.03; kp->params.penta.basey[2] = -325.44; kp->params.penta.basez[2] = 895.56;
            kp->params.penta.basex[3] =  417.96; kp->params.penta.basey[3] = -325.44; kp->params.penta.basez[3] = 895.56;
            kp->params.penta.basex[4] =   -0.06; kp->params.penta.basey[4] = -492.96; kp->params.penta.basez[4] = 895.56;
            kp->params.penta.effectorr[0] = kp->params.penta.effectorr[1] = kp->params.penta.effectorr[2] =
            kp->params.penta.effectorr[3] = kp->params.penta.effectorr[4] = 80.32;
            kp->params.penta.effectorz[0] = -185.50;
            kp->params.penta.effectorz[1] = -159.50;
            kp->params.penta.effectorz[2] =  -67.50;
            kp->params.penta.effectorz[3] =  -41.50;
            kp->params.penta.effectorz[4] =  -14.00;
            kp->params.penta.conv_criterion = 1e-9;
            kp->params.penta.iter_limit = 120;
            break;

        case KINS_TYPE_GENSERFUNCS:
            ctx->inverse = genserkins_inverse;
            ctx->forward = genserkins_forward;
            ctx->refresh = genserkins_refresh;
            /* Set default PUMA-like DH parameters */
            kp->params.genser.link_num = 6;
            kp->params.genser.max_iterations = GENSERKINS_DEFAULT_MAX_ITERATIONS;
            kp->params.genser.last_iterations = 0;
            /* Joint 0 */
            kp->params.genser.a[0] = GENSERKINS_DEFAULT_A1;
            kp->params.genser.alpha[0] = GENSERKINS_DEFAULT_ALPHA1;
            kp->params.genser.d[0] = GENSERKINS_DEFAULT_D1;
            /* Joint 1 */
            kp->params.genser.a[1] = GENSERKINS_DEFAULT_A2;
            kp->params.genser.alpha[1] = GENSERKINS_DEFAULT_ALPHA2;
            kp->params.genser.d[1] = GENSERKINS_DEFAULT_D2;
            /* Joint 2 */
            kp->params.genser.a[2] = GENSERKINS_DEFAULT_A3;
            kp->params.genser.alpha[2] = GENSERKINS_DEFAULT_ALPHA3;
            kp->params.genser.d[2] = GENSERKINS_DEFAULT_D3;
            /* Joint 3 */
            kp->params.genser.a[3] = GENSERKINS_DEFAULT_A4;
            kp->params.genser.alpha[3] = GENSERKINS_DEFAULT_ALPHA4;
            kp->params.genser.d[3] = GENSERKINS_DEFAULT_D4;
            /* Joint 4 */
            kp->params.genser.a[4] = GENSERKINS_DEFAULT_A5;
            kp->params.genser.alpha[4] = GENSERKINS_DEFAULT_ALPHA5;
            kp->params.genser.d[4] = GENSERKINS_DEFAULT_D5;
            /* Joint 5 */
            kp->params.genser.a[5] = GENSERKINS_DEFAULT_A6;
            kp->params.genser.alpha[5] = GENSERKINS_DEFAULT_ALPHA6;
            kp->params.genser.d[5] = GENSERKINS_DEFAULT_D6;
            /* Initialize remaining joints to zero */
            kp->params.genser.a[6] = kp->params.genser.a[7] = kp->params.genser.a[8] = 0.0;
            kp->params.genser.alpha[6] = kp->params.genser.alpha[7] = kp->params.genser.alpha[8] = 0.0;
            kp->params.genser.d[6] = kp->params.genser.d[7] = kp->params.genser.d[8] = 0.0;
            /* Initialize unrotate flags to zero */
            kp->params.genser.unrotate[0] = kp->params.genser.unrotate[1] = kp->params.genser.unrotate[2] = 0;
            kp->params.genser.unrotate[3] = kp->params.genser.unrotate[4] = kp->params.genser.unrotate[5] = 0;
            kp->params.genser.unrotate[6] = kp->params.genser.unrotate[7] = kp->params.genser.unrotate[8] = 0;
            break;

        case KINS_TYPE_SCORBOTKINS:
            ctx->inverse = scorbotkins_inverse;
            ctx->forward = scorbotkins_forward;
            ctx->refresh = scorbotkins_refresh;
            /* Set default Scorbot ER-3 values (mm) */
            ctx->params.params.scorbot.l0_horizontal = 16.0;
            ctx->params.params.scorbot.l0_vertical = 140.0;
            ctx->params.params.scorbot.l1_length = 221.0;
            ctx->params.params.scorbot.l2_length = 221.0;
            break;

        default:
            fprintf(stderr, "kinematicsUserInit: kinematics type '%s' not yet implemented\n",
                    kins_type);
            fprintf(stderr, "  Supported types: trivkins, 5axiskins, xyzac-trt-kins, xyzbc-trt-kins,\n");
            fprintf(stderr, "                   maxkins, pumakins, scarakins, tripodkins, genhexkins, pentakins,\n");
            fprintf(stderr, "                   genserkins, lineardeltakins, rotarydeltakins, rosekins,\n");
            fprintf(stderr, "                   corexykins, rotatekins, scorbot-kins\n");
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

    fprintf(stderr, "kinematicsUserInit: initialized '%s' with %d joints, coords='%s'\n",
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
    return (ctx->params.type_id == KINS_TYPE_TRIVKINS) ? 1 : 0;
}

int kinematicsUserGetNumJoints(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return 0;
    }
    return ctx->params.num_joints;
}

kinematics_type_id_t kinematicsUserGetTypeId(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return KINS_TYPE_UNKNOWN;
    }
    return ctx->params.type_id;
}

KINEMATICS_TYPE kinematicsUserGetType(KinematicsUserContext* ctx)
{
    if (!ctx || !ctx->initialized) {
        return KINEMATICS_IDENTITY;
    }

    if (ctx->params.type_id == KINS_TYPE_TRIVKINS) {
        return KINEMATICS_IDENTITY;
    }

    /* Most kinematics support both forward and inverse */
    return KINEMATICS_BOTH;
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
        free(ctx);
    }
}
