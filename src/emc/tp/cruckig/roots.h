/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_ROOTS_H
#define CRUCKIG_ROOTS_H

#include "cruckig_internal.h"

/* A set of positive double roots, stored on the stack */
typedef struct {
    double data[4];
    size_t size;
} CRuckigRootSet;

CRUCKIG_FORCE_INLINE void cruckig_root_set_init(CRuckigRootSet *s) {
    s->size = 0;
}

CRUCKIG_FORCE_INLINE void cruckig_root_set_insert(CRuckigRootSet *s, double value) {
    if (value >= 0.0) {
        s->data[s->size] = value;
        s->size++;
    }
}

/*
 * Cube root, portable replacement for cbrt() (not available in kernel).
 * Optimized implementation from musl libc / FreeBSD libmsun.
 */
double cruckig_cbrt(double x);

/* Sort the root set (simple insertion sort for small N) */
void cruckig_root_set_sort(CRuckigRootSet *s);

/* Solve a*x^3 + b*x^2 + c*x + d = 0, returning positive roots */
CRuckigRootSet cruckig_roots_solve_cubic(double a, double b, double c, double d);

/* Solve resolvent equation, returns number of zeros */
int cruckig_roots_solve_resolvent(double x[3], double a, double b, double c);

/* Solve monic quartic x^4 + a*x^3 + b*x^2 + c*x + d = 0 */
CRuckigRootSet cruckig_roots_solve_quart_monic(double a, double b, double c, double d);

/* Evaluate polynomial of order N at x. Coefficients in descending order: p[0]*x^(N-1) + ... + p[N-1] */
double cruckig_roots_poly_eval(const double *p, size_t n, double x);

/* Calculate derivative coefficients */
void cruckig_roots_poly_derivative(const double *coeffs, size_t n, double *deriv);

/* Safe Newton method: find root in [l, h] where p(l)*p(h) < 0 */
double cruckig_roots_shrink_interval(const double *p, size_t n, double l, double h);

#endif /* CRUCKIG_ROOTS_H */
