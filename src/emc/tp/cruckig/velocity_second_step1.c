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

void cruckig_vel2_step1_init(CRuckigVelocitySecondOrderStep1 *s,
                     double v0, double vf, double aMax, double aMin)
{
    s->_aMax = aMax;
    s->_aMin = aMin;
    s->vd = vf - v0;
}

bool cruckig_vel2_step1_get_profile(CRuckigVelocitySecondOrderStep1 *s,
                            const CRuckigProfile *input, CRuckigBlock *block)
{
    CRuckigProfile *p = &block->p_min;
    cruckig_profile_set_boundary_from_profile(p, input);

    const double af = (s->vd > 0) ? s->_aMax : s->_aMin;
    p->t[0] = 0;
    p->t[1] = s->vd / af;
    p->t[2] = 0;
    p->t[3] = 0;
    p->t[4] = 0;
    p->t[5] = 0;
    p->t[6] = 0;

    if (cruckig_profile_check_for_second_order_velocity(p, ControlSignsUDDU, ReachedLimitsACC0, af)) {
        block->t_min = p->t_sum[6] + p->brake.duration + p->accel.duration;
        return true;
    }
    return false;
}
