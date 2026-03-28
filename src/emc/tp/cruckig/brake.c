/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "brake.h"
#include "utils.h"

static const double brake_eps = 2.2e-14;

void cruckig_brake_init(CRuckigBrakeProfile *bp) {
    bp->duration = 0.0;
    bp->t[0] = 0.0;
    bp->t[1] = 0.0;
    bp->j[0] = 0.0;
    bp->j[1] = 0.0;
    bp->a[0] = 0.0;
    bp->a[1] = 0.0;
    bp->v[0] = 0.0;
    bp->v[1] = 0.0;
    bp->p[0] = 0.0;
    bp->p[1] = 0.0;
}

static inline double brake_v_at_t(double v0, double a0, double j, double t) {
    return v0 + t * (a0 + j * t / 2);
}

static inline double brake_v_at_a_zero(double v0, double a0, double j) {
    return v0 + (a0 * a0) / (2 * j);
}

static void acceleration_brake(CRuckigBrakeProfile *bp, double v0, double a0,
                                double vMax, double vMin, double aMax, double aMin, double jMax);
static void velocity_brake(CRuckigBrakeProfile *bp, double v0, double a0,
                            double vMax, double vMin, double aMax, double aMin, double jMax);

static void acceleration_brake(CRuckigBrakeProfile *bp, double v0, double a0,
                                double vMax, double vMin, double aMax, double aMin, double jMax) {
    bp->j[0] = -jMax;

    const double t_to_a_max = (a0 - aMax) / jMax;
    const double t_to_a_zero = a0 / jMax;

    const double v_at_a_max = brake_v_at_t(v0, a0, -jMax, t_to_a_max);
    const double v_at_a_zero_val = brake_v_at_t(v0, a0, -jMax, t_to_a_zero);

    if ((v_at_a_zero_val > vMax && jMax > 0) || (v_at_a_zero_val < vMax && jMax < 0)) {
        velocity_brake(bp, v0, a0, vMax, vMin, aMax, aMin, jMax);

    } else if ((v_at_a_max < vMin && jMax > 0) || (v_at_a_max > vMin && jMax < 0)) {
        const double t_to_v_min = -(v_at_a_max - vMin) / aMax;
        const double t_to_v_max = -aMax / (2 * jMax) - (v_at_a_max - vMax) / aMax;

        bp->t[0] = t_to_a_max + brake_eps;
        {
            double val = t_to_v_min < (t_to_v_max - brake_eps) ? t_to_v_min : (t_to_v_max - brake_eps);
            bp->t[1] = val > 0.0 ? val : 0.0;
        }

    } else {
        bp->t[0] = t_to_a_max + brake_eps;
    }
}

static void velocity_brake(CRuckigBrakeProfile *bp, double v0, double a0,
                            double vMax, double vMin, double aMax, double aMin, double jMax) {
    (void)aMax;
    bp->j[0] = -jMax;
    const double t_to_a_min = (a0 - aMin) / jMax;
    const double t_to_v_max = a0 / jMax + sqrt(a0 * a0 + 2 * jMax * (v0 - vMax)) / fabs(jMax);
    const double t_to_v_min = a0 / jMax + sqrt(a0 * a0 / 2 + jMax * (v0 - vMin)) / fabs(jMax);
    const double t_min_to_v_max = t_to_v_max < t_to_v_min ? t_to_v_max : t_to_v_min;

    if (t_to_a_min < t_min_to_v_max) {
        const double v_at_a_min = brake_v_at_t(v0, a0, -jMax, t_to_a_min);
        const double t_to_v_max_with_constant = -(v_at_a_min - vMax) / aMin;
        const double t_to_v_min_with_constant = aMin / (2 * jMax) - (v_at_a_min - vMin) / aMin;

        bp->t[0] = (t_to_a_min - brake_eps) > 0.0 ? (t_to_a_min - brake_eps) : 0.0;
        {
            double val = t_to_v_max_with_constant < t_to_v_min_with_constant ? t_to_v_max_with_constant : t_to_v_min_with_constant;
            bp->t[1] = val > 0.0 ? val : 0.0;
        }

    } else {
        bp->t[0] = (t_min_to_v_max - brake_eps) > 0.0 ? (t_min_to_v_max - brake_eps) : 0.0;
    }
}

void cruckig_brake_get_position_brake_trajectory(CRuckigBrakeProfile *bp, double v0, double a0,
                                         double vMax, double vMin, double aMax, double aMin, double jMax) {
    bp->t[0] = 0.0;
    bp->t[1] = 0.0;
    bp->j[0] = 0.0;
    bp->j[1] = 0.0;

    if (jMax == 0.0 || aMax == 0.0 || aMin == 0.0) {
        return; /* Ignore braking for zero-limits */
    }

    if (a0 > aMax) {
        acceleration_brake(bp, v0, a0, vMax, vMin, aMax, aMin, jMax);

    } else if (a0 < aMin) {
        acceleration_brake(bp, v0, a0, vMin, vMax, aMin, aMax, -jMax);

    } else if ((v0 > vMax && brake_v_at_a_zero(v0, a0, -jMax) > vMin) || (a0 > 0 && brake_v_at_a_zero(v0, a0, jMax) > vMax)) {
        velocity_brake(bp, v0, a0, vMax, vMin, aMax, aMin, jMax);

    } else if ((v0 < vMin && brake_v_at_a_zero(v0, a0, jMax) < vMax) || (a0 < 0 && brake_v_at_a_zero(v0, a0, -jMax) < vMin)) {
        velocity_brake(bp, v0, a0, vMin, vMax, aMin, aMax, -jMax);
    }
}

void cruckig_brake_get_second_order_position_brake_trajectory(CRuckigBrakeProfile *bp, double v0,
                                                      double vMax, double vMin, double aMax, double aMin) {
    bp->t[0] = 0.0;
    bp->t[1] = 0.0;
    bp->j[0] = 0.0;
    bp->j[1] = 0.0;
    bp->a[0] = 0.0;
    bp->a[1] = 0.0;

    if (aMax == 0.0 || aMin == 0.0) {
        return; /* Ignore braking for zero-limits */
    }

    if (v0 > vMax) {
        bp->a[0] = aMin;
        bp->t[0] = (vMax - v0) / aMin + brake_eps;

    } else if (v0 < vMin) {
        bp->a[0] = aMax;
        bp->t[0] = (vMin - v0) / aMax + brake_eps;
    }
}

void cruckig_brake_get_velocity_brake_trajectory(CRuckigBrakeProfile *bp, double a0,
                                         double aMax, double aMin, double jMax) {
    bp->t[0] = 0.0;
    bp->t[1] = 0.0;
    bp->j[0] = 0.0;
    bp->j[1] = 0.0;

    if (jMax == 0.0) {
        return; /* Ignore braking for zero-limits */
    }

    if (a0 > aMax) {
        bp->j[0] = -jMax;
        bp->t[0] = (a0 - aMax) / jMax + brake_eps;

    } else if (a0 < aMin) {
        bp->j[0] = jMax;
        bp->t[0] = -(a0 - aMin) / jMax + brake_eps;
    }
}

void cruckig_brake_get_second_order_velocity_brake_trajectory(CRuckigBrakeProfile *bp) {
    bp->t[0] = 0.0;
    bp->t[1] = 0.0;
    bp->j[0] = 0.0;
    bp->j[1] = 0.0;
}

void cruckig_brake_finalize(CRuckigBrakeProfile *bp, double *ps, double *vs, double *as) {
    if (bp->t[0] <= 0.0 && bp->t[1] <= 0.0) {
        bp->duration = 0.0;
        return;
    }

    bp->duration = bp->t[0];
    bp->p[0] = *ps;
    bp->v[0] = *vs;
    bp->a[0] = *as;
    cruckig_integrate(bp->t[0], *ps, *vs, *as, bp->j[0], ps, vs, as);

    if (bp->t[1] > 0.0) {
        bp->duration += bp->t[1];
        bp->p[1] = *ps;
        bp->v[1] = *vs;
        bp->a[1] = *as;
        cruckig_integrate(bp->t[1], *ps, *vs, *as, bp->j[1], ps, vs, as);
    }
}

void cruckig_brake_finalize_second_order(CRuckigBrakeProfile *bp, double *ps, double *vs, double *as) {
    if (bp->t[0] <= 0.0) {
        bp->duration = 0.0;
        return;
    }

    bp->duration = bp->t[0];
    bp->p[0] = *ps;
    bp->v[0] = *vs;
    cruckig_integrate(bp->t[0], *ps, *vs, bp->a[0], 0.0, ps, vs, as);
}
