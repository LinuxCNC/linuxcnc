/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_VELOCITY_H
#define CRUCKIG_VELOCITY_H

#include "cruckig_internal.h"
#include "profile.h"
#include "block.h"

/* ---- Third Order Step 1 ---- */
typedef struct {
    double a0, af;
    double _aMax, _aMin, _jMax;
    double vd;
    CRuckigProfile valid_profiles[3];
} CRuckigVelocityThirdOrderStep1;

void cruckig_vel3_step1_init(CRuckigVelocityThirdOrderStep1 *s,
                     double v0, double a0, double vf, double af,
                     double aMax, double aMin, double jMax);
bool cruckig_vel3_step1_get_profile(CRuckigVelocityThirdOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block);

/* ---- Third Order Step 2 ---- */
typedef struct {
    double a0, tf, af;
    double _aMax, _aMin, _jMax;
    double vd, ad;
} CRuckigVelocityThirdOrderStep2;

void cruckig_vel3_step2_init(CRuckigVelocityThirdOrderStep2 *s,
                     double tf, double v0, double a0, double vf, double af,
                     double aMax, double aMin, double jMax);
bool cruckig_vel3_step2_get_profile(CRuckigVelocityThirdOrderStep2 *s, CRuckigProfile *profile);

/* ---- Second Order Step 1 ---- */
typedef struct {
    double _aMax, _aMin;
    double vd;
} CRuckigVelocitySecondOrderStep1;

void cruckig_vel2_step1_init(CRuckigVelocitySecondOrderStep1 *s,
                     double v0, double vf, double aMax, double aMin);
bool cruckig_vel2_step1_get_profile(CRuckigVelocitySecondOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block);

/* ---- Second Order Step 2 ---- */
typedef struct {
    double tf;
    double _aMax, _aMin;
    double vd;
} CRuckigVelocitySecondOrderStep2;

void cruckig_vel2_step2_init(CRuckigVelocitySecondOrderStep2 *s,
                     double tf, double v0, double vf, double aMax, double aMin);
bool cruckig_vel2_step2_get_profile(CRuckigVelocitySecondOrderStep2 *s, CRuckigProfile *profile);

#endif /* CRUCKIG_VELOCITY_H */
