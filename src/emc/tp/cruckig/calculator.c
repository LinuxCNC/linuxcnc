/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#include "calculator.h"
#include "position.h"
#include "velocity.h"
#include "utils.h"


static const double eps = DBL_EPSILON;

CRuckigCalculator* cruckig_calculator_create(size_t dofs) {
    CRuckigCalculator *calc = (CRuckigCalculator*)cruckig_calloc(1, sizeof(CRuckigCalculator));
    if (!calc) return NULL;

    calc->degrees_of_freedom = dofs;

    calc->new_phase_control = (double*)cruckig_calloc(dofs, sizeof(double));
    calc->pd = (double*)cruckig_calloc(dofs, sizeof(double));
    calc->possible_t_syncs = (double*)cruckig_calloc(3 * dofs + 1, sizeof(double));
    calc->idx = (size_t*)cruckig_calloc(3 * dofs + 1, sizeof(size_t));
    calc->blocks = (CRuckigBlock*)cruckig_calloc(dofs, sizeof(CRuckigBlock));
    calc->inp_min_velocity = (double*)cruckig_calloc(dofs, sizeof(double));
    calc->inp_min_acceleration = (double*)cruckig_calloc(dofs, sizeof(double));
    calc->inp_per_dof_control_interface = (CRuckigControlInterface*)cruckig_calloc(dofs, sizeof(CRuckigControlInterface));
    calc->inp_per_dof_synchronization = (CRuckigSynchronization*)cruckig_calloc(dofs, sizeof(CRuckigSynchronization));
    calc->segment_input = NULL; /* Created on demand for waypoint calculation */

    if (!calc->new_phase_control || !calc->pd || !calc->possible_t_syncs ||
        !calc->idx || !calc->blocks || !calc->inp_min_velocity ||
        !calc->inp_min_acceleration || !calc->inp_per_dof_control_interface ||
        !calc->inp_per_dof_synchronization) {
        cruckig_calculator_destroy(calc);
        return NULL;
    }

    for (size_t i = 0; i < dofs; ++i) {
        cruckig_block_init(&calc->blocks[i]);
    }

    return calc;
}

void cruckig_calculator_destroy(CRuckigCalculator *calc) {
    if (!calc) return;
    cruckig_free(calc->new_phase_control);
    cruckig_free(calc->pd);
    cruckig_free(calc->possible_t_syncs);
    cruckig_free(calc->idx);
    cruckig_free(calc->blocks);
    cruckig_free(calc->inp_min_velocity);
    cruckig_free(calc->inp_min_acceleration);
    cruckig_free(calc->inp_per_dof_control_interface);
    cruckig_free(calc->inp_per_dof_synchronization);
    cruckig_input_destroy(calc->segment_input);
    cruckig_free(calc);
}

/* Is the trajectory (in principle) phase synchronizable? */
static bool is_input_collinear(CRuckigCalculator *calc,
                               const CRuckigInputParameter *inp,
                               CRuckigDirection limiting_direction,
                               size_t limiting_dof)
{
    const size_t dofs = calc->degrees_of_freedom;

    /* Compute pd = target_position - current_position */
    for (size_t dof = 0; dof < dofs; ++dof) {
        calc->pd[dof] = inp->target_position[dof] - inp->current_position[dof];
    }

    /* Find scale vector and scale DOF */
    const double *scale_vector = NULL;
    size_t scale_dof = 0;
    bool scale_dof_found = false;

    for (size_t dof = 0; dof < dofs; ++dof) {
        if (calc->inp_per_dof_synchronization[dof] != CRuckigSyncPhase) {
            continue;
        }

        if (calc->inp_per_dof_control_interface[dof] == CRuckigPosition && fabs(calc->pd[dof]) > eps) {
            scale_vector = calc->pd;
            scale_dof = dof;
            scale_dof_found = true;
            break;
        } else if (fabs(inp->current_velocity[dof]) > eps) {
            scale_vector = inp->current_velocity;
            scale_dof = dof;
            scale_dof_found = true;
            break;
        } else if (fabs(inp->current_acceleration[dof]) > eps) {
            scale_vector = inp->current_acceleration;
            scale_dof = dof;
            scale_dof_found = true;
            break;
        } else if (fabs(inp->target_velocity[dof]) > eps) {
            scale_vector = inp->target_velocity;
            scale_dof = dof;
            scale_dof_found = true;
            break;
        } else if (fabs(inp->target_acceleration[dof]) > eps) {
            scale_vector = inp->target_acceleration;
            scale_dof = dof;
            scale_dof_found = true;
            break;
        }
    }

    if (!scale_dof_found) {
        return false;
    }

    const double scale = scale_vector[scale_dof];
    const double pd_scale = calc->pd[scale_dof] / scale;
    const double v0_scale = inp->current_velocity[scale_dof] / scale;
    const double vf_scale = inp->target_velocity[scale_dof] / scale;
    const double a0_scale = inp->current_acceleration[scale_dof] / scale;
    const double af_scale = inp->target_acceleration[scale_dof] / scale;

    const double scale_limiting = scale_vector[limiting_dof];
    double control_limiting;
    if (isinf(inp->max_jerk[limiting_dof])) {
        control_limiting = (limiting_direction == DirectionUP)
            ? inp->max_acceleration[limiting_dof]
            : calc->inp_min_acceleration[limiting_dof];
    } else {
        control_limiting = (limiting_direction == DirectionUP)
            ? inp->max_jerk[limiting_dof]
            : -inp->max_jerk[limiting_dof];
    }

    for (size_t dof = 0; dof < dofs; ++dof) {
        if (calc->inp_per_dof_synchronization[dof] != CRuckigSyncPhase) {
            continue;
        }

        const double current_scale = scale_vector[dof];
        if (
            (calc->inp_per_dof_control_interface[dof] == CRuckigPosition && fabs(calc->pd[dof] - pd_scale * current_scale) > eps)
            || fabs(inp->current_velocity[dof] - v0_scale * current_scale) > eps
            || fabs(inp->current_acceleration[dof] - a0_scale * current_scale) > eps
            || fabs(inp->target_velocity[dof] - vf_scale * current_scale) > eps
            || fabs(inp->target_acceleration[dof] - af_scale * current_scale) > eps
        ) {
            return false;
        }

        calc->new_phase_control[dof] = control_limiting * current_scale / scale_limiting;
    }

    return true;
}

/* Simple insertion sort for index array by values */
static void sort_indices(size_t *idx_arr, const double *values, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        size_t key = idx_arr[i];
        double key_val = values[key];
        size_t j = i;
        while (j > 0 && values[idx_arr[j - 1]] > key_val) {
            idx_arr[j] = idx_arr[j - 1];
            --j;
        }
        idx_arr[j] = key;
    }
}

/*
 * synchronize: Find a valid synchronization time.
 * Returns true if found; sets t_sync, limiting_dof, and updates profiles.
 *
 * has_t_min: whether t_min is valid
 * t_min: minimum duration
 * limiting_dof_out: set to the limiting DOF index; has_limiting_dof set to true/false
 */
static bool synchronize(CRuckigCalculator *calc,
                        bool has_t_min, double t_min,
                        double *t_sync,
                        bool *has_limiting_dof, size_t *limiting_dof_out,
                        CRuckigProfile *profiles,
                        bool discrete_duration, double delta_time)
{
    const size_t dofs = calc->degrees_of_freedom;

    /* Fill possible_t_syncs */
    bool any_interval = false;
    for (size_t dof = 0; dof < dofs; ++dof) {
        if (calc->inp_per_dof_synchronization[dof] == CRuckigSyncNone) {
            calc->possible_t_syncs[dof] = 0.0;
            calc->possible_t_syncs[dofs + dof] = INFINITY;
            calc->possible_t_syncs[2 * dofs + dof] = INFINITY;
            continue;
        }

        calc->possible_t_syncs[dof] = calc->blocks[dof].t_min;
        calc->possible_t_syncs[dofs + dof] = calc->blocks[dof].a.valid
            ? calc->blocks[dof].a.right : INFINITY;
        calc->possible_t_syncs[2 * dofs + dof] = calc->blocks[dof].b.valid
            ? calc->blocks[dof].b.right : INFINITY;
        any_interval = any_interval || calc->blocks[dof].a.valid || calc->blocks[dof].b.valid;
    }
    calc->possible_t_syncs[3 * dofs] = has_t_min ? t_min : INFINITY;
    any_interval = any_interval || has_t_min;

    /* Discrete duration rounding */
    if (discrete_duration) {
        size_t count = 3 * dofs + 1;
        for (size_t i = 0; i < count; ++i) {
            if (isinf(calc->possible_t_syncs[i])) continue;
            double remainder = fmod(calc->possible_t_syncs[i], delta_time);
            if (remainder > eps) {
                calc->possible_t_syncs[i] += delta_time - remainder;
            }
        }
    }

    /* Initialize and sort indices */
    size_t idx_end_count = any_interval ? (3 * dofs + 1) : dofs;
    for (size_t i = 0; i < idx_end_count; ++i) {
        calc->idx[i] = i;
    }
    sort_indices(calc->idx, calc->possible_t_syncs, idx_end_count);

    /* Start at dofs-1 (skip the dofs-1 smallest t_min values since we need ALL dofs at or past their t_min) */
    size_t start_idx = (dofs >= 1) ? (dofs - 1) : 0;
    for (size_t iter = start_idx; iter < idx_end_count; ++iter) {
        size_t i = calc->idx[iter];
        double possible_t_sync = calc->possible_t_syncs[i];

        /* Check if any DOF is blocked */
        bool is_blocked = false;
        for (size_t dof = 0; dof < dofs; ++dof) {
            if (calc->inp_per_dof_synchronization[dof] == CRuckigSyncNone) {
                continue;
            }
            if (cruckig_block_is_blocked(&calc->blocks[dof], possible_t_sync)) {
                is_blocked = true;
                break;
            }
        }

        double t_min_or_zero = has_t_min ? t_min : 0.0;
        if (is_blocked || possible_t_sync < t_min_or_zero || isinf(possible_t_sync)) {
            continue;
        }

        *t_sync = possible_t_sync;

        if (i == 3 * dofs) {
            /* Optional t_min was the winning candidate */
            *has_limiting_dof = false;
            return true;
        }

        /* Determine which DOF and which block part */
        size_t quot = i / dofs;
        size_t rem = i % dofs;
        *limiting_dof_out = rem;
        *has_limiting_dof = true;

        switch (quot) {
            case 0:
                profiles[rem] = calc->blocks[rem].p_min;
                break;
            case 1:
                profiles[rem] = calc->blocks[rem].a.profile;
                break;
            case 2:
                profiles[rem] = calc->blocks[rem].b.profile;
                break;
        }
        return true;
    }

    return false;
}

CRUCKIG_HOT
/*
 * Find the optimal profile for a single DOF (Step 1).
 * Separated to keep large Step1 structs (~3.6KB) off the main function's stack,
 * which matters for the kernel's limited stack size.
 */
static bool find_profile_step1(
    CRuckigCalculator *calc,
    const CRuckigInputParameter *inp,
    CRuckigProfile *p,
    size_t dof)
{
    switch (calc->inp_per_dof_control_interface[dof]) {
        case CRuckigPosition: {
            if (!isinf(inp->max_jerk[dof])) {
                CRuckigPositionThirdOrderStep1 *step1 = &calc->step1_workspace.pos3_step1;
                cruckig_pos3_step1_init(step1,
                    p->p[0], p->v[0], p->a[0], p->pf, p->vf, p->af,
                    inp->max_velocity[dof], calc->inp_min_velocity[dof],
                    inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                    inp->max_jerk[dof]);
                return cruckig_pos3_step1_get_profile(step1, p, &calc->blocks[dof]);
            } else if (!isinf(inp->max_acceleration[dof])) {
                CRuckigPositionSecondOrderStep1 *step1 = &calc->step1_workspace.pos2_step1;
                cruckig_pos2_step1_init(step1,
                    p->p[0], p->v[0], p->pf, p->vf,
                    inp->max_velocity[dof], calc->inp_min_velocity[dof],
                    inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                return cruckig_pos2_step1_get_profile(step1, p, &calc->blocks[dof]);
            } else {
                CRuckigPositionFirstOrderStep1 *step1 = &calc->step1_workspace.pos1_step1;
                cruckig_pos1_step1_init(step1,
                    p->p[0], p->pf,
                    inp->max_velocity[dof], calc->inp_min_velocity[dof]);
                return cruckig_pos1_step1_get_profile(step1, p, &calc->blocks[dof]);
            }
        } break;
        case CRuckigVelocity: {
            if (!isinf(inp->max_jerk[dof])) {
                CRuckigVelocityThirdOrderStep1 *step1 = &calc->step1_workspace.vel3_step1;
                cruckig_vel3_step1_init(step1,
                    p->v[0], p->a[0], p->vf, p->af,
                    inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                    inp->max_jerk[dof]);
                return cruckig_vel3_step1_get_profile(step1, p, &calc->blocks[dof]);
            } else {
                CRuckigVelocitySecondOrderStep1 *step1 = &calc->step1_workspace.vel2_step1;
                cruckig_vel2_step1_init(step1,
                    p->v[0], p->vf,
                    inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                return cruckig_vel2_step1_get_profile(step1, p, &calc->blocks[dof]);
            }
        } break;
    }
    return false;
}

CRuckigResult cruckig_calculator_calculate(CRuckigCalculator *calc,
                                           const CRuckigInputParameter *inp,
                                           CRuckigTrajectory *traj,
                                           double delta_time,
                                           bool *was_interrupted)
{
    *was_interrupted = false;
    const size_t dofs = calc->degrees_of_freedom;

    for (size_t dof = 0; dof < dofs; ++dof) {
        CRuckigProfile *p = &traj->profiles[dof];

        calc->inp_min_velocity[dof] = inp->min_velocity
            ? inp->min_velocity[dof] : -inp->max_velocity[dof];
        calc->inp_min_acceleration[dof] = inp->min_acceleration
            ? inp->min_acceleration[dof] : -inp->max_acceleration[dof];
        calc->inp_per_dof_control_interface[dof] = inp->per_dof_control_interface
            ? inp->per_dof_control_interface[dof] : inp->control_interface;
        calc->inp_per_dof_synchronization[dof] = inp->per_dof_synchronization
            ? inp->per_dof_synchronization[dof] : inp->synchronization;

        if (!inp->enabled[dof]) {
            p->p[7] = inp->current_position[dof];
            p->v[7] = inp->current_velocity[dof];
            p->a[7] = inp->current_acceleration[dof];
            p->t_sum[6] = 0.0;
            calc->blocks[dof].t_min = 0.0;
            calc->blocks[dof].a.valid = false;
            calc->blocks[dof].b.valid = false;
            continue;
        }

        /* Calculate brake (if input exceeds or will exceed limits) */
        switch (calc->inp_per_dof_control_interface[dof]) {
            case CRuckigPosition: {
                if (!isinf(inp->max_jerk[dof])) {
                    cruckig_brake_get_position_brake_trajectory(&p->brake,
                        inp->current_velocity[dof], inp->current_acceleration[dof],
                        inp->max_velocity[dof], calc->inp_min_velocity[dof],
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                        inp->max_jerk[dof]);
                } else if (!isinf(inp->max_acceleration[dof])) {
                    cruckig_brake_get_second_order_position_brake_trajectory(&p->brake,
                        inp->current_velocity[dof],
                        inp->max_velocity[dof], calc->inp_min_velocity[dof],
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                }
                cruckig_profile_set_boundary(p,
                    inp->current_position[dof], inp->current_velocity[dof],
                    inp->current_acceleration[dof],
                    inp->target_position[dof], inp->target_velocity[dof],
                    inp->target_acceleration[dof]);
            } break;
            case CRuckigVelocity: {
                if (!isinf(inp->max_jerk[dof])) {
                    cruckig_brake_get_velocity_brake_trajectory(&p->brake,
                        inp->current_acceleration[dof],
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                        inp->max_jerk[dof]);
                } else {
                    cruckig_brake_get_second_order_velocity_brake_trajectory(&p->brake);
                }
                cruckig_profile_set_boundary_for_velocity(p,
                    inp->current_position[dof], inp->current_velocity[dof],
                    inp->current_acceleration[dof],
                    inp->target_velocity[dof], inp->target_acceleration[dof]);
            } break;
        }

        /* Finalize pre-trajectory */
        if (!isinf(inp->max_jerk[dof])) {
            cruckig_brake_finalize(&p->brake, &p->p[0], &p->v[0], &p->a[0]);
        } else if (!isinf(inp->max_acceleration[dof])) {
            cruckig_brake_finalize_second_order(&p->brake, &p->p[0], &p->v[0], &p->a[0]);
        }

        if (!find_profile_step1(calc, inp, p, dof)) {
            bool has_zero_limits = (inp->max_acceleration[dof] == 0.0 ||
                                    calc->inp_min_acceleration[dof] == 0.0 ||
                                    inp->max_jerk[dof] == 0.0);
            if (has_zero_limits) {
                return CRuckigErrorZeroLimits;
            } else {
                return CRuckigErrorExecutionTimeCalculation;
            }
        }

        traj->independent_min_durations[dof] = calc->blocks[dof].t_min;
    }

    const bool discrete_duration = (inp->duration_discretization == CRuckigDiscrete);

    if (dofs == 1 && !inp->has_minimum_duration && !discrete_duration) {
        traj->duration = calc->blocks[0].t_min;
        traj->profiles[0] = calc->blocks[0].p_min;
        traj->cumulative_times[0] = traj->duration;
        return CRuckigWorking;
    }

    /* Synchronize */
    bool has_limiting_dof = false;
    size_t limiting_dof = 0;
    bool found_synchronization = synchronize(calc,
        inp->has_minimum_duration, inp->minimum_duration,
        &traj->duration, &has_limiting_dof, &limiting_dof,
        traj->profiles, discrete_duration, delta_time);

    if (!found_synchronization) {
        bool has_zero_limits = false;
        for (size_t dof = 0; dof < dofs; ++dof) {
            if (inp->max_acceleration[dof] == 0.0 ||
                calc->inp_min_acceleration[dof] == 0.0 ||
                inp->max_jerk[dof] == 0.0) {
                has_zero_limits = true;
                break;
            }
        }
        if (has_zero_limits) {
            return CRuckigErrorZeroLimits;
        } else {
            return CRuckigErrorSynchronizationCalculation;
        }
    }

    /* None Synchronization */
    for (size_t dof = 0; dof < dofs; ++dof) {
        if (inp->enabled[dof] && calc->inp_per_dof_synchronization[dof] == CRuckigSyncNone) {
            traj->profiles[dof] = calc->blocks[dof].p_min;
            if (calc->blocks[dof].t_min > traj->duration) {
                traj->duration = calc->blocks[dof].t_min;
                has_limiting_dof = true;
                limiting_dof = dof;
            }
        }
    }
    traj->cumulative_times[0] = traj->duration;

    /* Check maximal duration */
    if (traj->duration > 7.6e3) {
        return CRuckigErrorTrajectoryDuration;
    }

    if (traj->duration == 0.0) {
        /* Copy all profiles for end state */
        for (size_t dof = 0; dof < dofs; ++dof) {
            traj->profiles[dof] = calc->blocks[dof].p_min;
        }
        return CRuckigWorking;
    }

    /* Check if all synchronizations are None */
    if (!discrete_duration) {
        bool all_none = true;
        for (size_t dof = 0; dof < dofs; ++dof) {
            if (calc->inp_per_dof_synchronization[dof] != CRuckigSyncNone) {
                all_none = false;
                break;
            }
        }
        if (all_none) {
            return CRuckigWorking;
        }
    }

    /* Phase Synchronization */
    if (has_limiting_dof) {
        bool any_phase = false;
        for (size_t dof = 0; dof < dofs; ++dof) {
            if (calc->inp_per_dof_synchronization[dof] == CRuckigSyncPhase) {
                any_phase = true;
                break;
            }
        }

        if (any_phase) {
            const CRuckigProfile *p_limiting = &traj->profiles[limiting_dof];
            if (is_input_collinear(calc, inp, p_limiting->direction, limiting_dof)) {
                bool found_time_synchronization = true;

                for (size_t dof = 0; dof < dofs; ++dof) {
                    if (!inp->enabled[dof] || dof == limiting_dof ||
                        calc->inp_per_dof_synchronization[dof] != CRuckigSyncPhase) {
                        continue;
                    }

                    CRuckigProfile *p = &traj->profiles[dof];
                    double t_profile = traj->duration - p->brake.duration - p->accel.duration;

                    /* Copy timing information from limiting DOF */
                    memcpy(p->t, p_limiting->t, sizeof(p->t));
                    p->control_signs = p_limiting->control_signs;

                    switch (calc->inp_per_dof_control_interface[dof]) {
                        case CRuckigPosition: {
                            switch (p->control_signs) {
                                case ControlSignsUDDU: {
                                    if (!isinf(inp->max_jerk[dof])) {
                                        found_time_synchronization &= cruckig_profile_check_with_timing_full(p,
                                            ControlSignsUDDU, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_velocity[dof], calc->inp_min_velocity[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                                            inp->max_jerk[dof]);
                                    } else if (!isinf(inp->max_acceleration[dof])) {
                                        found_time_synchronization &= cruckig_profile_check_for_second_order_with_timing_full(p,
                                            ControlSignsUDDU, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            -calc->new_phase_control[dof],
                                            inp->max_velocity[dof], calc->inp_min_velocity[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                                    } else {
                                        found_time_synchronization &= cruckig_profile_check_for_first_order_with_timing_full(p,
                                            ControlSignsUDDU, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_velocity[dof], calc->inp_min_velocity[dof]);
                                    }
                                } break;
                                case ControlSignsUDUD: {
                                    if (!isinf(inp->max_jerk[dof])) {
                                        found_time_synchronization &= cruckig_profile_check_with_timing_full(p,
                                            ControlSignsUDUD, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_velocity[dof], calc->inp_min_velocity[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                                            inp->max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= cruckig_profile_check_for_second_order_with_timing_full(p,
                                            ControlSignsUDUD, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            -calc->new_phase_control[dof],
                                            inp->max_velocity[dof], calc->inp_min_velocity[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                                    }
                                } break;
                            }
                        } break;
                        case CRuckigVelocity: {
                            switch (p->control_signs) {
                                case ControlSignsUDDU: {
                                    if (!isinf(inp->max_jerk[dof])) {
                                        found_time_synchronization &= cruckig_profile_check_for_velocity_with_timing_full(p,
                                            ControlSignsUDDU, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                                            inp->max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= cruckig_profile_check_for_second_order_velocity_with_timing_full(p,
                                            ControlSignsUDDU, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                                    }
                                } break;
                                case ControlSignsUDUD: {
                                    if (!isinf(inp->max_jerk[dof])) {
                                        found_time_synchronization &= cruckig_profile_check_for_velocity_with_timing_full(p,
                                            ControlSignsUDUD, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                                            inp->max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= cruckig_profile_check_for_second_order_velocity_with_timing_full(p,
                                            ControlSignsUDUD, ReachedLimitsNONE,
                                            t_profile, calc->new_phase_control[dof],
                                            inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                                    }
                                } break;
                            }
                        } break;
                    }

                    p->limits = p_limiting->limits; /* After check method call */
                }

                if (found_time_synchronization) {
                    bool all_phase_or_none = true;
                    for (size_t dof = 0; dof < dofs; ++dof) {
                        if (calc->inp_per_dof_synchronization[dof] != CRuckigSyncPhase &&
                            calc->inp_per_dof_synchronization[dof] != CRuckigSyncNone) {
                            all_phase_or_none = false;
                            break;
                        }
                    }
                    if (all_phase_or_none) {
                        return CRuckigWorking;
                    }
                }
            }
        }
    }

    /* Time Synchronization (Step 2) */
    for (size_t dof = 0; dof < dofs; ++dof) {
        bool skip_synchronization = ((has_limiting_dof && dof == limiting_dof) ||
                                      calc->inp_per_dof_synchronization[dof] == CRuckigSyncNone) &&
                                      !discrete_duration;
        if (!inp->enabled[dof] || skip_synchronization) {
            continue;
        }

        CRuckigProfile *p = &traj->profiles[dof];
        double t_profile = traj->duration - p->brake.duration - p->accel.duration;

        if (calc->inp_per_dof_synchronization[dof] == CRuckigSyncTimeIfNecessary &&
            fabs(inp->target_velocity[dof]) < eps &&
            fabs(inp->target_acceleration[dof]) < eps) {
            *p = calc->blocks[dof].p_min;
            continue;
        }

        /* Check if the final time corresponds to an extremal profile from step 1 */
        if (fabs(t_profile - calc->blocks[dof].t_min) < 2 * eps) {
            *p = calc->blocks[dof].p_min;
            continue;
        } else if (calc->blocks[dof].a.valid && fabs(t_profile - calc->blocks[dof].a.right) < 2 * eps) {
            *p = calc->blocks[dof].a.profile;
            continue;
        } else if (calc->blocks[dof].b.valid && fabs(t_profile - calc->blocks[dof].b.right) < 2 * eps) {
            *p = calc->blocks[dof].b.profile;
            continue;
        }

        bool found_time_synchronization = false;
        switch (calc->inp_per_dof_control_interface[dof]) {
            case CRuckigPosition: {
                if (!isinf(inp->max_jerk[dof])) {
                    CRuckigPositionThirdOrderStep2 step2;
                    cruckig_pos3_step2_init(&step2,
                        t_profile, p->p[0], p->v[0], p->a[0], p->pf, p->vf, p->af,
                        inp->max_velocity[dof], calc->inp_min_velocity[dof],
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                        inp->max_jerk[dof]);
                    found_time_synchronization = cruckig_pos3_step2_get_profile(&step2, p);
                } else if (!isinf(inp->max_acceleration[dof])) {
                    CRuckigPositionSecondOrderStep2 step2;
                    cruckig_pos2_step2_init(&step2,
                        t_profile, p->p[0], p->v[0], p->pf, p->vf,
                        inp->max_velocity[dof], calc->inp_min_velocity[dof],
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                    found_time_synchronization = cruckig_pos2_step2_get_profile(&step2, p);
                } else {
                    CRuckigPositionFirstOrderStep2 step2;
                    cruckig_pos1_step2_init(&step2,
                        t_profile, p->p[0], p->pf,
                        inp->max_velocity[dof], calc->inp_min_velocity[dof]);
                    found_time_synchronization = cruckig_pos1_step2_get_profile(&step2, p);
                }
            } break;
            case CRuckigVelocity: {
                if (!isinf(inp->max_jerk[dof])) {
                    CRuckigVelocityThirdOrderStep2 step2;
                    cruckig_vel3_step2_init(&step2,
                        t_profile, p->v[0], p->a[0], p->vf, p->af,
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof],
                        inp->max_jerk[dof]);
                    found_time_synchronization = cruckig_vel3_step2_get_profile(&step2, p);
                } else {
                    CRuckigVelocitySecondOrderStep2 step2;
                    cruckig_vel2_step2_init(&step2,
                        t_profile, p->v[0], p->vf,
                        inp->max_acceleration[dof], calc->inp_min_acceleration[dof]);
                    found_time_synchronization = cruckig_vel2_step2_get_profile(&step2, p);
                }
            } break;
        }

        if (!found_time_synchronization) {
            return CRuckigErrorSynchronizationCalculation;
        }
    }

    return CRuckigWorking;
}

/*
 * Multi-segment waypoint calculation.
 *
 * Strategy: sequential segment planning. For each segment between consecutive
 * waypoints, use the existing single-segment planner. The end state of segment i
 * becomes the start state of segment i+1. At intermediate waypoints, velocity
 * and acceleration pass through continuously (zero target velocity at waypoints
 * for robustness, with option to optimize).
 */
CRuckigResult cruckig_calculator_calculate_waypoints(CRuckigCalculator *calc,
                                                     const CRuckigInputParameter *inp,
                                                     CRuckigTrajectory *traj,
                                                     double delta_time,
                                                     bool *was_interrupted)
{
    const size_t dofs = calc->degrees_of_freedom;
    const size_t nwp = inp->num_intermediate_waypoints;
    const size_t nsec = nwp + 1; /* Number of sections */

    /* Resize trajectory for multi-section */
    if (!cruckig_trajectory_resize(traj, nsec)) {
        return CRuckigError;
    }

    /* Create reusable segment input if needed */
    if (!calc->segment_input) {
        calc->segment_input = cruckig_input_create(dofs);
        if (!calc->segment_input) return CRuckigError;
    }

    CRuckigInputParameter *seg = calc->segment_input;

    /* Build a temporary single-section trajectory for each segment */
    CRuckigTrajectory *seg_traj = cruckig_trajectory_create(dofs);
    if (!seg_traj) return CRuckigError;

    double cumulative_time = 0.0;
    CRuckigResult final_result = CRuckigWorking;

    for (size_t s = 0; s < nsec; ++s) {
        /* Set segment input: copy global settings */
        seg->control_interface = CRuckigPosition;
        seg->synchronization = inp->synchronization;
        seg->duration_discretization = CRuckigContinuous;
        seg->has_minimum_duration = false;

        /* Per-section minimum duration */
        if (inp->per_section_minimum_duration) {
            seg->minimum_duration = inp->per_section_minimum_duration[s];
            seg->has_minimum_duration = true;
        }

        /* Set start state */
        if (s == 0) {
            /* First segment starts from input current state */
            memcpy(seg->current_position, inp->current_position, dofs * sizeof(double));
            memcpy(seg->current_velocity, inp->current_velocity, dofs * sizeof(double));
            memcpy(seg->current_acceleration, inp->current_acceleration, dofs * sizeof(double));
        }
        /* else: current state was set by previous iteration's end state */

        /* Set target state */
        if (s < nwp) {
            /* Target is the next intermediate waypoint */
            const double *wp = inp->intermediate_positions + s * dofs;
            memcpy(seg->target_position, wp, dofs * sizeof(double));
            /* Zero velocity/acceleration at intermediate waypoints */
            memset(seg->target_velocity, 0, dofs * sizeof(double));
            memset(seg->target_acceleration, 0, dofs * sizeof(double));
        } else {
            /* Last segment targets the final position */
            memcpy(seg->target_position, inp->target_position, dofs * sizeof(double));
            memcpy(seg->target_velocity, inp->target_velocity, dofs * sizeof(double));
            memcpy(seg->target_acceleration, inp->target_acceleration, dofs * sizeof(double));
        }

        /* Set kinematic constraints (per-section or global) */
        if (inp->per_section_max_velocity) {
            memcpy(seg->max_velocity, inp->per_section_max_velocity + s * dofs, dofs * sizeof(double));
        } else {
            memcpy(seg->max_velocity, inp->max_velocity, dofs * sizeof(double));
        }
        if (inp->per_section_max_acceleration) {
            memcpy(seg->max_acceleration, inp->per_section_max_acceleration + s * dofs, dofs * sizeof(double));
        } else {
            memcpy(seg->max_acceleration, inp->max_acceleration, dofs * sizeof(double));
        }
        if (inp->per_section_max_jerk) {
            memcpy(seg->max_jerk, inp->per_section_max_jerk + s * dofs, dofs * sizeof(double));
        } else {
            memcpy(seg->max_jerk, inp->max_jerk, dofs * sizeof(double));
        }

        /* Optional min limits */
        if (inp->per_section_min_velocity) {
            if (!seg->min_velocity) seg->min_velocity = (double*)cruckig_malloc(dofs * sizeof(double));
            memcpy(seg->min_velocity, inp->per_section_min_velocity + s * dofs, dofs * sizeof(double));
        } else if (inp->min_velocity) {
            if (!seg->min_velocity) seg->min_velocity = (double*)cruckig_malloc(dofs * sizeof(double));
            memcpy(seg->min_velocity, inp->min_velocity, dofs * sizeof(double));
        } else {
            cruckig_free(seg->min_velocity);
            seg->min_velocity = NULL;
        }

        if (inp->per_section_min_acceleration) {
            if (!seg->min_acceleration) seg->min_acceleration = (double*)cruckig_malloc(dofs * sizeof(double));
            memcpy(seg->min_acceleration, inp->per_section_min_acceleration + s * dofs, dofs * sizeof(double));
        } else if (inp->min_acceleration) {
            if (!seg->min_acceleration) seg->min_acceleration = (double*)cruckig_malloc(dofs * sizeof(double));
            memcpy(seg->min_acceleration, inp->min_acceleration, dofs * sizeof(double));
        } else {
            cruckig_free(seg->min_acceleration);
            seg->min_acceleration = NULL;
        }

        /* Enable all DOFs for segment */
        for (size_t d = 0; d < dofs; ++d) seg->enabled[d] = true;

        /* Calculate this segment */
        bool seg_interrupted = false;
        CRuckigResult seg_result = cruckig_calculator_calculate(calc, seg, seg_traj,
                                                                 delta_time, &seg_interrupted);
        if (seg_result != CRuckigWorking) {
            cruckig_trajectory_destroy(seg_traj);
            *was_interrupted = false;
            return seg_result;
        }

        /* Copy segment profiles into the multi-section trajectory */
        double seg_duration = cruckig_trajectory_get_duration(seg_traj);
        cumulative_time += seg_duration;
        traj->cumulative_times[s] = cumulative_time;

        for (size_t d = 0; d < dofs; ++d) {
            traj->profiles[s * dofs + d] = seg_traj->profiles[d];
            if (s == 0) {
                traj->independent_min_durations[d] = seg_traj->independent_min_durations[d];
            }
        }

        /* Set next segment's start state from this segment's end state */
        if (s < nsec - 1) {
            for (size_t d = 0; d < dofs; ++d) {
                const CRuckigProfile *p = &seg_traj->profiles[d];
                seg->current_position[d] = p->p[7];
                seg->current_velocity[d] = p->v[7];
                seg->current_acceleration[d] = p->a[7];
            }
        }
    }

    traj->duration = cumulative_time;
    cruckig_trajectory_destroy(seg_traj);

    /* Position limits check */
    if (inp->max_position || inp->min_position ||
        inp->per_section_max_position || inp->per_section_min_position)
    {
        /* Sample trajectory and check bounds */
        double *pos = (double*)cruckig_malloc(dofs * sizeof(double));
        double *vel = (double*)cruckig_malloc(dofs * sizeof(double));
        double *acc = (double*)cruckig_malloc(dofs * sizeof(double));
        size_t sec;

        bool violated = false;
        /* Check at fine time steps */
        double dt_check = (delta_time > 0.0) ? delta_time : 0.001;
        for (double t = 0.0; t <= cumulative_time && !violated; t += dt_check) {
            cruckig_trajectory_at_time(traj, t, pos, vel, acc, NULL, &sec);

            for (size_t d = 0; d < dofs; ++d) {
                double p_max = INFINITY, p_min = -INFINITY;

                if (inp->max_position) p_max = inp->max_position[d];
                if (inp->min_position) p_min = inp->min_position[d];

                /* Per-section position limits */
                if (sec < nsec) {
                    if (inp->per_section_max_position) {
                        double sec_max = inp->per_section_max_position[sec * dofs + d];
                        if (sec_max < p_max) p_max = sec_max;
                    }
                    if (inp->per_section_min_position) {
                        double sec_min = inp->per_section_min_position[sec * dofs + d];
                        if (sec_min > p_min) p_min = sec_min;
                    }
                }

                if (pos[d] > p_max + 1e-8 || pos[d] < p_min - 1e-8) {
                    violated = true;
                    break;
                }
            }
        }

        /* Also check position extrema */
        if (!violated) {
            cruckig_trajectory_get_position_extrema(traj);
            for (size_t d = 0; d < dofs; ++d) {
                double p_max = INFINITY, p_min = -INFINITY;
                if (inp->max_position) p_max = inp->max_position[d];
                if (inp->min_position) p_min = inp->min_position[d];

                if (traj->position_extrema[d].max > p_max + 1e-8 ||
                    traj->position_extrema[d].min < p_min - 1e-8) {
                    violated = true;
                    break;
                }
            }
        }

        cruckig_free(pos);
        cruckig_free(vel);
        cruckig_free(acc);

        if (violated) {
            final_result = CRuckigErrorPositionalLimits;
        }
    }

    *was_interrupted = false;
    return final_result;
}

CRuckigResult cruckig_calculator_continue(CRuckigCalculator *calc,
                                          const CRuckigInputParameter *inp,
                                          CRuckigTrajectory *traj,
                                          double delta_time,
                                          bool *was_interrupted)
{
    /* For now, continue_calculation simply re-runs the full calculation.
     * A future optimization could resume from partial state. */
    if (inp->num_intermediate_waypoints > 0 && inp->control_interface == CRuckigPosition) {
        return cruckig_calculator_calculate_waypoints(calc, inp, traj, delta_time, was_interrupted);
    }
    return cruckig_calculator_calculate(calc, inp, traj, delta_time, was_interrupted);
}
