/********************************************************************
* Description: blendmath.h
*   Circular arc blend math functions
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2014 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef BLENDMATH_H
#define BLENDMATH_H

#include "posemath.h"
#include "tc_types.h"
#include "tp_enums.h"
#include "blendmath_types.h"
#include "pm_vector.h"

double findIntersectionAngle(PmVector const * const u1,
        PmVector const * const u2);

int findAccelScale(PmVector const * const acc,
        PmVector const * const bounds,
        PmVector * const scale);

int findMaxValueOnPlane(PmVector const * plane_envelope_sq,
        PmVector const * bounds,
        double * max_planar_value);

int quadraticFormula(
    double A,
    double B,
    double C,
    double * const root0,
    double * const root1);

double findTrapezoidalDesiredVel(double a_max,
                              double dx,
                              double v_final,
                              double currentvel,
                              double cycle_time);

int blendCheckConsume(BlendParameters * const param,
        double L_prev,
        TC_STRUCT const * const prev_tc, int gap_cycles);

int find_blend_parameters(
    const PmVector * const u_tan1,
    const PmVector * const u_tan2,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound,
    const BlendControls * const controls,
    BlendParameters * const param);

EndCondition checkEndCondition(double cycleTime,
                               double dtg,
                               double currentvel,
                               double v_f,
                               double a_max);

#define PSEUDO_SQRT_EPSILON 0.001
double pseudo_sqrt(double x);

int find_blend_vel_accel_planar_limits(
    PmVector const * const u_tan1,
    PmVector const * const u_tan2,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound,
    double *a_max_planar,
    double *v_max_planar);

void findVMaxByAltitude(PmVector const * const u1,
    PmVector const * const u2, const BlendControls * const controls,
    BlendParameters * const param);

int blendParamInitVelocities(TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double override_allowance,
    BlendControls * const controls);

tp_err_t init_blend_segment_from_points(
    TC_STRUCT * const blend_tc,
    BlendPoints const *points,
    BlendParameters * const param);

// API for biarc blends

const char * biarc_result_to_str(BiarcSolverResults r);

double find_blend_region_from_tolerance(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double tolerance);

double find_blend_region_from_tolerance_simple(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    double tolerance);

tp_err_t find_blend_points_and_tangents(
    double Rb,
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    blend_boundary_t * const out);

int find_blend_intermediate_segments(blend_boundary_t const * const blend_params,
    biarc_control_points_t * const control_pts);

double find_max_blend_region(TC_STRUCT const * const prev_tc, TC_STRUCT const * const tc, double v_goal);

tp_err_t find_blend_size_from_intermediates(blend_boundary_t const * const blend_boundary,
    biarc_control_points_t const * const intermediates,
    double *R_geom, double * arc_len_est);

tp_err_t optimize_biarc_blend_size(TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    const biarc_solver_config_t * const config,
    PmVector const * const vel_bound,
    PmVector const * const acc_bound,
    biarc_solver_results_t * const biarc_results, BlendControls * const controls,
    BlendParameters * const param,
    double cycle_time);

// For testing only
tp_err_t scan_blend_properties(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const tc,
    biarc_solver_config_t const * const config,
    PmVector const * const vel_bound,
    PmVector const * const acc_bound,
    biarc_solver_results_t * const biarc_results,
    BlendParameters * const param,
    double cycle_time,
    double resolution);

int blend_find_arcpoints3(
    BlendPoints * const points,
    biarc_solver_results_t const * const);

int find_biarc_points_from_solution(PmVector const * const u1,
    PmVector const * const u2,
    PmVector const * const P1,
    PmVector const * const P2,
    PmVector const * const P,
    double d,
    PmVector const * const acc_bound,
    PmVector const * const vel_bound, const BlendControls * const controls,
    BlendPoints * const points,
    BlendParameters *const param);

ContinuityCheck calc_C1_continuity(
    TC_STRUCT const * const prev_tc,
    TC_STRUCT const * const next_tc);

#endif
