/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_CALCULATOR_H
#define CRUCKIG_CALCULATOR_H

#include "cruckig_internal.h"
#include "result.h"
#include "block.h"
#include "input_parameter.h"
#include "trajectory.h"
#include "position.h"
#include "velocity.h"

typedef struct {
    size_t degrees_of_freedom;

    double *new_phase_control;
    double *pd;
    double *possible_t_syncs;
    size_t *idx;

    CRuckigBlock *blocks;
    double *inp_min_velocity;
    double *inp_min_acceleration;
    CRuckigControlInterface *inp_per_dof_control_interface;
    CRuckigSynchronization *inp_per_dof_synchronization;

    /* Scratch space for waypoint calculation */
    CRuckigInputParameter *segment_input;  /* Reusable per-segment input */

    /* Step1 workspace: kept off the stack to stay within kernel frame limits.
     * Only one Step1 type is active at a time, so a union suffices. */
    union {
        CRuckigPositionThirdOrderStep1 pos3_step1;
        CRuckigPositionSecondOrderStep1 pos2_step1;
        CRuckigPositionFirstOrderStep1 pos1_step1;
        CRuckigVelocityThirdOrderStep1 vel3_step1;
        CRuckigVelocitySecondOrderStep1 vel2_step1;
    } step1_workspace;
} CRuckigCalculator;

CRuckigCalculator* cruckig_calculator_create(size_t dofs);
void cruckig_calculator_destroy(CRuckigCalculator *calc);

/* Single-segment calculation (existing, backward compatible) */
CRuckigResult cruckig_calculator_calculate(CRuckigCalculator *calc,
                                           const CRuckigInputParameter *inp,
                                           CRuckigTrajectory *traj,
                                           double delta_time,
                                           bool *was_interrupted);

/* Multi-segment waypoint calculation */
CRuckigResult cruckig_calculator_calculate_waypoints(CRuckigCalculator *calc,
                                                     const CRuckigInputParameter *inp,
                                                     CRuckigTrajectory *traj,
                                                     double delta_time,
                                                     bool *was_interrupted);

/* Continue an interrupted calculation */
CRuckigResult cruckig_calculator_continue(CRuckigCalculator *calc,
                                          const CRuckigInputParameter *inp,
                                          CRuckigTrajectory *traj,
                                          double delta_time,
                                          bool *was_interrupted);

#endif /* CRUCKIG_CALCULATOR_H */
