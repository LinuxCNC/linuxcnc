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

void cruckig_pos3_step1_init(CRuckigPositionThirdOrderStep1 *s,
                     double p0, double v0, double a0,
                     double pf, double vf, double af,
                     double vMax, double vMin, double aMax, double aMin, double jMax)
{
    s->v0 = v0;
    s->a0 = a0;
    s->vf = vf;
    s->af = af;
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->_jMax = jMax;

    s->pd = pf - p0;

    s->v0_v0 = v0 * v0;
    s->vf_vf = vf * vf;

    s->a0_a0 = a0 * a0;
    s->af_af = af * af;

    s->a0_p3 = a0 * s->a0_a0;
    s->a0_p4 = s->a0_a0 * s->a0_a0;
    s->af_p3 = af * s->af_af;
    s->af_p4 = s->af_af * s->af_af;

    s->jMax_jMax = jMax * jMax;
}

/* Helper: add_profile equivalent - increment counter, copy boundary to next */
static inline void add_profile(CRuckigProfile *valid_profiles, size_t *counter, size_t max_profiles)
{
    const size_t prev = *counter;
    ++(*counter);
    if (*counter < max_profiles) {
        cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], &valid_profiles[prev]);
    }
}

static void time_all_vel(CRuckigPositionThirdOrderStep1 *s,
                         CRuckigProfile *valid_profiles, size_t *counter,
                         double vMax, double vMin, double aMax, double aMin, double jMax,
                         bool return_after_found)
{
    CRuckigProfile *profile = &valid_profiles[*counter];
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double v0_v0 = s->v0_v0, vf_vf = s->vf_vf;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    (void)return_after_found;

    /* ACC0_ACC1_VEL */
    profile->t[0] = (-a0 + aMax) / jMax;
    profile->t[1] = (a0_a0 / 2 - aMax * aMax - jMax * (v0 - vMax)) / (aMax * jMax);
    profile->t[2] = aMax / jMax;
    profile->t[3] = (3 * (a0_p4 * aMin - af_p4 * aMax) + 8 * aMax * aMin * (af_p3 - a0_p3 + 3 * jMax * (a0 * v0 - af * vf)) + 6 * a0_a0 * aMin * (aMax * aMax - 2 * jMax * v0) - 6 * af_af * aMax * (aMin * aMin - 2 * jMax * vf) - 12 * jMax * (aMax * aMin * (aMax * (v0 + vMax) - aMin * (vf + vMax) - 2 * jMax * pd) + (aMin - aMax) * jMax * vMax * vMax + jMax * (aMax * vf_vf - aMin * v0_v0))) / (24 * aMax * aMin * jMax_jMax * vMax);
    profile->t[4] = -aMin / jMax;
    profile->t[5] = -(af_af / 2 - aMin * aMin - jMax * (vf - vMax)) / (aMin * jMax);
    profile->t[6] = profile->t[4] + af / jMax;

    if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1_VEL, false, jMax, vMax, vMin, aMax, aMin)) {
        add_profile(valid_profiles, counter, 6);
        return;
    }

    /* ACC1_VEL */
    {
        const double t_acc0 = sqrt(a0_a0 / (2 * jMax_jMax) + (vMax - v0) / jMax);

        profile->t[0] = t_acc0 - a0 / jMax;
        profile->t[1] = 0;
        profile->t[2] = t_acc0;
        profile->t[3] = -(3 * af_p4 - 8 * aMin * (af_p3 - a0_p3) - 24 * aMin * jMax * (a0 * v0 - af * vf) + 6 * af_af * (aMin * aMin - 2 * jMax * vf) - 12 * jMax * (2 * aMin * jMax * pd + aMin * aMin * (vf + vMax) + jMax * (vMax * vMax - vf_vf) + aMin * t_acc0 * (a0_a0 - 2 * jMax * (v0 + vMax)))) / (24 * aMin * jMax_jMax * vMax);

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC1_VEL, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* ACC0_VEL */
    {
        const double t_acc1 = sqrt(af_af / (2 * jMax_jMax) + (vMax - vf) / jMax);

        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (a0_a0 / 2 - aMax * aMax - jMax * (v0 - vMax)) / (aMax * jMax);
        profile->t[2] = aMax / jMax;
        profile->t[3] = (3 * a0_p4 + 8 * aMax * (af_p3 - a0_p3) + 24 * aMax * jMax * (a0 * v0 - af * vf) + 6 * a0_a0 * (aMax * aMax - 2 * jMax * v0) - 12 * jMax * (-2 * aMax * jMax * pd + aMax * aMax * (v0 + vMax) + jMax * (vMax * vMax - v0_v0) + aMax * t_acc1 * (-af_af + 2 * (vf + vMax) * jMax))) / (24 * aMax * jMax_jMax * vMax);
        profile->t[4] = t_acc1;
        profile->t[5] = 0;
        profile->t[6] = t_acc1 + af / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0_VEL, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* VEL */
    {
        const double t_acc0 = sqrt(a0_a0 / (2 * jMax_jMax) + (vMax - v0) / jMax);
        const double t_acc1 = sqrt(af_af / (2 * jMax_jMax) + (vMax - vf) / jMax);

        /* Solution 3/4 */
        profile->t[0] = t_acc0 - a0 / jMax;
        profile->t[1] = 0;
        profile->t[2] = t_acc0;
        profile->t[3] = (af_p3 - a0_p3) / (3 * jMax_jMax * vMax) + (a0 * v0 - af * vf + (af_af * t_acc1 + a0_a0 * t_acc0) / 2) / (jMax * vMax) - (v0 / vMax + 1.0) * t_acc0 - (vf / vMax + 1.0) * t_acc1 + pd / vMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsVEL, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
        }
    }
}

static void time_acc0_acc1(CRuckigPositionThirdOrderStep1 *s,
                           CRuckigProfile *valid_profiles, size_t *counter,
                           double vMax, double vMin, double aMax, double aMin, double jMax,
                           bool return_after_found)
{
    CRuckigProfile *profile = &valid_profiles[*counter];
    const double a0 = s->a0, af = s->af;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double v0 = s->v0, vf = s->vf;
    const double v0_v0 = s->v0_v0, vf_vf = s->vf_vf;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    double h1 = (3 * (af_p4 * aMax - a0_p4 * aMin) + aMax * aMin * (8 * (a0_p3 - af_p3) + 3 * aMax * aMin * (aMax - aMin) + 6 * aMin * af_af - 6 * aMax * a0_a0) + 12 * jMax * (aMax * aMin * ((aMax - 2 * a0) * v0 - (aMin - 2 * af) * vf) + aMin * a0_a0 * v0 - aMax * af_af * vf)) / (3 * (aMax - aMin) * jMax_jMax) + 4 * (aMax * vf_vf - aMin * v0_v0 - 2 * aMin * aMax * pd) / (aMax - aMin);

    if (h1 >= 0) {
        h1 = sqrt(h1) / 2;
        const double h2 = a0_a0 / (2 * aMax * jMax) + (aMin - 2 * aMax) / (2 * jMax) - v0 / aMax;
        const double h3 = -af_af / (2 * aMin * jMax) - (aMax - 2 * aMin) / (2 * jMax) + vf / aMin;

        /* UDDU: Solution 2 */
        if (h2 > h1 / aMax && h3 > -h1 / aMin) {
            profile->t[0] = (-a0 + aMax) / jMax;
            profile->t[1] = h2 - h1 / aMax;
            profile->t[2] = aMax / jMax;
            profile->t[3] = 0;
            profile->t[4] = -aMin / jMax;
            profile->t[5] = h3 + h1 / aMin;
            profile->t[6] = profile->t[4] + af / jMax;

            if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1, true, jMax, vMax, vMin, aMax, aMin)) {
                add_profile(valid_profiles, counter, 6);
                if (return_after_found) {
                    return;
                }
            }
        }

        /* UDDU: Solution 1 */
        profile = &valid_profiles[*counter];
        if (h2 > -h1 / aMax && h3 > h1 / aMin) {
            profile->t[0] = (-a0 + aMax) / jMax;
            profile->t[1] = h2 + h1 / aMax;
            profile->t[2] = aMax / jMax;
            profile->t[3] = 0;
            profile->t[4] = -aMin / jMax;
            profile->t[5] = h3 - h1 / aMin;
            profile->t[6] = profile->t[4] + af / jMax;

            if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0_ACC1, true, jMax, vMax, vMin, aMax, aMin)) {
                add_profile(valid_profiles, counter, 6);
            }
        }
    }
}

static void time_all_none_acc0_acc1(CRuckigPositionThirdOrderStep1 *s,
                                     CRuckigProfile *valid_profiles, size_t *counter,
                                     double vMax, double vMin, double aMax, double aMin, double jMax,
                                     bool return_after_found)
{
    CRuckigProfile *profile = &valid_profiles[*counter];
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double v0_v0 = s->v0_v0, vf_vf = s->vf_vf;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    /* NONE UDDU / UDUD Strategy */
    const double h2_none = (a0_a0 - af_af) / (2 * jMax) + (vf - v0);
    const double h2_h2 = h2_none * h2_none;
    const double t_min_none = (a0 - af) / jMax;
    const double t_max_none = (aMax - aMin) / jMax;

    double polynom_none[4];
    polynom_none[0] = 0;
    polynom_none[1] = -2 * (a0_a0 + af_af - 2 * jMax * (v0 + vf)) / jMax_jMax;
    polynom_none[2] = 4 * (a0_p3 - af_p3 + 3 * jMax * (af * vf - a0 * v0)) / (3 * jMax * jMax_jMax) - 4 * pd / jMax;
    polynom_none[3] = -h2_h2 / jMax_jMax;

    /* ACC0 */
    const double h3_acc0 = (a0_a0 - af_af) / (2 * aMax * jMax) + (vf - v0) / aMax;
    const double t_min_acc0 = (aMax - af) / jMax;
    const double t_max_acc0 = (aMax - aMin) / jMax;

    const double h0_acc0 = 3 * (af_p4 - a0_p4) + 8 * (a0_p3 - af_p3) * aMax + 24 * aMax * jMax * (af * vf - a0 * v0) - 6 * a0_a0 * (aMax * aMax - 2 * jMax * v0) + 6 * af_af * (aMax * aMax - 2 * jMax * vf) + 12 * jMax * (jMax * (vf_vf - v0_v0 - 2 * aMax * pd) - aMax * aMax * (vf - v0));
    const double h2_acc0 = -af_af + aMax * aMax + 2 * jMax * vf;

    double polynom_acc0[4];
    polynom_acc0[0] = -2 * aMax / jMax;
    polynom_acc0[1] = h2_acc0 / jMax_jMax;
    polynom_acc0[2] = 0;
    polynom_acc0[3] = h0_acc0 / (12 * jMax_jMax * jMax_jMax);

    /* ACC1 */
    const double h3_acc1 = -(a0_a0 + af_af) / (2 * jMax * aMin) + aMin / jMax + (vf - v0) / aMin;
    const double t_min_acc1 = (aMin - a0) / jMax;
    const double t_max_acc1 = (aMax - a0) / jMax;

    const double h0_acc1 = (a0_p4 - af_p4) / 4 + 2 * (af_p3 - a0_p3) * aMin / 3 + (a0_a0 - af_af) * aMin * aMin / 2 + jMax * (af_af * vf + a0_a0 * v0 + 2 * aMin * (jMax * pd - a0 * v0 - af * vf) + aMin * aMin * (v0 + vf) + jMax * (v0_v0 - vf_vf));
    const double h2_acc1 = a0_a0 - a0 * aMin + 2 * jMax * v0;

    double polynom_acc1[4];
    polynom_acc1[0] = 2 * (2 * a0 - aMin) / jMax;
    polynom_acc1[1] = (5 * a0_a0 + aMin * (aMin - 6 * a0) + 2 * jMax * v0) / jMax_jMax;
    polynom_acc1[2] = 2 * (a0 - aMin) * h2_acc1 / (jMax_jMax * jMax);
    polynom_acc1[3] = h0_acc1 / (jMax_jMax * jMax_jMax);

    CRuckigRootSet roots_none = cruckig_roots_solve_quart_monic(polynom_none[0], polynom_none[1], polynom_none[2], polynom_none[3]);
    CRuckigRootSet roots_acc0 = cruckig_roots_solve_quart_monic(polynom_acc0[0], polynom_acc0[1], polynom_acc0[2], polynom_acc0[3]);
    CRuckigRootSet roots_acc1 = cruckig_roots_solve_quart_monic(polynom_acc1[0], polynom_acc1[1], polynom_acc1[2], polynom_acc1[3]);

    cruckig_root_set_sort(&roots_none);
    cruckig_root_set_sort(&roots_acc0);
    cruckig_root_set_sort(&roots_acc1);

    for (size_t i = 0; i < roots_none.size; ++i) {
        double t = roots_none.data[i];
        if (t < t_min_none || t > t_max_none) {
            continue;
        }

        /* Single Newton-step (regarding pd) */
        if (t > DBL_EPSILON) {
            const double h1 = jMax * t * t;
            const double orig = -h2_h2 / (4 * jMax * t) + h2_none * (af / jMax + t) + (4 * a0_p3 + 2 * af_p3 - 6 * a0_a0 * (af + 2 * jMax * t) + 12 * (af - a0) * jMax * v0 + 3 * jMax_jMax * (-4 * pd + (h1 + 8 * v0) * t)) / (12 * jMax_jMax);
            const double deriv = h2_none + 2 * v0 - a0_a0 / jMax + h2_h2 / (4 * h1) + (3 * h1) / 4;

            t -= orig / deriv;
        }

        const double h0 = h2_none / (2 * jMax * t);
        profile = &valid_profiles[*counter];
        profile->t[0] = h0 + t / 2 - a0 / jMax;
        profile->t[1] = 0;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = -h0 + t / 2 + af / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            if (return_after_found) {
                return;
            }
        }
    }

    for (size_t i = 0; i < roots_acc0.size; ++i) {
        double t = roots_acc0.data[i];
        if (t < t_min_acc0 || t > t_max_acc0) {
            continue;
        }

        /* Single Newton step (regarding pd) */
        if (t > DBL_EPSILON) {
            const double h1 = jMax * t;
            const double orig = h0_acc0 / (12 * jMax_jMax * t) + t * (h2_acc0 + h1 * (h1 - 2 * aMax));
            const double deriv = 2 * (h2_acc0 + h1 * (2 * h1 - 3 * aMax));

            t -= orig / deriv;
        }

        profile = &valid_profiles[*counter];
        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = h3_acc0 - 2 * t + jMax / aMax * t * t;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = (af - aMax) / jMax + t;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            if (return_after_found) {
                return;
            }
        }
    }

    for (size_t i = 0; i < roots_acc1.size; ++i) {
        double t = roots_acc1.data[i];
        if (t < t_min_acc1 || t > t_max_acc1) {
            continue;
        }

        /* Double Newton step (regarding pd) */
        if (t > DBL_EPSILON) {
            const double h5 = a0_p3 + 2 * jMax * a0 * v0;
            double h1 = jMax * t;
            double orig = -(h0_acc1 / 2 + h1 * (h5 + a0 * (aMin - 2 * h1) * (aMin - h1) + a0_a0 * (5 * h1 / 2 - 2 * aMin) + aMin * aMin * h1 / 2 + jMax * (h1 / 2 - aMin) * (h1 * t + 2 * v0))) / jMax;
            double deriv = (aMin - a0 - h1) * (h2_acc1 + h1 * (4 * a0 - aMin + 2 * h1));
            {
                double correction = orig / deriv;
                if (correction > t) correction = t;
                t -= correction;
            }

            h1 = jMax * t;
            orig = -(h0_acc1 / 2 + h1 * (h5 + a0 * (aMin - 2 * h1) * (aMin - h1) + a0_a0 * (5 * h1 / 2 - 2 * aMin) + aMin * aMin * h1 / 2 + jMax * (h1 / 2 - aMin) * (h1 * t + 2 * v0))) / jMax;

            if (fabs(orig) > 1e-9) {
                deriv = (aMin - a0 - h1) * (h2_acc1 + h1 * (4 * a0 - aMin + 2 * h1));
                t -= orig / deriv;

                h1 = jMax * t;
                orig = -(h0_acc1 / 2 + h1 * (h5 + a0 * (aMin - 2 * h1) * (aMin - h1) + a0_a0 * (5 * h1 / 2 - 2 * aMin) + aMin * aMin * h1 / 2 + jMax * (h1 / 2 - aMin) * (h1 * t + 2 * v0))) / jMax;

                if (fabs(orig) > 1e-9) {
                    deriv = (aMin - a0 - h1) * (h2_acc1 + h1 * (4 * a0 - aMin + 2 * h1));
                    t -= orig / deriv;
                }
            }
        }

        profile = &valid_profiles[*counter];
        profile->t[0] = t;
        profile->t[1] = 0;
        profile->t[2] = (a0 - aMin) / jMax + t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = h3_acc1 - (2 * a0 + jMax * t) * t / aMin;
        profile->t[6] = (af - aMin) / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC1, true, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            if (return_after_found) {
                return;
            }
        }
    }
}

static void time_acc1_vel_two_step(CRuckigPositionThirdOrderStep1 *s,
                                   CRuckigProfile *valid_profiles, size_t *counter,
                                   double vMax, double vMin, double aMax, double aMin, double jMax)
{
    CRuckigProfile *profile = &valid_profiles[*counter];
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double vf_vf = s->vf_vf;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = a0 / jMax;
    profile->t[3] = -(3 * af_p4 - 8 * aMin * (af_p3 - a0_p3) - 24 * aMin * jMax * (a0 * v0 - af * vf) + 6 * af_af * (aMin * aMin - 2 * jMax * vf) - 12 * jMax * (2 * aMin * jMax * pd + aMin * aMin * (vf + vMax) + jMax * (vMax * vMax - vf_vf) + aMin * a0 * (a0_a0 - 2 * jMax * (v0 + vMax)) / jMax)) / (24 * aMin * jMax_jMax * vMax);
    profile->t[4] = -aMin / jMax;
    profile->t[5] = -(af_af / 2 - aMin * aMin + jMax * (vMax - vf)) / (aMin * jMax);
    profile->t[6] = profile->t[4] + af / jMax;

    if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC1_VEL, false, jMax, vMax, vMin, aMax, aMin)) {
        add_profile(valid_profiles, counter, 6);
    }
}

static void time_acc0_two_step(CRuckigPositionThirdOrderStep1 *s,
                               CRuckigProfile *valid_profiles, size_t *counter,
                               double vMax, double vMin, double aMax, double aMin, double jMax)
{
    CRuckigProfile *profile = &valid_profiles[*counter];
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double v0_v0 = s->v0_v0, vf_vf = s->vf_vf;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double a0_p4 = s->a0_p4, af_p4 = s->af_p4;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    /* Two step */
    {
        profile->t[0] = 0;
        profile->t[1] = (af_af - a0_a0 + 2 * jMax * (vf - v0)) / (2 * a0 * jMax);
        profile->t[2] = (a0 - af) / jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* Three step - Removed pf */
    {
        profile = &valid_profiles[*counter];
        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (a0_a0 + af_af - 2 * aMax * aMax + 2 * jMax * (vf - v0)) / (2 * aMax * jMax);
        profile->t[2] = (-af + aMax) / jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* Three step - Removed aMax */
    {
        profile = &valid_profiles[*counter];
        const double h0 = 3 * (af_af - a0_a0 + 2 * jMax * (v0 + vf));
        const double h2 = a0_p3 + 2 * af_p3 + 6 * jMax_jMax * pd + 6 * (af - a0) * jMax * vf - 3 * a0 * af_af;
        const double h1_sq = 2 * (2 * h2 * h2 + h0 * (a0_p4 - 6 * a0_a0 * (af_af + 2 * jMax * vf) + 8 * a0 * (af_p3 + 3 * jMax_jMax * pd + 3 * af * jMax * vf) - 3 * (af_p4 + 4 * af_af * jMax * vf + 4 * jMax_jMax * (vf_vf - v0_v0))));
        const double h1 = sqrt(h1_sq) * fabs(jMax) / jMax;
        profile->t[0] = (4 * af_p3 + 2 * a0_p3 - 6 * a0 * af_af + 12 * jMax_jMax * pd + 12 * (af - a0) * jMax * vf + h1) / (2 * jMax * h0);
        profile->t[1] = -h1 / (jMax * h0);
        profile->t[2] = (-4 * a0_p3 - 2 * af_p3 + 6 * a0_a0 * af + 12 * jMax_jMax * pd - 12 * (af - a0) * jMax * v0 + h1) / (2 * jMax * h0);
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* Three step - t=(aMax - aMin)/jMax */
    {
        profile = &valid_profiles[*counter];
        const double t = (aMax - aMin) / jMax;

        profile->t[0] = (-a0 + aMax) / jMax;
        profile->t[1] = (a0_a0 - af_af) / (2 * aMax * jMax) + (vf - v0 + jMax * t * t) / aMax - 2 * t;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = (af - aMin) / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsACC0, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }
}

static void time_vel_two_step(CRuckigPositionThirdOrderStep1 *s,
                              CRuckigProfile *valid_profiles, size_t *counter,
                              double vMax, double vMin, double aMax, double aMin, double jMax)
{
    CRuckigProfile *profile;
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double af_af = s->af_af;
    const double a0_p3 = s->a0_p3, af_p3 = s->af_p3;
    const double jMax_jMax = s->jMax_jMax;
    const double pd = s->pd;

    const double h1 = sqrt(af_af / (2 * jMax_jMax) + (vMax - vf) / jMax);

    /* Four step - Solution 3/4 */
    {
        profile = &valid_profiles[*counter];
        profile->t[0] = -a0 / jMax;
        profile->t[1] = 0;
        profile->t[2] = 0;
        profile->t[3] = (af_p3 - a0_p3) / (3 * jMax_jMax * vMax) + (a0 * v0 - af * vf + (af_af * h1) / 2) / (jMax * vMax) - (vf / vMax + 1.0) * h1 + pd / vMax;
        profile->t[4] = h1;
        profile->t[5] = 0;
        profile->t[6] = h1 + af / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsVEL, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* Four step */
    {
        profile = &valid_profiles[*counter];
        profile->t[0] = 0;
        profile->t[1] = 0;
        profile->t[2] = a0 / jMax;
        profile->t[3] = (af_p3 - a0_p3) / (3 * jMax_jMax * vMax) + (a0 * v0 - af * vf + (af_af * h1 + a0_p3 / jMax) / 2) / (jMax * vMax) - (v0 / vMax + 1.0) * a0 / jMax - (vf / vMax + 1.0) * h1 + pd / vMax;
        profile->t[4] = h1;
        profile->t[5] = 0;
        profile->t[6] = h1 + af / jMax;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsVEL, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }
}

static void time_none_two_step(CRuckigPositionThirdOrderStep1 *s,
                               CRuckigProfile *valid_profiles, size_t *counter,
                               double vMax, double vMin, double aMax, double aMin, double jMax)
{
    CRuckigProfile *profile;
    const double v0 = s->v0, a0 = s->a0, vf = s->vf, af = s->af;
    const double a0_a0 = s->a0_a0, af_af = s->af_af;

    /* Two step */
    {
        profile = &valid_profiles[*counter];
        const double h0 = sqrt((a0_a0 + af_af) / 2 + jMax * (vf - v0)) * fabs(jMax) / jMax;
        profile->t[0] = (h0 - a0) / jMax;
        profile->t[1] = 0;
        profile->t[2] = (h0 - af) / jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }

    /* Single step */
    {
        profile = &valid_profiles[*counter];
        profile->t[0] = (af - a0) / jMax;
        profile->t[1] = 0;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, jMax, vMax, vMin, aMax, aMin)) {
            add_profile(valid_profiles, counter, 6);
            return;
        }
    }
}

static bool time_all_single_step(CRuckigPositionThirdOrderStep1 *s,
                                 CRuckigProfile *profile, double vMax, double vMin, double aMax, double aMin)
{
    const double v0 = s->v0, a0 = s->a0, af = s->af;
    const double v0_v0 = s->v0_v0;
    const double pd = s->pd;

    if (fabs(af - a0) > DBL_EPSILON) {
        return false;
    }

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = 0;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (fabs(a0) > DBL_EPSILON) {
        const double q = sqrt(2 * a0 * pd + v0_v0);

        /* Solution 1 */
        profile->t[3] = (-v0 + q) / a0;
        if (profile->t[3] >= 0.0 && cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, 0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

        /* Solution 2 */
        profile->t[3] = -(v0 + q) / a0;
        if (profile->t[3] >= 0.0 && cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, 0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

    } else if (fabs(v0) > DBL_EPSILON) {
        profile->t[3] = pd / v0;
        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, 0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

    } else if (fabs(pd) < DBL_EPSILON) {
        if (cruckig_profile_check(profile, ControlSignsUDDU, ReachedLimitsNONE, false, 0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    return false;
}

CRUCKIG_HOT
bool cruckig_pos3_step1_get_profile(CRuckigPositionThirdOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block)
{
    /* Zero-limits special case */
    if (s->_jMax == 0.0 || s->_aMax == 0.0 || s->_aMin == 0.0) {
        CRuckigProfile *p = &block->p_min;
        cruckig_profile_set_boundary_from_profile(p, input);

        if (time_all_single_step(s, p, s->_vMax, s->_vMin, s->_aMax, s->_aMin)) {
            block->t_min = p->t_sum[6] + p->brake.duration + p->accel.duration;
            if (fabs(s->v0) > DBL_EPSILON || fabs(s->a0) > DBL_EPSILON) {
                block->a.valid = true;
                block->a.left = block->t_min;
                block->a.right = INFINITY;
            }
            return true;
        }
        return false;
    }

    size_t valid_profile_counter = 0;
    cruckig_profile_set_boundary_from_profile(&s->valid_profiles[0], input);

    if (fabs(s->vf) < DBL_EPSILON && fabs(s->af) < DBL_EPSILON) {
        const double vMax = (s->pd >= 0) ? s->_vMax : s->_vMin;
        const double vMin = (s->pd >= 0) ? s->_vMin : s->_vMax;
        const double aMax = (s->pd >= 0) ? s->_aMax : s->_aMin;
        const double aMin = (s->pd >= 0) ? s->_aMin : s->_aMax;
        const double jMax = (s->pd >= 0) ? s->_jMax : -s->_jMax;

        if (fabs(s->v0) < DBL_EPSILON && fabs(s->a0) < DBL_EPSILON && fabs(s->pd) < DBL_EPSILON) {
            time_all_none_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, jMax, true);

        } else {
            /* There is no blocked interval when vf==0 && af==0, so return after first found profile */
            time_all_vel(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, jMax, true);
            if (valid_profile_counter > 0) { goto return_block; }
            time_all_none_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, jMax, true);
            if (valid_profile_counter > 0) { goto return_block; }
            time_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, jMax, true);
            if (valid_profile_counter > 0) { goto return_block; }

            time_all_vel(s, s->valid_profiles, &valid_profile_counter, vMin, vMax, aMin, aMax, -jMax, true);
            if (valid_profile_counter > 0) { goto return_block; }
            time_all_none_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, vMin, vMax, aMin, aMax, -jMax, true);
            if (valid_profile_counter > 0) { goto return_block; }
            time_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, vMin, vMax, aMin, aMax, -jMax, true);
        }

    } else {
        time_all_none_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax, false);
        time_all_none_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax, false);
        time_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax, false);
        time_acc0_acc1(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax, false);
        time_all_vel(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax, false);
        time_all_vel(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax, false);
    }

    if (valid_profile_counter == 0) {
        time_none_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_none_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_vel_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_vel_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc1_vel_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, s->_jMax);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc1_vel_two_step(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, -s->_jMax);
    }

return_block:
    return cruckig_block_calculate(block, s->valid_profiles, valid_profile_counter, 6);
}
