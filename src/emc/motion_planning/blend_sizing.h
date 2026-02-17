/********************************************************************
 * Description: blend_sizing.h
 *   Blend sizing and infrastructure for 9D Bezier blend curves.
 *
 *   Ported from Tormach PathPilot's biarc blend sizing logic,
 *   adapted to produce a single Bezier9 blend curve instead of
 *   two biarc segments.
 *
 * Author: aitekcnc
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef BLEND_SIZING_H
#define BLEND_SIZING_H

#include "posemath.h"
#include "emcpos.h"
#include "tc_types.h"
#include "bezier9.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Blend sizing constants */
#define BLEND9_TOLERANCE_RATIO      0.5
#define BLEND9_ALPHA_FACTOR         0.30
#define BLEND9_MAX_ITERATIONS       12
#define BLEND9_VEL_REL_TOL         0.01
#define BLEND9_VEL_ABS_TOL         0.1
#define BLEND9_CIRC_MAX_ANGLE      (PM_PI / 3.0)
#define BLEND9_MIN_THETA           (PM_PI / 180.0)

/**
 * Per-axis velocity and acceleration bounds in 9D.
 * Each PmCartesian holds bounds for 3 axes of a subspace.
 * Zero bounds mean the axis is not active (no constraint applied).
 */
typedef struct {
    PmCartesian xyz;                    /* Per-axis bounds for X, Y, Z */
    PmCartesian abc;                    /* Per-axis bounds for A, B, C */
    PmCartesian uvw;                    /* Per-axis bounds for U, V, W */
} AxisBounds9;

/**
 * Blend boundary conditions sampled from two adjacent segments.
 * Contains 9D positions and tangent vectors at the blend region edges,
 * plus the xyz intersection (corner) point for tolerance checking.
 */
typedef struct {
    EmcPose P_start;                    /* 9D position at blend start (on prev_tc) */
    EmcPose P_end;                      /* 9D position at blend end (on tc) */

    PmCartesian u_start_xyz;            /* Unit tangent at start (xyz) */
    PmCartesian u_end_xyz;              /* Unit tangent at end (xyz) */
    PmCartesian u_start_abc;            /* Unit tangent at start (abc) */
    PmCartesian u_end_abc;              /* Unit tangent at end (abc) */
    PmCartesian u_start_uvw;            /* Unit tangent at start (uvw) */
    PmCartesian u_end_uvw;              /* Unit tangent at end (uvw) */

    PmCartesian intersection_point;     /* XYZ corner point */

    double s_prev;                      /* Progress on prev_tc at blend start */
    double s_tc;                        /* Progress on tc at blend end */
} BlendBoundary9;

/**
 * Kinematic and geometric parameters for blend optimization.
 */
typedef struct {
    double theta;                       /* Half-angle between tangent vectors */
    double a_n_max;                     /* Normal acceleration budget */
    double v_goal;                      /* Target velocity at max feed override */
    double R_goal;                      /* Goal radius = v_goal^2 / a_n_max */
    double v_plan;                      /* Planned velocity after constraints */
} BlendParams9;

/**
 * Blend optimization status codes.
 */
typedef enum {
    BLEND9_OK = 0,
    BLEND9_FAIL = -1,
    BLEND9_ANGLE_TOO_SMALL = -2,
    BLEND9_TOLERANCE_EXCEEDED = -3,
    BLEND9_ZERO_LENGTH = -4,
    BLEND9_COLLINEAR = -5
} blend9_status_t;

/**
 * Complete blend optimization result.
 * Contains everything needed to create the blend segment and trim neighbors.
 */
typedef struct {
    double Rb;                          /* Final blend region size */
    Bezier9 bezier;                     /* Constructed blend curve */
    BlendParams9 params;                /* Computed kinematic parameters */
    BlendBoundary9 boundary;            /* Boundary conditions used */
    double trim_prev;                   /* Amount to trim from prev_tc end */
    double trim_tc;                     /* Amount to trim from tc start */
    blend9_status_t status;             /* Result status */
} BlendSolution9;

/**
 * Compute blend tolerance from G64 P values.
 * Caps at half of each segment's nominal length (tolerance_ratio = 0.5).
 */
int tcFindBlendTolerance9(TC_STRUCT const * const prev_tc,
                          TC_STRUCT const * const tc,
                          double * const tolerance);

/**
 * Compute maximum blend region size (upper bound for optimizer).
 * For circular arcs, limits to 60 degrees of arc.
 */
double findMaxBlendRegion9(TC_STRUCT const * const prev_tc,
                           TC_STRUCT const * const tc);

/**
 * Sample 9D positions and tangent vectors at blend boundary points.
 * Given Rb (blend region size), finds where the blend starts on prev_tc
 * and ends on tc, then samples positions and tangent vectors there.
 */
int findBlendPointsAndTangents9(double Rb,
                                TC_STRUCT const * const prev_tc,
                                TC_STRUCT const * const tc,
                                BlendBoundary9 * const boundary);

/**
 * Compute kinematic parameters from boundary conditions.
 * Finds intersection angle, normal acceleration limit, velocity goal,
 * and target radius. Per-axis bounds constrain velocity and acceleration
 * across all 9 dimensions in the blend plane.
 */
int findBlendParameters9(BlendBoundary9 const * const boundary,
                         TC_STRUCT const * const prev_tc,
                         TC_STRUCT const * const tc,
                         double max_feed_scale,
                         AxisBounds9 const * const vel_bounds,
                         AxisBounds9 const * const acc_bounds,
                         BlendParams9 * const params);

/**
 * Binary search optimizer for blend region size.
 * Finds optimal Rb that satisfies both G64 P tolerance and velocity
 * constraints. Per-axis bounds limit blend velocity and acceleration
 * to stay within each axis's individual limits.
 */
int optimizeBlendSize9(TC_STRUCT const * const prev_tc,
                       TC_STRUCT const * const tc,
                       double tolerance,
                       double max_feed_scale,
                       AxisBounds9 const * const vel_bounds,
                       AxisBounds9 const * const acc_bounds,
                       BlendSolution9 * const result);

/**
 * Create a TC_STRUCT for the blend segment from optimization result.
 * Sets motion_type = TC_BEZIER, kinematic limits from Bezier curvature,
 * and term_cond = TC_TERM_COND_TANGENT.
 */
int createBlendSegment9(TC_STRUCT const * const prev_tc,
                        TC_STRUCT const * const tc,
                        BlendSolution9 const * const solution,
                        TC_STRUCT * const blend_tc,
                        double cycle_time);

/**
 * Trim a TC_STRUCT by removing length from its end or start.
 * Dispatches by motion type to handle xyz, abc, uvw components.
 * @param from_end  1 = keep end, trim start; 0 = keep start, trim end
 */
int trimSegment9(TC_STRUCT * const tc,
                 double trim_amount,
                 int from_end);

#ifdef __cplusplus
}
#endif

#endif /* BLEND_SIZING_H */
