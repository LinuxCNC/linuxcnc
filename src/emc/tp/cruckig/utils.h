/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_UTILS_H
#define CRUCKIG_UTILS_H

#include "cruckig_internal.h"

CRUCKIG_FORCE_INLINE void cruckig_integrate(double t, double p0, double v0, double a0, double j,
                                            double * CRUCKIG_RESTRICT p_out,
                                            double * CRUCKIG_RESTRICT v_out,
                                            double * CRUCKIG_RESTRICT a_out) {
    *p_out = p0 + t * (v0 + t * (a0 / 2.0 + t * j / 6.0));
    *v_out = v0 + t * (a0 + t * j / 2.0);
    *a_out = a0 + t * j;
}

CRUCKIG_FORCE_INLINE double cruckig_pow2(double v) {
    return v * v;
}

#endif /* CRUCKIG_UTILS_H */
