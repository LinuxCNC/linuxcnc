/********************************************************************
* Description: sp_scurve.h
*   Discriminate-based trajectory planning
*
*   Derived from a work by Yang Yang
*
* Author: Yang Yang
* Contact: mika-net@outlook.com
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef SP_SCURVE_H
#define SP_SCURVE_H

#include "simple_tp.h"
#include "rtapi_math.h"

/**
 * Initialize the S-curve planner (call at program entry).
 *
 * @param cycle_time  cycle time in seconds
 * @return 0 on success, -1 on failure
 */
int sp_scurve_init(double cycle_time);

/**
 * Clean up the S-curve planner (call at program exit).
 */
void sp_scurve_cleanup(void);

/* Legacy functions kept for simple_tp.c compatibility */
double nextAccel(double t, double targetV, double v, double a, double maxA, double maxJ);
double sc_distance(double t, double v, double a, double j);
double delta_velocity(double t, double a, double j);
double delta_accel(double t, double j);
double nextSpeed(double v, double a, double t, double targetV, double maxA, double maxJ, double* req_v, double* req_a, double* req_j);
double stoppingDist(double v, double a, double maxA, double maxJ);

int findSCurveVSpeed(double distence,/* double maxV, */double maxA, double maxJ, double *req_v);
int findSCurveVSpeedWithEndSpeed(double distence, double Ve, double maxA, double maxJ, double* req_v);
int findSCurveMaxStartSpeed(double distance, double Ve, double maxA, double maxJ, double* req_v);
double calcDecelerateTimes(double v, double amax, double jerk, double* t1, double* t2);
double calcSCurveSpeedWithT(double amax, double jerk, double T);

/**
 * tpCalculateSCurveAccel return value definitions
 *
 * TP_SCURVE_ACCEL_ERROR - calculation failed (maxjerk invalid or less than/equal to 1)
 * TP_SCURVE_ACCEL_ACCEL - acceleration or normal state (no deceleration needed)
 * TP_SCURVE_ACCEL_DECEL - deceleration needed
 */
#define TP_SCURVE_ACCEL_ERROR  -5
#define TP_SCURVE_ACCEL_ACCEL   0
#define TP_SCURVE_ACCEL_DECEL   1

#endif
