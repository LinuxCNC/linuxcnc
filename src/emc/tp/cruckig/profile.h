/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_PROFILE_H
#define CRUCKIG_PROFILE_H

#include "cruckig_internal.h"
#include "brake.h"

/* Constants */
#define PROFILE_V_EPS    1e-12
#define PROFILE_A_EPS    1e-12
#define PROFILE_J_EPS    1e-12
#define PROFILE_P_PREC   1e-8
#define PROFILE_V_PREC   1e-8
#define PROFILE_A_PREC   1e-10
#define PROFILE_T_PREC   1e-12
#define PROFILE_T_MAX    1e12

typedef enum {
    ReachedLimitsACC0_ACC1_VEL = 0,
    ReachedLimitsVEL,
    ReachedLimitsACC0,
    ReachedLimitsACC1,
    ReachedLimitsACC0_ACC1,
    ReachedLimitsACC0_VEL,
    ReachedLimitsACC1_VEL,
    ReachedLimitsNONE
} CRuckigReachedLimits;

typedef enum {
    DirectionUP = 0,
    DirectionDOWN
} CRuckigDirection;

typedef enum {
    ControlSignsUDDU = 0,
    ControlSignsUDUD
} CRuckigControlSigns;

/* Position extrema info */
typedef struct {
    double min, max;
    double t_min, t_max;
} CRuckigBound;

/* Single-DOF kinematic profile */
typedef struct {
    double t[7];
    double t_sum[7];
    double j[7];
    double a[8];
    double v[8];
    double p[8];

    CRuckigBrakeProfile brake;
    CRuckigBrakeProfile accel;

    double pf, vf, af;

    CRuckigReachedLimits limits;
    CRuckigDirection direction;
    CRuckigControlSigns control_signs;
} CRuckigProfile;

void cruckig_profile_init(CRuckigProfile *p);

/* Set boundary conditions */
void cruckig_profile_set_boundary(CRuckigProfile *p, double p0, double v0, double a0,
                          double pf, double vf, double af);
void cruckig_profile_set_boundary_from_profile(CRuckigProfile *p, const CRuckigProfile *src);
void cruckig_profile_set_boundary_for_velocity(CRuckigProfile *p, double p0, double v0, double a0,
                                       double vf, double af);

/* Third-order position check */
bool cruckig_profile_check(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                   bool set_limits, double jf, double vMax, double vMin, double aMax, double aMin);
bool cruckig_profile_check_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                               double tf, double jf, double vMax, double vMin, double aMax, double aMin);
bool cruckig_profile_check_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                    double tf, double jf, double vMax, double vMin, double aMax, double aMin, double jMax);

/* Third-order velocity check */
bool cruckig_profile_check_for_velocity(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                double jf, double aMax, double aMin);
bool cruckig_profile_check_for_velocity_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                            double tf, double jf, double aMax, double aMin);
bool cruckig_profile_check_for_velocity_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                 double tf, double jf, double aMax, double aMin, double jMax);

/* Second-order position check */
bool cruckig_profile_check_for_second_order(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                    double aUp, double aDown, double vMax, double vMin);
bool cruckig_profile_check_for_second_order_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                double tf, double aUp, double aDown, double vMax, double vMin);
bool cruckig_profile_check_for_second_order_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                     double tf, double aUp, double aDown, double vMax, double vMin,
                                                     double aMax, double aMin);

/* Second-order velocity check */
bool cruckig_profile_check_for_second_order_velocity(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                             double aUp);
bool cruckig_profile_check_for_second_order_velocity_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                         double tf, double aUp);
bool cruckig_profile_check_for_second_order_velocity_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                              double tf, double aUp, double aMax, double aMin);

/* First-order position check */
bool cruckig_profile_check_for_first_order(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                   double vUp);
bool cruckig_profile_check_for_first_order_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                               double tf, double vUp);
bool cruckig_profile_check_for_first_order_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                    double tf, double vUp, double vMax, double vMin);

/* Position extrema */
CRuckigBound cruckig_profile_get_position_extrema(const CRuckigProfile *p);

/* First time at position */
bool cruckig_profile_get_first_state_at_position(const CRuckigProfile *p, double pt, double *time, double time_after);

#endif /* CRUCKIG_PROFILE_H */
