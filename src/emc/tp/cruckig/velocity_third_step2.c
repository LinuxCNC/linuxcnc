/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "velocity.h"
#include "block.h"
#include "profile.h"

/* ---- Internal helper functions ---- */

static bool time_acc0(CRuckigVelocityThirdOrderStep2 *s, CRuckigProfile *profile,
                      double aMax, double aMin, double jMax)
{
    /* UD Solution 1/2 */
    {
        const double h1 = sqrt((-s->ad * s->ad + 2 * jMax * ((s->a0 + s->af) * s->tf - 2 * s->vd)) / (jMax * jMax) + s->tf * s->tf);

        profile->t[0] = s->ad / (2 * jMax) + (s->tf - h1) / 2;
        profile->t[1] = h1;
        profile->t[2] = s->tf - (profile->t[0] + h1);
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_for_velocity_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, jMax, aMax, aMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UU Solution */
    {
        const double h1 = (-s->ad + jMax * s->tf);

        profile->t[0] = -s->ad * s->ad / (2 * jMax * h1) + (s->vd - s->a0 * s->tf) / h1;
        profile->t[1] = -s->ad / jMax + s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = s->tf - (profile->t[0] + profile->t[1]);

        if (cruckig_profile_check_for_velocity_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, jMax, aMax, aMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UU Solution - 2 step */
    {
        profile->t[0] = 0;
        profile->t[1] = -s->ad / jMax + s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = s->ad / jMax;

        if (cruckig_profile_check_for_velocity_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, jMax, aMax, aMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    return false;
}

static bool time_none(CRuckigVelocityThirdOrderStep2 *s, CRuckigProfile *profile,
                      double aMax, double aMin, double jMax)
{
    if (fabs(s->a0) < DBL_EPSILON && fabs(s->af) < DBL_EPSILON && fabs(s->vd) < DBL_EPSILON) {
        profile->t[0] = 0;
        profile->t[1] = s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_for_velocity_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, s->tf, jMax, aMax, aMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UD Solution 1/2 */
    {
        const double h1 = 2 * (s->af * s->tf - s->vd);

        profile->t[0] = h1 / s->ad;
        profile->t[1] = s->tf - profile->t[0];
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        const double jf = s->ad * s->ad / h1;

        if (fabs(jf) < fabs(jMax) + 1e-12 && cruckig_profile_check_for_velocity_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, s->tf, jf, aMax, aMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    return false;
}

static bool check_all(CRuckigVelocityThirdOrderStep2 *s, CRuckigProfile *profile,
                      double aMax, double aMin, double jMax)
{
    return time_acc0(s, profile, aMax, aMin, jMax) || time_none(s, profile, aMax, aMin, jMax);
}


/* ---- Public interface ---- */

void cruckig_vel3_step2_init(CRuckigVelocityThirdOrderStep2 *s,
                     double tf, double v0, double a0, double vf, double af,
                     double aMax, double aMin, double jMax)
{
    s->a0 = a0;
    s->tf = tf;
    s->af = af;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->_jMax = jMax;
    s->vd = vf - v0;
    s->ad = af - a0;
}

bool cruckig_vel3_step2_get_profile(CRuckigVelocityThirdOrderStep2 *s, CRuckigProfile *profile)
{
    /* Test all cases to get ones that match */
    /* However we should guess which one is correct and try them first... */
    if (s->vd > 0) {
        return check_all(s, profile, s->_aMax, s->_aMin, s->_jMax) || check_all(s, profile, s->_aMin, s->_aMax, -s->_jMax);
    }

    return check_all(s, profile, s->_aMin, s->_aMax, -s->_jMax) || check_all(s, profile, s->_aMax, s->_aMin, s->_jMax);
}
