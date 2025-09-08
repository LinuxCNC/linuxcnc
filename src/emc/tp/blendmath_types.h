#ifndef BLENDMATH_TYPES_H
#define BLENDMATH_TYPES_H

#include "posemath.h"
#include "pm_vector.h"
#include "tc_types.h"

#define BLEND_ACC_RATIO_TANGENTIAL 0.5
#define BLEND_ACC_RATIO_NORMAL (pmSqrt(1.0 - pmSq(BLEND_ACC_RATIO_TANGENTIAL)))
#define BLEND_KINK_FACTOR 0.25

/**
 * Blend "controls" imposed by user settings and the segments themselves.
 * This captures externally imposed limits like max velocity, the amount of
 * each segment to consume, blend tolerance, etc.
 */
typedef struct {
    double v_max_geom1;      /* ini_maxvel for each segment */
    double v_max_geom2;
    double v_req;       /* requsted velocity for the blend arc */
    double v_goal;      /* desired velocity at max feed override */

    double tolerance;   /* Net blend tolerance (min of line 1 and 2) */

    double L1;          /* Available part of line 1 to blend over */
    double L2;          /* Available part of line 2 to blend over */
} BlendControls;

/**
 * Blend arc parameters (abstracted).
 * This structure holds blend arc parameters that have been abstracted from the
 * physical geometry. This data is used to find the maximum radius given the
 * constraints on the blend. By abstracting the parameters from the geometry,
 * the same calculations can be used with any input geometry (lines, arcs, 6 or
 * 9 dimensional lines).
 */
typedef struct {
    /* These fields are considered "output", and may be refactored into a
     * separate structure in the future */

    double theta;       /* Intersection angle, half of angle between -u1 and u2 */
    double phi;         /* supplement of intersection angle, angle between u1 and u2 */
    double a_n_max;     /* max normal acceleration allowed */
    double a_max_planar;       /* max acceleration allowed for blend */

    double R_plan;      /* planned radius for blend arc */
    double d_plan;      /* distance along each line to arc endpoints */

    double v_max_planar; /* maximum possible velocity in blend plane */
    double v_max_altitude;      /* overall maximum possible velocity during blend segment */
    double v_plan;      /* planned max velocity at max feed override */
    double v_actual;    /* velocity at feedscale = 1.0 */
    double s_arc;       /* arc length */
    int is_consumable;        /* Consume the previous segment */
    int consume;        /* Consume the previous segment */
    double line_length;
} BlendParameters;

/**
 * Output geometry in 3D.
 * Stores the three points representing a simple 3D spherical arc.
 */
typedef struct {
    PmVector arc_start;      /* start point for blend arc */
    PmVector arc_end;        /* end point for blend arc */
    PmVector arc_center;     /* center point for blend arc */
    PmVector u_tan;
    tc_motion_type_t motion_type;
} BlendPoints;

// Start by mimicing the current structure for minimal changes
typedef struct {
    double v_f;
    double dt;
} EndCondition;

// BiArc blend structs (solution and intermediate variables)

typedef struct
{
    PmVector P1;
    PmVector u1;
    PmVector P2;
    PmVector u2;
    double s1;
    double s2;
} blend_boundary_t;

typedef struct
{
    PmVector Pb1;
    PmVector Pb2;
    PmVector u_mid;
    PmVector P_mid;
    double d; //!< Length of intermediate blend segment
} biarc_control_points_t;


typedef enum
{
    BIARC_DEGENERATE_SEGMENTS=-3,
    BIARC_FAILED_TO_CONVERGE=-2,
    BIARC_EXCEEDED_MAX_ITERATIONS=-1,
    BIARC_NOT_SOLVED,
    BIARC_REACHED_V_GOAL,
    BIARC_REACHED_TOLERANCE,
    BIARC_CONVERGED,
    BIARC_MIN_LENGTH_LIMITED,
} BiarcSolverResults;

typedef struct
{
    double deviation;
    double radius_rel;
    double radius_abs;
} biarc_solution_errors_t;

typedef struct {
    double Rb; //!< size of blend region (i.e. distance to go along motion segments from intersection to blend boundary points)
    double R_geom; //!< Estimated blend radius based on geometry
    double arc_len_est; //!< Estimated arc length of first blend
    double R_plan; //!< Effective blend "radius" for a max blend velocity, based on both arc length and geometry .
    double T_plan; //!< Blend tolerance (deviation from intersection point) for Rb
    biarc_solution_errors_t err; //!< error terms from tolerance / velocity goals
    blend_boundary_t boundary; //!< Boundary of the blend solution corresponding to Rb
    biarc_control_points_t control_pts; //!< Control points for the intermediate line segments making up the biarc blend
} biarc_solution_t;

typedef struct
{
    biarc_solution_t upper;
    biarc_solution_t lower;
    int bias_count; //!< KLUDGE track number of times the solution has been on the same side of the upper / lower divide
} blend_solver_constraints_t;

typedef struct
{
    blend_solver_constraints_t constraints; //!< blend solver internals (upper / lower bounds on Rb)
    unsigned iterations; //!< solver iterations
    BiarcSolverResults result;
    biarc_solution_t solution;
} biarc_solver_results_t;

typedef struct
{
    PmVector p_diff;
    PmVector u_diff;
    double dot;
} ContinuityCheck;

#endif // BLENDMATH_TYPES_H
