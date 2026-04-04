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

void cruckig_pos2_step1_init(CRuckigPositionSecondOrderStep1 *s,
                     double p0, double v0, double pf, double vf,
                     double vMax, double vMin, double aMax, double aMin)
{
    s->v0 = v0;
    s->vf = vf;
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->pd = pf - p0;
}

static void time_acc0(CRuckigPositionSecondOrderStep1 *s,
                      CRuckigProfile *valid_profiles, size_t *counter,
                      double vMax, double vMin, double aMax, double aMin, bool return_after_found)
{
    CRuckigProfile *profile = &valid_profiles[*counter];

    profile->t[0] = (-s->v0 + vMax) / aMax;
    profile->t[1] = (aMin * s->v0 * s->v0 - aMax * s->vf * s->vf) / (2 * aMax * aMin * vMax) + vMax * (aMax - aMin) / (2 * aMax * aMin) + s->pd / vMax;
    profile->t[2] = (s->vf - vMax) / aMin;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (cruckig_profile_check_for_second_order(profile, ControlSignsUDDU, ReachedLimitsACC0, aMax, aMin, vMax, vMin)) {
        ++(*counter);
        if (*counter < 4) {
            cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], profile);
        }
    }

    (void)return_after_found;
}

static void time_none(CRuckigPositionSecondOrderStep1 *s,
                      CRuckigProfile *valid_profiles, size_t *counter,
                      double vMax, double vMin, double aMax, double aMin, bool return_after_found)
{
    double h1 = (aMax * s->vf * s->vf - aMin * s->v0 * s->v0 - 2 * aMax * aMin * s->pd) / (aMax - aMin);
    if (h1 >= 0.0) {
        h1 = sqrt(h1);

        /* Solution 1 */
        {
            CRuckigProfile *profile = &valid_profiles[*counter];

            profile->t[0] = -(s->v0 + h1) / aMax;
            profile->t[1] = 0;
            profile->t[2] = (s->vf + h1) / aMin;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (cruckig_profile_check_for_second_order(profile, ControlSignsUDDU, ReachedLimitsNONE, aMax, aMin, vMax, vMin)) {
                ++(*counter);
                if (*counter < 4) {
                    cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], profile);
                }
                if (return_after_found) {
                    return;
                }
            }
        }

        /* Solution 2 */
        {
            CRuckigProfile *profile = &valid_profiles[*counter];

            profile->t[0] = (-s->v0 + h1) / aMax;
            profile->t[1] = 0;
            profile->t[2] = (s->vf - h1) / aMin;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (cruckig_profile_check_for_second_order(profile, ControlSignsUDDU, ReachedLimitsNONE, aMax, aMin, vMax, vMin)) {
                ++(*counter);
                if (*counter < 4) {
                    cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], profile);
                }
            }
        }
    }
}

static bool time_all_single_step(CRuckigPositionSecondOrderStep1 *s,
                                 CRuckigProfile *profile, double vMax, double vMin)
{
    if (fabs(s->vf - s->v0) > DBL_EPSILON) {
        return false;
    }

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = 0;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (fabs(s->v0) > DBL_EPSILON) {
        profile->t[3] = s->pd / s->v0;
        if (cruckig_profile_check_for_second_order(profile, ControlSignsUDDU, ReachedLimitsNONE, 0.0, 0.0, vMax, vMin)) {
            return true;
        }
    } else if (fabs(s->pd) < DBL_EPSILON) {
        if (cruckig_profile_check_for_second_order(profile, ControlSignsUDDU, ReachedLimitsNONE, 0.0, 0.0, vMax, vMin)) {
            return true;
        }
    }

    return false;
}

bool cruckig_pos2_step1_get_profile(CRuckigPositionSecondOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block)
{
    /* Zero-limits special case */
    if (s->_vMax == 0.0 && s->_vMin == 0.0) {
        CRuckigProfile *p = &block->p_min;
        cruckig_profile_set_boundary_from_profile(p, input);

        if (time_all_single_step(s, p, s->_vMax, s->_vMin)) {
            block->t_min = p->t_sum[6] + p->brake.duration + p->accel.duration;
            if (fabs(s->v0) > DBL_EPSILON) {
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

    if (fabs(s->vf) < DBL_EPSILON) {
        /* There is no blocked interval when vf==0, so return after first found profile */
        const double vMax = (s->pd >= 0) ? s->_vMax : s->_vMin;
        const double vMin = (s->pd >= 0) ? s->_vMin : s->_vMax;
        const double aMax = (s->pd >= 0) ? s->_aMax : s->_aMin;
        const double aMin = (s->pd >= 0) ? s->_aMin : s->_aMax;

        time_none(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, true);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0(s, s->valid_profiles, &valid_profile_counter, vMax, vMin, aMax, aMin, true);
        if (valid_profile_counter > 0) { goto return_block; }

        time_none(s, s->valid_profiles, &valid_profile_counter, vMin, vMax, aMin, aMax, true);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0(s, s->valid_profiles, &valid_profile_counter, vMin, vMax, aMin, aMax, true);
    } else {
        time_none(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, false);
        time_none(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, false);
        time_acc0(s, s->valid_profiles, &valid_profile_counter, s->_vMax, s->_vMin, s->_aMax, s->_aMin, false);
        time_acc0(s, s->valid_profiles, &valid_profile_counter, s->_vMin, s->_vMax, s->_aMin, s->_aMax, false);
    }

return_block:
    return cruckig_block_calculate(block, s->valid_profiles, valid_profile_counter, 4);
}
