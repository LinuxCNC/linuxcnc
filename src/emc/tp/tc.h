/********************************************************************
* Description: tc.h
*   Discriminate-based trajectory planning
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef TC_H
#define TC_H

#include "spherical_arc.h"
#include "posemath.h"
#include "emcpos.h"
#include "emcmotcfg.h"
#include "tc_types.h"
#include "tp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

double tcGetMaxTargetVel(TC_STRUCT const * const tc,
        double max_scale);

double tcGetOverallMaxAccel(TC_STRUCT const * tc);
double tcGetTangentialMaxAccel(TC_STRUCT const * const tc);

int tcSetKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc, double kink_vel, double accel_reduction);
int tcInitKinkProperties(TC_STRUCT *tc);
int tcRemoveKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc);
int tcGetEndpoint(TC_STRUCT const * const tc, EmcPose * const out);
int tcGetStartpoint(TC_STRUCT const * const tc, EmcPose * const out);
int tcGetPos(TC_STRUCT const * const tc,  EmcPose * const out);
int tcGetPosReal(TC_STRUCT const * const tc, int of_endpoint,  EmcPose * const out);
int tcGetEndAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);
int tcGetStartAccelUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);
int tcGetEndTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);
int tcGetStartTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);
int tcGetCurrentTangentUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);

double tcGetDistanceToGo(TC_STRUCT const * const tc, int direction);
double tcGetTarget(TC_STRUCT const * const tc, int direction);

int tcGetIntersectionPoint(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, PmCartesian * const point);

int tcCanConsume(TC_STRUCT const * const tc);

int tcSetTermCond(TC_STRUCT * prev_tc, TC_STRUCT * tc, int term_cond);

int tcConnectBlendArc(TC_STRUCT * const prev_tc, TC_STRUCT * const tc,
        PmCartesian const * const circ_start,
        PmCartesian const * const circ_end);

int tcIsBlending(TC_STRUCT * const tc);


int tcFindBlendTolerance(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, double * const T_blend, double * const nominal_tolerance);

int pmCircleTangentVector(PmCircle const * const circle,
        double angle_in, PmCartesian * const out);

int tcFlagEarlyStop(TC_STRUCT * const tc,
        TC_STRUCT * const nexttc);

double pmLine9Target(PmLine9 * const line9);

int pmLine9Init(PmLine9 * const line9,
        EmcPose const * const start,
        EmcPose const * const end);

double pmCircle9Target(PmCircle9 const * const circ9);

int pmCircle9Init(PmCircle9 * const circ9,
        EmcPose const * const start,
        EmcPose const * const end,
        PmCartesian const * const center,
        PmCartesian const * const normal,
        int turn);

int pmRigidTapInit(PmRigidTap * const tap,
        EmcPose const * const start,
        EmcPose const * const end,
        double reversal_scale);

double pmRigidTapTarget(PmRigidTap * const tap, double uu_per_rev);

int tcInit(TC_STRUCT * const tc,
        int motion_type,
        int canon_motion_type,
        double cycle_time,
        unsigned char enables,
        char atspeed);

int tcSetupFromTP(TC_STRUCT * const tc, TP_STRUCT const * const tp);

int tcSetupMotion(TC_STRUCT * const tc,
        double vel,
        double ini_maxvel,
        double acc,
        double ini_maxjerk);

int tcSetupState(TC_STRUCT * const tc, TP_STRUCT const * const tp);

int tcUpdateArcLimits(TC_STRUCT * tc);

int tcFinalizeLength(TC_STRUCT * const tc);

int tcClampVelocityByLength(TC_STRUCT * const tc);

int tcPureRotaryCheck(TC_STRUCT const * const tc);

int tcSetCircleXYZ(TC_STRUCT * const tc, PmCircle const * const circ);

int tcClearFlags(TC_STRUCT * const tc);

/**
 * Sample Ruckig profile at given time
 *
 * Evaluates the pre-computed Ruckig trajectory profile at time t.
 * Uses polynomial integration: p(t) = p0 + v0*dt + 0.5*a0*dt^2 + (1/6)*j*dt^3
 *
 * @param profile   Pointer to ruckig_profile_t
 * @param t         Time since segment start (seconds)
 * @param pos       Output: position at time t
 * @param vel       Output: velocity at time t
 * @param acc       Output: acceleration at time t
 * @param jerk      Output: jerk at time t
 * @return          0 on success, -1 if profile invalid
 */
int ruckigProfileSample(ruckig_profile_t const * const profile,
                        double t,
                        double *pos,
                        double *vel,
                        double *acc,
                        double *jerk);

/**
 * Compute one cycle of jerk-limited deceleration toward velocity = 0.
 *
 * RT-safe jerk-limited emergency stop implementation.
 *
 * When abort/pause is triggered during Ruckig profile execution, we can't
 * follow the pre-computed profile (it would run to segment end). Instead,
 * we compute jerk-limited deceleration cycle-by-cycle in RT to stop
 * smoothly along the path.
 *
 * Current approach: Simple sqrt-profile targeting. Computes optimal
 * deceleration each cycle to bring velocity to zero while respecting
 * jerk and acceleration limits.
 *
 * Future refinement: Could use userspace to pre-compute a
 * proper stopping trajectory via Ruckig when abort is detected, then
 * hand off to RT via predictive handoff mechanism.
 *
 * @param v0        Current velocity (signed, positive = forward)
 * @param a0        Current acceleration (signed)
 * @param j_max     Maximum jerk (positive value)
 * @param a_max     Maximum acceleration (positive value)
 * @param dt        Time step (cycle time, typically 1ms)
 * @param v_out     Output: new velocity after this cycle
 * @param a_out     Output: new acceleration after this cycle
 * @param j_out     Output: jerk applied this cycle
 * @param dist_out  Output: distance traveled this cycle (always >= 0)
 */
void tcComputeJerkLimitedStop(double v0, double a0,
                              double j_max, double a_max,
                              double dt,
                              double *v_out, double *a_out,
                              double *j_out, double *dist_out);

/**
 * Clean up Ruckig planner resources in a TC_STRUCT.
 * Called when the trajectory segment is removed or reset.
 */
void tcCleanupRuckig(TC_STRUCT * const tc);

#ifdef __cplusplus
}
#endif

#endif				/* TC_H */
