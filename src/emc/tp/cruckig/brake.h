/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_BRAKE_H
#define CRUCKIG_BRAKE_H

#include "cruckig_internal.h"

/* Two-phase brake profile */
typedef struct {
    double duration;
    double t[2];
    double j[2];
    double a[2];
    double v[2];
    double p[2];
} CRuckigBrakeProfile;

void cruckig_brake_init(CRuckigBrakeProfile *bp);

/* Calculate brake trajectories */
void cruckig_brake_get_position_brake_trajectory(CRuckigBrakeProfile *bp, double v0, double a0,
                                         double vMax, double vMin, double aMax, double aMin, double jMax);
void cruckig_brake_get_second_order_position_brake_trajectory(CRuckigBrakeProfile *bp, double v0,
                                                      double vMax, double vMin, double aMax, double aMin);
void cruckig_brake_get_velocity_brake_trajectory(CRuckigBrakeProfile *bp, double a0,
                                         double aMax, double aMin, double jMax);
void cruckig_brake_get_second_order_velocity_brake_trajectory(CRuckigBrakeProfile *bp);

/* Finalize by integrating */
void cruckig_brake_finalize(CRuckigBrakeProfile *bp, double *ps, double *vs, double *as);
void cruckig_brake_finalize_second_order(CRuckigBrakeProfile *bp, double *ps, double *vs, double *as);

#endif /* CRUCKIG_BRAKE_H */
