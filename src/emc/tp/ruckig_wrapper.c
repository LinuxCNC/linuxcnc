/********************************************************************
* Description: ruckig_wrapper.c
*   Cruckig (pure C) trajectory planning library wrapper implementation
*
*   This file provides a C wrapper around the Cruckig C library
*   for S-curve trajectory planning in LinuxCNC.
*   Replaces the C++ Ruckig implementation to enable RTAI kernel builds.
*
* License: GPL Version 2
* System: Linux
* Original Author: 杨阳 (mika-net@outlook.com)
* Cruckig port: LinuxCNC contributors
*
* Copyright (c) 2024-2026 All rights reserved.
********************************************************************/

#include "ruckig_wrapper.h"
#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_slab.h>

/* LinuxCNC precision constants (consistent with tp_types.h) */
#ifndef TP_POS_EPSILON
#define TP_POS_EPSILON 1e-12
#endif
#ifndef TP_VEL_EPSILON
#define TP_VEL_EPSILON 1e-8
#endif

/* Cruckig C headers */
#include "cruckig/cruckig.h"

/* Internal implementation struct */
struct RuckigPlannerImpl {
    CRuckig *otg;                   /* cruckig planner instance */
    CRuckigInputParameter *input;   /* input parameters */
    CRuckigTrajectory *trajectory;  /* trajectory result */
    double cycle_time;              /* cycle time */
    int planned;                    /* whether planning has been done */
    double start_time;              /* trajectory start time */
    double target_pos;              /* target position (used for precision correction) */
    double target_vel;              /* target velocity (used for precision correction) */
    double target_acc;              /* target acceleration (used for precision correction) */
    int use_position_control;       /* 1=position control, 0=velocity control */
    double last_actual_acc;         /* previous actual acceleration (for jerk calculation) */
    int is_first_cycle;             /* first cycle after replanning */
    int enable_logging;             /* 1=enabled, 0=disabled */
};

/* Helper macro: conditionally output log based on planner's logging setting */
#define RUCKIG_LOG_IF_ENABLED(planner, level, fmt, ...) \
    do { \
        if (planner) { \
            struct RuckigPlannerImpl *_impl = (struct RuckigPlannerImpl *)planner; \
            if (_impl->enable_logging) { \
                rtapi_print_msg(level, fmt, ##__VA_ARGS__); \
            } \
        } else { \
            rtapi_print_msg(level, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

RuckigPlanner ruckig_create(double cycle_time) {
    if (cycle_time <= 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_create: invalid cycle_time %f\n", cycle_time);
        return NULL;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)rtapi_kmalloc(sizeof(struct RuckigPlannerImpl), RTAPI_GFP_KERNEL);
    if (!impl) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_create: memory allocation failed\n");
        return NULL;
    }

    impl->otg = cruckig_create(1, cycle_time);
    impl->input = cruckig_input_create(1);
    impl->trajectory = cruckig_trajectory_create(1);

    if (!impl->otg || !impl->input || !impl->trajectory) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_create: cruckig allocation failed\n");
        if (impl->otg) cruckig_destroy(impl->otg);
        if (impl->input) cruckig_input_destroy(impl->input);
        if (impl->trajectory) cruckig_trajectory_destroy(impl->trajectory);
        rtapi_kfree(impl);
        return NULL;
    }

    impl->cycle_time = cycle_time;
    impl->planned = 0;
    impl->start_time = 0.0;
    impl->target_pos = 0.0;
    impl->target_vel = 0.0;
    impl->target_acc = 0.0;
    impl->use_position_control = 0;
    impl->last_actual_acc = 0.0;
    impl->is_first_cycle = 0;
    impl->enable_logging = 1;

    return (RuckigPlanner)impl;
}

void ruckig_destroy(RuckigPlanner planner) {
    if (planner) {
        struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;
        if (impl->otg) cruckig_destroy(impl->otg);
        if (impl->input) cruckig_input_destroy(impl->input);
        if (impl->trajectory) cruckig_trajectory_destroy(impl->trajectory);
        rtapi_kfree(impl);
    }
}

/* Helper: copy trajectory state for backup/restore on planning failure.
 * We cannot just memcpy the CRuckigTrajectory because it contains owned pointers.
 * Instead we save/restore the profile data and scalar fields. */
struct TrajectoryBackup {
    CRuckigProfile profile;   /* single-DOF single-section profile copy */
    double duration;
    double cumulative_time;
    double independent_min_duration;
    CRuckigBound position_extremum;
};

static void backup_trajectory(const CRuckigTrajectory *traj, struct TrajectoryBackup *bk) {
    bk->duration = traj->duration;
    if (traj->profiles)
        bk->profile = traj->profiles[0];           /* 1 DOF, 1 section */
    if (traj->cumulative_times)
        bk->cumulative_time = traj->cumulative_times[0];
    if (traj->independent_min_durations)
        bk->independent_min_duration = traj->independent_min_durations[0];
    if (traj->position_extrema)
        bk->position_extremum = traj->position_extrema[0];
}

static void restore_trajectory(CRuckigTrajectory *traj, const struct TrajectoryBackup *bk) {
    traj->duration = bk->duration;
    if (traj->profiles)
        traj->profiles[0] = bk->profile;
    if (traj->cumulative_times)
        traj->cumulative_times[0] = bk->cumulative_time;
    if (traj->independent_min_durations)
        traj->independent_min_durations[0] = bk->independent_min_duration;
    if (traj->position_extrema)
        traj->position_extrema[0] = bk->position_extremum;
}

/* Helper: handle cruckig result codes, return 0 on success, -1 or -2 on failure.
 * On failure with a previous plan, restores the backup. */
static int handle_result(CRuckigResult result, RuckigPlanner planner,
                         const char *func_name,
                         int had_previous_plan,
                         const struct TrajectoryBackup *bk,
                         double bk_target_pos, double bk_target_vel,
                         int bk_use_position_control, double bk_last_actual_acc) {
    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (result == CRuckigWorking || result == CRuckigFinished) {
        if (result == CRuckigFinished) {
            double duration = cruckig_trajectory_get_duration(impl->trajectory);
            if (duration < 0.001) {
                RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO,
                    "%s: already at target (duration=%f)\n", func_name, duration);
            } else {
                RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO,
                    "%s: trajectory finished (duration=%f)\n", func_name, duration);
            }
        }
        return 0;  /* success */
    }

    /* Planning failed: restore previous trajectory if it exists */
    if (had_previous_plan) {
        restore_trajectory(impl->trajectory, bk);
        impl->target_pos = bk_target_pos;
        impl->target_vel = bk_target_vel;
        impl->use_position_control = bk_use_position_control;
        impl->last_actual_acc = bk_last_actual_acc;
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO,
            "%s: planning failed, restored previous trajectory\n", func_name);
    }

    /* Log error */
    switch (result) {
    case CRuckigErrorInvalidInput:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: invalid input parameters\n", func_name);
        break;
    case CRuckigErrorTrajectoryDuration:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: trajectory duration exceeds numerical limits\n", func_name);
        break;
    case CRuckigErrorPositionalLimits:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: positional limits exceeded\n", func_name);
        break;
    case CRuckigErrorZeroLimits:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: zero limits conflict\n", func_name);
        break;
    case CRuckigErrorExecutionTimeCalculation:
        return -2;
    case CRuckigErrorSynchronizationCalculation:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: synchronization calculation error\n", func_name);
        break;
    case CRuckigError:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: general error\n", func_name);
        break;
    default:
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "%s: unknown error result %d\n", func_name, (int)result);
        break;
    }
    return -1;
}

int ruckig_plan_position(RuckigPlanner planner,
                         double current_pos,
                         double current_vel,
                         double current_acc,
                         double target_pos,
                         double target_vel,
                         double target_acc,
                         double min_vel,
                         double max_vel,
                         double max_acc,
                         double max_jerk) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    /* Parameter validation */
    if (max_vel <= 0.0 || max_acc <= 0.0 || max_jerk <= 0.0) {
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "ruckig_plan_position: invalid limits (v=%f, a=%f, j=%f)\n",
            max_vel, max_acc, max_jerk);
        return -1;
    }

    /* Set input parameters (position control mode) */
    impl->input->control_interface = CRuckigPosition;
    impl->input->synchronization = CRuckigSyncTime;

    impl->input->current_position[0] = current_pos;
    impl->input->current_velocity[0] = current_vel;
    impl->input->current_acceleration[0] = current_acc;
    impl->input->target_position[0] = target_pos;
    impl->input->target_velocity[0] = target_vel;
    impl->input->target_acceleration[0] = target_acc;
    impl->input->max_velocity[0] = max_vel;
    impl->input->max_acceleration[0] = max_acc;
    impl->input->max_jerk[0] = max_jerk;

    /* Set min_velocity: cruckig uses NULL for default (-max), or a pointer for explicit */
    if (impl->input->min_velocity == NULL) {
        impl->input->min_velocity = (double *)rtapi_kmalloc(sizeof(double), RTAPI_GFP_KERNEL);
        if (!impl->input->min_velocity) return -1;
    }
    impl->input->min_velocity[0] = min_vel;

    /* Backup trajectory on failure */
    int had_previous_plan = impl->planned;
    struct TrajectoryBackup bk;
    double bk_target_pos = 0.0, bk_target_vel = 0.0, bk_last_actual_acc = 0.0;
    int bk_use_position_control = 0;

    if (had_previous_plan) {
        backup_trajectory(impl->trajectory, &bk);
        bk_target_pos = impl->target_pos;
        bk_target_vel = impl->target_vel;
        bk_use_position_control = impl->use_position_control;
        bk_last_actual_acc = impl->last_actual_acc;
    }

    /* Execute planning */
    CRuckigResult result = cruckig_calculate(impl->otg, impl->input, impl->trajectory);

    int rc = handle_result(result, planner, "ruckig_plan_position",
                          had_previous_plan, &bk,
                          bk_target_pos, bk_target_vel,
                          bk_use_position_control, bk_last_actual_acc);
    if (rc != 0) return rc;

    /* Update state on success */
    int was_planned = impl->planned;
    if (!was_planned) {
        impl->last_actual_acc = current_acc;
    }

    impl->planned = 1;
    impl->start_time = 0.0;
    impl->target_pos = target_pos;
    impl->target_vel = target_vel;
    impl->target_acc = target_acc;
    impl->use_position_control = 1;
    impl->is_first_cycle = 1;
    return 0;
}

int ruckig_plan_velocity(RuckigPlanner planner,
                         double current_vel,
                         double current_acc,
                         double target_vel,
                         double target_acc,
                         double min_vel,
                         double max_acc,
                         double max_jerk) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    /* Parameter validation */
    if (max_acc <= 0.0 || max_jerk <= 0.0) {
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR,
            "ruckig_plan_velocity: invalid limits (a=%f, j=%f)\n",
            max_acc, max_jerk);
        return -1;
    }

    /* Set input parameters (velocity control mode) */
    impl->input->control_interface = CRuckigVelocity;
    impl->input->synchronization = CRuckigSyncNone;

    impl->input->current_position[0] = 0.0;
    impl->input->current_velocity[0] = current_vel;
    impl->input->current_acceleration[0] = current_acc;
    impl->input->target_position[0] = 0.0;
    impl->input->target_velocity[0] = target_vel;
    impl->input->target_acceleration[0] = target_acc;
    impl->input->max_velocity[0] = INFINITY;
    impl->input->max_acceleration[0] = max_acc;
    impl->input->max_jerk[0] = max_jerk;

    /* Set min_velocity */
    if (impl->input->min_velocity == NULL) {
        impl->input->min_velocity = (double *)rtapi_kmalloc(sizeof(double), RTAPI_GFP_KERNEL);
        if (!impl->input->min_velocity) return -1;
    }
    impl->input->min_velocity[0] = min_vel;

    /* Backup trajectory on failure */
    int had_previous_plan = impl->planned;
    struct TrajectoryBackup bk;
    double bk_target_pos = 0.0, bk_target_vel = 0.0, bk_last_actual_acc = 0.0;
    int bk_use_position_control = 0;

    if (had_previous_plan) {
        backup_trajectory(impl->trajectory, &bk);
        bk_target_pos = impl->target_pos;
        bk_target_vel = impl->target_vel;
        bk_use_position_control = impl->use_position_control;
        bk_last_actual_acc = impl->last_actual_acc;
    }

    /* Execute planning */
    CRuckigResult result = cruckig_calculate(impl->otg, impl->input, impl->trajectory);

    int rc = handle_result(result, planner, "ruckig_plan_velocity",
                          had_previous_plan, &bk,
                          bk_target_pos, bk_target_vel,
                          bk_use_position_control, bk_last_actual_acc);
    if (rc != 0) return rc;

    /* Update state on success */
    int was_planned = impl->planned;
    if (!was_planned) {
        impl->last_actual_acc = current_acc;
    }

    impl->planned = 1;
    impl->start_time = 0.0;
    impl->target_pos = 0.0;
    impl->target_vel = target_vel;
    impl->target_acc = target_acc;
    impl->use_position_control = 0;
    impl->is_first_cycle = 1;
    return 0;
}

int ruckig_at_time(RuckigPlanner planner,
                   double time,
                   double *pos,
                   double *vel,
                   double *acc,
                   double *jerk) {
    if (!planner || !pos || !vel || !acc || !jerk) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_at_time: trajectory not planned\n");
        return -1;
    }

    double duration = cruckig_trajectory_get_duration(impl->trajectory);

    /* Clamp time */
    double query_time = time;
    if (time < 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_at_time: time %f is negative\n", time);
        return -1;
    }
    if (time > duration) {
        query_time = duration;
    }

    /* Get state at specified time */
    double new_pos, new_vel, new_acc, new_jerk_unused;
    size_t new_section;
    cruckig_trajectory_at_time(impl->trajectory, query_time,
                               &new_pos, &new_vel, &new_acc, &new_jerk_unused,
                               &new_section);

    *pos = new_pos;
    *vel = new_vel;
    *acc = new_acc;

    /* Precision correction: ensure position and velocity exactly match target values */
    if (impl->use_position_control) {
        const double TIME_THRESHOLD = fmax(duration * 0.1, impl->cycle_time * 10.0);
        const double POS_ERROR_THRESHOLD = 1e-6;

        if (time >= duration - TIME_THRESHOLD || time >= duration) {
            double pos_error = fabs(*pos - impl->target_pos);
            if (pos_error < POS_ERROR_THRESHOLD) {
                *pos = impl->target_pos;
            }

            if (time >= duration) {
                *vel = impl->target_vel;
            }
            /* During trajectory: let S-curve complete naturally */
        }
    } else {
        /* Velocity control mode: only correct at trajectory end */
        if (time >= duration) {
            *vel = impl->target_vel;
            *acc = impl->target_acc;
        }
    }

    /* Calculate jerk */
    if (time > duration) {
        if (impl->use_position_control) {
            double pos_error = fabs(*pos - impl->target_pos);
            double vel_error = fabs(*vel - impl->target_vel);
            double acc_threshold = 1e-6;
            int acc_near_zero = (fabs(*acc) < acc_threshold);
            if (pos_error < TP_POS_EPSILON * 100.0 && vel_error < TP_VEL_EPSILON * 10.0 && acc_near_zero) {
                *jerk = 0.0;
                *acc = 0.0;
            }
        } else {
            double vel_error = fabs(*vel - impl->target_vel);
            double acc_threshold = 1e-6;
            int acc_near_zero = (fabs(*acc) < acc_threshold);
            if (vel_error < TP_VEL_EPSILON * 10.0 && acc_near_zero) {
                *jerk = 0.0;
                *acc = 0.0;
            }
        }
    } else if (query_time > impl->cycle_time) {
        /* Compute jerk from acceleration difference */
        double prev_pos, prev_vel, prev_acc_val, prev_jerk_unused;
        size_t prev_section;
        double prev_time = query_time - impl->cycle_time;
        if (prev_time < 0.0) prev_time = 0.0;
        cruckig_trajectory_at_time(impl->trajectory, prev_time,
                                   &prev_pos, &prev_vel, &prev_acc_val, &prev_jerk_unused,
                                   &prev_section);
        *jerk = (new_acc - prev_acc_val) / impl->cycle_time;
    } else {
        /* First cycle after replanning */
        if (impl->is_first_cycle) {
            double base_acc = impl->last_actual_acc;
            *jerk = (new_acc - base_acc) / impl->cycle_time;
            impl->is_first_cycle = 0;
        } else {
            /* Use initial acceleration from planning time */
            *jerk = (query_time > 0.0) ?
                (new_acc - impl->input->current_acceleration[0]) / query_time : 0.0;
        }
    }

    /* Save current acceleration for jerk calculation in next cycle */
    impl->last_actual_acc = *acc;

    return 0;
}

int ruckig_next_cycle(RuckigPlanner planner,
                      double current_time,
                      double cycle_time,
                      double *pos,
                      double *vel,
                      double *acc,
                      double *jerk) {
    if (!planner) {
        return -1;
    }

    double next_time = current_time + cycle_time;
    return ruckig_at_time(planner, next_time, pos, vel, acc, jerk);
}

double ruckig_get_duration(RuckigPlanner planner) {
    if (!planner) {
        return -1.0;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        return -1.0;
    }

    return cruckig_trajectory_get_duration(impl->trajectory);
}

int ruckig_is_finished(RuckigPlanner planner, double current_time) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        return -1;
    }

    double duration = ruckig_get_duration(planner);
    if (duration < 0.0) {
        return -1;
    }

    return (current_time >= duration) ? 1 : 0;
}

void ruckig_reset(RuckigPlanner planner) {
    if (!planner) {
        return;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    /* Reset all state fields */
    impl->planned = 0;
    impl->start_time = 0.0;
    impl->target_pos = 0.0;
    impl->target_vel = 0.0;
    impl->target_acc = 0.0;
    impl->use_position_control = 0;
    impl->last_actual_acc = 0.0;
    impl->is_first_cycle = 0;
    /* Note: do not reset enable_logging, preserve user setting */

    /* Reset cruckig objects */
    cruckig_reset(impl->otg);
}

void ruckig_set_logging(RuckigPlanner planner, int enable) {
    if (!planner) {
        return;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;
    impl->enable_logging = (enable != 0) ? 1 : 0;
}

int ruckig_get_decelerate_phases(RuckigPlanner planner, double *t1, double *t2) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_decelerate_phases: trajectory not planned\n");
        return -1;
    }

    /* Get Profile (1 DOF, section 0) */
    const CRuckigProfile *profile = cruckig_trajectory_get_profile(impl->trajectory, 0);
    if (!profile) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_decelerate_phases: no profile available\n");
        return -1;
    }

    /* Deceleration phases: t[4]=T1 (jerk), t[5]=T2 (constant accel) */
    if (t1 != NULL) {
        *t1 = (profile->t[4] > 0.0) ? profile->t[4] : 0.0;
    }
    if (t2 != NULL) {
        *t2 = (profile->t[5] > 0.0) ? profile->t[5] : 0.0;
    }

    return 0;
}

int ruckig_get_peak_velocity(RuckigPlanner planner, double *peak_vel) {
    if (!planner || !peak_vel) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_peak_velocity: trajectory not planned\n");
        return -1;
    }

    const CRuckigProfile *profile = cruckig_trajectory_get_profile(impl->trajectory, 0);
    if (!profile) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_peak_velocity: no profile available\n");
        return -1;
    }

    /* Peak velocity is the maximum of v[0] through v[7] */
    double max_v = 0.0;
    size_t i;
    for (i = 0; i < 8; i++) {
        if (profile->v[i] > max_v) {
            max_v = profile->v[i];
        }
    }

    *peak_vel = max_v;
    return 0;
}

int ruckig_get_start_velocity(RuckigPlanner planner, double *start_vel) {
    if (!planner || !start_vel) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_start_velocity: trajectory not planned\n");
        return -1;
    }

    const CRuckigProfile *profile = cruckig_trajectory_get_profile(impl->trajectory, 0);
    if (!profile) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_start_velocity: no profile available\n");
        return -1;
    }

    *start_vel = profile->v[0];
    return 0;
}

int ruckig_get_time_at_position(RuckigPlanner planner, double position, double time_after, double *time) {
    if (!planner || time == NULL) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_time_at_position: trajectory not planned\n");
        return -1;
    }

    double result_time;
    if (cruckig_trajectory_get_first_time_at_position(impl->trajectory, 0, position,
                                                       &result_time, time_after)) {
        *time = result_time;
        return 0;
    } else {
        return -1;
    }
}
