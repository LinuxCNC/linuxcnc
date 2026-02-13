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
********************************************************************/
#ifndef TC_TYPES_H
#define TC_TYPES_H

#include "spherical_arc.h"
#include "posemath.h"
#include "emcpos.h"
#include "emcmotcfg.h"
#include "state_tag.h"

#define BLEND_DIST_FRACTION 0.5
/* values for endFlag */
typedef enum {
    TC_TERM_COND_STOP = 0,
    TC_TERM_COND_EXACT = 1,
    TC_TERM_COND_PARABOLIC = 2,
    TC_TERM_COND_TANGENT = 3
} tc_term_cond_t;

typedef enum {
    TC_LINEAR = 1,
    TC_CIRCULAR = 2,
    TC_RIGIDTAP = 3,
    TC_SPHERICAL = 4,
    TC_DWELL = 5        // Zero-length dwell segment (G4)
} tc_motion_type_t;

/**
 * Segment action types for inline M-code execution
 *
 * Industrial controllers (Siemens, Fanuc) process M-codes inline with motion.
 * This enum defines action types that can be attached to segments and fired
 * by RT when the segment starts, maintaining proper synchronization without
 * forcing queue drain (which breaks velocity continuity in planner_type 2).
 *
 * Actions fire via tpFireSegmentActions() when segment becomes active.
 */
typedef enum {
    SEG_ACTION_NONE = 0,
    SEG_ACTION_SPINDLE_CW,      // M3: Spindle on clockwise
    SEG_ACTION_SPINDLE_CCW,     // M4: Spindle on counter-clockwise
    SEG_ACTION_SPINDLE_OFF,     // M5: Spindle off
    SEG_ACTION_COOLANT_MIST,    // M7: Mist coolant on
    SEG_ACTION_COOLANT_FLOOD,   // M8: Flood coolant on
    SEG_ACTION_COOLANT_OFF,     // M9: All coolant off
    SEG_ACTION_TOOL_CHANGE,     // M6: Tool change (may need special handling)
    SEG_ACTION_PROGRAM_STOP,    // M0/M1: Program stop
    SEG_ACTION_PROBE_START,     // G38.x: Start probe move
    SEG_ACTION_CUSTOM,          // For future extensibility
    SEG_ACTION_MAX
} segment_action_type_t;

/**
 * Segment action structure
 *
 * Attached to TC_STRUCT to fire actions when segment activates.
 * Multiple actions can be queued by setting multiple flags.
 * RT fires these via HAL pins or direct calls in tpActivateSegment().
 */
typedef struct {
    unsigned int action_mask;       // Bitmask of segment_action_type_t flags
    int spindle_num;                // Which spindle (0-based)
    double spindle_speed;           // RPM for spindle on
    int custom_action_id;           // For SEG_ACTION_CUSTOM
    double custom_value;            // Parameter for custom action
} segment_actions_t;

typedef enum {
    TC_SYNC_NONE = 0,
    TC_SYNC_VELOCITY,
    TC_SYNC_POSITION
} tc_spindle_sync_t;

typedef enum {
    TC_DIR_FORWARD = 0,
    TC_DIR_REVERSE
} tc_direction_t;

#define TC_GET_PROGRESS 0
#define TC_GET_STARTPOINT 1
#define TC_GET_ENDPOINT 2

#define TC_OPTIM_UNTOUCHED 0
#define TC_OPTIM_AT_MAX 1

#define TC_ACCEL_TRAPZ 0
#define TC_ACCEL_RAMP 1

/**
 * Spiral arc length approximation by quadratic fit.
 */
typedef struct {
    double b0;                  /* 2nd order coefficient */
    double b1;                  /* 1st order coefficient */
    double total_planar_length; /* total arc length in plane */
    int spiral_in;              /* flag indicating spiral is inward,
                                   rather than outward */
} SpiralArcLengthFit;


/* structure for individual trajectory elements */

typedef struct {
    PmCartLine xyz;
    PmCartLine abc;
    PmCartLine uvw;
} PmLine9;

typedef struct {
    PmCircle xyz;
    PmCartLine abc;
    PmCartLine uvw;
    SpiralArcLengthFit fit;
} PmCircle9;

typedef struct {
    SphericalArc xyz;
    PmCartesian abc;
    PmCartesian uvw;
} Arc9;

/**
 * Joint-space linear segment for userspace kinematics
 * Stores start/end joint positions for linear interpolation.
 * Polynomial coefficients may be added later for smoother trajectories.
 */
#define JOINT_SPACE_MAX_JOINTS 9

typedef struct {
    double start[JOINT_SPACE_MAX_JOINTS];  // Joint positions at segment start
    double end[JOINT_SPACE_MAX_JOINTS];    // Joint positions at segment end
    double vel_limit_start;                 // Max velocity at start (from Jacobian)
    double vel_limit_end;                   // Max velocity at end
    double acc_limit;                       // Max acceleration through segment
    int num_joints;                         // Number of active joints
    int valid;                              // True if joint data is valid
} JointSpaceSegment;

typedef enum {
    RIGIDTAP_START,
    TAPPING, REVERSING, RETRACTION, FINAL_REVERSAL, FINAL_PLACEMENT
} RIGIDTAP_STATE;

typedef unsigned long long iomask_t; // 64 bits on both x86 and x86_64

typedef struct {
    char anychanged;
    iomask_t dio_mask;
    iomask_t aio_mask;
    signed char dios[EMCMOT_MAX_DIO];
    double aios[EMCMOT_MAX_AIO];
} syncdio_t;

typedef struct {
    PmCartLine xyz;             // original, but elongated, move down
    PmCartLine aux_xyz;         // this will be generated on the fly, for the other
                            // two moves: retraction, final placement
    PmCartesian abc;
    PmCartesian uvw;
    double reversal_target;
    double reversal_scale;
    double spindlerevs_at_reversal;
    RIGIDTAP_STATE state;
} PmRigidTap;

/**
 * Planning state for 9D optimizer
 * Tracks optimization status of segments
 */
typedef enum {
    TC_PLAN_UNTOUCHED = 0,    // Segment not yet optimized
    TC_PLAN_OPTIMIZED = 1,    // Velocity limits computed
    TC_PLAN_SMOOTHED = 2,     // Peak smoothing applied
    TC_PLAN_FINALIZED = 3     // Ready for execution
} TCPlanningState;

/**
 * Ruckig trajectory profile for a single segment
 *
 * Stores the 9-phase jerk-limited profile computed by Ruckig.
 * RT layer samples this at each servo cycle using polynomial evaluation.
 *
 * Phases 0-1: Brake pre-trajectory (brings initial state within limits)
 *             Zero-duration when no brake needed.
 * Phases 2-8: Main 7-phase S-curve:
 *             accel_jerk -> accel_const -> accel_jerk -> cruise ->
 *             decel_jerk -> decel_const -> decel_jerk
 *
 * Each phase has constant jerk from Ruckig's native arrays (not derived).
 */
#define RUCKIG_PROFILE_PHASES 9

typedef struct {
    int valid;                              // Profile has been computed
    int locked;                             // DEBUG: 1 = Ruckig Working result (non-monotonic risk)
    volatile int generation;                // Incremented each time profile is rewritten (stopwatch reset detection)
    double duration;                        // Total profile duration (seconds)
    double computed_feed_scale;             // Feed scale when profile was computed
    double computed_vel_limit;              // Velocity limit when profile was computed (for vLimit change detection)
    double computed_vLimit;                 // tp->vLimit when profile was computed (absolute cap, not scaled by feed)
    double computed_desired_fvel;            // Desired final velocity when profile was computed (for convergence detection)
    double t[RUCKIG_PROFILE_PHASES];        // Phase durations
    double t_sum[RUCKIG_PROFILE_PHASES];    // Cumulative times (t_sum[i] = sum of t[0..i])
    double j[RUCKIG_PROFILE_PHASES];        // Jerk for each phase
    double p[RUCKIG_PROFILE_PHASES + 1];    // Position at phase boundaries
    double v[RUCKIG_PROFILE_PHASES + 1];    // Velocity at phase boundaries
    double a[RUCKIG_PROFILE_PHASES + 1];    // Acceleration at phase boundaries
} ruckig_profile_t;

/**
 * Pending branch for feed override replanning
 *
 * Branch/Merge Architecture:
 * - Userspace computes speculative branches when feed override changes
 * - RT takes the branch if it reaches handoff_time before window_end_time
 * - RT sets 'taken' flag when it takes the branch
 * - Userspace merges (branch becomes canonical) when it sees taken=1
 * - If RT passes window_end_time without taking, branch is discarded
 *
 * Invariants:
 * - RT never waits - old plan always valid
 * - Only userspace sets 'valid', only RT sets 'taken'
 * - Single branch slot per segment (simplicity)
 */
typedef struct {
    ruckig_profile_t profile;           // The main trajectory (position control)
    ruckig_profile_t brake_profile;     // Optional brake trajectory (velocity control)
    int has_brake;                      // 1 if brake_profile should execute first
    double brake_end_position;          // Position where brake ends, main begins
    double handoff_time;                // When RT should take the branch (elapsed_time)
    double handoff_position;            // Position at handoff (for merge reconciliation)
    double feed_scale;                  // Feed scale this branch was computed for
    double window_end_time;             // Deadline - if RT past this, branch is stale
    volatile int valid;                 // Userspace sets: branch is ready
    volatile int taken;                 // RT sets: I took this branch
    volatile int brake_done;            // RT sets: brake phase complete, now on main
} pending_branch_t;

/**
 * Shared optimization data for 9D planner
 *
 * This structure holds data that is shared between the userspace
 * planning layer and the RT execution layer using atomic operations.
 *
 * Used only when planner_type == 2 (9D planner)
 */
typedef struct {
    TCPlanningState optimization_state;  // Atomic state flag
    double final_vel;                    // Target exit velocity
    double final_vel_limit;              // Max reachable exit velocity
    double computed_acc;                 // Computed acceleration limit
    double entry_vel;                    // Computed entry velocity
    ruckig_profile_t profile;            // Active profile being sampled by RT

    // Branch/Merge architecture for feed override handling
    pending_branch_t branch;             // Single pending branch slot
    double canonical_feed_scale;         // Feed scale of current canonical plan

    // Sequence counter for torn read detection during profile copy
    // RT increments before copy (odd = in progress), after copy (even = complete)
    // Userspace checks: if odd or changed, retry read
    volatile unsigned int copy_sequence;

    // Achievable feed cascade
    // When segment is too short to achieve requested feed, we apply what's
    // achievable and pass the remainder to the next segment.
    double requested_feed_scale;         // What user requested (may not be achievable)
    double achieved_exit_vel;            // Actual exit velocity we can achieve
    // Phase 4 TODO: With blending, achieved_exit_vel becomes the entry velocity
    // constraint for the next segment, enabling smooth velocity handoff.

    // Metadata for active profile (legacy, may be removed)
    double active_feed_scale;            // Feed scale of currently active profile
} shared_optimization_data_9d_t;

typedef struct {
    double cycle_time;
    //Position stuff
    double target;          // actual segment length
    double progress;        // where are we in the segment?  0..target
    double nominal_length;

    //Velocity
    double reqvel;          // vel requested by F word, calc'd by task
    double target_vel;      // velocity to actually track, limited by other factors
    double maxvel;          // max possible vel (feed override stops here)
    double currentvel;      // keep track of current step (vel * cycle_time)
    double last_move_length;// last move length
    double finalvel;        // velocity to aim for at end of segment
    double term_vel;        // actual velocity at termination of segment
    double kink_vel;        // Temporary way to store our calculation of maximum velocity we can handle if this segment is declared tangent with the next
    double kink_accel_reduce_prev; // How much to reduce the allowed tangential acceleration to account for the extra acceleration at an approximate tangent intersection.
    double kink_accel_reduce; // How much to reduce the allowed tangential acceleration to account for the extra acceleration at an approximate tangent intersection.

    double factor;

    double targetvel;
    double vt;

    //Jerk
    double maxjerk;                // max jerk for S-curve motion
    double blend_maxjerk;          // max jerk during blend (set by look-ahead)
    double currentjerk;            // current jerk for S-curve planning
    double currentacc;             // current acceleration for S-curve planning
    double lastacc;

    //Acceleration
    double maxaccel;        // accel calc'd by task
    double acc_ratio_tan;// ratio between normal and tangential accel

    //S-curve execution state
    double initialvel;      // initial velocity when segment activated
    int accel_phase;        // current phase of S-curve acceleration
    double elapsed_time;    // time elapsed since segment activation

    // Execution state for predictive handoff (owned by RT, atomically readable from userspace)
    double position_base;           // Accumulated offset from profile swaps
    int last_profile_generation;    // Last profile generation seen by RT (stopwatch reset detection)
    volatile int active_segment_id; // Unique ID, atomically updated by RT

    int id;                 // segment's serial number
    struct state_tag_t tag; // state tag corresponding to running motion

    union {                 // describes the segment's start and end positions
        PmLine9 line;
        PmCircle9 circle;
        PmRigidTap rigidtap;
        Arc9 arc;
    } coords;

    // Joint-space segment data (when using userspace kinematics)
    JointSpaceSegment joint_space;

    int motion_type;       // TC_LINEAR (coords.line) or
                            // TC_CIRCULAR (coords.circle) or
                            // TC_RIGIDTAP (coords.rigidtap)
    int active;            // this motion is being executed
    int canon_motion_type;  // this motion is due to which canon function?
    int term_cond;          // gcode requests continuous feed at the end of
                            // this segment (g64 mode)

    int blending_next;      // segment is being blended into following segment
    double blend_vel;       // velocity below which we should start blending
    double tolerance;       // during the blend at the end of this move,
                            // stay within this distance from the path.
    int synchronized;       // spindle sync state
    double uu_per_rev;      // for sync, user units per rev (e.g. 0.0625 for 16tpi)
    double vel_at_blend_start;
    int sync_accel;         // we're accelerating up to sync with the spindle
    unsigned char enables;  // Feed scale, etc, enable bits for this move
    int atspeed;           // wait for the spindle to be at-speed before starting this move
    syncdio_t syncdio;      // synched DIO's for this move. what to turn on/off

    // Inline segment actions (planner_type 2)
    // Fire when segment activates, without forcing queue drain
    segment_actions_t actions;

    // Dwell support (TC_DWELL segments)
    double dwell_time;      // Total dwell duration in seconds
    double dwell_remaining; // Countdown timer (decremented by RT each cycle)
    int indexer_jnum;  // which joint to unlock (for a locking indexer) to make this move, -1 for none
    int optimization_state;             // At peak velocity during blends)
    int on_final_decel;

    // 9D planner shared optimization data (planner_type == 2 only)
    // Shared between userspace planning layer and RT execution layer
    // Access via atomic operations only
    shared_optimization_data_9d_t shared_9d;
    int blend_prev;
    int accel_mode;
    int splitting;          // the segment is less than 1 cycle time
                            // away from the end.
    int remove;             // Flag to remove the segment from the queue
    int active_depth;       /* Active depth (i.e. how many segments
                            * after this will it take to slow to zero
                            * speed) */
    int finalized;

    // Temporary status flags (reset each cycle)
    int is_blending;

    // Ruckig trajectory planner support
    void *ruckig_planner;              // Ruckig planner handle (opaque pointer)
    double ruckig_trajectory_time;     // current trajectory time (seconds from trajectory start)
    int ruckig_planned;                // whether Ruckig planning completed (1=planned, 0=not)
    // Store last planning parameters for detecting parameter changes
    double ruckig_last_maxaccel;       // max acceleration used in last planning
    double ruckig_last_maxjerk;        // max jerk used in last planning
    double ruckig_last_target_vel;     // target velocity used in last planning
    double ruckig_last_final_vel;      // final velocity used in last planning
    double ruckig_last_target_pos;     // target position used in last planning
    int ruckig_last_use_velocity_control;  // control mode used in last planning (1=velocity, 0=position)
    double ruckig_last_req_pos;        // last req_pos value from Ruckig (for velocity control incremental calc)
    double ruckig_last_feed_override;  // feed override value at last planning (for debug and change detection)
} TC_STRUCT;

#endif				/* TC_TYPES_H */
