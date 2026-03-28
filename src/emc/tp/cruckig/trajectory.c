/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#include "trajectory.h"
#include "utils.h"


CRuckigTrajectory* cruckig_trajectory_create(size_t dofs) {
    CRuckigTrajectory *traj = (CRuckigTrajectory*)cruckig_calloc(1, sizeof(CRuckigTrajectory));
    if (!traj) return NULL;

    traj->degrees_of_freedom = dofs;
    traj->num_sections = 1;
    traj->section_capacity = 1;
    traj->duration = 0.0;

    traj->profiles = (CRuckigProfile*)cruckig_calloc(dofs, sizeof(CRuckigProfile));
    traj->cumulative_times = (double*)cruckig_calloc(1, sizeof(double));
    traj->independent_min_durations = (double*)cruckig_calloc(dofs, sizeof(double));
    traj->position_extrema = (CRuckigBound*)cruckig_calloc(dofs, sizeof(CRuckigBound));

    if (!traj->profiles || !traj->cumulative_times ||
        !traj->independent_min_durations || !traj->position_extrema) {
        cruckig_trajectory_destroy(traj);
        return NULL;
    }

    for (size_t dof = 0; dof < dofs; ++dof) {
        cruckig_profile_init(&traj->profiles[dof]);
    }

    return traj;
}

void cruckig_trajectory_destroy(CRuckigTrajectory *traj) {
    if (!traj) return;
    cruckig_free(traj->profiles);
    cruckig_free(traj->cumulative_times);
    cruckig_free(traj->independent_min_durations);
    cruckig_free(traj->position_extrema);
    cruckig_free(traj);
}

bool cruckig_trajectory_resize(CRuckigTrajectory *traj, size_t num_sections) {
    if (!traj || num_sections == 0) return false;

    const size_t dofs = traj->degrees_of_freedom;

    if (num_sections > traj->section_capacity) {
        CRuckigProfile *new_profiles = (CRuckigProfile*)cruckig_realloc(
            traj->profiles, num_sections * dofs * sizeof(CRuckigProfile));
        double *new_times = (double*)cruckig_realloc(
            traj->cumulative_times, num_sections * sizeof(double));

        if (!new_profiles || !new_times) {
            /* Restore on failure */
            if (new_profiles) traj->profiles = new_profiles;
            if (new_times) traj->cumulative_times = new_times;
            return false;
        }

        traj->profiles = new_profiles;
        traj->cumulative_times = new_times;
        traj->section_capacity = num_sections;

        /* Initialize new profiles */
        for (size_t s = traj->num_sections; s < num_sections; ++s) {
            for (size_t d = 0; d < dofs; ++d) {
                cruckig_profile_init(&traj->profiles[s * dofs + d]);
            }
            traj->cumulative_times[s] = 0.0;
        }
    }

    traj->num_sections = num_sections;
    return true;
}

/*
 * state_to_integrate_from: Determine the integration base state at a given time.
 * Supports multi-section trajectories via binary search on cumulative_times.
 */
static void state_to_integrate_from(const CRuckigTrajectory *traj, double time,
                                    size_t *new_section,
                                    double *t_out, double *p_out, double *v_out,
                                    double *a_out, double *j_out)
{
    const size_t dofs = traj->degrees_of_freedom;
    const size_t nsec = traj->num_sections;

    if (time >= traj->duration) {
        /* Past the end of trajectory */
        *new_section = nsec;
        size_t last = nsec - 1;
        for (size_t dof = 0; dof < dofs; ++dof) {
            const CRuckigProfile *prof = &traj->profiles[last * dofs + dof];
            double t_pre = prof->brake.duration;
            double t_diff = time - (traj->duration - (t_pre + prof->t_sum[6]) + t_pre + prof->t_sum[6]);
            /* Simplify: time past the end of last section's profile */
            double section_start = (last > 0) ? traj->cumulative_times[last - 1] : 0.0;
            t_diff = time - section_start - t_pre - prof->t_sum[6];
            t_out[dof] = t_diff;
            p_out[dof] = prof->p[7];
            v_out[dof] = prof->v[7];
            a_out[dof] = prof->a[7];
            j_out[dof] = 0.0;
        }
        return;
    }

    /* Binary search to find current section */
    size_t section = 0;
    if (nsec > 1) {
        size_t lo = 0, hi = nsec;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (traj->cumulative_times[mid] <= time) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        section = lo;
        if (section >= nsec) section = nsec - 1;
    }

    *new_section = section;

    /* Time offset within this section */
    double section_start = (section > 0) ? traj->cumulative_times[section - 1] : 0.0;
    double t_diff = time - section_start;

    for (size_t dof = 0; dof < dofs; ++dof) {
        const CRuckigProfile *prof = &traj->profiles[section * dofs + dof];
        double t_diff_dof = t_diff;

        /* Brake pre-trajectory (only in first section, or in each section for waypoints) */
        if (prof->brake.duration > 0.0) {
            if (t_diff_dof < prof->brake.duration) {
                size_t index = (t_diff_dof < prof->brake.t[0]) ? 0 : 1;
                if (index > 0) {
                    t_diff_dof -= prof->brake.t[index - 1];
                }
                t_out[dof] = t_diff_dof;
                p_out[dof] = prof->brake.p[index];
                v_out[dof] = prof->brake.v[index];
                a_out[dof] = prof->brake.a[index];
                j_out[dof] = prof->brake.j[index];
                continue;
            } else {
                t_diff_dof -= prof->brake.duration;
            }
        }

        /* Non-time synchronization: past the end of this DOF's profile */
        if (t_diff_dof >= prof->t_sum[6]) {
            t_out[dof] = t_diff_dof - prof->t_sum[6];
            p_out[dof] = prof->p[7];
            v_out[dof] = prof->v[7];
            a_out[dof] = prof->a[7];
            j_out[dof] = 0.0;
            continue;
        }

        /* Binary search in t_sum[0..6] */
        size_t index_dof = 0;
        {
            size_t lo = 0, hi = 7;
            while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2;
                if (prof->t_sum[mid] <= t_diff_dof) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            index_dof = lo;
        }

        if (index_dof > 0) {
            t_diff_dof -= prof->t_sum[index_dof - 1];
        }

        t_out[dof] = t_diff_dof;
        p_out[dof] = prof->p[index_dof];
        v_out[dof] = prof->v[index_dof];
        a_out[dof] = prof->a[index_dof];
        j_out[dof] = prof->j[index_dof];
    }
}

CRUCKIG_HOT
void cruckig_trajectory_at_time(const CRuckigTrajectory *traj, double time,
                                double * CRUCKIG_RESTRICT new_position,
                                double * CRUCKIG_RESTRICT new_velocity,
                                double * CRUCKIG_RESTRICT new_acceleration,
                                double * CRUCKIG_RESTRICT new_jerk,
                                size_t *new_section)
{
    const size_t dofs = traj->degrees_of_freedom;

    /* Implementation limit: max 16 DOF (stack-allocated work arrays) */
    double t_buf[16], p_buf[16], v_buf[16], a_buf[16], j_buf[16];
    const size_t ndofs = (dofs > 16) ? 16 : dofs;

    state_to_integrate_from(traj, time, new_section, t_buf, p_buf, v_buf, a_buf, j_buf);

    for (size_t dof = 0; dof < ndofs; ++dof) {
        double p_out, v_out, a_out;
        cruckig_integrate(t_buf[dof], p_buf[dof], v_buf[dof], a_buf[dof], j_buf[dof],
                          &p_out, &v_out, &a_out);
        new_position[dof] = p_out;
        new_velocity[dof] = v_out;
        new_acceleration[dof] = a_out;
        if (new_jerk) {
            new_jerk[dof] = j_buf[dof];
        }
    }
}

void cruckig_trajectory_at_time_simple(const CRuckigTrajectory *traj, double time,
                                       double *new_position, double *new_velocity,
                                       double *new_acceleration)
{
    size_t new_section;
    cruckig_trajectory_at_time(traj, time, new_position, new_velocity,
                               new_acceleration, NULL, &new_section);
}

double cruckig_trajectory_get_duration(const CRuckigTrajectory *traj) {
    return traj->duration;
}

size_t cruckig_trajectory_get_intermediate_durations(const CRuckigTrajectory *traj,
                                                     double *out_durations)
{
    for (size_t s = 0; s < traj->num_sections; ++s) {
        out_durations[s] = traj->cumulative_times[s];
    }
    return traj->num_sections;
}

void cruckig_trajectory_get_position_extrema(CRuckigTrajectory *traj) {
    const size_t dofs = traj->degrees_of_freedom;
    for (size_t dof = 0; dof < dofs; ++dof) {
        /* Initialize from first section */
        CRuckigBound bound = cruckig_profile_get_position_extrema(&traj->profiles[dof]);

        /* Merge across all sections */
        for (size_t s = 1; s < traj->num_sections; ++s) {
            double section_start = traj->cumulative_times[s - 1];
            CRuckigBound sb = cruckig_profile_get_position_extrema(
                &traj->profiles[s * dofs + dof]);
            if (sb.min < bound.min) {
                bound.min = sb.min;
                bound.t_min = sb.t_min + section_start;
            }
            if (sb.max > bound.max) {
                bound.max = sb.max;
                bound.t_max = sb.t_max + section_start;
            }
        }

        traj->position_extrema[dof] = bound;
    }
}

bool cruckig_trajectory_get_first_time_at_position(const CRuckigTrajectory *traj,
                                                   size_t dof, double position,
                                                   double *time, double time_after)
{
    if (dof >= traj->degrees_of_freedom) return false;

    const size_t dofs = traj->degrees_of_freedom;

    /* Search through all sections */
    for (size_t s = 0; s < traj->num_sections; ++s) {
        double section_start = (s > 0) ? traj->cumulative_times[s - 1] : 0.0;
        double adjusted_time_after = time_after - section_start;
        if (adjusted_time_after < 0.0) adjusted_time_after = 0.0;

        if (cruckig_profile_get_first_state_at_position(
                &traj->profiles[s * dofs + dof], position, time, adjusted_time_after)) {
            *time += section_start;
            return true;
        }
    }

    return false;
}

void cruckig_trajectory_get_independent_min_durations(const CRuckigTrajectory *traj,
                                                      double *out_durations)
{
    for (size_t dof = 0; dof < traj->degrees_of_freedom; ++dof) {
        out_durations[dof] = traj->independent_min_durations[dof];
    }
}

const CRuckigProfile* cruckig_trajectory_get_profile(const CRuckigTrajectory *traj, size_t dof)
{
    if (dof >= traj->degrees_of_freedom) return NULL;
    return &traj->profiles[dof];
}

const CRuckigProfile* cruckig_trajectory_get_section_profile(const CRuckigTrajectory *traj,
                                                              size_t section, size_t dof)
{
    if (section >= traj->num_sections || dof >= traj->degrees_of_freedom) return NULL;
    return &traj->profiles[section * traj->degrees_of_freedom + dof];
}
