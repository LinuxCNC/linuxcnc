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

void cruckig_pos1_step1_init(CRuckigPositionFirstOrderStep1 *s,
                     double p0, double pf, double vMax, double vMin)
{
    s->_vMax = vMax;
    s->_vMin = vMin;
    s->pd = pf - p0;
}

bool cruckig_pos1_step1_get_profile(CRuckigPositionFirstOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block)
{
    CRuckigProfile *p = &block->p_min;
    cruckig_profile_set_boundary_from_profile(p, input);

    const double vf = (s->pd > 0) ? s->_vMax : s->_vMin;
    p->t[0] = 0;
    p->t[1] = 0;
    p->t[2] = 0;
    p->t[3] = s->pd / vf;
    p->t[4] = 0;
    p->t[5] = 0;
    p->t[6] = 0;

    if (cruckig_profile_check_for_first_order(p, ControlSignsUDDU, ReachedLimitsVEL, vf)) {
        block->t_min = p->t_sum[6] + p->brake.duration + p->accel.duration;
        return true;
    }
    return false;
}
