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
#include "pm_vector.h"

double tcGetMaxVelFromLength(TC_STRUCT const * const tc);
double tcGetPlanMaxTargetVel(TC_STRUCT const * const tc,
        double max_feed_scale);

double tcGetAccelScale(TC_STRUCT const * tc);
double tcGetOverallMaxAccel(TC_STRUCT const * tc);
double tcGetTangentialMaxAccel(TC_STRUCT const * const tc);

int tcSetKinkProperties(TC_STRUCT *prev_tc, TC_STRUCT *tc, double kink_vel, double accel_reduction);
int tcGetEndpoint(TC_STRUCT const * const tc, PmVector * const out);
int tcGetStartpoint(TC_STRUCT const * const tc, PmVector * const pos);
int tcGetPos(TC_STRUCT const * const tc,  PmVector * const out);
int tcGetPosReal(TC_STRUCT const * const tc, double progress,  PmVector * const out);
int tcGetStartTangentUnitVector(TC_STRUCT const * const tc, PmVector * const out);
int tcGetEndTangentUnitVector(TC_STRUCT const * const tc, PmVector * const out);
int tcGetTangentUnitVector(TC_STRUCT const * const tc, double progress, PmVector * const out);
double tcGetVLimit(TC_STRUCT const * const tc, double v_target, double v_limit_linear, double v_limit_angular);

double tcGetDistanceToGo(TC_STRUCT const * const tc, int direction);
double tcGetTarget(TC_STRUCT const * const tc, int direction);

int tcCanConsume(TC_STRUCT const * const tc);

int tcSetTermCond(TC_STRUCT * tc, tc_term_cond_t term_cond);

int tcFindBlendTolerance(TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc, double * const T_blend);

int pmRigidTapInit(PmRigidTap * const tap,
        PmVector const * const start,
        PmVector const * const end,
        double reversal_scale);

int tcInit(TC_STRUCT * const tc,
        tc_motion_type_t motion_type,
        int canon_motion_type,
        double cycle_time,
        unsigned char enables,
        char atspeed);

int tcSetupMotion(TC_STRUCT * const tc,
        double vel,
        double ini_maxvel,
        double acc);

int tcSetupState(TC_STRUCT * const tc, TP_STRUCT const * const tp);

int tcUpdateCircleAccRatio(TC_STRUCT * tc, double v_max);

int tcFinalizeLength(TC_STRUCT * const tc, double max_feed_override);

int tcPureRotaryCheck(TC_STRUCT const * const tc);

int tcSetLineXYZ(TC_STRUCT * const tc, PmCartLine const * const line);

int tcSetCircleXYZ(TC_STRUCT * const tc, PmCircle const * const circ);

int tcSetLine9(TC_STRUCT * const tc, PmLine9 const * const line9);

int tcSetCircle9(TC_STRUCT * const tc, PmCircle9 const * const circ9);

const char *tcTermCondAsString(tc_term_cond_t c);
const char *tcMotionTypeAsString(tc_motion_type_t c);
const char *tcSyncModeAsString(tc_spindle_sync_t c);

#endif				/* TC_H */
