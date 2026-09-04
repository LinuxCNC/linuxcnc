/********************************************************************
 * Description: kinematics_user.h
 *   Userspace kinematics interface for trajectory planning
 *
 * This provides a userspace-compatible kinematics interface that mirrors
 * the RT kinematics interface. Used by the 9D planner to compute joint
 * positions from world coordinates without requiring RT kernel calls.
 *
 * The kinematics module is loaded into this process and evaluated through
 * its parameter block form (see kinematics.h).  The block is filled from
 * input pins belonging to the caller's HAL component, connected to the
 * same signals the running RT instance reads, and from motion's tool
 * offset pins where motion is loaded, so the maths runs on live values.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef KINEMATICS_USER_H
#define KINEMATICS_USER_H

#include <emcpos.h>           /* EmcPose */
#include <kinematics.h>       /* KINEMATICS_TYPE, flags */
#include <hal.h>              /* hal_type_t, HAL_NAME_LEN */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of joints supported */
#define KINEMATICS_USER_MAX_JOINTS 9

/* Axis coordinate indices for EmcPose */
typedef enum {
    AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2,
    AXIS_A = 3, AXIS_B = 4, AXIS_C = 5,
    AXIS_U = 6, AXIS_V = 7, AXIS_W = 8,
    AXIS_COUNT = 9
} AxisIndex;

/* Opaque context for userspace kinematics */
typedef struct KinematicsUserContext KinematicsUserContext;

/**
 * Initialize userspace kinematics context
 *
 * The pins this creates belong to the caller's component, so call this
 * after hal_init() and before hal_ready(): HAL refuses new pins once a
 * component is ready.
 *
 * @param kins_type   Kinematics module name (e.g., "trivkins", "5axiskins", "maxkins")
 * @param num_joints  Number of joints in the machine
 * @param coordinates Coordinate string (e.g., "XYZABC", "XYZBCW")
 * @param comp_id     Caller's HAL component, from hal_init()
 * @param prefix      Its name, which the created pin names start with
 * @return Allocated context, or NULL if kinematics type not supported
 */
KinematicsUserContext* kinematicsUserInit(const char* kins_type,
                                          int num_joints,
                                          const char* coordinates,
                                          int comp_id,
                                          const char* prefix);

/**
 * As kinematicsUserInit(), with the module's sparm= parameter as well, for
 * a module whose kinematics types depend on it (5axiskins identityfirst).
 */
KinematicsUserContext* kinematicsUserInitSparm(const char* kins_type,
                                               int num_joints,
                                               const char* coordinates,
                                               const char* sparm,
                                               int comp_id,
                                               const char* prefix);

/**
 * Select which kinematics type of a switchable module to evaluate.
 * Type 0 is selected after init.
 *
 * @return 0, or -1 if the module has no such type in the block form
 */
int kinematicsUserSetType(KinematicsUserContext* ctx, int ktype);

/**
 * How many kinematics types the module has (1 for one that does not switch).
 */
int kinematicsUserGetNumTypes(KinematicsUserContext* ctx);

/**
 * Perform inverse kinematics (world coords -> joint positions)
 *
 * @param ctx     Kinematics context from kinematicsUserInit
 * @param world   World coordinates (X, Y, Z, A, B, C, U, V, W)
 * @param joints  Output array of joint positions [KINEMATICS_USER_MAX_JOINTS]
 * @return 0 on success, -1 on failure
 */
int kinematicsUserInverse(KinematicsUserContext* ctx,
                          const EmcPose* world,
                          double* joints);

/**
 * Perform forward kinematics (joint positions -> world coords)
 *
 * @param ctx     Kinematics context from kinematicsUserInit
 * @param joints  Array of joint positions [KINEMATICS_USER_MAX_JOINTS]
 * @param world   Output world coordinates
 * @return 0 on success, -1 on failure
 */
int kinematicsUserForward(KinematicsUserContext* ctx,
                          const double* joints,
                          EmcPose* world);

/**
 * The Jacobian at a pose, J[joint][axis] = d joint / d axis, from the
 * module's closed form where it has one and by differencing its inverse
 * where it does not.  The inverse is run at the pose first, so the
 * derivative is taken on the solution branch the module picks there.
 *
 * @return 0 on success, -1 on failure
 */
int kinematicsUserJacobian(KinematicsUserContext* ctx,
                           const EmcPose* world,
                           double J[KINEMATICS_USER_MAX_JOINTS][AXIS_COUNT]);

/**
 * The parameter block as it stands, refreshed from HAL first.  For
 * reporting; the block belongs to the context.
 */
const kins_params* kinematicsUserParams(KinematicsUserContext* ctx);

/**
 * Check if kinematics type is identity (world coords = joint coords)
 *
 * @param ctx  Kinematics context
 * @return 1 if identity, 0 if not
 */
int kinematicsUserIsIdentity(KinematicsUserContext* ctx);

/**
 * Get number of joints
 *
 * @param ctx  Kinematics context
 * @return Number of joints
 */
int kinematicsUserGetNumJoints(KinematicsUserContext* ctx);

/**
 * Get KINEMATICS_TYPE (IDENTITY, BOTH, FORWARD_ONLY, INVERSE_ONLY)
 *
 * @param ctx  Kinematics context
 * @return KINEMATICS_TYPE enum value
 */
KINEMATICS_TYPE kinematicsUserGetType(KinematicsUserContext* ctx);

/**
 * Get kinematics module name
 *
 * @param ctx  Kinematics context
 * @return Module name string (e.g., "5axiskins")
 */
const char* kinematicsUserGetModuleName(KinematicsUserContext* ctx);

/**
 * Copy the bound pins into the block now.  Every evaluation does this
 * itself; call it only to observe the values.
 *
 * @param ctx  Kinematics context
 * @return 0, or -1 for an RT-only context
 */
int kinematicsUserRefreshParams(KinematicsUserContext* ctx);

/**
 * Check if this context is RT-only
 *
 * An RT-only module exports no kinsDescribe() and so cannot be evaluated
 * outside RT.  Planner 2 is unavailable for such modules.
 *
 * @param ctx  Kinematics context
 * @return 1 if RT-only (planner 2 unavailable), 0 if the module is bound
 */
int kinematicsUserIsRtOnly(KinematicsUserContext* ctx);

/**
 * Free kinematics context
 *
 * @param ctx  Context to free
 */
void kinematicsUserFree(KinematicsUserContext* ctx);

/**
 * Get axis value from EmcPose by index
 *
 * @param pose  Pointer to EmcPose
 * @param axis  Axis index (AXIS_X through AXIS_W)
 * @return Axis value
 */
static inline double emcPoseGetAxis(const EmcPose* pose, int axis) {
    switch (axis) {
        case AXIS_X: return pose->tran.x;
        case AXIS_Y: return pose->tran.y;
        case AXIS_Z: return pose->tran.z;
        case AXIS_A: return pose->a;
        case AXIS_B: return pose->b;
        case AXIS_C: return pose->c;
        case AXIS_U: return pose->u;
        case AXIS_V: return pose->v;
        case AXIS_W: return pose->w;
        default: return 0.0;
    }
}

/**
 * Set axis value in EmcPose by index
 *
 * @param pose  Pointer to EmcPose
 * @param axis  Axis index (AXIS_X through AXIS_W)
 * @param value Value to set
 */
static inline void emcPoseSetAxis(EmcPose* pose, int axis, double value) {
    switch (axis) {
        case AXIS_X: pose->tran.x = value; break;
        case AXIS_Y: pose->tran.y = value; break;
        case AXIS_Z: pose->tran.z = value; break;
        case AXIS_A: pose->a = value; break;
        case AXIS_B: pose->b = value; break;
        case AXIS_C: pose->c = value; break;
        case AXIS_U: pose->u = value; break;
        case AXIS_V: pose->v = value; break;
        case AXIS_W: pose->w = value; break;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* KINEMATICS_USER_H */
