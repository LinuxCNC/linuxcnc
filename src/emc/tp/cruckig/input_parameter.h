/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_INPUT_PARAMETER_H
#define CRUCKIG_INPUT_PARAMETER_H

#include "cruckig_internal.h"
#include "result.h"

typedef struct {
    size_t degrees_of_freedom;

    CRuckigControlInterface control_interface;
    CRuckigSynchronization synchronization;
    CRuckigDurationDiscretization duration_discretization;

    /* Current state */
    double *current_position;
    double *current_velocity;
    double *current_acceleration;

    /* Target state */
    double *target_position;
    double *target_velocity;
    double *target_acceleration;

    /* Kinematic constraints */
    double *max_velocity;
    double *max_acceleration;
    double *max_jerk;

    /* Optional min limits (NULL = use -max) */
    double *min_velocity;     /* NULL or array of dofs */
    double *min_acceleration; /* NULL or array of dofs */

    /* Per-DOF enable flags */
    bool *enabled;

    /* Optional per-DOF control interface / synchronization (NULL = use global) */
    CRuckigControlInterface *per_dof_control_interface; /* NULL or array of dofs */
    CRuckigSynchronization *per_dof_synchronization;    /* NULL or array of dofs */

    /* Optional minimum trajectory duration (-1 = not set) */
    double minimum_duration;
    bool has_minimum_duration;

    /* ---- Pro features ---- */

    /* Intermediate waypoints: flat array of num_waypoints * dofs doubles.
     * Each waypoint is dofs consecutive doubles. NULL if no waypoints. */
    double *intermediate_positions;
    size_t num_intermediate_waypoints;

    /* Per-section kinematic constraints: flat arrays of (num_waypoints+1) * dofs.
     * Section i constraints at offset i*dofs. NULL = use global. */
    double *per_section_max_velocity;
    double *per_section_max_acceleration;
    double *per_section_max_jerk;
    double *per_section_min_velocity;
    double *per_section_min_acceleration;

    /* Per-section position limits: flat arrays of (num_waypoints+1) * dofs. */
    double *per_section_max_position;
    double *per_section_min_position;

    /* Global position limits during trajectory (NULL = no limits) */
    double *max_position;     /* NULL or array of dofs */
    double *min_position;     /* NULL or array of dofs */

    /* Per-section minimum duration: array of (num_waypoints+1). NULL = no constraint. */
    double *per_section_minimum_duration;

    /* Calculation interruption budget in microseconds. 0 = no interruption. */
    double interrupt_calculation_duration;
} CRuckigInputParameter;

CRuckigInputParameter* cruckig_input_create(size_t dofs);
void cruckig_input_destroy(CRuckigInputParameter *inp);
bool cruckig_input_validate(const CRuckigInputParameter *inp,
                            bool check_current_within_limits,
                            bool check_target_within_limits);
bool cruckig_input_is_equal(const CRuckigInputParameter *a, const CRuckigInputParameter *b);
void cruckig_input_copy(CRuckigInputParameter *dst, const CRuckigInputParameter *src);

/* Set intermediate waypoints. Copies the data. positions is num_waypoints * dofs doubles. */
void cruckig_input_set_intermediate_positions(CRuckigInputParameter *inp,
                                               const double *positions,
                                               size_t num_waypoints);

#endif /* CRUCKIG_INPUT_PARAMETER_H */
