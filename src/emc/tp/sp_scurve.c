/*!
********************************************************************
* Description: sp_scurve.c
*\brief Ruckig-based S-curve trajectory planning with legacy helpers
*
*\author Derived from a work by Yang Yang
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#include "sp_scurve.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "tp_types.h"
#include "ruckig_wrapper.h"
#ifndef __KERNEL__
#include <stdio.h>
#include <string.h>
#endif

/* ========== Cached Ruckig planner ==========
 * Use a static variable to cache the planner, avoiding creation and
 * destruction on every call.
 */
static RuckigPlanner cached_planner = NULL;
static double cached_cycle_time = 0.0;  /* cycle time used by the current planner */

/**
 * @brief Initialize the S-curve planner (call at program entry).
 *
 * @param cycle_time  cycle time in seconds
 * @return 0 on success, -1 on failure
 */
int sp_scurve_init(double cycle_time) {
    /* Parameter validation */
    if (cycle_time <= 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "sp_scurve_init: invalid cycle_time=%f\n", cycle_time);
        return -1;
    }

    /* If planner already exists with the same cycle time, nothing to do */
    if (cached_planner != NULL && fabs(cached_cycle_time - cycle_time) < 1e-12) {
        return 0;
    }

    /* If planner exists but cycle time changed, destroy the old one first */
    if (cached_planner != NULL) {
        rtapi_print_msg(RTAPI_MSG_INFO, "sp_scurve_init: cycle time changed from %f to %f, recreating planner\n",
                        cached_cycle_time, cycle_time);
        ruckig_destroy(cached_planner);
        cached_planner = NULL;
    }

    /* Create new planner */
    cached_planner = ruckig_create(cycle_time);
    if (cached_planner == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "sp_scurve_init: ruckig_create() failed with cycle_time=%f\n", cycle_time);
        return -1;
    }

    /* Disable log output (used for velocity planning — avoids unnecessary warnings) */
    ruckig_set_logging(cached_planner, 0);

    cached_cycle_time = cycle_time;
    rtapi_print_msg(RTAPI_MSG_INFO, "sp_scurve_init: planner created with cycle_time=%f (logging disabled)\n", cycle_time);
    return 0;
}

/**
 * @brief Clean up the S-curve planner (call at program exit).
 */
void sp_scurve_cleanup(void) {
    if (cached_planner != NULL) {
        ruckig_destroy(cached_planner);
        cached_planner = NULL;
        cached_cycle_time = 0.0;
    }
}

/**
 * @brief Get the cached Ruckig planner.
 *
 * Note: sp_scurve_init() must be called before using this.
 *
 * @return RuckigPlanner handle, or NULL if not initialized
 */
static RuckigPlanner get_cached_planner(void) {
    /* If planner is not initialized, return NULL.
     * Callers should check the return value and handle the error. */
    return cached_planner;
}

/* ================================================================
 * Ruckig-based S-curve functions
 * ================================================================ */

/**
 * @brief Compute the S-curve peak velocity from rest to end-speed
 *        (using Ruckig planning).
 *
 * Given total distance and end velocity, plan a complete trajectory
 * from (0, 0, 0) to (distance, Ve, 0), then read the peak velocity
 * directly from the profile — no iteration required.
 *
 * @param distance  total distance
 * @param Ve        end velocity
 * @param maxA      maximum acceleration
 * @param maxJ      maximum jerk
 * @param req_v     [out] computed peak velocity
 * @return          1 on success, -1 on failure
 */
int findSCurveVSpeedWithEndSpeed(double distance, double Ve,
                                  double maxA, double maxJ, double* req_v) {
    /* Parameter validation */
    if (distance <= 0 || maxA <= 0 || maxJ <= 0) {
        *req_v = fabs(Ve);
        return -1;
    }

    /* When Ve is approximately zero, use the symmetric function */
    if (fabs(Ve) <= TP_VEL_EPSILON) {
        return findSCurveVSpeed(distance, maxA, maxJ, req_v);
    }

    /* Use the cached planner */
    RuckigPlanner planner = get_cached_planner();
    if (!planner) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeedWithEndSpeed: planner not initialized, call sp_scurve_init() first\n");
        *req_v = fabs(Ve);
        return -1;
    }

    /* Reset planner state */
    ruckig_reset(planner);

    /* Plan a complete trajectory from (0, 0, 0) to (distance, Ve, 0).
     * Ruckig will automatically find the peak velocity that satisfies
     * the distance and end-velocity constraints. */
    int result = ruckig_plan_position(planner,
                                      0.0,            /* start position */
                                      0.0,            /* start velocity */
                                      0.0,            /* start acceleration */
                                      distance,       /* target position */
                                      Ve,             /* target velocity */
                                      0.0,            /* target acceleration */
                                      0.0,            /* min velocity (unidirectional) */
                                      sqrt(maxA * distance + Ve * Ve) * 2.0,  /* max velocity (conservative, ensures no limiting) */
                                      maxA,           /* max acceleration */
                                      maxJ);          /* max jerk */

    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeedWithEndSpeed: ruckig_plan_position failed (result=%d)\n", result);
        *req_v = fabs(Ve);
        return -1;
    }

    /* Read the peak velocity directly from the profile */
    double peak_vel = 0.0;
    result = ruckig_get_peak_velocity(planner, &peak_vel);
    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeedWithEndSpeed: ruckig_get_peak_velocity failed\n");
        *req_v = fabs(Ve);
        return -1;
    }

    *req_v = peak_vel;
    return 1;
}

/**
 * @brief Compute the maximum start speed that can decelerate to Ve within
 *        a given distance (jerk-constrained).
 *
 * Find the largest Vs such that a trajectory exists from (0, Vs, 0) to
 * (distance, Ve, 0) under (maxA, maxJ) constraints.
 *
 * Method: use the constant-acceleration upper bound
 *   Vs_estimate = sqrt(Ve^2 + 2*maxA*distance)
 * as an initial guess and pass it to Ruckig.  If planning succeeds,
 * Vs_estimate is feasible.  If it fails, the jerk constraint requires
 * more distance — return a guaranteed-feasible upper bound instead.
 *
 * On failure, instead of returning 0.9*Vs_estimate (which may still
 * exceed the jerk-feasible value), return the 0->0 S-curve peak for
 * the same distance.  That value is always jerk-feasible and prevents
 * downstream planning failures.  On success the same peak is used as
 * an upper-bound clamp.
 *
 * @param distance  total distance
 * @param Ve        end velocity
 * @param maxA      maximum acceleration
 * @param maxJ      maximum jerk
 * @param req_v     [out] computed maximum start speed
 * @return          1 on success, -1 on failure
 */
int findSCurveMaxStartSpeed(double distance, double Ve,
                            double maxA, double maxJ, double* req_v) {
    if (distance <= 0 || maxA <= 0 || maxJ <= 0) {
        *req_v = fabs(Ve);
        return -1;
    }

    if (fabs(Ve) <= TP_VEL_EPSILON) {
        return findSCurveVSpeed(distance, maxA, maxJ, req_v);
    }

    /* 0->0 S-curve peak for this distance — reliable jerk-constrained upper bound,
     * used as fallback on failure and as a clamp on success. */
    double v_0_to_0_peak = 0.0;
    if (findSCurveVSpeed(distance, maxA, maxJ, &v_0_to_0_peak) != 1) {
        /* findSCurveVSpeed failed: use triangular upper bound to avoid unbounded result */
        v_0_to_0_peak = sqrt(maxA * distance);
    }

    RuckigPlanner planner = get_cached_planner();
    if (!planner) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveMaxStartSpeed: planner not initialized, call sp_scurve_init() first\n");
        *req_v = fmin(fabs(Ve) * 2.0, v_0_to_0_peak);
        return -1;
    }

    ruckig_reset(planner);

    double Vs_estimate = sqrt(Ve * Ve + 2.0 * maxA * distance);
    if (Vs_estimate < fabs(Ve)) {
        Vs_estimate = fabs(Ve) * 2.0;
    }

    int result = ruckig_plan_position(planner,
                                      0.0,
                                      Vs_estimate,
                                      0.0,
                                      distance,
                                      Ve,
                                      0.0,
                                      0.0,
                                      Vs_estimate * 2.0,
                                      maxA,
                                      maxJ);

    if (result == 0) {
        double duration = ruckig_get_duration(planner);
        if (duration > 0.0) {
            double actual_pos, actual_vel, actual_acc, actual_jerk;
            int query_result = ruckig_at_time(planner, duration,
                                             &actual_pos, &actual_vel,
                                             &actual_acc, &actual_jerk);
            if (query_result == 0) {
                double pos_error = fabs(actual_pos - distance);
                if (pos_error < 1e-6) {
                    double start_vel = 0.0;
                    if (ruckig_get_start_velocity(planner, &start_vel) == 0) {
                        *req_v = fmin(start_vel, v_0_to_0_peak);
                        return 1;
                    }
                }
            }
        }
        *req_v = fmin(Vs_estimate, v_0_to_0_peak);
        return 1;
    }

    /* Planning failed: jerk constraint makes Vs_estimate infeasible.
     * Return the guaranteed-feasible 0->0 peak to avoid downstream failures. */
    *req_v = fmax(fabs(Ve), v_0_to_0_peak);
    return 1;
}

/**
 * @brief Compute the rest-to-rest S-curve peak velocity (using Ruckig planning).
 *
 * Given a total distance, plan a complete trajectory from (0, 0, 0) to
 * (distance, 0, 0), then read the peak velocity directly from the
 * profile — no iteration required.
 *
 * @param distence  total distance (rest to rest)
 * @param maxA      maximum acceleration
 * @param maxJ      maximum jerk
 * @param req_v     [out] computed peak velocity
 * @return          1 on success, -1 on failure
 */
int findSCurveVSpeed(double distence, double maxA, double maxJ, double* req_v){
    /* Parameter validation */
    if (distence <= 0 || maxA <= 0 || maxJ <= 0) {
        *req_v = 0.0;
        return -1;
    }

    /* Use the cached planner */
    RuckigPlanner planner = get_cached_planner();
    if (!planner) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeed: planner not initialized, call sp_scurve_init() first\n");
        *req_v = 0.0;
        return -1;
    }

    /* Reset planner state */
    ruckig_reset(planner);

    /* Plan a complete trajectory from (0, 0, 0) to (distance, 0, 0) */
    int result = ruckig_plan_position(planner,
                                      0.0,            /* start position */
                                      0.0,            /* start velocity */
                                      0.0,            /* start acceleration */
                                      distence,       /* target position */
                                      0.0,            /* target velocity */
                                      0.0,            /* target acceleration */
                                      0.0,            /* min velocity (unidirectional) */
                                      sqrt(maxA * distence) * 2.0,  /* max velocity (conservative, ensures no limiting) */
                                      maxA,           /* max acceleration */
                                      maxJ);          /* max jerk */

    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeed: ruckig_plan_position failed (result=%d)\n", result);
        *req_v = 0.0;
        return -1;
    }

    /* Read the peak velocity directly from the profile */
    double peak_vel = 0.0;
    result = ruckig_get_peak_velocity(planner, &peak_vel);
    if (result != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "findSCurveVSpeed: ruckig_get_peak_velocity failed\n");
        *req_v = 0.0;
        return -1;
    }

    *req_v = peak_vel;
    return 1;
}

/**
 * @brief Compute S-curve deceleration time parameters using analytical formulas
 *        (real-time optimized version).
 *
 * S-curve deceleration consists of three phases:
 *   T1: jerk ramp-up phase (j = -jerk), acceleration goes from 0 to -amax
 *   T2: constant deceleration phase (j = 0), acceleration stays at -amax
 *   T1: jerk ramp-down phase (j = +jerk), acceleration goes from -amax to 0
 *
 * ========== Velocity-time curve ==========
 *
 *   velocity v
 *     ^
 *   V |------\
 *     |       \
 *     |        \____
 *     |             \
 *     |              \
 *     +---------------\----> time t
 *     0    T1   T1+T2  2T1+T2
 *
 * ========== Analytical formula derivation ==========
 *
 * For S-curve deceleration:
 * - T1 = amax / jerk  (time for acceleration to go from 0 to -amax)
 * - Phase 1 velocity loss: dv1 = 0.5 * jerk * T1^2 = 0.5 * amax^2 / jerk
 * - Phase 3 velocity loss: dv3 = 0.5 * jerk * T1^2 = 0.5 * amax^2 / jerk  (same as phase 1)
 * - Phase 2 velocity loss: dv2 = amax * T2
 * - Total velocity loss: v = dv1 + dv2 + dv3 = amax^2 / jerk + amax * T2
 * - Therefore: T2 = (v - amax^2 / jerk) / amax
 *
 * Special case (triangular profile):
 * - If v < amax^2 / jerk, the velocity is too small for a full S-curve
 *   (no constant deceleration phase)
 * - For triangular profile: v = jerk * T1^2, so T1 = sqrt(v / jerk), T2 = 0
 *
 * ========== Optimization notes ==========
 *
 * This function uses analytical formulas for direct computation, avoiding
 * frequent trajectory planning — suitable for real-time system calls.
 * Compared to using Ruckig, performance is significantly better and results
 * are fully consistent.
 *
 * @param v     initial velocity (absolute value is taken)
 * @param amax  maximum acceleration
 * @param jerk  maximum jerk
 * @param t1    [out, optional] jerk phase time T1
 * @param t2    [out, optional] constant deceleration phase time T2
 * @return      total deceleration time = 2*T1 + T2
 */
double calcDecelerateTimes(double v, double amax, double jerk, double* t1, double* t2){
    v = fabs(v);

    /* Parameter validation */
    if (v < TP_VEL_EPSILON) {
        if (t1 != NULL) *t1 = 0.0;
        if (t2 != NULL) *t2 = 0.0;
        return 0.0;
    }

    if (amax <= 0.0 || jerk <= 0.0) {
        if (t1 != NULL) *t1 = 0.0;
        if (t2 != NULL) *t2 = 0.0;
        return 0.0;
    }

    /* Compute T1 (jerk phase time) */
    double T1 = amax / jerk;

    /* Total velocity loss from phase 1 and phase 3:
     * dv1 + dv3 = 2 * (0.5 * amax^2 / jerk) = amax^2 / jerk */
    double v_loss_jerk_phases = amax * amax / jerk;

    double T2 = 0.0;

    /* Determine whether this is a full S-curve or a triangular profile */
    if (v >= v_loss_jerk_phases) {
        /* Full S-curve: constant deceleration phase exists */
        T2 = (v - v_loss_jerk_phases) / amax;
        if (T2 < 0.0) {
            T2 = 0.0;  /* guard against numerical error */
        }
    } else {
        /* Triangular profile: no constant deceleration phase, recompute T1.
         * v = jerk * T1^2, so T1 = sqrt(v / jerk) */
        T1 = sqrt(v / jerk);
        T2 = 0.0;
    }

    /* Output results */
    if (t1 != NULL) *t1 = T1;
    if (t2 != NULL) *t2 = T2;

    /* Total time: 2*T1 + T2
     * (T1 to ramp accel to -amax, T2 at constant -amax, T1 to ramp back to 0) */
    return T1 * 2.0 + T2;
}

/**
 * @brief Compute the maximum speed reachable from rest in time T using
 *        an S-curve profile (via Ruckig planning).
 *
 * Given maximum acceleration amax, maximum jerk, and time T, compute the
 * maximum velocity achievable from rest using an S-curve acceleration
 * profile within time T.
 *
 * Algorithm: use Ruckig position-control mode to plan toward a sufficiently
 * large target position (ensuring the target is not reached within time T),
 * then sample the velocity at time T.
 *
 * @param amax  maximum acceleration
 * @param jerk  maximum jerk
 * @param T     time in seconds
 * @return      maximum velocity at time T, or 0.0 on failure
 */
double calcSCurveSpeedWithT(double amax, double jerk, double T) {
    /* Parameter validation */
    if (amax <= 0.0 || jerk <= 0.0 || T <= 0.0) {
        return 0.0;
    }

    /* Use the cached planner */
    RuckigPlanner planner = get_cached_planner();
    if (!planner) {
        rtapi_print_msg(RTAPI_MSG_ERR, "calcSCurveSpeedWithT: planner not initialized, call sp_scurve_init() first\n");
        return 0.0;
    }

    /* Reset planner state */
    ruckig_reset(planner);

    /* Estimate a target position large enough that the trajectory will not
     * reach it within time T.  Use the trapezoidal formula as a conservative
     * estimate: s = 0.5 * amax * T^2.  Double it for safety. */
    double target_pos = 0.5 * amax * T * T * 2.0;

    /* Set a max velocity large enough to not be the limiting factor */
    double max_vel = amax * T * 2.0;  /* conservative estimate */

    int result = ruckig_plan_position(planner,
                                      0.0,        /* start position */
                                      0.0,        /* start velocity */
                                      0.0,        /* start acceleration */
                                      target_pos, /* target position (large enough) */
                                      max_vel,    /* target velocity (large, not limiting) */
                                      0.0,        /* target acceleration */
                                      0.0,        /* min velocity (unidirectional) */
                                      max_vel * 2.0,  /* max velocity (ensures no limiting) */
                                      amax,       /* max acceleration */
                                      jerk);      /* max jerk */

    if (result != 0) {
        /* Planning failed — use conservative fallback estimate.
         * For an S-curve the velocity upper bound at time T is amax*T
         * (trapezoidal), but the S-curve value is smaller. */
        return fmin(amax * T, sqrt(amax * amax * T / jerk));
    }

    /* Sample velocity at time T */
    double pos, vel, acc, jerk_val;
    result = ruckig_at_time(planner, T, &pos, &vel, &acc, &jerk_val);
    if (result != 0) {
        /* Sampling failed — use conservative fallback */
        return fmin(amax * T, sqrt(amax * amax * T / jerk));
    }

    return vel;
}

/* ================================================================
 * Legacy functions kept for simple_tp.c compatibility
 * ================================================================ */

/* PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
 * VT = V0 + A0 * T + J * T^2 / 2
 * AT = A0 + J * T
 */

double nextAccel(double t, double targetV, double v, double a, double maxA,
                        double maxJ) {
  double max_da, tiny_da, vel_err, acc_req;
  max_da = delta_accel(t, maxJ);
  tiny_da = max_da * t * 0.001;
  vel_err = targetV - v;
  if (vel_err > tiny_da){
    acc_req = -max_da +
              sqrt(2.0 * maxJ * vel_err + max_da * max_da);
  }else if (vel_err < -tiny_da){
    acc_req = max_da -
              sqrt(-2.0 * maxJ * vel_err + max_da * max_da);
  }else{
    /* within 'tiny_da' of desired velocity, no need to move */
    acc_req = 0.0;
  }
  /* limit acceleration request */
  if (acc_req > maxA){
    acc_req = maxA;
  }else if (acc_req < -maxA){
    acc_req = -maxA;
  }
  /* ramp acceleration toward request at jerk limit */
  if (acc_req > a + max_da){
    return a + max_da;
  }else if (acc_req < a - max_da){
    return a - max_da;
  }else{
    return acc_req;
  }
}

/* PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
 * VT = V0 + A0 * T + J * T^2 / 2
 * AT = A0 + J * T
 */
double nextSpeed(double v, double a, double t, double targetV, double maxA, double maxJ, double* req_v, double* req_a, double* req_j) {
  /* Compute next acceleration */
  double nextA = nextAccel(t, targetV, v, a, maxA, maxJ);

  /* Compute next velocity using trapezoidal rule:
   * VT - V0 = (A0 + AT) * T / 2 */
  double deltaV = (a + nextA) * t / 2.0;
  if ((deltaV < 0 && targetV < v && v + deltaV < targetV) ||
      (0 < deltaV && v < targetV && targetV < v + deltaV)) {
    /* Would overshoot target velocity — clamp */
    nextA = 2.0 * (targetV - v) / t - a;
    if(nextA >= maxA){
      nextA = maxA;
      targetV = (a + nextA) * t / 2.0;
    }
    v = targetV;
  } else {
    v += deltaV;
  }

  /* Compute jerk = delta accel / time */
  *req_j = (nextA - a) / t;
  if(*req_j > maxJ){
    *req_j = maxJ;
    nextA = a + maxJ * t;
  } else if (*req_j < -maxJ) {
    *req_j = -maxJ;
    nextA = a - maxJ * t;
  }
  *req_a = nextA;
  *req_v = v;

  return v;
}

double stoppingDist(double v, double a, double maxA, double maxJ) {
    /* Already stopped */
    if (fabs(v) < 0.0001) return 0;
    /* Handle negative velocity */
    if (v < 0) {
    v = -v;
    a = -a;
    }

    double d = 0;

    /* Compute distance and velocity change to bring acceleration to 0 */
    if (0 < a) {
      double t = a / maxJ;
      d += sc_distance(t, v, a, -maxJ);
      v += delta_velocity(t, a, -maxJ);
      a = 0;
    }

    /* Compute maximum deceleration.
     *
     * At target velocity, both velocity and acceleration are 0.
     * VT = 0 + 0*T1 + J*T1^2/2, and because Amax = J*T1:
     *   VT = Amax^2 / (2*J)
     * From the other side:  VT = v + (a + Amax)*T2/2
     * Combining: Amax^2 = v*J + 0.5*a*a
     */
    double maxDeccel = -sqrt(v * maxJ + 0.5 * a * a);
    if (maxDeccel < -maxA) maxDeccel = -maxA;

    /* Compute distance and velocity change to reach max deceleration */
    if (maxDeccel < a) {
        double t = (a - maxDeccel) / maxJ;
        d += sc_distance(t, v, a, -maxJ);
        v += delta_velocity(t, a, -maxJ);
        a = maxDeccel;
    }

    /* Velocity remaining when entering final jerk phase:
     * VT = Amax^2 / (2*J) */
    double deltaV = 0.5 * a * a / maxJ;

    /* Constant deceleration phase (if needed) */
    if (deltaV < v) {
        double t = (v - deltaV) / -a;
        d += sc_distance(t, v, a, 0);
        v += delta_velocity(t, a, 0);
    }

    /* Distance to zero velocity (final jerk phase) */
    d += sc_distance(-a / maxJ, v, a, maxJ);

    return d;
}

/* S-curve displacement: P = v*t + (1/2)*a*t^2 + (1/6)*j*t^3 */
double sc_distance(double t, double v, double a, double j) {
  return t * (v + t * (0.5 * a + 1.0 / 6.0 * j * t));
}

/* Velocity change: dV = a*t + (1/2)*j*t^2 */
double delta_velocity(double t, double a, double j) {
  return t * (a + 0.5 * j * t);
}

/* Acceleration change: dA = j*t */
double delta_accel(double t, double j) {return j * t;}
