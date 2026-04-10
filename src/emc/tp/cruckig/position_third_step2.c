/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "position.h"
#include "block.h"
#include "profile.h"
#include "roots.h"
#include "utils.h"

#define ROOTS_TOLERANCE 1e-14

/* Helper: compute monic derivative of a monic polynomial.
   Input: coeffs[0..n-1] where coeffs[0] == 1.0 (monic).
   Output: deriv[0..n-2] where deriv[0] = 1.0 (monic).
   For i in 1..n-2: deriv[i] = (n-1-i) * coeffs[i] / (n-1)
*/
static void poly_monic_derivative(const double *coeffs, size_t n, double *deriv)
{
    deriv[0] = 1.0;
    for (size_t i = 1; i < n - 1; ++i) {
        deriv[i] = (double)(n - 1 - i) * coeffs[i] / (double)(n - 1);
    }
}

void cruckig_pos3_step2_init(CRuckigPositionThirdOrderStep2 *s,
                     double tf, double p0, double v0, double a0,
                     double pf, double vf, double af,
                     double vMax, double vMin, double aMax, double aMin, double jMax)
{
    s->v0 = v0;
    s->a0 = a0;
    s->tf = tf;
    s->vf = vf;
    s->af = af;
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->_jMax = jMax;

    s->pd = pf - p0;
    s->tf_tf = tf * tf;
    s->tf_p3 = s->tf_tf * tf;
    s->tf_p4 = s->tf_tf * s->tf_tf;

    s->vd = vf - v0;
    s->vd_vd = s->vd * s->vd;
    s->v0_v0 = v0 * v0;
    s->vf_vf = vf * vf;

    s->ad = af - a0;
    s->ad_ad = s->ad * s->ad;
    s->a0_a0 = a0 * a0;
    s->af_af = af * af;

    s->a0_p3 = a0 * s->a0_a0;
    s->a0_p4 = s->a0_a0 * s->a0_a0;
    s->a0_p5 = s->a0_p3 * s->a0_a0;
    s->a0_p6 = s->a0_p4 * s->a0_a0;
    s->af_p3 = af * s->af_af;
    s->af_p4 = s->af_af * s->af_af;
    s->af_p5 = s->af_p3 * s->af_af;
    s->af_p6 = s->af_p4 * s->af_af;

    s->jMax_jMax = jMax * jMax;

    s->g1 = -s->pd + tf * v0;
    s->g2 = -2 * s->pd + tf * (v0 + vf);
}

static bool time_acc0_acc1_vel(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                               double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1;

    /* Profile UDDU, Solution 1 */
    if ((2 * (aMax - aMin) + ad) / jMax < tf) {
        const double h1 = sqrt((a0_p4 + af_p4 - 4 * a0_p3 * (2 * aMax + aMin) / 3 - 4 * af_p3 * (aMax + 2 * aMin) / 3 + 2 * (a0_a0 - af_af) * aMax * aMax + (4 * a0 * aMax - 2 * a0_a0) * (af_af - 2 * af * aMin + (aMin - aMax) * aMin + 2 * jMax * (aMin * tf - vd)) + 2 * af_af * (aMin * aMin + 2 * jMax * (aMax * tf - vd)) + 4 * jMax * (2 * aMin * (af * vd + jMax * g1) + (aMax * aMax - aMin * aMin) * vd + jMax * vd_vd) + 8 * aMax * jMax_jMax * (pd - tf * vf)) / (aMax * aMin) + 4 * af_af + 2 * a0_a0 + (4 * af + aMax - aMin) * (aMax - aMin) + 4 * jMax * (aMin - aMax + jMax * tf - 2 * af) * tf) * fabs(jMax) / jMax;

        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (-(af_af - a0_a0 + 2 * aMax * aMax + aMin * (aMin - 2 * ad - 3 * aMax) + 2 * jMax * (aMin * tf - vd)) + aMin * h1) / (2 * (aMax - aMin) * jMax);
        profile->t[2] = aMax / jMax;
        profile->t[3] = (aMin - aMax + h1) / (2 * jMax);
        profile->t[4] = -aMin / jMax;
        profile->t[5] = tf - (profile->t[0] + profile->t[1] + profile->t[2] + profile->t[3] + 2 * profile->t[4] + af / jMax);
        profile->t[6] = profile->t[4] + af / jMax;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* Profile UDUD */
    if ((-a0 + 4 * aMax - af) / jMax < tf) {
        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (3 * (a0_p4 + af_p4) - 4 * (a0_p3 + af_p3) * aMax - 4 * af_p3 * aMax + 24 * (a0 + af) * aMax * aMax * aMax - 6 * (af_af + a0_a0) * (aMax * aMax - 2 * jMax * vd) + 6 * a0_a0 * (af_af - 2 * af * aMax - 2 * aMax * jMax * tf) - 12 * aMax * aMax * (2 * aMax * aMax - 2 * aMax * jMax * tf + jMax * vd) - 24 * af * aMax * jMax * vd + 12 * jMax_jMax * (2 * aMax * g1 + vd_vd)) / (12 * aMax * jMax * (a0_a0 + af_af - 2 * (a0 + af) * aMax + 2 * (aMax * aMax - aMax * jMax * tf + jMax * vd)));
        profile->t[2] = aMax / jMax;
        profile->t[3] = (-a0_a0 - af_af + 2 * aMax * (a0 + af - 2 * aMax) - 2 * jMax * vd) / (2 * aMax * jMax) + tf;
        profile->t[4] = profile->t[2];
        profile->t[5] = tf - (profile->t[0] + profile->t[1] + profile->t[2] + profile->t[3] + 2 * profile->t[4] - af / jMax);
        profile->t[6] = profile->t[4] - af / jMax;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsACC0_ACC1_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    return false;
}

static bool time_acc1_vel(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                          double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double v0 = s->v0, a0 = s->a0, af = s->af;
    const double tf = s->tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1;

    /* Profile UDDU */
    {
        const double ph1 = a0_a0 + af_af - aMin * (a0 + 2 * af - aMin) - 2 * jMax * (vd - aMin * tf);
        const double ph2 = 2 * aMin * (jMax * g1 + af * vd) - aMin * aMin * vd + jMax * vd_vd;
        const double ph3 = af_af + aMin * (aMin - 2 * af) - 2 * jMax * (vd - aMin * tf);

        double polynom[4];
        polynom[0] = (2 * (2 * a0 - aMin)) / jMax;
        polynom[1] = (4 * a0_a0 + ph1 - 3 * a0 * aMin) / jMax_jMax;
        polynom[2] = (2 * a0 * ph1) / (jMax_jMax * jMax);
        polynom[3] = (3 * (a0_p4 + af_p4) - 4 * (a0_p3 + 2 * af_p3) * aMin + 6 * af_af * (aMin * aMin - 2 * jMax * vd) + 12 * jMax * ph2 + 6 * a0_a0 * ph3) / (12 * jMax_jMax * jMax_jMax);

        const double t_min = -a0 / jMax;
        double t_max_a = (tf + 2 * aMin / jMax - (a0 + af) / jMax) / 2;
        double t_max_b = (aMax - a0) / jMax;
        const double t_max = (t_max_a < t_max_b) ? t_max_a : t_max_b;

        CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t < t_min || t > t_max) {
                continue;
            }

            /* Single Newton step (regarding pd) */
            if (fabs(a0 + jMax * t) > 16 * DBL_EPSILON) {
                const double h0 = jMax * t * t;
                const double orig = -pd + (3 * (a0_p4 + af_p4) - 8 * af_p3 * aMin - 4 * a0_p3 * aMin + 6 * af_af * (aMin * aMin + 2 * jMax * (h0 - vd)) + 6 * a0_a0 * (af_af - 2 * af * aMin + aMin * aMin + 2 * aMin * jMax * (-2 * t + tf) + 2 * jMax * (5 * h0 - vd)) + 24 * a0 * jMax * t * (a0_a0 + af_af - 2 * af * aMin + aMin * aMin + 2 * jMax * (aMin * (-t + tf) + h0 - vd)) - 24 * af * aMin * jMax * (h0 - vd) + 12 * jMax * (aMin * aMin * (h0 - vd) + jMax * (h0 - vd) * (h0 - vd))) / (24 * aMin * jMax_jMax) + h0 * (tf - t) + tf * v0;
                const double deriv = (a0 + jMax * t) * ((a0_a0 + af_af) / (aMin * jMax) + (aMin - a0 - 2 * af) / jMax + (4 * a0 * t + 2 * h0 - 2 * vd) / aMin + 2 * tf - 3 * t);

                t -= orig / deriv;
            }

            {
                const double h1 = -((a0_a0 + af_af) / 2 + jMax * (-vd + 2 * a0 * t + jMax * t * t)) / aMin;

                profile->t[0] = t;
                profile->t[1] = 0;
                profile->t[2] = a0 / jMax + t;
                profile->t[3] = tf - (h1 - aMin + a0 + af) / jMax - 2 * t;
                profile->t[4] = -aMin / jMax;
                profile->t[5] = (h1 + aMin) / jMax;
                profile->t[6] = profile->t[4] + af / jMax;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC1_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }
    }

    /* Profile UDUD */
    {
        const double ph1 = a0_a0 - af_af + (2 * af - a0) * aMax - aMax * aMax - 2 * jMax * (vd - aMax * tf);
        const double ph2 = aMax * aMax + 2 * jMax * vd;
        const double ph3 = af_af + ph2 - 2 * aMax * (af + jMax * tf);
        const double ph4 = 2 * aMax * jMax * g1 + aMax * aMax * vd + jMax * vd_vd;

        double polynom[4];
        polynom[0] = (4 * a0 - 2 * aMax) / jMax;
        polynom[1] = (4 * a0_a0 - 3 * a0 * aMax + ph1) / jMax_jMax;
        polynom[2] = (2 * a0 * ph1) / (jMax_jMax * jMax);
        polynom[3] = (3 * (a0_p4 + af_p4) - 4 * (a0_p3 + 2 * af_p3) * aMax - 24 * af * aMax * jMax * vd + 12 * jMax * ph4 - 6 * a0_a0 * ph3 + 6 * af_af * ph2) / (12 * jMax_jMax * jMax_jMax);

        const double t_min = -a0 / jMax;
        double t_max_a = (tf + ad / jMax - 2 * aMax / jMax) / 2;
        double t_max_b = (aMax - a0) / jMax;
        const double t_max = (t_max_a < t_max_b) ? t_max_a : t_max_b;

        CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t > t_max || t < t_min) {
                continue;
            }

            {
                const double h1 = ((a0_a0 - af_af) / 2 + jMax_jMax * t * t - jMax * (vd - 2 * a0 * t)) / aMax;

                profile->t[0] = t;
                profile->t[1] = 0;
                profile->t[2] = t + a0 / jMax;
                profile->t[3] = tf + (h1 + ad - aMax) / jMax - 2 * t;
                profile->t[4] = aMax / jMax;
                profile->t[5] = -(h1 + aMax) / jMax;
                profile->t[6] = profile->t[4] - af / jMax;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsACC1_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool time_acc0_vel(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                          double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;

    {
        double max_a = (-a0 + aMax) / jMax;
        double max_b = aMax / jMax;
        if (max_a < 0.0) max_a = 0.0;
        if (max_b < 0.0) max_b = 0.0;
        if (tf < max_a + max_b) {
            return false;
        }
    }

    const double ph1 = 12 * jMax * (-aMax * aMax * vd - jMax * vd_vd + 2 * aMax * jMax * (-pd + tf * vf));

    /* Profile UDDU */
    {
        double polynom[4];
        polynom[0] = (2 * aMax) / jMax;
        polynom[1] = (a0_a0 - af_af + 2 * ad * aMax + aMax * aMax + 2 * jMax * (vd - aMax * tf)) / jMax_jMax;
        polynom[2] = 0;
        polynom[3] = -(-3 * (a0_p4 + af_p4) + 4 * (af_p3 + 2 * a0_p3) * aMax - 12 * a0 * aMax * (af_af - 2 * jMax * vd) + 6 * a0_a0 * (af_af - aMax * aMax - 2 * jMax * vd) + 6 * af_af * (aMax * aMax - 2 * aMax * jMax * tf + 2 * jMax * vd) + ph1) / (12 * jMax_jMax * jMax_jMax);

        const double t_min = -af / jMax;
        double t_max_a = tf - (2 * aMax - a0) / jMax;
        double t_max_b = -aMin / jMax;
        const double t_max = (t_max_a < t_max_b) ? t_max_a : t_max_b;

        CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t < t_min || t > t_max) {
                continue;
            }

            /* Single Newton step (regarding pd) */
            if (t > DBL_EPSILON) {
                double h1 = jMax * t * t + vd;
                double orig = (-3 * (a0_p4 + af_p4) + 4 * (af_p3 + 2 * a0_p3) * aMax - 24 * af * aMax * jMax_jMax * t * t - 12 * a0 * aMax * (af_af - 2 * jMax * h1) + 6 * a0_a0 * (af_af - aMax * aMax - 2 * jMax * h1) + 6 * af_af * (aMax * aMax - 2 * aMax * jMax * tf + 2 * jMax * h1) - 12 * jMax * (aMax * aMax * h1 + jMax * h1 * h1 + 2 * aMax * jMax * (pd + jMax * t * t * (t - tf) - tf * vf))) / (24 * aMax * jMax_jMax);
                double deriv = -t * (a0_a0 - af_af + 2 * aMax * (ad - jMax * tf) + aMax * aMax + 3 * aMax * jMax * t + 2 * jMax * h1) / aMax;

                t -= orig / deriv;
            }

            {
                const double h1 = ((a0_a0 - af_af) / 2 + jMax * (jMax * t * t + vd)) / aMax;

                profile->t[0] = (-a0 + aMax) / jMax;
                profile->t[1] = (h1 - aMax) / jMax;
                profile->t[2] = aMax / jMax;
                profile->t[3] = tf - (h1 + ad + aMax) / jMax - 2 * t;
                profile->t[4] = t;
                profile->t[5] = 0;
                profile->t[6] = af / jMax + t;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }
    }

    /* Profile UDUD */
    {
        double polynom[4];
        polynom[0] = (-2 * aMax) / jMax;
        polynom[1] = -(a0_a0 + af_af - 2 * (a0 + af) * aMax + aMax * aMax + 2 * jMax * (vd - aMax * tf)) / jMax_jMax;
        polynom[2] = 0;
        polynom[3] = (3 * (a0_p4 + af_p4) - 4 * (af_p3 + 2 * a0_p3) * aMax + 6 * a0_a0 * (af_af + aMax * aMax + 2 * jMax * vd) - 12 * a0 * aMax * (af_af + 2 * jMax * vd) + 6 * af_af * (aMax * aMax - 2 * aMax * jMax * tf + 2 * jMax * vd) - ph1) / (12 * jMax_jMax * jMax_jMax);

        const double t_min = af / jMax;
        double t_max_a = tf - aMax / jMax;
        double t_max_b = aMax / jMax;
        const double t_max = (t_max_a < t_max_b) ? t_max_a : t_max_b;

        CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t < t_min || t > t_max) {
                continue;
            }

            /* Single Newton step (regarding pd) */
            {
                double h1 = jMax * t * t - vd;
                double orig = -(3 * (a0_p4 + af_p4) - 4 * (2 * a0_p3 + af_p3) * aMax + 24 * af * aMax * jMax_jMax * t * t - 12 * a0 * aMax * (af_af - 2 * jMax * h1) + 6 * a0_a0 * (af_af + aMax * aMax - 2 * jMax * h1) + 6 * af_af * (aMax * aMax - 2 * jMax * (tf * aMax + h1)) + 12 * jMax * (-aMax * aMax * h1 + jMax * h1 * h1 - 2 * aMax * jMax * (-pd + jMax * t * t * (t - tf) + tf * vf))) / (24 * aMax * jMax_jMax);
                double deriv = t * (a0_a0 + af_af - 2 * jMax * h1 - 2 * (a0 + af + jMax * tf) * aMax + aMax * aMax + 3 * aMax * jMax * t) / aMax;

                t -= orig / deriv;
            }

            {
                const double h1 = ((a0_a0 + af_af) / 2 + jMax * (vd - jMax * t * t)) / aMax;

                profile->t[0] = (-a0 + aMax) / jMax;
                profile->t[1] = (h1 - aMax) / jMax;
                profile->t[2] = aMax / jMax;
                profile->t[3] = tf - (h1 - a0 - af + aMax) / jMax - 2 * t;
                profile->t[4] = t;
                profile->t[5] = 0;
                profile->t[6] = -(af / jMax) + t;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsACC0_VEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool time_vel(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                     double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf, tf_tf = s->tf_tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double a0_p5 = s->a0_p5, a0_p6 = s->a0_p6;
    const double af_p6 = s->af_p6;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1;

    double tz_min = -a0 / jMax;
    if (tz_min < 0.0) tz_min = 0.0;
    double tz_max_a = (tf - a0 / jMax) / 2;
    double tz_max_b = (aMax - a0) / jMax;
    const double tz_max = (tz_max_a < tz_max_b) ? tz_max_a : tz_max_b;

    /* Profile UDDU */
    if (fabs(v0) < DBL_EPSILON && fabs(a0) < DBL_EPSILON && fabs(vf) < DBL_EPSILON && fabs(af) < DBL_EPSILON) {
        double polynom[4];
        polynom[0] = 1;
        polynom[1] = -tf / 2;
        polynom[2] = 0;
        polynom[3] = pd / (2 * jMax);

        CRuckigRootSet roots = cruckig_roots_solve_cubic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t > tf / 4) {
                continue;
            }

            /* Single Newton step (regarding pd) */
            if (t > DBL_EPSILON) {
                const double orig = -pd + jMax * t * t * (tf - 2 * t);
                const double deriv = 2 * jMax * t * (tf - 3 * t);
                t -= orig / deriv;
            }

            profile->t[0] = t;
            profile->t[1] = 0;
            profile->t[2] = t;
            profile->t[3] = tf - 4 * t;
            profile->t[4] = t;
            profile->t[5] = 0;
            profile->t[6] = t;

            if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                return true;
            }
        }

    } else {
        const double p1 = af_af - 2 * jMax * (-2 * af * tf + jMax * tf_tf + 3 * vd);
        const double ph1 = af_p3 - 3 * jMax_jMax * g1 - 3 * af * jMax * vd;
        const double ph2 = af_p4 + 8 * af_p3 * jMax * tf + 12 * jMax * (3 * jMax * vd_vd - af_af * vd + 2 * af * jMax * (g1 - tf * vd) - 2 * jMax_jMax * tf * g1);
        const double ph3 = a0 * (af - jMax * tf);
        const double ph4 = jMax * (-ad + jMax * tf);

        /* Find root of 5th order polynom */
        double polynom[6];
        polynom[0] = 1.0;
        polynom[1] = (15 * a0_a0 + af_af + 4 * af * jMax * tf - 16 * ph3 - 2 * jMax * (jMax * tf_tf + 3 * vd)) / (4 * ph4);
        polynom[2] = (29 * a0_p3 - 2 * af_p3 - 33 * a0 * ph3 + 6 * jMax_jMax * g1 + 6 * af * jMax * vd + 6 * a0 * p1) / (6 * jMax * ph4);
        polynom[3] = (61 * a0_p4 - 76 * a0_a0 * ph3 - 16 * a0 * ph1 + 30 * a0_a0 * p1 + ph2) / (24 * jMax_jMax * ph4);
        polynom[4] = (a0 * (7 * a0_p4 - 10 * a0_a0 * ph3 - 4 * a0 * ph1 + 6 * a0_a0 * p1 + ph2)) / (12 * jMax_jMax * jMax * ph4);
        polynom[5] = (7 * a0_p6 + af_p6 - 12 * a0_p4 * ph3 + 48 * af_p3 * jMax_jMax * g1 - 8 * a0_p3 * ph1 - 72 * jMax_jMax * jMax * (jMax * g1 * g1 + vd_vd * vd + 2 * af * g1 * vd) - 6 * af_p4 * jMax * vd + 36 * af_af * jMax_jMax * vd_vd + 9 * a0_p4 * p1 + 3 * a0_a0 * ph2) / (144 * jMax_jMax * jMax_jMax * ph4);

        double deriv[5];
        poly_monic_derivative(polynom, 6, deriv);
        double dderiv[4];
        cruckig_roots_poly_derivative(deriv, 5, dderiv);

        /* Solve 4th order derivative analytically */
        CRuckigRootSet d_extremas = cruckig_roots_solve_quart_monic(dderiv[1] / dderiv[0], dderiv[2] / dderiv[0], dderiv[3] / dderiv[0], 0.0);
        /* Correct: dderiv has 4 coeffs, the quartic monic needs a,b,c,d where leading is 1 */
        /* Actually dderiv[0] is leading coefficient. Let me recompute correctly */
        /* dderiv = derivative of deriv (which has 5 coefficients, monic). So dderiv has 4 coefficients, not monic. */
        /* For solve_quart_monic, we need the monic form: divide by leading coeff */
        if (fabs(dderiv[0]) > DBL_EPSILON) {
            d_extremas = cruckig_roots_solve_quart_monic(dderiv[1] / dderiv[0], dderiv[2] / dderiv[0], dderiv[3] / dderiv[0], 0.0);
        }
        /* Actually poly_derivative of 5-element monic gives 4 elements, which is a cubic.
           We should use solve_cubic instead. */
        /* deriv has 5 elements (degree 4 monic polynomial): deriv[0]=1, ...
           dderiv = derivative of deriv -> 4 elements, degree 3 polynomial
           dderiv[0] = 4*deriv[0] = 4, dderiv[1] = 3*deriv[1], dderiv[2] = 2*deriv[2], dderiv[3] = 1*deriv[3]
           We need to find extremas of deriv, i.e., zeros of dderiv.
           d_extremas should come from solve_cubic or solve_quart depending on degree. */
        /* Let's use solve_quart_monic for degree-4 polynomial deriv (which is the correct one for extremas of the 5th order poly). Wait, deriv is degree 4, so its derivative dderiv is degree 3 (4 coefficients). We should use solve_cubic. */
        /* But the C++ code does: d_extremas = roots::solve_quart_monic(deriv[1], deriv[2], deriv[3], deriv[4]);
           That solves the quartic x^4 + deriv[1]*x^3 + deriv[2]*x^2 + deriv[3]*x + deriv[4] = 0
           which is finding zeros of deriv (the degree 4 monic polynomial). Those are the extremas of the 5th order polynomial. */
        d_extremas = cruckig_roots_solve_quart_monic(deriv[1], deriv[2], deriv[3], deriv[4]);

        {
            double tz_current = tz_min;
            cruckig_root_set_sort(&d_extremas);

            for (size_t idx = 0; idx < d_extremas.size; ++idx) {
                double tz = d_extremas.data[idx];
                if (tz >= tz_max) {
                    continue;
                }

                {
                    const double orig_d = cruckig_roots_poly_eval(deriv, 5, tz);
                    if (fabs(orig_d) > ROOTS_TOLERANCE) {
                        tz -= orig_d / cruckig_roots_poly_eval(dderiv, 4, tz);
                    }
                }

                const double val_new = cruckig_roots_poly_eval(polynom, 6, tz);
                if (fabs(val_new) < 64 * fabs(cruckig_roots_poly_eval(dderiv, 4, tz)) * ROOTS_TOLERANCE) {
                    /* check_root inline for UDDU */
                    double t = tz;
                    {
                        const double h1 = sqrt((a0_a0 + af_af) / (2 * jMax_jMax) + (2 * a0 * t + jMax * t * t - vd) / jMax);
                        const double orig_nr = -pd - (2 * a0_p3 + 4 * af_p3 + 24 * a0 * jMax * t * (af + jMax * (h1 + t - tf)) + 6 * a0_a0 * (af + jMax * (2 * t - tf)) + 6 * (a0_a0 + af_af) * jMax * h1 + 12 * af * jMax * (jMax * t * t - vd) + 12 * jMax_jMax * (jMax * t * t * (h1 + t - tf) - tf * v0 - h1 * vd)) / (12 * jMax_jMax);
                        const double deriv_newton = -(a0 + jMax * t) * (3 * (h1 + t) - 2 * tf + (a0 + 2 * af) / jMax);
                        if (!isnan(orig_nr) && !isnan(deriv_newton) && fabs(deriv_newton) > DBL_EPSILON) {
                            t -= orig_nr / deriv_newton;
                        }
                    }

                    if (t <= tf && !isnan(t)) {
                        const double h1 = sqrt((a0_a0 + af_af) / (2 * jMax_jMax) + (t * (2 * a0 + jMax * t) - vd) / jMax);

                        profile->t[0] = t;
                        profile->t[1] = 0;
                        profile->t[2] = t + a0 / jMax;
                        profile->t[3] = tf - 2 * (t + h1) - (a0 + af) / jMax;
                        profile->t[4] = h1;
                        profile->t[5] = 0;
                        profile->t[6] = h1 + af / jMax;

                        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                            return true;
                        }
                    }
                } else if (cruckig_roots_poly_eval(polynom, 6, tz_current) * val_new < 0) {
                    double t = cruckig_roots_shrink_interval(polynom, 6, tz_current, tz);
                    /* check_root inline for UDDU */
                    {
                        const double h1_sq = (a0_a0 + af_af) / (2 * jMax_jMax) + (2 * a0 * t + jMax * t * t - vd) / jMax;
                        if (h1_sq >= 0) {
                            const double h1 = sqrt(h1_sq);
                            const double orig_nr = -pd - (2 * a0_p3 + 4 * af_p3 + 24 * a0 * jMax * t * (af + jMax * (h1 + t - tf)) + 6 * a0_a0 * (af + jMax * (2 * t - tf)) + 6 * (a0_a0 + af_af) * jMax * h1 + 12 * af * jMax * (jMax * t * t - vd) + 12 * jMax_jMax * (jMax * t * t * (h1 + t - tf) - tf * v0 - h1 * vd)) / (12 * jMax_jMax);
                            const double deriv_newton = -(a0 + jMax * t) * (3 * (h1 + t) - 2 * tf + (a0 + 2 * af) / jMax);
                            if (!isnan(orig_nr) && !isnan(deriv_newton) && fabs(deriv_newton) > DBL_EPSILON) {
                                t -= orig_nr / deriv_newton;
                            }
                        }
                    }

                    if (t <= tf && !isnan(t)) {
                        const double h1 = sqrt((a0_a0 + af_af) / (2 * jMax_jMax) + (t * (2 * a0 + jMax * t) - vd) / jMax);

                        profile->t[0] = t;
                        profile->t[1] = 0;
                        profile->t[2] = t + a0 / jMax;
                        profile->t[3] = tf - 2 * (t + h1) - (a0 + af) / jMax;
                        profile->t[4] = h1;
                        profile->t[5] = 0;
                        profile->t[6] = h1 + af / jMax;

                        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                            return true;
                        }
                    }
                }
                tz_current = tz;
            }

            {
                const double val_max = cruckig_roots_poly_eval(polynom, 6, tz_max);
                if (cruckig_roots_poly_eval(polynom, 6, tz_current) * val_max < 0) {
                    double t = cruckig_roots_shrink_interval(polynom, 6, tz_current, tz_max);
                    {
                        const double h1_sq = (a0_a0 + af_af) / (2 * jMax_jMax) + (2 * a0 * t + jMax * t * t - vd) / jMax;
                        if (h1_sq >= 0) {
                            const double h1 = sqrt(h1_sq);
                            const double orig_nr = -pd - (2 * a0_p3 + 4 * af_p3 + 24 * a0 * jMax * t * (af + jMax * (h1 + t - tf)) + 6 * a0_a0 * (af + jMax * (2 * t - tf)) + 6 * (a0_a0 + af_af) * jMax * h1 + 12 * af * jMax * (jMax * t * t - vd) + 12 * jMax_jMax * (jMax * t * t * (h1 + t - tf) - tf * v0 - h1 * vd)) / (12 * jMax_jMax);
                            const double deriv_newton = -(a0 + jMax * t) * (3 * (h1 + t) - 2 * tf + (a0 + 2 * af) / jMax);
                            if (!isnan(orig_nr) && !isnan(deriv_newton) && fabs(deriv_newton) > DBL_EPSILON) {
                                t -= orig_nr / deriv_newton;
                            }
                        }
                    }

                    if (t <= tf && !isnan(t)) {
                        const double h1 = sqrt((a0_a0 + af_af) / (2 * jMax_jMax) + (t * (2 * a0 + jMax * t) - vd) / jMax);

                        profile->t[0] = t;
                        profile->t[1] = 0;
                        profile->t[2] = t + a0 / jMax;
                        profile->t[3] = tf - 2 * (t + h1) - (a0 + af) / jMax;
                        profile->t[4] = h1;
                        profile->t[5] = 0;
                        profile->t[6] = h1 + af / jMax;

                        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                            return true;
                        }
                    }
                } else if (fabs(val_max) < 8 * DBL_EPSILON) {
                    double t = tz_max;
                    {
                        const double h1_sq = (a0_a0 + af_af) / (2 * jMax_jMax) + (2 * a0 * t + jMax * t * t - vd) / jMax;
                        if (h1_sq >= 0) {
                            const double h1 = sqrt(h1_sq);
                            const double orig_nr = -pd - (2 * a0_p3 + 4 * af_p3 + 24 * a0 * jMax * t * (af + jMax * (h1 + t - tf)) + 6 * a0_a0 * (af + jMax * (2 * t - tf)) + 6 * (a0_a0 + af_af) * jMax * h1 + 12 * af * jMax * (jMax * t * t - vd) + 12 * jMax_jMax * (jMax * t * t * (h1 + t - tf) - tf * v0 - h1 * vd)) / (12 * jMax_jMax);
                            const double deriv_newton = -(a0 + jMax * t) * (3 * (h1 + t) - 2 * tf + (a0 + 2 * af) / jMax);
                            if (!isnan(orig_nr) && !isnan(deriv_newton) && fabs(deriv_newton) > DBL_EPSILON) {
                                t -= orig_nr / deriv_newton;
                            }
                        }
                    }

                    if (t <= tf && !isnan(t)) {
                        const double h1 = sqrt((a0_a0 + af_af) / (2 * jMax_jMax) + (t * (2 * a0 + jMax * t) - vd) / jMax);

                        profile->t[0] = t;
                        profile->t[1] = 0;
                        profile->t[2] = t + a0 / jMax;
                        profile->t[3] = tf - 2 * (t + h1) - (a0 + af) / jMax;
                        profile->t[4] = h1;
                        profile->t[5] = 0;
                        profile->t[6] = h1 + af / jMax;

                        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    /* Profile UDUD */
    {
        const double ph1 = af_af - 2 * jMax * (2 * af * tf + jMax * tf_tf - 3 * vd);
        const double ph2 = af_p3 - 3 * jMax_jMax * g1 + 3 * af * jMax * vd;
        const double ph3 = 2 * jMax * tf * g1 + 3 * vd_vd;
        const double ph4 = af_p4 - 8 * af_p3 * jMax * tf + 12 * jMax * (jMax * ph3 + af_af * vd + 2 * af * jMax * (g1 - tf * vd));
        const double ph5 = af + jMax * tf;

        /* Find root of 6th order polynom */
        double polynom[7];
        polynom[0] = 1.0;
        polynom[1] = (5 * a0 - ph5) / jMax;
        polynom[2] = (39 * a0_a0 - ph1 - 16 * a0 * ph5) / (4 * jMax_jMax);
        polynom[3] = (55 * a0_p3 - 33 * a0_a0 * ph5 - 6 * a0 * ph1 + 2 * ph2) / (6 * jMax_jMax * jMax);
        polynom[4] = (101 * a0_p4 + ph4 - 76 * a0_p3 * ph5 - 30 * a0_a0 * ph1 + 16 * a0 * ph2) / (24 * jMax_jMax * jMax_jMax);
        polynom[5] = (a0 * (11 * a0_p4 + ph4 - 10 * a0_p3 * ph5 - 6 * a0_a0 * ph1 + 4 * a0 * ph2)) / (12 * jMax_jMax * jMax_jMax * jMax);
        polynom[6] = (11 * a0_p6 - af_p6 - 12 * a0_p5 * ph5 - 48 * af_p3 * jMax_jMax * g1 - 9 * a0_p4 * ph1 + 72 * jMax_jMax * jMax * (jMax * g1 * g1 - vd_vd * vd - 2 * af * g1 * vd) - 6 * af_p4 * jMax * vd - 36 * af_af * jMax_jMax * vd_vd + 8 * a0_p3 * ph2 + 3 * a0_a0 * ph4) / (144 * jMax_jMax * jMax_jMax * jMax_jMax);

        double deriv6[6]; /* degree 5 monic poly, derivative of polynom */
        poly_monic_derivative(polynom, 7, deriv6);

        double dderiv6[5]; /* degree 4, derivative of deriv6 */
        poly_monic_derivative(deriv6, 6, dderiv6);

        /* 4th order derivative of dderiv6 (degree 3) */
        double ddderiv6[4];
        cruckig_roots_poly_derivative(dderiv6, 5, ddderiv6);

        /* Solve the 4th order dderiv6 (monic quartic): find zeros of dderiv6 */
        CRuckigRootSet dd_extremas = cruckig_roots_solve_quart_monic(dderiv6[1], dderiv6[2], dderiv6[3], dderiv6[4]);
        cruckig_root_set_sort(&dd_extremas);

        /* Build intervals where deriv6 changes sign */
        typedef struct { double first; double second; } interval_pair;
        interval_pair dd_tz_intervals[6];
        size_t dd_tz_interval_count = 0;

        double dd_tz_current = tz_min;
        for (size_t idx = 0; idx < dd_extremas.size; ++idx) {
            double tz = dd_extremas.data[idx];
            if (tz >= tz_max) {
                continue;
            }

            {
                const double orig_dd = cruckig_roots_poly_eval(dderiv6, 5, tz);
                if (fabs(orig_dd) > ROOTS_TOLERANCE) {
                    tz -= orig_dd / cruckig_roots_poly_eval(ddderiv6, 4, tz);
                }
            }

            if (cruckig_roots_poly_eval(deriv6, 6, dd_tz_current) * cruckig_roots_poly_eval(deriv6, 6, tz) < 0) {
                dd_tz_intervals[dd_tz_interval_count].first = dd_tz_current;
                dd_tz_intervals[dd_tz_interval_count].second = tz;
                dd_tz_interval_count++;
            }
            dd_tz_current = tz;
        }
        if (cruckig_roots_poly_eval(deriv6, 6, dd_tz_current) * cruckig_roots_poly_eval(deriv6, 6, tz_max) < 0) {
            dd_tz_intervals[dd_tz_interval_count].first = dd_tz_current;
            dd_tz_intervals[dd_tz_interval_count].second = tz_max;
            dd_tz_interval_count++;
        }

        double tz_current = tz_min;

        for (size_t int_idx = 0; int_idx < dd_tz_interval_count; ++int_idx) {
            const double tz = cruckig_roots_shrink_interval(deriv6, 6, dd_tz_intervals[int_idx].first, dd_tz_intervals[int_idx].second);

            if (tz >= tz_max) {
                continue;
            }

            const double p_val = cruckig_roots_poly_eval(polynom, 7, tz);
            if (fabs(p_val) < 64 * fabs(cruckig_roots_poly_eval(dderiv6, 5, tz)) * ROOTS_TOLERANCE) {
                /* check_root UDUD inline */
                double t = tz;
                {
                    double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                    double orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                    double deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;

                    t -= orig_nr / deriv_newton;

                    h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                    orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                    if (fabs(orig_nr) > 1e-9) {
                        deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;
                        t -= orig_nr / deriv_newton;
                    }
                }

                {
                    const double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);

                    profile->t[0] = t;
                    profile->t[1] = 0;
                    profile->t[2] = t + a0 / jMax;
                    profile->t[3] = tf - 2 * (t + h1) + ad / jMax;
                    profile->t[4] = h1;
                    profile->t[5] = 0;
                    profile->t[6] = h1 - af / jMax;

                    if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                        return true;
                    }
                }
            } else if (cruckig_roots_poly_eval(polynom, 7, tz_current) * p_val < 0) {
                double t = cruckig_roots_shrink_interval(polynom, 7, tz_current, tz);
                /* check_root UDUD inline */
                {
                    double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                    double orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                    double deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;

                    t -= orig_nr / deriv_newton;

                    h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                    orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                    if (fabs(orig_nr) > 1e-9) {
                        deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;
                        t -= orig_nr / deriv_newton;
                    }
                }

                {
                    const double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);

                    profile->t[0] = t;
                    profile->t[1] = 0;
                    profile->t[2] = t + a0 / jMax;
                    profile->t[3] = tf - 2 * (t + h1) + ad / jMax;
                    profile->t[4] = h1;
                    profile->t[5] = 0;
                    profile->t[6] = h1 - af / jMax;

                    if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                        return true;
                    }
                }
            }
            tz_current = tz;
        }

        if (cruckig_roots_poly_eval(polynom, 7, tz_current) * cruckig_roots_poly_eval(polynom, 7, tz_max) < 0) {
            double t = cruckig_roots_shrink_interval(polynom, 7, tz_current, tz_max);
            {
                double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                double orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                double deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;

                t -= orig_nr / deriv_newton;

                h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);
                orig_nr = -pd + (af_p3 - a0_p3 + 3 * a0_a0 * jMax * (tf - 2 * t)) / (6 * jMax_jMax) + (2 * a0 + jMax * t) * t * (tf - t) + (jMax * h1 - af) * h1 * h1 + tf * v0;
                if (fabs(orig_nr) > 1e-9) {
                    deriv_newton = (a0 + jMax * t) * (2 * (af + jMax * tf) - 3 * jMax * (h1 + t) - a0) / jMax;
                    t -= orig_nr / deriv_newton;
                }
            }

            {
                const double h1 = sqrt((af_af - a0_a0) / (2 * jMax_jMax) - ((2 * a0 + jMax * t) * t - vd) / jMax);

                profile->t[0] = t;
                profile->t[1] = 0;
                profile->t[2] = t + a0 / jMax;
                profile->t[3] = tf - 2 * (t + h1) + ad / jMax;
                profile->t[4] = h1;
                profile->t[5] = 0;
                profile->t[6] = h1 - af / jMax;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsVEL, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool time_acc0_acc1(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                           double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf, tf_tf = s->tf_tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double g1 = s->g1, g2 = s->g2;

    if (fabs(a0) < DBL_EPSILON && fabs(af) < DBL_EPSILON) {
        const double h1 = 2 * aMin * g1 + vd_vd + aMax * (2 * pd + aMin * tf_tf - 2 * tf * vf);
        const double h2 = ((aMax - aMin) * (-aMin * vd + aMax * (aMin * tf - vd)));

        const double jf = h2 / h1;
        profile->t[0] = aMax / jf;
        profile->t[1] = (-2 * aMax * h1 + aMin * aMin * g2) / h2;
        profile->t[2] = profile->t[0];
        profile->t[3] = 0;
        profile->t[4] = -aMin / jf;
        profile->t[5] = tf - (2 * profile->t[0] + profile->t[1] + 2 * profile->t[4]);
        profile->t[6] = profile->t[4];

        return cruckig_profile_check_with_timing_full(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1, tf, jf, vMax, vMin, aMax, aMin, jMax);
    }

    /* UDDU */
    {
        const double h1 = sqrt(144 * cruckig_pow2((aMax - aMin) * (-aMin * vd + aMax * (aMin * tf - vd)) - af_af * (aMax * tf - vd) + 2 * af * aMin * (aMax * tf - vd) + a0_a0 * (aMin * tf + v0 - vf) - 2 * a0 * aMax * (aMin * tf - vd)) + 48 * ad * (3 * a0_p3 - 3 * af_p3 + 12 * aMax * aMin * (-aMax + aMin) + 4 * af_af * (aMax + 2 * aMin) + a0 * (-3 * af_af + 8 * af * (aMin - aMax) + 6 * (aMax * aMax + 2 * aMax * aMin - aMin * aMin)) + 6 * af * (aMax * aMax - 2 * aMax * aMin - aMin * aMin) + a0_a0 * (3 * af - 4 * (2 * aMax + aMin))) * (2 * aMin * g1 + vd * vd + aMax * (2 * pd + aMin * tf * tf - 2 * tf * vf)));

        const double jf = -(3 * af_af * aMax * tf - 3 * a0_a0 * aMin * tf - 6 * ad * aMax * aMin * tf + 3 * aMax * aMin * (aMin - aMax) * tf + 3 * (a0_a0 - af_af) * vd + 6 * vd * (af * aMin - a0 * aMax) + 3 * (aMax * aMax - aMin * aMin) * vd + h1 / 4) / (6 * (2 * aMin * g1 + vd * vd + aMax * (2 * pd + aMin * tf_tf - 2 * tf * vf)));
        profile->t[0] = (aMax - a0) / jf;
        profile->t[1] = (a0_a0 - af_af + 2 * ad * aMin - 2 * (aMax * aMax - 2 * aMax * aMin + aMin * aMin + aMin * jf * tf - jf * vd)) / (2 * (aMax - aMin) * jf);
        profile->t[2] = aMax / jf;
        profile->t[3] = 0;
        profile->t[4] = -aMin / jf;
        profile->t[5] = tf - (profile->t[0] + profile->t[1] + profile->t[2] + 2 * profile->t[4] + af / jf);
        profile->t[6] = profile->t[4] + af / jf;

        if (cruckig_profile_check_with_timing_full(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1, tf, jf, vMax, vMin, aMax, aMin, jMax)) {
            return true;
        }
    }

    return false;
}

static bool time_acc1(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf, tf_tf = s->tf_tf;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double ad = s->ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1, g2 = s->g2;

    /* Case UDDU */
    {
        const double h0 = sqrt(jMax_jMax * (a0_p4 + af_p4 - 4 * af_p3 * jMax * tf + 6 * af_af * jMax_jMax * tf_tf - 4 * a0_p3 * (af - jMax * tf) + 6 * a0_a0 * (af - jMax * tf) * (af - jMax * tf) + 24 * af * jMax_jMax * g1 - 4 * a0 * (af_p3 - 3 * af_af * jMax * tf + 6 * jMax_jMax * (-pd + tf * vf)) - 12 * jMax_jMax * (-vd_vd + jMax * tf * g2)) / 3) / jMax;
        const double h1 = sqrt((a0_a0 + af_af - 2 * a0 * af - 2 * ad * jMax * tf + 2 * h0) / jMax_jMax + tf_tf);

        profile->t[0] = -(a0_a0 + af_af + 2 * a0 * (jMax * tf - af) - 2 * jMax * vd + h0) / (2 * jMax * (-ad + jMax * tf));
        profile->t[1] = 0;
        profile->t[2] = (tf - h1) / 2 - ad / (2 * jMax);
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = h1;
        profile->t[6] = tf - (profile->t[0] + profile->t[2] + profile->t[5]);

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC1, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* Case UDUD */
    {
        const double h0 = sqrt(jMax_jMax * (a0_p4 + af_p4 + 4 * (af_p3 - a0_p3) * jMax * tf + 6 * af_af * jMax_jMax * tf_tf + 6 * a0_a0 * (af + jMax * tf) * (af + jMax * tf) + 24 * af * jMax_jMax * g1 - 4 * a0 * (a0_a0 * af + af_p3 + 3 * af_af * jMax * tf + 6 * jMax_jMax * (-pd + tf * vf)) + 12 * jMax_jMax * (vd_vd + jMax * tf * g2)) / 3) / jMax;
        const double h1 = sqrt((a0_a0 + af_af - 2 * a0 * af + 2 * ad * jMax * tf + 2 * h0) / jMax_jMax + tf_tf);

        profile->t[0] = 0;
        profile->t[1] = 0;
        profile->t[2] = -(a0_a0 + af_af - 2 * a0 * af + 2 * jMax * (vd - a0 * tf) + h0) / (2 * jMax * (ad + jMax * tf));
        profile->t[3] = 0;
        profile->t[4] = ad / (2 * jMax) + (tf - h1) / 2;
        profile->t[5] = h1;
        profile->t[6] = tf - (profile->t[5] + profile->t[4] + profile->t[2]);

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsACC1, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* Case UDDU, Solution 2 */
    {
        const double h0a = a0_p3 - af_p3 - 3 * a0_a0 * aMin + 3 * aMin * aMin * (a0 + jMax * tf) + 3 * af * aMin * (-aMin - 2 * jMax * tf) - 3 * af_af * (-aMin - jMax * tf) - 3 * jMax_jMax * (-2 * pd - aMin * tf_tf + 2 * tf * vf);
        const double h0b = a0_a0 + af_af - 2 * (a0 + af) * aMin + 2 * (aMin * aMin - jMax * (-aMin * tf + vd));
        const double h0c = a0_p4 + 3 * af_p4 - 4 * (a0_p3 + 2 * af_p3) * aMin + 6 * a0_a0 * aMin * aMin + 6 * af_af * (aMin * aMin - 2 * jMax * vd) + 12 * jMax * (2 * aMin * jMax * g1 - aMin * aMin * vd + jMax * vd_vd) + 24 * af * aMin * jMax * vd - 4 * a0 * (af_p3 - 3 * af * aMin * (-aMin - 2 * jMax * tf) + 3 * af_af * (-aMin - jMax * tf) + 3 * jMax * (-aMin * aMin * tf + jMax * (-2 * pd - aMin * tf_tf + 2 * tf * vf)));
        const double h1 = fabs(jMax) / jMax * sqrt(4 * h0a * h0a - 6 * h0b * h0c);
        const double h2 = 6 * jMax * h0b;

        profile->t[0] = 0;
        profile->t[1] = 0;
        profile->t[2] = (2 * h0a + h1) / h2;
        profile->t[3] = -(a0_a0 + af_af - 2 * (a0 + af) * aMin + 2 * (aMin * aMin + aMin * jMax * tf - jMax * vd)) / (2 * jMax * (a0 - aMin - jMax * profile->t[2]));
        profile->t[4] = (a0 - aMin) / jMax - profile->t[2];
        profile->t[5] = tf - (profile->t[2] + profile->t[3] + profile->t[4] + (af - aMin) / jMax);
        profile->t[6] = (af - aMin) / jMax;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC1, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* Case UDUD, Solution 1 */
    {
        const double h0a = -a0_p3 + af_p3 + 3 * (a0_a0 - af_af) * aMax - 3 * ad * aMax * aMax - 6 * af * aMax * jMax * tf + 3 * af_af * jMax * tf + 3 * jMax * (aMax * aMax * tf + jMax * (-2 * pd - aMax * tf_tf + 2 * tf * vf));
        const double h0b = a0_a0 - af_af + 2 * ad * aMax + 2 * jMax * (aMax * tf - vd);
        const double h0c = a0_p4 + 3 * af_p4 - 4 * (a0_p3 + 2 * af_p3) * aMax + 6 * a0_a0 * aMax * aMax - 24 * af * aMax * jMax * vd + 12 * jMax * (2 * aMax * jMax * g1 + jMax * vd_vd + aMax * aMax * vd) + 6 * af_af * (aMax * aMax + 2 * jMax * vd) - 4 * a0 * (af_p3 + 3 * af * aMax * (aMax - 2 * jMax * tf) - 3 * af_af * (aMax - jMax * tf) + 3 * jMax * (aMax * aMax * tf + jMax * (-2 * pd - aMax * tf_tf + 2 * tf * vf)));
        const double h1 = fabs(jMax) / jMax * sqrt(4 * h0a * h0a - 6 * h0b * h0c);
        const double h2 = 6 * jMax * h0b;

        profile->t[0] = 0;
        profile->t[1] = 0;
        profile->t[2] = -(2 * h0a + h1) / h2;
        profile->t[3] = 2 * h1 / h2;
        profile->t[4] = (aMax - a0) / jMax + profile->t[2];
        profile->t[5] = tf - (profile->t[2] + profile->t[3] + profile->t[4] + (-af + aMax) / jMax);
        profile->t[6] = (-af + aMax) / jMax;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsACC1, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }
    return false;
}

static bool time_acc0(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double v0 = s->v0, a0 = s->a0, af = s->af;
    const double tf = s->tf, tf_tf = s->tf_tf;
    const double pd = s->pd;
    const double vd = s->vd;
    const double ad = s->ad, ad_ad = s->ad_ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1, g2 = s->g2;

    /* UDUD */
    {
        const double h1 = sqrt(ad_ad / (2 * jMax_jMax) - ad * (aMax - a0) / (jMax_jMax) + (aMax * tf - vd) / jMax);

        profile->t[0] = (aMax - a0) / jMax;
        profile->t[1] = tf - ad / jMax - 2 * h1;
        profile->t[2] = h1;
        profile->t[3] = 0;
        profile->t[4] = (af - aMax) / jMax + h1;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* UDUD - second */
    {
        const double h0a = -a0_a0 + af_af - 2 * ad * aMax + 2 * jMax * (aMax * tf - vd);
        const double h0b = a0_p3 + 2 * af_p3 - 6 * af_af * aMax - 3 * a0_a0 * (af - jMax * tf) - 3 * a0 * aMax * (aMax - 2 * af + 2 * jMax * tf) - 3 * jMax * (jMax * (-2 * pd + aMax * tf_tf + 2 * tf * v0) + aMax * (aMax * tf - 2 * vd)) + 3 * af * (aMax * aMax + 2 * aMax * jMax * tf - 2 * jMax * vd);
        const double h0 = fabs(jMax) * sqrt(4 * h0b * h0b - 18 * h0a * h0a * h0a);
        const double h1 = 3 * jMax * h0a;

        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (-a0_p3 + af_p3 + af_af * (-6 * aMax + 3 * jMax * tf) + a0_a0 * (-3 * af + 6 * aMax + 3 * jMax * tf) + 6 * af * (aMax * aMax - jMax * vd) + 3 * a0 * (af_af - 2 * (aMax * aMax + jMax * vd)) - 6 * jMax * (aMax * (aMax * tf - 2 * vd) + jMax * g2)) / h1;
        profile->t[2] = -(ad + h0 / h1) / (2 * jMax) + tf / 2 - profile->t[1] / 2;
        profile->t[3] = h0 / (jMax * h1);
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = tf - (profile->t[0] + profile->t[1] + profile->t[2] + profile->t[3]);

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* a3 != 0 */
    /* UDDU Solution 1 */
    {
        const double h0a = a0_p3 + 2 * af_p3 - 6 * (af_af + aMax * aMax) * aMax - 6 * (a0 + af) * aMax * jMax * tf + 9 * aMax * aMax * (af + jMax * tf) + 3 * a0 * aMax * (-2 * af + 3 * aMax) + 3 * a0_a0 * (af - 2 * aMax + jMax * tf) - 6 * jMax_jMax * g1 + 6 * (af - aMax) * jMax * vd - 3 * aMax * jMax_jMax * tf_tf;
        const double h0b = a0_a0 + af_af + 2 * (aMax * aMax - (a0 + af) * aMax + jMax * (vd - aMax * tf));
        const double h1 = fabs(jMax) / jMax * sqrt(4 * h0a * h0a - 18 * h0b * h0b * h0b);
        const double h2 = 6 * jMax * h0b;

        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = ad / jMax - 2 * profile->t[0] - (2 * h0a - h1) / h2 + tf;
        profile->t[2] = -(2 * h0a + h1) / h2;
        profile->t[3] = (2 * h0a - h1) / h2;
        profile->t[4] = tf - (profile->t[0] + profile->t[1] + profile->t[2] + profile->t[3]);
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    return false;
}

static bool time_none(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin, double jMax)
{
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double tf = s->tf, tf_tf = s->tf_tf, tf_p3 = s->tf_p3, tf_p4 = s->tf_p4;
    const double pd = s->pd;
    const double vd = s->vd, vd_vd = s->vd_vd;
    const double vf_vf = s->vf_vf;
    const double ad = s->ad, ad_ad = s->ad_ad;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double a0_p5 = s->a0_p5, a0_p6 = s->a0_p6;
    const double af_p5 = s->af_p5, af_p6 = s->af_p6;
    const double jMax_jMax = s->jMax_jMax;
    const double g1 = s->g1, g2 = s->g2;

    if (fabs(v0) < DBL_EPSILON && fabs(a0) < DBL_EPSILON && fabs(af) < DBL_EPSILON) {
        const double h1 = sqrt(tf_tf * vf_vf + cruckig_pow2(4 * pd - tf * vf));
        const double jf = 4 * (4 * pd - 2 * tf * vf + h1) / tf_p3;

        profile->t[0] = tf / 4;
        profile->t[1] = 0;
        profile->t[2] = 2 * profile->t[0];
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = profile->t[0];

        if (cruckig_profile_check_with_timing_full(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jf, vMax, vMin, aMax, aMin, jMax)) {
            return true;
        }
    }

    if (fabs(a0) < DBL_EPSILON && fabs(af) < DBL_EPSILON) {
        /* Profiles with a3 != 0, Solution UDDU */
        {
            /* First acc, then constant */
            {
                double polynom[4];
                polynom[0] = -2 * tf;
                polynom[1] = 2 * vd / jMax + tf_tf;
                polynom[2] = 4 * (pd - tf * vf) / jMax;
                polynom[3] = (vd_vd + jMax * tf * g2) / (jMax_jMax);

                CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
                cruckig_root_set_sort(&roots);
                for (size_t i = 0; i < roots.size; ++i) {
                    double t = roots.data[i];
                    if (t > tf / 2 || t > (aMax - a0) / jMax) {
                        continue;
                    }

                    /* Single Newton step (regarding pd) */
                    {
                        const double h1 = (jMax * t * (t - tf) + vd) / (jMax * (2 * t - tf));
                        const double h2 = (2 * jMax * t * (t - tf) + jMax * tf_tf - 2 * vd) / (jMax * (2 * t - tf) * (2 * t - tf));
                        const double orig = (-2 * pd + 2 * tf * v0 + h1 * h1 * jMax * (tf - 2 * t) + jMax * tf * (2 * h1 * t - t * t - (h1 - t) * tf)) / 2;
                        const double deriv_n = (jMax * tf * (2 * t - tf) * (h2 - 1)) / 2 + h1 * jMax * (tf - (2 * t - tf) * h2 - h1);

                        t -= orig / deriv_n;
                    }

                    profile->t[0] = t;
                    profile->t[1] = 0;
                    profile->t[2] = (jMax * t * (t - tf) + vd) / (jMax * (2 * t - tf));
                    profile->t[3] = tf - 2 * t;
                    profile->t[4] = t - profile->t[2];
                    profile->t[5] = 0;
                    profile->t[6] = 0;

                    if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
                        return true;
                    }
                }
            }
        }
    }

    /* UDUD T 0246 */
    {
        const double h0 = sqrt(2 * jMax_jMax * (2 * cruckig_pow2(a0_p3 - af_p3 - 3 * af_af * jMax * tf + 9 * af * jMax_jMax * tf_tf - 3 * a0_a0 * (af + jMax * tf) + 3 * a0 * cruckig_pow2(af + jMax * tf) + 3 * jMax_jMax * (8 * pd + jMax * tf_tf * tf - 8 * tf * vf)) - 3 * (a0_a0 + af_af - 2 * af * jMax * tf - 2 * a0 * (af + jMax * tf) - jMax * (jMax * tf_tf + 4 * v0 - 4 * vf)) * (a0_p4 + af_p4 + 4 * af_p3 * jMax * tf + 6 * af_af * jMax_jMax * tf_tf - 3 * jMax_jMax * jMax_jMax * tf_tf * tf_tf - 4 * a0_p3 * (af + jMax * tf) + 6 * a0_a0 * cruckig_pow2(af + jMax * tf) - 12 * af * jMax_jMax * (8 * pd + jMax * tf_tf * tf - 8 * tf * v0) + 48 * jMax_jMax * vd_vd + 48 * jMax_jMax * jMax * tf * g2 - 4 * a0 * (af_p3 + 3 * af_af * jMax * tf - 9 * af * jMax_jMax * tf_tf - 3 * jMax_jMax * (8 * pd + jMax * tf_tf * tf - 8 * tf * vf))))) / jMax;
        const double h1 = 12 * jMax * (-a0_a0 - af_af + 2 * af * jMax * tf + 2 * a0 * (af + jMax * tf) + jMax * (jMax * tf_tf + 4 * v0 - 4 * vf));
        const double h2 = -4 * a0_p3 + 4 * af_p3 + 12 * a0_a0 * af - 12 * a0 * af_af + 48 * jMax_jMax * pd + 12 * (a0_a0 - af_af) * jMax * tf - 24 * jMax_jMax * tf * (v0 + vf) + 24 * ad * jMax * vd;
        const double h3 = 2 * a0_p3 - 2 * af_p3 - 6 * a0_a0 * af + 6 * a0 * af_af;

        profile->t[0] = (h3 - 48 * jMax_jMax * (tf * vf - pd) - 6 * (a0_a0 + af_af) * jMax * tf + 12 * a0 * af * jMax * tf + 6 * (a0 + 3 * af + jMax * tf) * tf_tf * jMax_jMax - h0) / h1;
        profile->t[1] = 0;
        profile->t[2] = (h2 + h0) / h1;
        profile->t[3] = 0;
        profile->t[4] = (-h2 + h0) / h1;
        profile->t[5] = 0;
        profile->t[6] = (-h3 + 48 * jMax_jMax * (tf * v0 - pd) - 6 * (a0_a0 + af_af) * jMax * tf + 12 * a0 * af * jMax * tf + 6 * (af + 3 * a0 + jMax * tf) * tf_tf * jMax_jMax - h0) / h1;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* Profiles with a3 != 0, Solution UDDU */
    {
        /* T 0234 */
        {
            const double ph1 = af + jMax * tf;

            double polynom[4];
            polynom[0] = -2 * (ad + jMax * tf) / jMax;
            polynom[1] = 2 * (a0_a0 + af_af + jMax * (af * tf + vd) - 2 * a0 * ph1) / jMax_jMax + tf_tf;
            polynom[2] = 2 * (a0_p3 - af_p3 - 3 * af_af * jMax * tf + 3 * a0 * ph1 * (ph1 - a0) - 6 * jMax_jMax * (-pd + tf * vf)) / (3 * jMax_jMax * jMax);
            polynom[3] = (a0_p4 + af_p4 + 4 * af_p3 * jMax * tf - 4 * a0_p3 * ph1 + 6 * a0_a0 * ph1 * ph1 + 24 * jMax_jMax * af * g1 - 4 * a0 * (af_p3 + 3 * af_af * jMax * tf + 6 * jMax_jMax * (-pd + tf * vf)) + 6 * jMax_jMax * af_af * tf_tf + 12 * jMax_jMax * (vd_vd + jMax * tf * g2)) / (12 * jMax_jMax * jMax_jMax);

            const double t_min = ad / jMax;
            double t_max_a = (aMax - a0) / jMax;
            double t_max_b = (ad / jMax + tf) / 2;
            const double t_max = (t_max_a < t_max_b) ? t_max_a : t_max_b;

            CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
            cruckig_root_set_sort(&roots);
            for (size_t i = 0; i < roots.size; ++i) {
                double t = roots.data[i];
                if (t < t_min || t > t_max) {
                    continue;
                }

                /* Single Newton step (regarding pd) */
                {
                    const double h0 = jMax * (2 * t - tf) - ad;
                    const double h1_v = (ad_ad - 2 * af * jMax * t + 2 * a0 * jMax * (t - tf) + 2 * jMax * (jMax * t * (t - tf) + vd)) / (2 * jMax * h0);
                    const double h2_v = (-ad_ad + 2 * jMax_jMax * (tf_tf + t * (t - tf)) + (a0 + af) * jMax * tf - ad * h0 - 2 * jMax * vd) / (h0 * h0);
                    const double orig = (-a0_p3 + af_p3 + 3 * ad_ad * jMax * (h1_v - t) + 3 * ad * jMax_jMax * (h1_v - t) * (h1_v - t) - 3 * a0 * af * ad + 3 * jMax_jMax * (a0 * tf_tf - 2 * pd + 2 * tf * v0 + h1_v * h1_v * jMax * (tf - 2 * t) + jMax * tf * (2 * h1_v * t - t * t - (h1_v - t) * tf))) / (6 * jMax_jMax);
                    const double deriv_n = (h0 * (-ad + jMax * tf) * (h2_v - 1)) / (2 * jMax) + h1_v * (-ad + jMax * (tf - h1_v) - h0 * h2_v);

                    t -= orig / deriv_n;
                }

                profile->t[0] = t;
                profile->t[1] = 0;
                profile->t[2] = (ad_ad + 2 * jMax * (-a0 * tf - ad * t + jMax * t * (t - tf) + vd)) / (2 * jMax * (-ad + jMax * (2 * t - tf)));
                profile->t[3] = ad / jMax + tf - 2 * t;
                profile->t[4] = tf - (t + profile->t[2] + profile->t[3]);
                profile->t[5] = 0;
                profile->t[6] = 0;

                if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
                    return true;
                }
            }
        }

        /* T 3456 */
        {
            const double h1_val = 3 * jMax * (ad_ad + 2 * jMax * (a0 * tf - vd));
            const double h2_val = ad_ad + 2 * jMax * (a0 * tf - vd);
            const double h0 = sqrt(4 * cruckig_pow2(2 * (a0_p3 - af_p3) - 6 * a0_a0 * (af - jMax * tf) + 6 * jMax_jMax * g1 + 3 * a0 * (2 * af_af - 2 * jMax * af * tf + jMax_jMax * tf_tf) + 6 * ad * jMax * vd) - 18 * h2_val * h2_val * h2_val) / h1_val * fabs(jMax) / jMax;

            profile->t[0] = 0;
            profile->t[1] = 0;
            profile->t[2] = 0;
            profile->t[3] = (af_p3 - a0_p3 + 3 * (af_af - a0_a0) * jMax * tf - 3 * ad * (a0 * af + 2 * jMax * vd) - 6 * jMax_jMax * g2) / h1_val;
            profile->t[4] = (tf - profile->t[3] - h0) / 2 - ad / (2 * jMax);
            profile->t[5] = h0;
            profile->t[6] = (tf - profile->t[3] + ad / jMax - h0) / 2;

            if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
                return true;
            }
        }

        /* T 2346 */
        {
            const double ph1 = ad_ad + 2 * (af + a0) * jMax * tf - jMax * (jMax * tf_tf + 4 * vd);
            const double ph2 = jMax * tf_tf * g1 - vd * (-2 * pd - tf * v0 + 3 * tf * vf);
            const double ph3 = 5 * af_af - 8 * af * jMax * tf + 2 * jMax * (2 * jMax * tf_tf - vd);
            const double ph4 = jMax_jMax * tf_p4 - 2 * vd_vd + 8 * jMax * tf * (-pd + tf * vf);
            const double ph5 = (5 * af_p4 - 8 * af_p3 * jMax * tf - 12 * af_af * jMax * (jMax * tf_tf + vd) + 24 * af * jMax_jMax * (-2 * pd + jMax * tf_p3 + 2 * tf * vf) - 6 * jMax_jMax * ph4);
            const double ph6 = -vd_vd + jMax * tf * (-2 * pd + 3 * tf * v0 - tf * vf) - af * g2;

            double polynom[4];
            polynom[0] = -(4 * (a0_p3 - af_p3) - 12 * a0_a0 * (af - jMax * tf) + 6 * a0 * (2 * af_af - 2 * af * jMax * tf + jMax * (jMax * tf_tf - 2 * vd)) + 6 * af * jMax * (3 * jMax * tf_tf + 2 * vd) - 6 * jMax_jMax * (-4 * pd + jMax * tf_p3 - 2 * tf * v0 + 6 * tf * vf)) / (3 * jMax * ph1);
            polynom[1] = -(-a0_p4 - af_p4 + 4 * a0_p3 * (af - jMax * tf) + a0_a0 * (-6 * af_af + 8 * af * jMax * tf - 4 * jMax * (jMax * tf_tf - vd)) + 2 * af_af * jMax * (jMax * tf_tf + 2 * vd) - 4 * af * jMax_jMax * (-3 * pd + jMax * tf_p3 + 2 * tf * v0 + tf * vf) + jMax_jMax * (jMax_jMax * tf_p4 - 8 * vd_vd + 4 * jMax * tf * (-3 * pd + tf * v0 + 2 * tf * vf)) + 2 * a0 * (2 * af_p3 - 2 * af_af * jMax * tf + af * jMax * (-3 * jMax * tf_tf - 4 * vd) + jMax_jMax * (-6 * pd + jMax * tf_p3 - 4 * tf * v0 + 10 * tf * vf))) / (jMax_jMax * ph1);
            polynom[2] = -(a0_p5 - af_p5 + af_p4 * jMax * tf - 5 * a0_p4 * (af - jMax * tf) + 2 * a0_p3 * ph3 + 4 * af_p3 * jMax * (jMax * tf_tf + vd) + 12 * jMax_jMax * af * ph6 - 2 * a0_a0 * (5 * af_p3 - 9 * af_af * jMax * tf - 6 * af * jMax * vd + 6 * jMax_jMax * (-2 * pd - tf * v0 + 3 * tf * vf)) - 12 * jMax_jMax * jMax * ph2 + a0 * ph5) / (3 * jMax_jMax * jMax * ph1);
            polynom[3] = -(-a0_p6 - af_p6 + 6 * a0_p5 * (af - jMax * tf) - 48 * af_p3 * jMax_jMax * g1 + 72 * jMax_jMax * jMax * (jMax * g1 * g1 + vd_vd * vd + 2 * af * g1 * vd) - 3 * a0_p4 * ph3 - 36 * af_af * jMax_jMax * vd_vd + 6 * af_p4 * jMax * vd + 4 * a0_p3 * (5 * af_p3 - 9 * af_af * jMax * tf - 6 * af * jMax * vd + 6 * jMax_jMax * (-2 * pd - tf * v0 + 3 * tf * vf)) - 3 * a0_a0 * ph5 + 6 * a0 * (af_p5 - af_p4 * jMax * tf - 4 * af_p3 * jMax * (jMax * tf_tf + vd) + 12 * jMax_jMax * (-af * ph6 + jMax * ph2))) / (18 * jMax_jMax * jMax_jMax * ph1);

            const double t_max_v = (a0 - aMin) / jMax;

            CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
            cruckig_root_set_sort(&roots);
            for (size_t i = 0; i < roots.size; ++i) {
                double t = roots.data[i];
                if (t > t_max_v) {
                    continue;
                }

                /* Single Newton step (regarding pd) */
                {
                    const double h1_v = ad_ad / 2 + jMax * (af * t + (jMax * t - a0) * (t - tf) - vd);
                    const double h2_v = -ad + jMax * (tf - 2 * t);
                    const double h3 = sqrt(h1_v);
                    const double orig = (af_p3 - a0_p3 + 3 * af * jMax * t * (af + jMax * t) + 3 * a0_a0 * (af + jMax * t) - 3 * a0 * (af_af + 2 * af * jMax * t + jMax_jMax * (t * t - tf_tf)) + 3 * jMax_jMax * (-2 * pd + jMax * t * (t - tf) * tf + 2 * tf * v0)) / (6 * jMax_jMax) - h3 * h3 * h3 / (jMax * fabs(jMax)) + ((-ad - jMax * t) * h1_v) / (jMax_jMax);
                    const double deriv_n = (6 * jMax * h2_v * h3 / fabs(jMax) + 2 * (-ad - jMax * tf) * h2_v - 2 * (3 * ad_ad + af * jMax * (8 * t - 2 * tf) + 4 * a0 * jMax * (-2 * t + tf) + 2 * jMax * (jMax * t * (3 * t - 2 * tf) - vd))) / (4 * jMax);

                    t -= orig / deriv_n;
                }

                {
                    const double h1_v = sqrt(2 * ad_ad + 4 * jMax * (ad * t + a0 * tf + jMax * t * (t - tf) - vd)) / fabs(jMax);

                    /* Solution 2 with aPlat */
                    profile->t[0] = 0;
                    profile->t[1] = 0;
                    profile->t[2] = t;
                    profile->t[3] = tf - 2 * t - ad / jMax - h1_v;
                    profile->t[4] = h1_v / 2;
                    profile->t[5] = 0;
                    profile->t[6] = tf - (t + profile->t[3] + profile->t[4]);

                    if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
                        return true;
                    }
                }
            }
        }
    }

    /* Profiles with a3 != 0, Solution UDUD */
    {
        /* T 0124 */
        {
            const double ph0 = -2 * pd - tf * v0 + 3 * tf * vf;
            const double ph1 = -ad + jMax * tf;
            const double ph2 = jMax * tf_tf * g1 - vd * ph0;
            const double ph3 = 5 * af_af + 2 * jMax * (2 * jMax * tf_tf - vd - 4 * af * tf);
            const double ph4 = jMax_jMax * tf_p4 - 2 * vd_vd + 8 * jMax * tf * (-pd + tf * vf);
            const double ph5 = (5 * af_p4 - 8 * af_p3 * jMax * tf - 12 * af_af * jMax * (jMax * tf_tf + vd) + 24 * af * jMax_jMax * (-2 * pd + jMax * tf_p3 + 2 * tf * vf) - 6 * jMax_jMax * ph4);
            const double ph6 = -vd_vd + jMax * tf * (-2 * pd + 3 * tf * v0 - tf * vf);
            const double ph7 = 3 * jMax_jMax * ph1 * ph1;

            double polynom[4];
            polynom[0] = (4 * af * tf - 2 * jMax * tf_tf - 4 * vd) / ph1;
            polynom[1] = (-2 * (a0_p4 + af_p4) + 8 * af_p3 * jMax * tf + 6 * af_af * jMax_jMax * tf_tf + 8 * a0_p3 * (af - jMax * tf) - 12 * a0_a0 * (af - jMax * tf) * (af - jMax * tf) - 12 * af * jMax_jMax * (-pd + jMax * tf_p3 - 2 * tf * v0 + 3 * tf * vf) + 2 * a0 * (4 * af_p3 - 12 * af_af * jMax * tf + 9 * af * jMax_jMax * tf_tf - 3 * jMax_jMax * (2 * pd + jMax * tf_p3 - 2 * tf * vf)) + 3 * jMax_jMax * (jMax_jMax * tf_p4 + 4 * vd_vd - 4 * jMax * tf * (pd + tf * v0 - 2 * tf * vf))) / ph7;
            polynom[2] = (-a0_p5 + af_p5 - af_p4 * jMax * tf + 5 * a0_p4 * (af - jMax * tf) - 2 * a0_p3 * ph3 - 4 * af_p3 * jMax * (jMax * tf_tf + vd) + 12 * af_af * jMax_jMax * g2 - 12 * af * jMax_jMax * ph6 + 2 * a0_a0 * (5 * af_p3 - 9 * af_af * jMax * tf - 6 * af * jMax * vd + 6 * jMax_jMax * ph0) + 12 * jMax_jMax * jMax * ph2 + a0 * (-5 * af_p4 + 8 * af_p3 * jMax * tf + 12 * af_af * jMax * (jMax * tf_tf + vd) - 24 * af * jMax_jMax * (-2 * pd + jMax * tf_p3 + 2 * tf * vf) + 6 * jMax_jMax * ph4)) / (jMax * ph7);
            polynom[3] = -(a0_p6 + af_p6 - 6 * a0_p5 * (af - jMax * tf) + 48 * af_p3 * jMax_jMax * g1 - 72 * jMax_jMax * jMax * (jMax * g1 * g1 + vd_vd * vd + 2 * af * g1 * vd) + 3 * a0_p4 * ph3 - 6 * af_p4 * jMax * vd + 36 * af_af * jMax_jMax * vd_vd - 4 * a0_p3 * (5 * af_p3 - 9 * af_af * jMax * tf - 6 * af * jMax * vd + 6 * jMax_jMax * ph0) + 3 * a0_a0 * ph5 - 6 * a0 * (af_p5 - af_p4 * jMax * tf - 4 * af_p3 * jMax * (jMax * tf_tf + vd) + 12 * jMax_jMax * (af_af * g2 - af * ph6 + jMax * ph2))) / (6 * jMax_jMax * ph7);

            CRuckigRootSet roots = cruckig_roots_solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
            cruckig_root_set_sort(&roots);
            for (size_t i = 0; i < roots.size; ++i) {
                double t = roots.data[i];
                if (t > tf || t > (aMax - a0) / jMax) {
                    continue;
                }

                {
                    const double h1_v = sqrt(ad_ad / (2 * jMax_jMax) + (a0 * (t + tf) - af * t + jMax * t * tf - vd) / jMax);

                    profile->t[0] = t;
                    profile->t[1] = tf - ad / jMax - 2 * h1_v;
                    profile->t[2] = h1_v;
                    profile->t[3] = 0;
                    profile->t[4] = ad / jMax + h1_v - t;
                    profile->t[5] = 0;
                    profile->t[6] = 0;

                    if (cruckig_profile_check_with_timing(profile, ControlSignsUDUD, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
                        return true;
                    }
                }
            }
        }
    }

    /* 3 step profile (ak. UZD), sometimes missed because of numerical errors T 012 */
    {
        const double h1 = sqrt(-ad_ad + jMax * (2 * (a0 + af) * tf - 4 * vd + jMax * tf_tf)) / fabs(jMax);

        profile->t[0] = (tf - h1 + ad / jMax) / 2;
        profile->t[1] = h1;
        profile->t[2] = (tf - h1 - ad / jMax) / 2;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    /* 3 step profile (ak. UZU), sometimes missed because of numerical errors */
    {
        double polynom[4];
        polynom[0] = ad_ad;
        polynom[1] = ad_ad * tf;
        polynom[2] = (a0_a0 + af_af + 10 * a0 * af) * tf_tf + 24 * (tf * (af * v0 - a0 * vf) - pd * ad) + 12 * vd_vd;
        polynom[3] = -3 * tf * ((a0_a0 + af_af + 2 * a0 * af) * tf_tf - 4 * vd * (a0 + af) * tf + 4 * vd_vd);

        CRuckigRootSet roots = cruckig_roots_solve_cubic(polynom[0], polynom[1], polynom[2], polynom[3]);
        cruckig_root_set_sort(&roots);
        for (size_t i = 0; i < roots.size; ++i) {
            double t = roots.data[i];
            if (t > tf) {
                continue;
            }

            const double jf = ad / (tf - t);

            profile->t[0] = (2 * (vd - a0 * tf) + ad * (t - tf)) / (2 * jf * t);
            profile->t[1] = t;
            profile->t[2] = 0;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = tf - (profile->t[0] + profile->t[1]);

            if (cruckig_profile_check_with_timing_full(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jf, vMax, vMin, aMax, aMin, jMax)) {
                return true;
            }
        }
    }

    /* 3 step profile (ak. UDU), sometimes missed because of numerical errors */
    {
        profile->t[0] = (ad_ad / jMax + 2 * (a0 + af) * tf - jMax * tf_tf - 4 * vd) / (4 * (ad - jMax * tf));
        profile->t[1] = 0;
        profile->t[2] = -ad / (2 * jMax) + tf / 2;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = tf - (profile->t[0] + profile->t[2]);

        if (cruckig_profile_check_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, tf, jMax, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    return false;
}

CRUCKIG_HOT
bool cruckig_pos3_step2_get_profile(CRuckigPositionThirdOrderStep2 *s, CRuckigProfile *profile)
{
    /* Test all cases to get ones that match */
    const bool up_first = (s->pd > s->tf * s->v0);
    const double vMax = up_first ? s->_vMax : s->_vMin;
    const double vMin = up_first ? s->_vMin : s->_vMax;
    const double aMax = up_first ? s->_aMax : s->_aMin;
    const double aMin = up_first ? s->_aMin : s->_aMax;
    const double jMax = up_first ? s->_jMax : -s->_jMax;

    return time_acc0_acc1_vel(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_vel(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc0_vel(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc1_vel(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc0_acc1_vel(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_vel(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_acc0_vel(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_acc1_vel(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_acc0_acc1(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc0(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc1(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_none(s, profile, vMax, vMin, aMax, aMin, jMax)
        || time_acc0_acc1(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_acc0(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_acc1(s, profile, vMin, vMax, aMin, aMax, -jMax)
        || time_none(s, profile, vMin, vMax, aMin, aMax, -jMax);
}
