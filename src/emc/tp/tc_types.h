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
#ifndef TC_TYPES_H
#define TC_TYPES_H

#include "spherical_arc9.h"
#include "posemath.h"
#include "emcpos.h"
#include "emcmotcfg.h"
#include "state_tag.h"
#include "motion_types.h"
#include "pm_circle9.h"
#include "pm_line9.h"

typedef unsigned long long tc_unique_id_t;

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
    TC_DWELL = 5
} tc_motion_type_t;

typedef enum {
    TC_SYNC_NONE = 0,
    TC_SYNC_VELOCITY,
    TC_SYNC_POSITION
} tc_spindle_sync_t;

typedef enum {
    TC_DIR_FORWARD = 0,
    TC_DIR_REVERSE
} tc_direction_t;

typedef enum {
    TC_OPTIM_UNTOUCHED,
    TC_OPTIM_AT_MAX,
} TCOptimizationLevel;

typedef enum {
    TC_ACCEL_TRAPZ,
    TC_ACCEL_RAMP,
} TCAccelMode;

/* structure for individual trajectory elements */

typedef struct {
    PmVector dwell_pos;
    double dwell_time_req; // Pre-calculated, fixed dwell time
    double delta_rpm_req; // Use to compute a dwell based on the spindle acceleration at run-time
    double dwell_time; // actual dwell time (determined at run-time when dwell starts)
    double remaining_time;
} TPDwell;

typedef unsigned long long iomask_t; // 64 bits on both x86 and x86_64

typedef struct {
    char anychanged;
    iomask_t dio_mask;
    iomask_t aio_mask;
    signed char dios[EMCMOT_MAX_DIO];
    double aios[EMCMOT_MAX_AIO];
} syncdio_t;


typedef struct {
    double position; //!< Reference position for displacement calculations
    double direction; //!< Direction of "positive" spindle motion (as -1, 1, or 0)
} spindle_origin_t;

typedef struct {
    PmCartLine nominal_xyz;     // nominal tapping motion from start to end)
    PmCartLine actual_xyz;         // this will be generated on the fly, for the actual rigid tapping motion
    PmCartesian abc;
    PmCartesian uvw;
    double reversal_target;
    double reversal_scale;
    double tap_uu_per_rev;
    double retract_uu_per_rev;
    rigid_tap_state_t state;
} PmRigidTap;

typedef enum {
    INDEX_NONE=-1,
    INDEX_X_AXIS=0,
    INDEX_Y_AXIS,
    INDEX_Z_AXIS,
    INDEX_A_AXIS,
    INDEX_B_AXIS,
    INDEX_C_AXIS,
    INDEX_U_AXIS,
    INDEX_V_AXIS,
    INDEX_W_AXIS,
} IndexRotaryAxis;

typedef struct {
    double cycle_time;
    //Position stuff
    double target;          // actual segment length
    double progress;        // where are we in the segment?  0..target
    double nominal_length;

    //Velocity
    double reqvel;          // vel requested by F word, calc'd by task
    double target_vel;      // velocity to actually track, limited by other factors
    double maxvel_geom;     // max possible vel from segment geometry (feed override stops here)
    double currentvel;      // keep track of current step (vel * cycle_time)
    double finalvel;        // velocity to aim for at end of segment
    double term_vel;        // actual velocity at termination of segment
    int use_kink;
    double kink_vel;        // Temporary way to store our calculation of maximum velocity we can handle if this segment is declared tangent with the next
    double kink_accel_reduce_prev; // How much to reduce the allowed tangential acceleration to account for the extra acceleration at an approximate tangent intersection.
    double kink_accel_reduce; // How much to reduce the allowed tangential acceleration to account for the extra acceleration at an approximate tangent intersection.
    double parabolic_equiv_vel; // Estimated equivalent velocity for a parabolic blend with the next segment

    double v_limit_linear_ratio;
    double v_limit_angular_ratio;

    //Acceleration
    double maxaccel;        // accel calc'd by task
    double acc_ratio_tan;// ratio between normal and tangential accel
    double acc_normal_max;         // Max acceleration allowed in normal direction (worst-case for the whole curve)
    
    int id;                 // segment's serial number
    tc_unique_id_t unique_id;  //!< "Unique" identifier for a motion command that's not related to source line
    struct state_tag_t tag; /* state tag corresponding to running motion */

    union {                 // describes the segment's start and end positions
        PmLine9 line;
        PmCircle9 circle;
        PmRigidTap rigidtap;
        SphericalArc9 arc;
        TPDwell dwell;
    } coords;

    tc_motion_type_t motion_type;       // TC_LINEAR (coords.line) or
                            // TC_CIRCULAR (coords.circle) or
                            // TC_RIGIDTAP (coords.rigidtap)
    int active;            // this motion is being executed
    int canon_motion_type;  // this motion is due to which canon function?
    tc_term_cond_t term_cond;          // gcode requests continuous feed at the end of
                            // this segment (g64 mode)

    double tolerance;       // during the blend at the end of this move,
                            // stay within this distance from the path.
    tc_spindle_sync_t synchronized;       // spindle sync state
    double uu_per_rev;      // for sync, user units per rev (e.g. 0.0625 for 16tpi)
    unsigned char enables;  // Feed scale, etc, enable bits for this move
    int atspeed;           // wait for the spindle to be at-speed before starting this move
    syncdio_t syncdio;      // synched DIO's for this move. what to turn on/off
    IndexRotaryAxis indexrotary;        // which rotary axis to unlock to make this move, -1 for none
    TCOptimizationLevel optimization_state;             // At peak velocity during blends)
    int on_final_decel;
    int accel_mode;
    int splitting;          // the segment is less than 1 cycle time
                            // away from the end.
    int remove;             // Flag to remove the segment from the queue
    int active_depth;       /* Active depth (i.e. how many segments
                            * after this will it take to slow to zero
                            * speed) */
    int finalized;

} TC_STRUCT;

#endif				/* TC_TYPES_H */
