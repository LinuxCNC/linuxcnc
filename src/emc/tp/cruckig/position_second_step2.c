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

void cruckig_pos2_step2_init(CRuckigPositionSecondOrderStep2 *s,
                     double tf, double p0, double v0, double pf, double vf,
                     double vMax, double vMin, double aMax, double aMin)
{
    s->v0 = v0;
    s->tf = tf;
    s->vf = vf;
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->pd = pf - p0;
    s->vd = vf - v0;
}

static bool time_acc0(CRuckigPositionSecondOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin)
{
    /* UD Solution 1/2 */
    {
        const double h1 = sqrt((2 * aMax * (s->pd - s->tf * s->vf) - 2 * aMin * (s->pd - s->tf * s->v0) + s->vd * s->vd) / (aMax * aMin) + s->tf * s->tf);

        profile->t[0] = (aMax * s->vd - aMax * aMin * (s->tf - h1)) / (aMax * (aMax - aMin));
        profile->t[1] = h1;
        profile->t[2] = s->tf - (profile->t[0] + h1);
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_for_second_order_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, aMax, aMin, vMax, vMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UU Solution */
    {
        const double h1 = (-s->vd + aMax * s->tf);

        profile->t[0] = -s->vd * s->vd / (2 * aMax * h1) + (s->pd - s->v0 * s->tf) / h1;
        profile->t[1] = -s->vd / aMax + s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = s->tf - (profile->t[0] + profile->t[1]);

        if (cruckig_profile_check_for_second_order_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, aMax, aMin, vMax, vMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UU Solution - 2 step */
    {
        profile->t[0] = 0;
        profile->t[1] = -s->vd / aMax + s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = s->vd / aMax;

        if (cruckig_profile_check_for_second_order_with_timing(profile, ControlSignsUDDU, ReachedLimitsACC0, s->tf, aMax, aMin, vMax, vMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    return false;
}

static bool time_none(CRuckigPositionSecondOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin)
{
    if (fabs(s->v0) < DBL_EPSILON && fabs(s->vf) < DBL_EPSILON && fabs(s->pd) < DBL_EPSILON) {
        profile->t[0] = 0;
        profile->t[1] = s->tf;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (cruckig_profile_check_for_second_order_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, s->tf, aMax, aMin, vMax, vMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    /* UD Solution 1/2 */
    {
        const double h1 = 2 * (s->vf * s->tf - s->pd);

        profile->t[0] = h1 / s->vd;
        profile->t[1] = s->tf - profile->t[0];
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        const double af = s->vd * s->vd / h1;

        if ((aMin - 1e-12 < af) && (af < aMax + 1e-12) &&
            cruckig_profile_check_for_second_order_with_timing(profile, ControlSignsUDDU, ReachedLimitsNONE, s->tf, af, -af, vMax, vMin)) {
            profile->pf = profile->p[7];
            return true;
        }
    }

    return false;
}

static bool check_all(CRuckigPositionSecondOrderStep2 *s, CRuckigProfile *profile,
                      double vMax, double vMin, double aMax, double aMin)
{
    return time_acc0(s, profile, vMax, vMin, aMax, aMin) ||
           time_none(s, profile, vMax, vMin, aMax, aMin);
}

bool cruckig_pos2_step2_get_profile(CRuckigPositionSecondOrderStep2 *s, CRuckigProfile *profile)
{
    /* Test all cases to get ones that match */
    if (s->pd > 0) {
        return check_all(s, profile, s->_vMax, s->_vMin, s->_aMax, s->_aMin) ||
               check_all(s, profile, s->_vMin, s->_vMax, s->_aMin, s->_aMax);
    }

    return check_all(s, profile, s->_vMin, s->_vMax, s->_aMin, s->_aMax) ||
           check_all(s, profile, s->_vMax, s->_vMin, s->_aMax, s->_aMin);
}
