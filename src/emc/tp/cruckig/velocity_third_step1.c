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

static void time_acc0(const CRuckigVelocityThirdOrderStep1 *s,
                      CRuckigProfile *valid_profiles, size_t *counter,
                      double aMax, double aMin, double jMax, bool return_after_found)
{
    (void)return_after_found;

    CRuckigProfile *profile = &valid_profiles[*counter];

    profile->t[0] = (-s->a0 + aMax) / jMax;
    profile->t[1] = (s->a0 * s->a0 + s->af * s->af) / (2 * aMax * jMax) - aMax / jMax + s->vd / aMax;
    profile->t[2] = (-s->af + aMax) / jMax;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (cruckig_profile_check_for_velocity(profile, ControlSignsUDDU, ReachedLimitsACC0, jMax, aMax, aMin)) {
        (*counter)++;
        if (*counter < 3) {
            cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], profile);
        }
    }
}

static void time_none(const CRuckigVelocityThirdOrderStep1 *s,
                      CRuckigProfile *valid_profiles, size_t *counter,
                      double aMax, double aMin, double jMax, bool return_after_found)
{
    double h1 = (s->a0 * s->a0 + s->af * s->af) / 2 + jMax * s->vd;
    if (h1 >= 0.0) {
        h1 = sqrt(h1);

        /* Solution 1 */
        {
            CRuckigProfile *profile = &valid_profiles[*counter];

            profile->t[0] = -(s->a0 + h1) / jMax;
            profile->t[1] = 0;
            profile->t[2] = -(s->af + h1) / jMax;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (cruckig_profile_check_for_velocity(profile, ControlSignsUDDU, ReachedLimitsNONE, jMax, aMax, aMin)) {
                (*counter)++;
                if (*counter < 3) {
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

            profile->t[0] = (-s->a0 + h1) / jMax;
            profile->t[1] = 0;
            profile->t[2] = (-s->af + h1) / jMax;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (cruckig_profile_check_for_velocity(profile, ControlSignsUDDU, ReachedLimitsNONE, jMax, aMax, aMin)) {
                (*counter)++;
                if (*counter < 3) {
                    cruckig_profile_set_boundary_from_profile(&valid_profiles[*counter], profile);
                }
            }
        }
    }
}

static bool time_all_single_step(const CRuckigVelocityThirdOrderStep1 *s,
                                 CRuckigProfile *profile,
                                 double aMax, double aMin, double jMax)
{
    (void)jMax;

    if (fabs(s->af - s->a0) > DBL_EPSILON) {
        return false;
    }

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = 0;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (fabs(s->a0) > DBL_EPSILON) {
        profile->t[3] = s->vd / s->a0;
        if (cruckig_profile_check_for_velocity(profile, ControlSignsUDDU, ReachedLimitsNONE, 0.0, aMax, aMin)) {
            return true;
        }

    } else if (fabs(s->vd) < DBL_EPSILON) {
        if (cruckig_profile_check_for_velocity(profile, ControlSignsUDDU, ReachedLimitsNONE, 0.0, aMax, aMin)) {
            return true;
        }
    }

    return false;
}


/* ---- Public interface ---- */

void cruckig_vel3_step1_init(CRuckigVelocityThirdOrderStep1 *s,
                     double v0, double a0, double vf, double af,
                     double aMax, double aMin, double jMax)
{
    s->a0 = a0;
    s->af = af;
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->_jMax = jMax;
    s->vd = vf - v0;
}

bool cruckig_vel3_step1_get_profile(CRuckigVelocityThirdOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block)
{
    /* Zero-limits special case */
    if (s->_jMax == 0.0) {
        CRuckigProfile *p = &block->p_min;
        cruckig_profile_set_boundary_from_profile(p, input);

        if (time_all_single_step(s, p, s->_aMax, s->_aMin, s->_jMax)) {
            block->t_min = p->t_sum[6] + p->brake.duration + p->accel.duration;
            if (fabs(s->a0) > DBL_EPSILON) {
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

    if (fabs(s->af) < DBL_EPSILON) {
        /* There is no blocked interval when af==0, so return after first found profile */
        const double aMax = (s->vd >= 0) ? s->_aMax : s->_aMin;
        const double aMin = (s->vd >= 0) ? s->_aMin : s->_aMax;
        const double jMax = (s->vd >= 0) ? s->_jMax : -s->_jMax;

        time_none(s, s->valid_profiles, &valid_profile_counter, aMax, aMin, jMax, true);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0(s, s->valid_profiles, &valid_profile_counter, aMax, aMin, jMax, true);
        if (valid_profile_counter > 0) { goto return_block; }

        time_none(s, s->valid_profiles, &valid_profile_counter, aMin, aMax, -jMax, true);
        if (valid_profile_counter > 0) { goto return_block; }
        time_acc0(s, s->valid_profiles, &valid_profile_counter, aMin, aMax, -jMax, true);

    } else {
        time_none(s, s->valid_profiles, &valid_profile_counter, s->_aMax, s->_aMin, s->_jMax, false);
        time_none(s, s->valid_profiles, &valid_profile_counter, s->_aMin, s->_aMax, -s->_jMax, false);
        time_acc0(s, s->valid_profiles, &valid_profile_counter, s->_aMax, s->_aMin, s->_jMax, false);
        time_acc0(s, s->valid_profiles, &valid_profile_counter, s->_aMin, s->_aMax, -s->_jMax, false);
    }

return_block:
    return cruckig_block_calculate(block, s->valid_profiles, valid_profile_counter, 3);
}
