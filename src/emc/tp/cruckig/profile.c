/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "profile.h"
#include "roots.h"
#include "utils.h"

void cruckig_profile_init(CRuckigProfile *p) {
    memset(p->t, 0, sizeof(p->t));
    memset(p->t_sum, 0, sizeof(p->t_sum));
    memset(p->j, 0, sizeof(p->j));
    memset(p->a, 0, sizeof(p->a));
    memset(p->v, 0, sizeof(p->v));
    memset(p->p, 0, sizeof(p->p));

    cruckig_brake_init(&p->brake);
    cruckig_brake_init(&p->accel);

    p->pf = 0.0;
    p->vf = 0.0;
    p->af = 0.0;

    p->limits = ReachedLimitsNONE;
    p->direction = DirectionUP;
    p->control_signs = ControlSignsUDDU;
}

void cruckig_profile_set_boundary(CRuckigProfile *p, double p0, double v0, double a0,
                          double pf, double vf, double af) {
    p->a[0] = a0;
    p->v[0] = v0;
    p->p[0] = p0;
    p->af = af;
    p->vf = vf;
    p->pf = pf;
}

void cruckig_profile_set_boundary_from_profile(CRuckigProfile *p, const CRuckigProfile *src) {
    p->a[0] = src->a[0];
    p->v[0] = src->v[0];
    p->p[0] = src->p[0];
    p->af = src->af;
    p->vf = src->vf;
    p->pf = src->pf;
    p->brake = src->brake;
    p->accel = src->accel;
}

void cruckig_profile_set_boundary_for_velocity(CRuckigProfile *p, double p0, double v0, double a0,
                                       double vf, double af) {
    p->a[0] = a0;
    p->v[0] = v0;
    p->p[0] = p0;
    p->af = af;
    p->vf = vf;
}

/* Third-order position check */
CRUCKIG_HOT
bool cruckig_profile_check(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                   bool set_limits, double jf, double vMax, double vMin, double aMax, double aMin) {
    if (CRUCKIG_UNLIKELY(p->t[0] < 0)) {
        return false;
    }

    p->t_sum[0] = p->t[0];
    for (size_t i = 0; i < 6; ++i) {
        if (CRUCKIG_UNLIKELY(p->t[i + 1] < 0)) {
            return false;
        }
        p->t_sum[i + 1] = p->t_sum[i] + p->t[i + 1];
    }

    if (lim == ReachedLimitsACC0_ACC1_VEL || lim == ReachedLimitsACC0_VEL || lim == ReachedLimitsACC1_VEL || lim == ReachedLimitsVEL) {
        if (CRUCKIG_UNLIKELY(p->t[3] < DBL_EPSILON)) {
            return false;
        }
    }

    if (lim == ReachedLimitsACC0 || lim == ReachedLimitsACC0_ACC1) {
        if (CRUCKIG_UNLIKELY(p->t[1] < DBL_EPSILON)) {
            return false;
        }
    }

    if (lim == ReachedLimitsACC1 || lim == ReachedLimitsACC0_ACC1) {
        if (CRUCKIG_UNLIKELY(p->t[5] < DBL_EPSILON)) {
            return false;
        }
    }

    if (CRUCKIG_UNLIKELY(p->t_sum[6] > PROFILE_T_MAX)) {
        return false;
    }

    if (cs == ControlSignsUDDU) {
        p->j[0] = (p->t[0] > 0 ? jf : 0);
        p->j[1] = 0;
        p->j[2] = (p->t[2] > 0 ? -jf : 0);
        p->j[3] = 0;
        p->j[4] = (p->t[4] > 0 ? -jf : 0);
        p->j[5] = 0;
        p->j[6] = (p->t[6] > 0 ? jf : 0);
    } else {
        p->j[0] = (p->t[0] > 0 ? jf : 0);
        p->j[1] = 0;
        p->j[2] = (p->t[2] > 0 ? -jf : 0);
        p->j[3] = 0;
        p->j[4] = (p->t[4] > 0 ? jf : 0);
        p->j[5] = 0;
        p->j[6] = (p->t[6] > 0 ? -jf : 0);
    }

    p->direction = (vMax > 0) ? DirectionUP : DirectionDOWN;
    const double vUppLim = (p->direction == DirectionUP ? vMax : vMin) + PROFILE_V_EPS;
    const double vLowLim = (p->direction == DirectionUP ? vMin : vMax) - PROFILE_V_EPS;

    for (size_t i = 0; i < 7; ++i) {
        p->a[i + 1] = p->a[i] + p->t[i] * p->j[i];
        p->v[i + 1] = p->v[i] + p->t[i] * (p->a[i] + p->t[i] * p->j[i] / 2);
        p->p[i + 1] = p->p[i] + p->t[i] * (p->v[i] + p->t[i] * (p->a[i] / 2 + p->t[i] * p->j[i] / 6));

        if (lim == ReachedLimitsACC0_ACC1_VEL || lim == ReachedLimitsACC0_ACC1 || lim == ReachedLimitsACC0_VEL || lim == ReachedLimitsACC1_VEL || lim == ReachedLimitsVEL) {
            if (i == 2) {
                p->a[3] = 0.0;
            }
        }

        if (set_limits) {
            if (lim == ReachedLimitsACC1) {
                if (i == 2) {
                    p->a[3] = aMin;
                }
            }

            if (lim == ReachedLimitsACC0_ACC1) {
                if (i == 0) {
                    p->a[1] = aMax;
                }
                if (i == 4) {
                    p->a[5] = aMin;
                }
            }
        }

        if (i > 1 && p->a[i + 1] * p->a[i] < -DBL_EPSILON) {
            const double v_a_zero = p->v[i] - (p->a[i] * p->a[i]) / (2 * p->j[i]);
            if (v_a_zero > vUppLim || v_a_zero < vLowLim) {
                return false;
            }
        }
    }

    p->control_signs = cs;
    p->limits = lim;

    const double aUppLim = (p->direction == DirectionUP ? aMax : aMin) + PROFILE_A_EPS;
    const double aLowLim = (p->direction == DirectionUP ? aMin : aMax) - PROFILE_A_EPS;

    return fabs(p->p[7] - p->pf) < PROFILE_P_PREC && fabs(p->v[7] - p->vf) < PROFILE_V_PREC && fabs(p->a[7] - p->af) < PROFILE_A_PREC
        && p->a[1] >= aLowLim && p->a[3] >= aLowLim && p->a[5] >= aLowLim
        && p->a[1] <= aUppLim && p->a[3] <= aUppLim && p->a[5] <= aUppLim
        && p->v[3] <= vUppLim && p->v[4] <= vUppLim && p->v[5] <= vUppLim && p->v[6] <= vUppLim
        && p->v[3] >= vLowLim && p->v[4] >= vLowLim && p->v[5] >= vLowLim && p->v[6] >= vLowLim;
}

bool cruckig_profile_check_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                               double tf, double jf, double vMax, double vMin, double aMax, double aMin) {
    (void)tf;
    /* Time doesn't need to be checked as every profile has a: tf - ... equation */
    return cruckig_profile_check(p, cs, lim, false, jf, vMax, vMin, aMax, aMin);
}

bool cruckig_profile_check_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                    double tf, double jf, double vMax, double vMin, double aMax, double aMin, double jMax) {
    return (fabs(jf) < fabs(jMax) + PROFILE_J_EPS) && cruckig_profile_check_with_timing(p, cs, lim, tf, jf, vMax, vMin, aMax, aMin);
}

/* Third-order velocity check */
CRUCKIG_HOT
bool cruckig_profile_check_for_velocity(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                double jf, double aMax, double aMin) {
    if (CRUCKIG_UNLIKELY(p->t[0] < 0)) {
        return false;
    }

    p->t_sum[0] = p->t[0];
    for (size_t i = 0; i < 6; ++i) {
        if (p->t[i + 1] < 0) {
            return false;
        }
        p->t_sum[i + 1] = p->t_sum[i] + p->t[i + 1];
    }

    if (lim == ReachedLimitsACC0) {
        if (p->t[1] < DBL_EPSILON) {
            return false;
        }
    }

    if (p->t_sum[6] > PROFILE_T_MAX) {
        return false;
    }

    if (cs == ControlSignsUDDU) {
        p->j[0] = (p->t[0] > 0 ? jf : 0);
        p->j[1] = 0;
        p->j[2] = (p->t[2] > 0 ? -jf : 0);
        p->j[3] = 0;
        p->j[4] = (p->t[4] > 0 ? -jf : 0);
        p->j[5] = 0;
        p->j[6] = (p->t[6] > 0 ? jf : 0);
    } else {
        p->j[0] = (p->t[0] > 0 ? jf : 0);
        p->j[1] = 0;
        p->j[2] = (p->t[2] > 0 ? -jf : 0);
        p->j[3] = 0;
        p->j[4] = (p->t[4] > 0 ? jf : 0);
        p->j[5] = 0;
        p->j[6] = (p->t[6] > 0 ? -jf : 0);
    }

    for (size_t i = 0; i < 7; ++i) {
        p->a[i + 1] = p->a[i] + p->t[i] * p->j[i];
        p->v[i + 1] = p->v[i] + p->t[i] * (p->a[i] + p->t[i] * p->j[i] / 2);
        p->p[i + 1] = p->p[i] + p->t[i] * (p->v[i] + p->t[i] * (p->a[i] / 2 + p->t[i] * p->j[i] / 6));
    }

    p->control_signs = cs;
    p->limits = lim;

    p->direction = (aMax > 0) ? DirectionUP : DirectionDOWN;
    const double aUppLim = (p->direction == DirectionUP ? aMax : aMin) + PROFILE_A_EPS;
    const double aLowLim = (p->direction == DirectionUP ? aMin : aMax) - PROFILE_A_EPS;

    return fabs(p->v[7] - p->vf) < PROFILE_V_PREC && fabs(p->a[7] - p->af) < PROFILE_A_PREC
        && p->a[1] >= aLowLim && p->a[3] >= aLowLim && p->a[5] >= aLowLim
        && p->a[1] <= aUppLim && p->a[3] <= aUppLim && p->a[5] <= aUppLim;
}

bool cruckig_profile_check_for_velocity_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                            double tf, double jf, double aMax, double aMin) {
    (void)tf;
    return cruckig_profile_check_for_velocity(p, cs, lim, jf, aMax, aMin);
}

bool cruckig_profile_check_for_velocity_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                 double tf, double jf, double aMax, double aMin, double jMax) {
    return (fabs(jf) < fabs(jMax) + PROFILE_J_EPS) && cruckig_profile_check_for_velocity_with_timing(p, cs, lim, tf, jf, aMax, aMin);
}

/* Second-order position check */
bool cruckig_profile_check_for_second_order(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                    double aUp, double aDown, double vMax, double vMin) {
    if (p->t[0] < 0) {
        return false;
    }

    p->t_sum[0] = p->t[0];
    for (size_t i = 0; i < 6; ++i) {
        if (p->t[i + 1] < 0) {
            return false;
        }
        p->t_sum[i + 1] = p->t_sum[i] + p->t[i + 1];
    }

    if (p->t_sum[6] > PROFILE_T_MAX) {
        return false;
    }

    p->j[0] = 0; p->j[1] = 0; p->j[2] = 0; p->j[3] = 0;
    p->j[4] = 0; p->j[5] = 0; p->j[6] = 0;

    if (cs == ControlSignsUDDU) {
        p->a[0] = (p->t[0] > 0 ? aUp : 0);
        p->a[1] = 0;
        p->a[2] = (p->t[2] > 0 ? aDown : 0);
        p->a[3] = 0;
        p->a[4] = (p->t[4] > 0 ? aDown : 0);
        p->a[5] = 0;
        p->a[6] = (p->t[6] > 0 ? aUp : 0);
        p->a[7] = p->af;
    } else {
        p->a[0] = (p->t[0] > 0 ? aUp : 0);
        p->a[1] = 0;
        p->a[2] = (p->t[2] > 0 ? aDown : 0);
        p->a[3] = 0;
        p->a[4] = (p->t[4] > 0 ? aUp : 0);
        p->a[5] = 0;
        p->a[6] = (p->t[6] > 0 ? aDown : 0);
        p->a[7] = p->af;
    }

    p->direction = (vMax > 0) ? DirectionUP : DirectionDOWN;
    const double vUppLim = (p->direction == DirectionUP ? vMax : vMin) + PROFILE_V_EPS;
    const double vLowLim = (p->direction == DirectionUP ? vMin : vMax) - PROFILE_V_EPS;

    for (size_t i = 0; i < 7; ++i) {
        p->v[i + 1] = p->v[i] + p->t[i] * p->a[i];
        p->p[i + 1] = p->p[i] + p->t[i] * (p->v[i] + p->t[i] * p->a[i] / 2);
    }

    p->control_signs = cs;
    p->limits = lim;

    return fabs(p->p[7] - p->pf) < PROFILE_P_PREC && fabs(p->v[7] - p->vf) < PROFILE_V_PREC
        && p->v[2] <= vUppLim && p->v[3] <= vUppLim && p->v[4] <= vUppLim && p->v[5] <= vUppLim && p->v[6] <= vUppLim
        && p->v[2] >= vLowLim && p->v[3] >= vLowLim && p->v[4] >= vLowLim && p->v[5] >= vLowLim && p->v[6] >= vLowLim;
}

bool cruckig_profile_check_for_second_order_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                double tf, double aUp, double aDown, double vMax, double vMin) {
    (void)tf;
    return cruckig_profile_check_for_second_order(p, cs, lim, aUp, aDown, vMax, vMin);
}

bool cruckig_profile_check_for_second_order_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                     double tf, double aUp, double aDown, double vMax, double vMin,
                                                     double aMax, double aMin) {
    return (aMin - PROFILE_A_EPS < aUp) && (aUp < aMax + PROFILE_A_EPS) && (aMin - PROFILE_A_EPS < aDown) && (aDown < aMax + PROFILE_A_EPS)
        && cruckig_profile_check_for_second_order_with_timing(p, cs, lim, tf, aUp, aDown, vMax, vMin);
}

/* Second-order velocity check */
bool cruckig_profile_check_for_second_order_velocity(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                             double aUp) {
    /* ReachedLimits::ACC0 */
    if (p->t[1] < 0.0) {
        return false;
    }

    p->t_sum[0] = 0;
    p->t_sum[1] = p->t[1];
    p->t_sum[2] = p->t[1];
    p->t_sum[3] = p->t[1];
    p->t_sum[4] = p->t[1];
    p->t_sum[5] = p->t[1];
    p->t_sum[6] = p->t[1];

    if (p->t_sum[6] > PROFILE_T_MAX) {
        return false;
    }

    p->j[0] = 0; p->j[1] = 0; p->j[2] = 0; p->j[3] = 0;
    p->j[4] = 0; p->j[5] = 0; p->j[6] = 0;

    p->a[0] = 0;
    p->a[1] = (p->t[1] > 0) ? aUp : 0;
    p->a[2] = 0; p->a[3] = 0; p->a[4] = 0; p->a[5] = 0; p->a[6] = 0;
    p->a[7] = p->af;

    for (size_t i = 0; i < 7; ++i) {
        p->v[i + 1] = p->v[i] + p->t[i] * p->a[i];
        p->p[i + 1] = p->p[i] + p->t[i] * (p->v[i] + p->t[i] * p->a[i] / 2);
    }

    p->control_signs = cs;
    p->limits = lim;

    p->direction = (aUp > 0) ? DirectionUP : DirectionDOWN;

    return fabs(p->v[7] - p->vf) < PROFILE_V_PREC;
}

bool cruckig_profile_check_for_second_order_velocity_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                         double tf, double aUp) {
    (void)tf;
    return cruckig_profile_check_for_second_order_velocity(p, cs, lim, aUp);
}

bool cruckig_profile_check_for_second_order_velocity_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                              double tf, double aUp, double aMax, double aMin) {
    return (aMin - PROFILE_A_EPS < aUp) && (aUp < aMax + PROFILE_A_EPS)
        && cruckig_profile_check_for_second_order_velocity_with_timing(p, cs, lim, tf, aUp);
}

/* First-order position check */
bool cruckig_profile_check_for_first_order(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                   double vUp) {
    /* ReachedLimits::VEL */
    if (p->t[3] < 0.0) {
        return false;
    }

    p->t_sum[0] = 0; p->t_sum[1] = 0; p->t_sum[2] = 0;
    p->t_sum[3] = p->t[3];
    p->t_sum[4] = p->t[3]; p->t_sum[5] = p->t[3]; p->t_sum[6] = p->t[3];

    if (p->t_sum[6] > PROFILE_T_MAX) {
        return false;
    }

    p->j[0] = 0; p->j[1] = 0; p->j[2] = 0; p->j[3] = 0;
    p->j[4] = 0; p->j[5] = 0; p->j[6] = 0;

    p->a[0] = 0; p->a[1] = 0; p->a[2] = 0; p->a[3] = 0;
    p->a[4] = 0; p->a[5] = 0; p->a[6] = 0; p->a[7] = p->af;

    p->v[0] = 0; p->v[1] = 0; p->v[2] = 0;
    p->v[3] = (p->t[3] > 0 ? vUp : 0);
    p->v[4] = 0; p->v[5] = 0; p->v[6] = 0; p->v[7] = p->vf;

    for (size_t i = 0; i < 7; ++i) {
        p->p[i + 1] = p->p[i] + p->t[i] * (p->v[i] + p->t[i] * p->a[i] / 2);
    }

    p->control_signs = cs;
    p->limits = lim;

    p->direction = (vUp > 0) ? DirectionUP : DirectionDOWN;

    return fabs(p->p[7] - p->pf) < PROFILE_P_PREC;
}

bool cruckig_profile_check_for_first_order_with_timing(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                               double tf, double vUp) {
    (void)tf;
    return cruckig_profile_check_for_first_order(p, cs, lim, vUp);
}

bool cruckig_profile_check_for_first_order_with_timing_full(CRuckigProfile *p, CRuckigControlSigns cs, CRuckigReachedLimits lim,
                                                    double tf, double vUp, double vMax, double vMin) {
    return (vMin - PROFILE_V_EPS < vUp) && (vUp < vMax + PROFILE_V_EPS)
        && cruckig_profile_check_for_first_order_with_timing(p, cs, lim, tf, vUp);
}

/* Position extrema helpers */
static void check_position_extremum(double t_ext, double t_sum_val, double t_seg, double pos, double vel, double acc, double jrk, CRuckigBound *ext) {
    if (0 < t_ext && t_ext < t_seg) {
        double p_ext, v_ext, a_ext;
        cruckig_integrate(t_ext, pos, vel, acc, jrk, &p_ext, &v_ext, &a_ext);
        (void)v_ext;
        if (a_ext > 0 && p_ext < ext->min) {
            ext->min = p_ext;
            ext->t_min = t_sum_val + t_ext;
        } else if (a_ext < 0 && p_ext > ext->max) {
            ext->max = p_ext;
            ext->t_max = t_sum_val + t_ext;
        }
    }
}

static void check_step_for_position_extremum(double t_sum_val, double t_seg, double pos, double vel, double acc, double jrk, CRuckigBound *ext) {
    if (pos < ext->min) {
        ext->min = pos;
        ext->t_min = t_sum_val;
    }
    if (pos > ext->max) {
        ext->max = pos;
        ext->t_max = t_sum_val;
    }

    if (jrk != 0) {
        const double D = acc * acc - 2 * jrk * vel;
        if (fabs(D) < DBL_EPSILON) {
            check_position_extremum(-acc / jrk, t_sum_val, t_seg, pos, vel, acc, jrk, ext);
        } else if (D > 0.0) {
            const double D_sqrt = sqrt(D);
            check_position_extremum((-acc - D_sqrt) / jrk, t_sum_val, t_seg, pos, vel, acc, jrk, ext);
            check_position_extremum((-acc + D_sqrt) / jrk, t_sum_val, t_seg, pos, vel, acc, jrk, ext);
        }
    }
}

CRuckigBound cruckig_profile_get_position_extrema(const CRuckigProfile *p) {
    CRuckigBound extrema;
    extrema.min = INFINITY;
    extrema.max = -INFINITY;
    extrema.t_min = 0.0;
    extrema.t_max = 0.0;

    if (p->brake.duration > 0.0) {
        if (p->brake.t[0] > 0.0) {
            check_step_for_position_extremum(0.0, p->brake.t[0], p->brake.p[0], p->brake.v[0], p->brake.a[0], p->brake.j[0], &extrema);

            if (p->brake.t[1] > 0.0) {
                check_step_for_position_extremum(p->brake.t[0], p->brake.t[1], p->brake.p[1], p->brake.v[1], p->brake.a[1], p->brake.j[1], &extrema);
            }
        }
    }

    double t_current_sum = 0.0;
    for (size_t i = 0; i < 7; ++i) {
        if (i > 0) {
            t_current_sum = p->t_sum[i - 1];
        }
        check_step_for_position_extremum(t_current_sum + p->brake.duration, p->t[i], p->p[i], p->v[i], p->a[i], p->j[i], &extrema);
    }

    if (p->pf < extrema.min) {
        extrema.min = p->pf;
        extrema.t_min = p->t_sum[6] + p->brake.duration;
    }
    if (p->pf > extrema.max) {
        extrema.max = p->pf;
        extrema.t_max = p->t_sum[6] + p->brake.duration;
    }

    return extrema;
}

bool cruckig_profile_get_first_state_at_position(const CRuckigProfile *p, double pt, double *time, double time_after) {
    double t_cum = 0.0;

    for (size_t i = 0; i < 7; ++i) {
        if (p->t[i] == 0.0) {
            continue;
        }

        if (fabs(p->p[i] - pt) < DBL_EPSILON && t_cum >= time_after) {
            *time = t_cum;
            return true;
        }

        CRuckigRootSet cubic_roots = cruckig_roots_solve_cubic(p->j[i] / 6, p->a[i] / 2, p->v[i], p->p[i] - pt);
        cruckig_root_set_sort(&cubic_roots);
        for (size_t r = 0; r < cubic_roots.size; ++r) {
            double _t = cubic_roots.data[r];
            if (0 < _t && time_after - t_cum <= _t && _t <= p->t[i]) {
                *time = _t + t_cum;
                return true;
            }
        }

        t_cum += p->t[i];
    }

    if ((p->t[6] > 0.0 || p->t_sum[6] == 0.0) && fabs(p->pf - pt) < 1e-9 && p->t_sum[6] >= time_after) {
        *time = p->t_sum[6];
        return true;
    }

    return false;
}
