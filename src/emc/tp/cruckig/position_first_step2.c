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

void cruckig_pos1_step2_init(CRuckigPositionFirstOrderStep2 *s,
                     double tf, double p0, double pf, double vMax, double vMin)
{
    s->tf = tf;
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->pd = pf - p0;
}

bool cruckig_pos1_step2_get_profile(CRuckigPositionFirstOrderStep2 *s, CRuckigProfile *profile)
{
    const double vf = s->pd / s->tf;

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = 0;
    profile->t[3] = s->tf;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    return cruckig_profile_check_for_first_order_with_timing_full(profile, ControlSignsUDDU, ReachedLimitsNONE,
                                                          s->tf, vf, s->_vMax, s->_vMin);
}
