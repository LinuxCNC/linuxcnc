/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_POSITION_H
#define CRUCKIG_POSITION_H

#include "cruckig_internal.h"
#include "profile.h"
#include "block.h"

/* ---- Third Order Step 1 ---- */
typedef struct {
    double v0, a0, vf, af;
    double _vMax, _vMin, _aMax, _aMin, _jMax;
    double pd;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4;
    double af_af, af_p3, af_p4;
    double jMax_jMax;
    CRuckigProfile valid_profiles[6];
} CRuckigPositionThirdOrderStep1;

void cruckig_pos3_step1_init(CRuckigPositionThirdOrderStep1 *s,
                     double p0, double v0, double a0,
                     double pf, double vf, double af,
                     double vMax, double vMin, double aMax, double aMin, double jMax);
bool cruckig_pos3_step1_get_profile(CRuckigPositionThirdOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block);

/* ---- Third Order Step 2 ---- */
typedef struct {
    double v0, a0, tf, vf, af;
    double _vMax, _vMin, _aMax, _aMin, _jMax;
    double pd;
    double tf_tf, tf_p3, tf_p4;
    double vd, vd_vd;
    double ad, ad_ad;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4, a0_p5, a0_p6;
    double af_af, af_p3, af_p4, af_p5, af_p6;
    double jMax_jMax;
    double g1, g2;
} CRuckigPositionThirdOrderStep2;

void cruckig_pos3_step2_init(CRuckigPositionThirdOrderStep2 *s,
                     double tf, double p0, double v0, double a0,
                     double pf, double vf, double af,
                     double vMax, double vMin, double aMax, double aMin, double jMax);
bool cruckig_pos3_step2_get_profile(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile);

/* ---- Second Order Step 1 ---- */
typedef struct {
    double v0, vf;
    double _vMax, _vMin, _aMax, _aMin;
    double pd;
    CRuckigProfile valid_profiles[4];
} CRuckigPositionSecondOrderStep1;

void cruckig_pos2_step1_init(CRuckigPositionSecondOrderStep1 *s,
                     double p0, double v0, double pf, double vf,
                     double vMax, double vMin, double aMax, double aMin);
bool cruckig_pos2_step1_get_profile(CRuckigPositionSecondOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block);

/* ---- Second Order Step 2 ---- */
typedef struct {
    double v0, tf, vf;
    double _vMax, _vMin, _aMax, _aMin;
    double pd, vd;
} CRuckigPositionSecondOrderStep2;

void cruckig_pos2_step2_init(CRuckigPositionSecondOrderStep2 *s,
                     double tf, double p0, double v0, double pf, double vf,
                     double vMax, double vMin, double aMax, double aMin);
bool cruckig_pos2_step2_get_profile(CRuckigPositionSecondOrderStep2 *s, CRuckigProfile *profile);

/* ---- First Order Step 1 ---- */
typedef struct {
    double _vMax, _vMin;
    double pd;
} CRuckigPositionFirstOrderStep1;

void cruckig_pos1_step1_init(CRuckigPositionFirstOrderStep1 *s,
                     double p0, double pf, double vMax, double vMin);
bool cruckig_pos1_step1_get_profile(CRuckigPositionFirstOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block);

/* ---- First Order Step 2 ---- */
typedef struct {
    double tf;
    double _vMax, _vMin;
    double pd;
} CRuckigPositionFirstOrderStep2;

void cruckig_pos1_step2_init(CRuckigPositionFirstOrderStep2 *s,
                     double tf, double p0, double pf, double vMax, double vMin);
bool cruckig_pos1_step2_get_profile(CRuckigPositionFirstOrderStep2 *s, CRuckigProfile *profile);

#endif /* CRUCKIG_POSITION_H */
