/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#include "input_parameter.h"


static double v_at_a_zero(double v0, double a0, double j) {
    return v0 + (a0 * a0) / (2.0 * j);
}

CRuckigInputParameter* cruckig_input_create(size_t dofs) {
    CRuckigInputParameter *inp = (CRuckigInputParameter*)cruckig_calloc(1, sizeof(CRuckigInputParameter));
    if (!inp) return NULL;

    inp->degrees_of_freedom = dofs;
    inp->control_interface = CRuckigPosition;
    inp->synchronization = CRuckigSyncTime;
    inp->duration_discretization = CRuckigContinuous;

    inp->current_position    = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->current_velocity    = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->current_acceleration = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->target_position     = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->target_velocity     = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->target_acceleration = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->max_velocity        = (double*)cruckig_calloc(dofs, sizeof(double));
    inp->max_acceleration    = (double*)cruckig_malloc(dofs * sizeof(double));
    inp->max_jerk            = (double*)cruckig_malloc(dofs * sizeof(double));
    inp->enabled             = (bool*)cruckig_malloc(dofs * sizeof(bool));

    if (!inp->current_position || !inp->current_velocity || !inp->current_acceleration ||
        !inp->target_position || !inp->target_velocity || !inp->target_acceleration ||
        !inp->max_velocity || !inp->max_acceleration || !inp->max_jerk || !inp->enabled) {
        cruckig_input_destroy(inp);
        return NULL;
    }

    /* Initialize defaults matching C++ */
    for (size_t dof = 0; dof < dofs; ++dof) {
        inp->max_acceleration[dof] = INFINITY;
        inp->max_jerk[dof] = INFINITY;
        inp->enabled[dof] = true;
    }

    inp->min_velocity = NULL;
    inp->min_acceleration = NULL;
    inp->per_dof_control_interface = NULL;
    inp->per_dof_synchronization = NULL;
    inp->minimum_duration = -1.0;
    inp->has_minimum_duration = false;

    /* Pro fields: initialize to defaults */
    inp->intermediate_positions = NULL;
    inp->num_intermediate_waypoints = 0;
    inp->per_section_max_velocity = NULL;
    inp->per_section_max_acceleration = NULL;
    inp->per_section_max_jerk = NULL;
    inp->per_section_min_velocity = NULL;
    inp->per_section_min_acceleration = NULL;
    inp->per_section_max_position = NULL;
    inp->per_section_min_position = NULL;
    inp->max_position = NULL;
    inp->min_position = NULL;
    inp->per_section_minimum_duration = NULL;
    inp->interrupt_calculation_duration = 0.0;

    return inp;
}

void cruckig_input_destroy(CRuckigInputParameter *inp) {
    if (!inp) return;
    cruckig_free(inp->current_position);
    cruckig_free(inp->current_velocity);
    cruckig_free(inp->current_acceleration);
    cruckig_free(inp->target_position);
    cruckig_free(inp->target_velocity);
    cruckig_free(inp->target_acceleration);
    cruckig_free(inp->max_velocity);
    cruckig_free(inp->max_acceleration);
    cruckig_free(inp->max_jerk);
    cruckig_free(inp->enabled);
    cruckig_free(inp->min_velocity);
    cruckig_free(inp->min_acceleration);
    cruckig_free(inp->per_dof_control_interface);
    cruckig_free(inp->per_dof_synchronization);
    /* Pro fields */
    cruckig_free(inp->intermediate_positions);
    cruckig_free(inp->per_section_max_velocity);
    cruckig_free(inp->per_section_max_acceleration);
    cruckig_free(inp->per_section_max_jerk);
    cruckig_free(inp->per_section_min_velocity);
    cruckig_free(inp->per_section_min_acceleration);
    cruckig_free(inp->per_section_max_position);
    cruckig_free(inp->per_section_min_position);
    cruckig_free(inp->max_position);
    cruckig_free(inp->min_position);
    cruckig_free(inp->per_section_minimum_duration);
    cruckig_free(inp);
}

void cruckig_input_set_intermediate_positions(CRuckigInputParameter *inp,
                                               const double *positions,
                                               size_t num_waypoints)
{
    if (!inp) return;
    const size_t dofs = inp->degrees_of_freedom;

    cruckig_free(inp->intermediate_positions);
    if (num_waypoints == 0 || !positions) {
        inp->intermediate_positions = NULL;
        inp->num_intermediate_waypoints = 0;
        return;
    }

    size_t total = num_waypoints * dofs;
    inp->intermediate_positions = (double*)cruckig_malloc(total * sizeof(double));
    memcpy(inp->intermediate_positions, positions, total * sizeof(double));
    inp->num_intermediate_waypoints = num_waypoints;
}

bool cruckig_input_validate(const CRuckigInputParameter *inp,
                            bool check_current_within_limits,
                            bool check_target_within_limits)
{
    if (!inp) return false;
    const size_t dofs = inp->degrees_of_freedom;

    /* Waypoint-specific validation */
    if (inp->num_intermediate_waypoints > 0) {
        /* Waypoints require Position control interface */
        if (inp->control_interface != CRuckigPosition) return false;
        /* Waypoints incompatible with Discrete discretization */
        if (inp->duration_discretization == CRuckigDiscrete) return false;
        /* Waypoints incompatible with minimum_duration */
        if (inp->has_minimum_duration) return false;

        /* Infinite jerk not supported with waypoints */
        for (size_t dof = 0; dof < dofs; ++dof) {
            if (isinf(inp->max_jerk[dof])) return false;
            if (isinf(inp->max_acceleration[dof])) return false;
        }
    }

    for (size_t dof = 0; dof < dofs; ++dof) {
        const double jMax = inp->max_jerk[dof];
        if (isnan(jMax) || jMax < 0.0) return false;

        const double aMax = inp->max_acceleration[dof];
        if (isnan(aMax) || aMax < 0.0) return false;

        const double aMin = inp->min_acceleration ? inp->min_acceleration[dof] : -aMax;
        if (isnan(aMin) || aMin > 0.0) return false;

        const double a0 = inp->current_acceleration[dof];
        if (isnan(a0)) return false;
        const double af = inp->target_acceleration[dof];
        if (isnan(af)) return false;

        if (check_current_within_limits) {
            if (a0 > aMax) return false;
            if (a0 < aMin) return false;
        }
        if (check_target_within_limits) {
            if (af > aMax) return false;
            if (af < aMin) return false;
        }

        const double v0 = inp->current_velocity[dof];
        if (isnan(v0)) return false;
        const double vf = inp->target_velocity[dof];
        if (isnan(vf)) return false;

        CRuckigControlInterface ci = inp->per_dof_control_interface
            ? inp->per_dof_control_interface[dof]
            : inp->control_interface;

        if (ci == CRuckigPosition) {
            const double p0 = inp->current_position[dof];
            if (isnan(p0)) return false;
            const double pf = inp->target_position[dof];
            if (isnan(pf)) return false;

            const double vMax = inp->max_velocity[dof];
            if (isnan(vMax) || vMax < 0.0) return false;

            const double vMin = inp->min_velocity ? inp->min_velocity[dof] : -vMax;
            if (isnan(vMin) || vMin > 0.0) return false;

            if (check_current_within_limits) {
                if (v0 > vMax) return false;
                if (v0 < vMin) return false;
            }
            if (check_target_within_limits) {
                if (vf > vMax) return false;
                if (vf < vMin) return false;
            }

            if (check_current_within_limits) {
                if (a0 > 0 && jMax > 0 && v_at_a_zero(v0, a0, jMax) > vMax)
                    return false;
                if (a0 < 0 && jMax > 0 && v_at_a_zero(v0, a0, -jMax) < vMin)
                    return false;
            }
            if (check_target_within_limits) {
                if (af < 0 && jMax > 0 && v_at_a_zero(vf, af, jMax) > vMax)
                    return false;
                if (af > 0 && jMax > 0 && v_at_a_zero(vf, af, -jMax) < vMin)
                    return false;
            }
        }
    }

    return true;
}

bool cruckig_input_is_equal(const CRuckigInputParameter *a, const CRuckigInputParameter *b) {
    if (!a || !b) return (a == b);
    if (a->degrees_of_freedom != b->degrees_of_freedom) return false;

    const size_t dofs = a->degrees_of_freedom;
    const size_t dsz = dofs * sizeof(double);

    if (memcmp(a->current_position, b->current_position, dsz) != 0) return false;
    if (memcmp(a->current_velocity, b->current_velocity, dsz) != 0) return false;
    if (memcmp(a->current_acceleration, b->current_acceleration, dsz) != 0) return false;
    if (memcmp(a->target_position, b->target_position, dsz) != 0) return false;
    if (memcmp(a->target_velocity, b->target_velocity, dsz) != 0) return false;
    if (memcmp(a->target_acceleration, b->target_acceleration, dsz) != 0) return false;
    if (memcmp(a->max_velocity, b->max_velocity, dsz) != 0) return false;
    if (memcmp(a->max_acceleration, b->max_acceleration, dsz) != 0) return false;
    if (memcmp(a->max_jerk, b->max_jerk, dsz) != 0) return false;

    if (memcmp(a->enabled, b->enabled, dofs * sizeof(bool)) != 0) return false;

    /* Compare optional min_velocity */
    if ((a->min_velocity == NULL) != (b->min_velocity == NULL)) return false;
    if (a->min_velocity && memcmp(a->min_velocity, b->min_velocity, dsz) != 0) return false;

    /* Compare optional min_acceleration */
    if ((a->min_acceleration == NULL) != (b->min_acceleration == NULL)) return false;
    if (a->min_acceleration && memcmp(a->min_acceleration, b->min_acceleration, dsz) != 0) return false;

    /* Compare optional per_dof_control_interface */
    if ((a->per_dof_control_interface == NULL) != (b->per_dof_control_interface == NULL)) return false;
    if (a->per_dof_control_interface &&
        memcmp(a->per_dof_control_interface, b->per_dof_control_interface,
               dofs * sizeof(CRuckigControlInterface)) != 0) return false;

    /* Compare optional per_dof_synchronization */
    if ((a->per_dof_synchronization == NULL) != (b->per_dof_synchronization == NULL)) return false;
    if (a->per_dof_synchronization &&
        memcmp(a->per_dof_synchronization, b->per_dof_synchronization,
               dofs * sizeof(CRuckigSynchronization)) != 0) return false;

    if (a->control_interface != b->control_interface) return false;
    if (a->synchronization != b->synchronization) return false;
    if (a->duration_discretization != b->duration_discretization) return false;

    if (a->has_minimum_duration != b->has_minimum_duration) return false;
    if (a->has_minimum_duration && a->minimum_duration != b->minimum_duration) return false;

    /* Compare Pro fields */
    if (a->num_intermediate_waypoints != b->num_intermediate_waypoints) return false;
    if (a->num_intermediate_waypoints > 0) {
        size_t wp_sz = a->num_intermediate_waypoints * dofs * sizeof(double);
        if (memcmp(a->intermediate_positions, b->intermediate_positions, wp_sz) != 0) return false;
    }

    /* Compare position limits */
    if ((a->max_position == NULL) != (b->max_position == NULL)) return false;
    if (a->max_position && memcmp(a->max_position, b->max_position, dsz) != 0) return false;
    if ((a->min_position == NULL) != (b->min_position == NULL)) return false;
    if (a->min_position && memcmp(a->min_position, b->min_position, dsz) != 0) return false;

    /* Compare per-section constraints */
    size_t nsec = a->num_intermediate_waypoints + 1;
    size_t sec_dsz = nsec * dofs * sizeof(double);

#define CMP_OPT_SEC(field) \
    if ((a->field == NULL) != (b->field == NULL)) return false; \
    if (a->field && memcmp(a->field, b->field, sec_dsz) != 0) return false;

    CMP_OPT_SEC(per_section_max_velocity)
    CMP_OPT_SEC(per_section_max_acceleration)
    CMP_OPT_SEC(per_section_max_jerk)
    CMP_OPT_SEC(per_section_min_velocity)
    CMP_OPT_SEC(per_section_min_acceleration)
    CMP_OPT_SEC(per_section_max_position)
    CMP_OPT_SEC(per_section_min_position)
#undef CMP_OPT_SEC

    if ((a->per_section_minimum_duration == NULL) != (b->per_section_minimum_duration == NULL)) return false;
    if (a->per_section_minimum_duration &&
        memcmp(a->per_section_minimum_duration, b->per_section_minimum_duration,
               nsec * sizeof(double)) != 0) return false;

    if (a->interrupt_calculation_duration != b->interrupt_calculation_duration) return false;

    return true;
}

/* Helper to copy an optional flat array */
static void copy_opt_array(double **dst, const double *src, size_t count) {
    if (src) {
        size_t sz = count * sizeof(double);
        if (!*dst) {
            *dst = (double*)cruckig_malloc(sz);
        }
        memcpy(*dst, src, sz);
    } else {
        cruckig_free(*dst);
        *dst = NULL;
    }
}

void cruckig_input_copy(CRuckigInputParameter *dst, const CRuckigInputParameter *src) {
    if (!dst || !src) return;
    if (dst == src) return;

    const size_t dofs = src->degrees_of_freedom;
    const size_t dsz = dofs * sizeof(double);

    /* dst must already be allocated with same dofs */
    dst->degrees_of_freedom = dofs;
    dst->control_interface = src->control_interface;
    dst->synchronization = src->synchronization;
    dst->duration_discretization = src->duration_discretization;

    memcpy(dst->current_position, src->current_position, dsz);
    memcpy(dst->current_velocity, src->current_velocity, dsz);
    memcpy(dst->current_acceleration, src->current_acceleration, dsz);
    memcpy(dst->target_position, src->target_position, dsz);
    memcpy(dst->target_velocity, src->target_velocity, dsz);
    memcpy(dst->target_acceleration, src->target_acceleration, dsz);
    memcpy(dst->max_velocity, src->max_velocity, dsz);
    memcpy(dst->max_acceleration, src->max_acceleration, dsz);
    memcpy(dst->max_jerk, src->max_jerk, dsz);
    memcpy(dst->enabled, src->enabled, dofs * sizeof(bool));

    copy_opt_array(&dst->min_velocity, src->min_velocity, dofs);
    copy_opt_array(&dst->min_acceleration, src->min_acceleration, dofs);

    /* Handle optional per_dof_control_interface */
    if (src->per_dof_control_interface) {
        if (!dst->per_dof_control_interface) {
            dst->per_dof_control_interface = (CRuckigControlInterface*)cruckig_malloc(dofs * sizeof(CRuckigControlInterface));
        }
        memcpy(dst->per_dof_control_interface, src->per_dof_control_interface,
               dofs * sizeof(CRuckigControlInterface));
    } else {
        cruckig_free(dst->per_dof_control_interface);
        dst->per_dof_control_interface = NULL;
    }

    /* Handle optional per_dof_synchronization */
    if (src->per_dof_synchronization) {
        if (!dst->per_dof_synchronization) {
            dst->per_dof_synchronization = (CRuckigSynchronization*)cruckig_malloc(dofs * sizeof(CRuckigSynchronization));
        }
        memcpy(dst->per_dof_synchronization, src->per_dof_synchronization,
               dofs * sizeof(CRuckigSynchronization));
    } else {
        cruckig_free(dst->per_dof_synchronization);
        dst->per_dof_synchronization = NULL;
    }

    dst->minimum_duration = src->minimum_duration;
    dst->has_minimum_duration = src->has_minimum_duration;

    /* Copy Pro fields */
    if (src->num_intermediate_waypoints > 0 && src->intermediate_positions) {
        size_t wp_sz = src->num_intermediate_waypoints * dofs;
        copy_opt_array(&dst->intermediate_positions, src->intermediate_positions, wp_sz);
        dst->num_intermediate_waypoints = src->num_intermediate_waypoints;
    } else {
        cruckig_free(dst->intermediate_positions);
        dst->intermediate_positions = NULL;
        dst->num_intermediate_waypoints = 0;
    }

    copy_opt_array(&dst->max_position, src->max_position, dofs);
    copy_opt_array(&dst->min_position, src->min_position, dofs);

    /* Per-section arrays */
    size_t nsec = src->num_intermediate_waypoints + 1;
    size_t sec_count = nsec * dofs;

    copy_opt_array(&dst->per_section_max_velocity, src->per_section_max_velocity, sec_count);
    copy_opt_array(&dst->per_section_max_acceleration, src->per_section_max_acceleration, sec_count);
    copy_opt_array(&dst->per_section_max_jerk, src->per_section_max_jerk, sec_count);
    copy_opt_array(&dst->per_section_min_velocity, src->per_section_min_velocity, sec_count);
    copy_opt_array(&dst->per_section_min_acceleration, src->per_section_min_acceleration, sec_count);
    copy_opt_array(&dst->per_section_max_position, src->per_section_max_position, sec_count);
    copy_opt_array(&dst->per_section_min_position, src->per_section_min_position, sec_count);

    if (src->per_section_minimum_duration) {
        copy_opt_array(&dst->per_section_minimum_duration, src->per_section_minimum_duration, nsec);
    } else {
        cruckig_free(dst->per_section_minimum_duration);
        dst->per_section_minimum_duration = NULL;
    }

    dst->interrupt_calculation_duration = src->interrupt_calculation_duration;
}
