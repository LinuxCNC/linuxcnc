/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */

#include "roots.h"

/*
 * cruckig_cbrt() - Cube root, used for cubic/quartic polynomial solving.
 * Optimized implementation from musl libc / FreeBSD libmsun.
 * Polynomial approximation to 23 bits + one Newton step to 53 bits.
 * Error < 0.667 ulps.
 *
 * Copyright (c) 1993 Sun Microsystems, Inc. All rights reserved.
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Copyright (c) 2005-2020 Rich Felker, et al. (musl libc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 */
double cruckig_cbrt(double x) {
    static const unsigned B1 = 715094163;
    static const unsigned B2 = 696219795;
    static const double P0 =  1.87595182427177009643;
    static const double P1 = -1.88497979543377169875;
    static const double P2 =  1.621429720105354466140;
    static const double P3 = -0.758397934778766047437;
    static const double P4 =  0.145996192886612446982;
    union { double f; unsigned long long i; } u = {x};
    double r, s, t, w;
    unsigned hx = u.i >> 32 & 0x7fffffff;

    if (hx >= 0x7ff00000)
        return x + x;

    if (hx < 0x00100000) {
        u.f = x * 0x1p54;
        hx = u.i >> 32 & 0x7fffffff;
        if (hx == 0) return x;
        hx = hx / 3 + B2;
    } else {
        hx = hx / 3 + B1;
    }
    u.i &= 1ULL << 63;
    u.i |= (unsigned long long)hx << 32;
    t = u.f;

    r = (t * t) * (t / x);
    t = t * ((P0 + r * (P1 + r * P2)) + ((r * r) * r) * (P3 + r * P4));

    u.f = t;
    u.i = (u.i + 0x80000000) & 0xffffffffc0000000ULL;
    t = u.f;

    s = t * t;
    r = x / s;
    w = t + t;
    r = (r - t) / (w + r);
    t = t + t * r;
    return t;
}

void cruckig_root_set_sort(CRuckigRootSet *s) {
    /* Insertion sort for small arrays (max 4 elements) */
    for (size_t i = 1; i < s->size; ++i) {
        double key = s->data[i];
        size_t j = i;
        while (j > 0 && s->data[j - 1] > key) {
            s->data[j] = s->data[j - 1];
            --j;
        }
        s->data[j] = key;
    }
}

CRUCKIG_HOT
CRuckigRootSet cruckig_roots_solve_cubic(double a, double b, double c, double d) {
    CRuckigRootSet roots;
    cruckig_root_set_init(&roots);

    if (fabs(d) < DBL_EPSILON) {
        /* First solution is x = 0 */
        cruckig_root_set_insert(&roots, 0.0);

        /* Converting to a quadratic equation */
        d = c;
        c = b;
        b = a;
        a = 0.0;
    }

    if (fabs(a) < DBL_EPSILON) {
        if (fabs(b) < DBL_EPSILON) {
            /* Linear equation */
            if (fabs(c) > DBL_EPSILON) {
                cruckig_root_set_insert(&roots, -d / c);
            }
        } else {
            /* Quadratic equation */
            const double discriminant = c * c - 4 * b * d;
            if (discriminant >= 0) {
                const double inv2b = 1.0 / (2 * b);
                const double y = sqrt(discriminant);
                cruckig_root_set_insert(&roots, (-c + y) * inv2b);
                cruckig_root_set_insert(&roots, (-c - y) * inv2b);
            }
        }
    } else {
        /* Cubic equation */
        const double inva = 1.0 / a;
        const double invaa = inva * inva;
        const double bb = b * b;
        const double bover3a = b * inva / 3;
        const double p = (a * c - bb / 3) * invaa;
        const double halfq = (2 * bb * b - 9 * a * b * c + 27 * a * a * d) / 54 * invaa * inva;
        const double yy = p * p * p / 27 + halfq * halfq;

        const double cos120 = -0.50;
        const double sin120 = 0.866025403784438646764;

        if (yy > DBL_EPSILON) {
            /* Sqrt is positive: one real solution */
            const double y = sqrt(yy);
            const double uuu = -halfq + y;
            const double vvv = -halfq - y;
            const double www = fabs(uuu) > fabs(vvv) ? uuu : vvv;
            const double w = cruckig_cbrt(www);
            cruckig_root_set_insert(&roots, w - p / (3 * w) - bover3a);
        } else if (yy < -DBL_EPSILON) {
            /* Sqrt is negative: three real solutions */
            const double x = -halfq;
            const double y = sqrt(-yy);
            double theta;
            double r;

            /* Convert to polar form */
            if (fabs(x) > DBL_EPSILON) {
                theta = (x > 0.0) ? atan(y / x) : (atan(y / x) + M_PI);
                r = sqrt(x * x - yy);
            } else {
                /* Vertical line */
                theta = M_PI / 2;
                r = y;
            }
            /* Calculate cube root */
            theta /= 3;
            r = 2 * cruckig_cbrt(r);
            /* Convert to complex coordinate */
            const double ux = cos(theta) * r;
            const double uyi = sin(theta) * r;

            cruckig_root_set_insert(&roots, ux - bover3a);
            cruckig_root_set_insert(&roots, ux * cos120 - uyi * sin120 - bover3a);
            cruckig_root_set_insert(&roots, ux * cos120 + uyi * sin120 - bover3a);
        } else {
            /* Sqrt is zero: two real solutions */
            const double www = -halfq;
            const double w = 2 * cruckig_cbrt(www);

            cruckig_root_set_insert(&roots, w - bover3a);
            cruckig_root_set_insert(&roots, w * cos120 - bover3a);
        }
    }
    return roots;
}

int cruckig_roots_solve_resolvent(double x[3], double a, double b, double c) {
    const double cos120 = -0.50;
    const double sin120 = 0.866025403784438646764;

    a /= 3;
    const double a2 = a * a;
    double q = a2 - b / 3;
    const double r = (a * (2 * a2 - b) + c) / 2;
    const double r2 = r * r;
    const double q3 = q * q * q;

    if (r2 < q3) {
        const double qsqrt = sqrt(q);
        double t_val = r / (q * qsqrt);
        if (t_val < -1.0) t_val = -1.0;
        if (t_val > 1.0) t_val = 1.0;
        q = -2 * qsqrt;

        const double theta = acos(t_val) / 3;
        const double ux = cos(theta) * q;
        const double uyi = sin(theta) * q;
        x[0] = ux - a;
        x[1] = ux * cos120 - uyi * sin120 - a;
        x[2] = ux * cos120 + uyi * sin120 - a;
        return 3;
    } else {
        double A = -cruckig_cbrt(fabs(r) + sqrt(r2 - q3));
        if (r < 0.0) {
            A = -A;
        }
        const double B = (0.0 == A ? 0.0 : q / A);

        x[0] = (A + B) - a;
        x[1] = -(A + B) / 2 - a;
        x[2] = sqrt(3.0) * (A - B) / 2;
        if (fabs(x[2]) < DBL_EPSILON) {
            x[2] = x[1];
            return 2;
        }

        return 1;
    }
}

CRUCKIG_HOT
CRuckigRootSet cruckig_roots_solve_quart_monic(double a, double b, double c, double d) {
    CRuckigRootSet roots;
    cruckig_root_set_init(&roots);

    if (fabs(d) < DBL_EPSILON) {
        if (fabs(c) < DBL_EPSILON) {
            cruckig_root_set_insert(&roots, 0.0);

            const double D = a * a - 4 * b;
            if (fabs(D) < DBL_EPSILON) {
                cruckig_root_set_insert(&roots, -a / 2);
            } else if (D > 0.0) {
                const double sqrtD = sqrt(D);
                cruckig_root_set_insert(&roots, (-a - sqrtD) / 2);
                cruckig_root_set_insert(&roots, (-a + sqrtD) / 2);
            }
            return roots;
        }

        if (fabs(a) < DBL_EPSILON && fabs(b) < DBL_EPSILON) {
            cruckig_root_set_insert(&roots, 0.0);
            cruckig_root_set_insert(&roots, -cruckig_cbrt(c));
            return roots;
        }
    }

    const double a3 = -b;
    const double b3 = a * c - 4 * d;
    const double c3 = -a * a * d - c * c + 4 * b * d;

    double x3[3];
    const int number_zeroes = cruckig_roots_solve_resolvent(x3, a3, b3, c3);

    double y = x3[0];
    /* Choosing Y with maximal absolute value */
    if (number_zeroes != 1) {
        if (fabs(x3[1]) > fabs(y)) {
            y = x3[1];
        }
        if (fabs(x3[2]) > fabs(y)) {
            y = x3[2];
        }
    }

    double q1, q2, p1, p2;
    double D;

    D = y * y - 4 * d;
    if (fabs(D) < DBL_EPSILON) {
        q1 = q2 = y / 2;
        D = a * a - 4 * (b - y);
        if (fabs(D) < DBL_EPSILON) {
            p1 = p2 = a / 2;
        } else {
            const double sqrtD = sqrt(D);
            p1 = (a + sqrtD) / 2;
            p2 = (a - sqrtD) / 2;
        }
    } else {
        const double sqrtD = sqrt(D);
        q1 = (y + sqrtD) / 2;
        q2 = (y - sqrtD) / 2;
        p1 = (a * q1 - c) / (q1 - q2);
        p2 = (c - a * q2) / (q1 - q2);
    }

    {
        const double eps = 16 * DBL_EPSILON;

        D = p1 * p1 - 4 * q1;
        if (fabs(D) < eps) {
            cruckig_root_set_insert(&roots, -p1 / 2);
        } else if (D > 0.0) {
            const double sqrtD = sqrt(D);
            cruckig_root_set_insert(&roots, (-p1 - sqrtD) / 2);
            cruckig_root_set_insert(&roots, (-p1 + sqrtD) / 2);
        }

        D = p2 * p2 - 4 * q2;
        if (fabs(D) < eps) {
            cruckig_root_set_insert(&roots, -p2 / 2);
        } else if (D > 0.0) {
            const double sqrtD = sqrt(D);
            cruckig_root_set_insert(&roots, (-p2 - sqrtD) / 2);
            cruckig_root_set_insert(&roots, (-p2 + sqrtD) / 2);
        }
    }

    return roots;
}

double cruckig_roots_poly_eval(const double *p, size_t n, double x) {
    if (n == 0) {
        return 0.0;
    }

    double retVal = 0.0;

    if (fabs(x) < DBL_EPSILON) {
        retVal = p[n - 1];
    } else if (x == 1.0) {
        for (int i = (int)n - 1; i >= 0; i--) {
            retVal += p[i];
        }
    } else {
        double xn = 1.0;
        for (int i = (int)n - 1; i >= 0; i--) {
            retVal += p[i] * xn;
            xn *= x;
        }
    }

    return retVal;
}

void cruckig_roots_poly_derivative(const double *coeffs, size_t n, double *deriv) {
    for (size_t i = 0; i < n - 1; ++i) {
        deriv[i] = (double)(n - 1 - i) * coeffs[i];
    }
}

double cruckig_roots_shrink_interval(const double *p, size_t n, double l, double h) {
    const size_t maxIts = 128;
    const double tolerance = 1e-14;

    const double fl = cruckig_roots_poly_eval(p, n, l);
    const double fh = cruckig_roots_poly_eval(p, n, h);
    if (fl == 0.0) {
        return l;
    }
    if (fh == 0.0) {
        return h;
    }
    if (fl > 0.0) {
        /* swap l and h */
        double tmp = l;
        l = h;
        h = tmp;
    }

    double rts = (l + h) / 2;
    double dxold = fabs(h - l);
    double dx = dxold;

    /* Compute derivative coefficients (n-1 elements) */
    double deriv[16]; /* max polynomial degree supported */
    cruckig_roots_poly_derivative(p, n, deriv);
    size_t dn = n - 1;

    double f = cruckig_roots_poly_eval(p, n, rts);
    double df = cruckig_roots_poly_eval(deriv, dn, rts);
    double temp;

    for (size_t j = 0; j < maxIts; j++) {
        if ((((rts - h) * df - f) * ((rts - l) * df - f) > 0.0) || (fabs(2 * f) > fabs(dxold * df))) {
            dxold = dx;
            dx = (h - l) / 2;
            rts = l + dx;
            if (l == rts) {
                break;
            }
        } else {
            dxold = dx;
            dx = f / df;
            temp = rts;
            rts -= dx;
            if (temp == rts) {
                break;
            }
        }

        if (fabs(dx) < tolerance) {
            break;
        }

        f = cruckig_roots_poly_eval(p, n, rts);
        df = cruckig_roots_poly_eval(deriv, dn, rts);
        if (f < 0.0) {
            l = rts;
        } else {
            h = rts;
        }
    }

    return rts;
}
