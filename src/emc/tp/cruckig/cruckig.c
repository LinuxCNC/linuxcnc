/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "cruckig.h"


static CRuckig* cruckig_create_internal(size_t dofs, double delta_time, size_t max_waypoints) {
    CRuckig *r = (CRuckig*)cruckig_calloc(1, sizeof(CRuckig));
    if (!r) return NULL;

    r->degrees_of_freedom = dofs;
    r->delta_time = delta_time;
    r->max_number_of_waypoints = max_waypoints;

    r->calculator = cruckig_calculator_create(dofs);
    if (!r->calculator) {
        cruckig_free(r);
        return NULL;
    }

    r->current_input = cruckig_input_create(dofs);
    if (!r->current_input) {
        cruckig_calculator_destroy(r->calculator);
        cruckig_free(r);
        return NULL;
    }

    r->current_input_initialized = false;

    return r;
}

CRuckig* cruckig_create(size_t dofs, double delta_time) {
    return cruckig_create_internal(dofs, delta_time, 0);
}

CRuckig* cruckig_create_waypoints(size_t dofs, double delta_time, size_t max_waypoints) {
    return cruckig_create_internal(dofs, delta_time, max_waypoints);
}

void cruckig_destroy(CRuckig *r) {
    if (!r) return;
    cruckig_calculator_destroy(r->calculator);
    cruckig_input_destroy(r->current_input);
    cruckig_free(r);
}

void cruckig_reset(CRuckig *r) {
    if (!r) return;
    r->current_input_initialized = false;
}

static inline bool use_waypoints(const CRuckigInputParameter *input) {
    return input->num_intermediate_waypoints > 0 &&
           input->control_interface == CRuckigPosition;
}

bool cruckig_validate_input(const CRuckig *r, const CRuckigInputParameter *input,
                            bool check_current_within_limits,
                            bool check_target_within_limits)
{
    if (!r || !input) return false;

    if (!cruckig_input_validate(input, check_current_within_limits, check_target_within_limits)) {
        return false;
    }

    if (r->delta_time <= 0.0 && input->duration_discretization != CRuckigContinuous) {
        return false;
    }

    /* Validate waypoint count against max */
    if (input->num_intermediate_waypoints > r->max_number_of_waypoints &&
        r->max_number_of_waypoints > 0) {
        return false;
    }

    return true;
}

static CRuckigResult dispatch_calculate(CRuckig *r, const CRuckigInputParameter *input,
                                        CRuckigTrajectory *trajectory, bool *was_interrupted)
{
    if (use_waypoints(input)) {
        /* Ensure trajectory has enough capacity */
        size_t nsec = input->num_intermediate_waypoints + 1;
        if (!cruckig_trajectory_resize(trajectory, nsec)) {
            return CRuckigError;
        }
        return cruckig_calculator_calculate_waypoints(r->calculator, input, trajectory,
                                                       r->delta_time, was_interrupted);
    } else {
        /* Single-segment: ensure single section */
        if (trajectory->num_sections != 1) {
            cruckig_trajectory_resize(trajectory, 1);
        }
        return cruckig_calculator_calculate(r->calculator, input, trajectory,
                                            r->delta_time, was_interrupted);
    }
}

CRuckigResult cruckig_calculate(CRuckig *r, const CRuckigInputParameter *input,
                                CRuckigTrajectory *trajectory)
{
    if (!r || !input || !trajectory) return CRuckigError;

    if (!cruckig_validate_input(r, input, false, true)) {
        return CRuckigErrorInvalidInput;
    }

    bool was_interrupted = false;
    return dispatch_calculate(r, input, trajectory, &was_interrupted);
}

static double get_time_us(void) {
    /* Timing measurement for interrupt budget feature.
     * Not used by LinuxCNC (only cruckig_update, not cruckig_calculate). */
    return 0.0;
}

CRUCKIG_HOT
CRuckigResult cruckig_update(CRuckig *r, const CRuckigInputParameter *input,
                             CRuckigOutputParameter *output)
{
    if (CRUCKIG_UNLIKELY(!r || !input || !output)) return CRuckigError;

    double start_us = get_time_us();

    output->new_calculation = false;

    CRuckigResult result = CRuckigWorking;
    if (!r->current_input_initialized || !cruckig_input_is_equal(input, r->current_input)) {
        if (!cruckig_validate_input(r, input, false, true)) {
            return CRuckigErrorInvalidInput;
        }

        result = dispatch_calculate(r, input, output->trajectory,
                                    &output->was_calculation_interrupted);
        if (result != CRuckigWorking && result != CRuckigErrorPositionalLimits) {
            return result;
        }

        cruckig_input_copy(r->current_input, input);
        r->current_input_initialized = true;
        output->time = 0.0;
        output->new_section = 0;
        output->new_calculation = true;
    }

    size_t old_section = output->new_section;
    output->time += r->delta_time;
    cruckig_trajectory_at_time(output->trajectory, output->time,
                               output->new_position, output->new_velocity,
                               output->new_acceleration, output->new_jerk,
                               &output->new_section);
    output->did_section_change = (output->new_section > old_section);

    double stop_us = get_time_us();
    output->calculation_duration = stop_us - start_us;

    cruckig_output_pass_to_input(output, r->current_input);

    if (output->time > cruckig_trajectory_get_duration(output->trajectory)) {
        return CRuckigFinished;
    }

    return result;
}
