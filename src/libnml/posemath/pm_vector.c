/**
 * @file pm_vector.c
 * 
 * API for PmVector (N-dimensional, compile-time sized vector object similar to PmCartesian)
 *
*  Derived from work by Fred Proctor & Will Shackleford (PmCartesian API)
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * @copyright Copyright 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#include "posemath.h"
#include "pm_vector.h"
#include "rtapi_math.h"
#include "string.h"

// For internal use only
typedef enum {
    X,
    Y,
    Z,
    A,
    B,
    C,
    U,
    V,
    W,
} PmVectorAxes;

static const double PM_MIN_VECTOR_MAG = 1e-9;
const PmVector PmVector_zero = {};

int VecCopyFrom(PmVector * const v, const PmVector * const from)
{
    memcpy(v, from, sizeof(PmVector));
    return 0;
}

int VecVecAdd(PmVector const * const v1, PmVector const * const v2, PmVector * const out)
{
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i] + v2->ax[i];
    }
    return 0;
}

int VecVecAddEq(PmVector * const v, PmVector const * const v2)
{
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        v->ax[i] += v2->ax[i];
    }
    return 0;
}

int VecVecSub(PmVector const * const v1, PmVector const * const v2, PmVector * const out)
{
    if (!v1 || !v2 || !out) {
        return -1;
    }
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i] - v2->ax[i];
    }
    return 0;
}

int VecVecSubEq(PmVector * const v, PmVector const * const v2)
{
    if (!v || !v2) {
        return -1;
    }
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        v->ax[i] -= v2->ax[i];
    }
    return 0;
}

int VecScalMult(PmVector const * const v1, double s, PmVector * const out)
{
    if (!v1 || !out) {
        return -1;
    }
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i]*s;
    }
    return 0;
}

int VecScalMultEq(PmVector * const v, double s)
{
    if (!v) {
        return -1;
    }
    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        v->ax[i] *= s;
    }
    return 0;
}

double VecVecDot(PmVector const * const v1, PmVector const * const v2)
{
    if (!v1 || !v2) {
        return NAN;
    }
    double dot = 0.0;
    for (int i = 0; i < PM_VECTOR_SIZE; ++i) {
        dot += v1->ax[i] * v2->ax[i];
    }
    return dot;
}

double VecMagSq(PmVector const * const v1)
{
    return VecVecDot(v1,v1);
}

double VecMag(PmVector const * const v1)
{
    double mag_sq = VecMagSq(v1);
    return pmSqrt(mag_sq);
}

double VecVecDisp(PmVector const * const v1, PmVector const * const v2)
{
    int i;
    double d_sq = 0;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        d_sq += pmSq(v2->ax[i]-v1->ax[i]);
    }

    return pmSqrt(d_sq);
}

int VecUnit(PmVector const * const v1, PmVector * const out)
{
    *out = *v1;
    return VecUnitEq(out);
}

int VecUnitEq(PmVector * const v1)
{
    double mag = VecMag(v1);
    if (fabs(mag) < PM_MIN_VECTOR_MAG)
        return -1;

    VecScalMult(v1, 1.0 / mag, v1);
    return 0;
}


int VecAbs(const PmVector * const v1, PmVector * const out)
{
    if (!v1 || !out) {
        return -1;
    }

    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        out->ax[i] = fabs(v1->ax[i]);
    }
    return 0;
}

int VecVecDirection(
    PmVector const * const to,
    PmVector const * const from,
    PmVector * const u)
{
    VecVecSub(to, from, u);
    return VecUnitEq(u);
}

double VecMin(PmVector const * const v1)
{
    if (!v1) {
        return NAN;
    }

    int i;
    double v_min = v1->ax[0];
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        v_min = fmin(v1->ax[i], v_min);
    }
    return v_min;
}


double VecMax(PmVector const * const v1)
{
    if (!v1) {
        return NAN;
    }

    int i;
    double v_max = v1->ax[0];
    for (i = 1; i < PM_VECTOR_SIZE; ++i) {
        v_max = fmax(v1->ax[i], v_max);
    }
    return v_max;
}

double VecAbsMax(const PmVector * const v1)
{
    if (!v1) {
        return NAN;
    }

    int i;
    double v_max = fabs(v1->ax[0]);
    for (i = 1; i < PM_VECTOR_SIZE; ++i) {
        v_max = fmax(fabs(v1->ax[i]), v_max);
    }
    return v_max;
}

int CartToVec(
    PmCartesian const * p_xyz,
    PmCartesian const * p_abc,
    PmCartesian const * p_uvw,
    PmVector * const out)
{
    *out = PmVector_zero;
    if (p_xyz) {
        // Copy out elements into our array
        out->ax[X] = p_xyz->x;
        out->ax[Y] = p_xyz->y;
        out->ax[Z] = p_xyz->z;
    }

    if (p_abc) {
        out->ax[A] = p_abc->x;
        out->ax[B] = p_abc->y;
        out->ax[C] = p_abc->z;
    }

    if (p_uvw) {
        out->ax[U] = p_uvw->x;
        out->ax[V] = p_uvw->y;
        out->ax[W] = p_uvw->z;
    }

    return 0;
}

int VecSetXYZ(const PmCartesian * const p_xyz, PmVector * const out)
{
    if (p_xyz) {
        // Copy out elements into our array
        out->ax[X] = p_xyz->x;
        out->ax[Y] = p_xyz->y;
        out->ax[Z] = p_xyz->z;
    }
    return 0;
}

int VecSetABC(const PmCartesian * const p_abc, PmVector * const out)
{
    if (p_abc) {
        // Copy out elements into our array
        out->ax[X] = p_abc->x;
        out->ax[Y] = p_abc->y;
        out->ax[Z] = p_abc->z;
    }
    return 0;
}

int VecToCart(
    PmVector const * const vec,
    PmCartesian * p_xyz,
    PmCartesian * p_abc,
    PmCartesian * p_uvw)
{
    if (!vec) {
        return -1;
    }

    if (p_xyz) {
        p_xyz->x = vec->ax[X];
        p_xyz->y = vec->ax[Y];
        p_xyz->z = vec->ax[Z];
    }

    if (p_abc) {
        p_abc->x = vec->ax[A];
        p_abc->y = vec->ax[B];
        p_abc->z = vec->ax[C];
    }

    if (p_uvw) {
        p_uvw->x = vec->ax[U];
        p_uvw->y = vec->ax[V];
        p_uvw->z = vec->ax[W];
    }

    return 0;
}

int VecVecOrthonormal(
    PmVector const * const a,
    PmVector const * const b,
    PmVector * const u,
    PmVector * const v)
{
    *u = *a;

    // Find component of b in direction of a
    // NOTE assumes both a and b are already unit
    double dot = VecVecDot(a, b);
    //Build the component in the direction of a
    VecScalMult(u, -dot, v);
    VecVecAddEq(v, b);

    // Subtract component of a in direction of b
    VecUnitEq(v);

    return 0;
}

/**
 * Performs "harmonic" addition elementwise on the vector and returns the square of the result.
 * out(i) = arg_max(cos(theta_i) * a(i) + sin(theta_i) * b(i))
 *
 * According to the harmonic addition theorem, this is equivalent to
 * out(i) = arg_max(c(i) * cos(theta_i + d_i)), where d_i is a phase angle, and c(i)^2 = a(i)^2+b(i)^2
 *
 * Therfore, the square of the harmonic addition out(i)^2 = a(i)^2 + b(i)^2.
 *
 * Returning the square term saves 3 square root operations (which can sometimes be avoided depending on the final calculation).
 */
int VecVecHarmonicAddSq(PmVector const * const u,
                        PmVector const * const v,
                        PmVector * const scales)
{
    if (!u || !v || !scales) {
        return -1;
    }

    int i;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        scales->ax[i] = pmSq(u->ax[i]) + pmSq(v->ax[i]);
    }
    return 0;
}

/**
 * Check if two UNIT vectors are parallel to within a numerical threshold.
 * In this case, parallel means the magnitude of their difference is below the specified threshold.
 */
int VecVecUnitParallel(
        PmVector const * const u, //!< first vector to compare
        PmVector const * const v, //!< second vector to compare
        double threshold_sq //!< square of the magnitude threshold
        )
{
    if (!u || !v) {
        return -1;
    }
    PmVector u_diff;
    VecVecSub(u, v, &u_diff);
    return VecMagSq(&u_diff) < threshold_sq;
}

/**
 * Check if two UNIT vectors are anti-parallel to within a numerical threshold.
 * In this case, parallel means the magnitude of their sum is below the specified threshold.
 */
int VecVecUnitAntiParallel(
        PmVector const * const u, //!< first vector
        PmVector const * const v, //!< second vector
        double threshold_sq //!< square of the magnitude threshold
        )
{
    if (!u || !v) {
        return -1;
    }
    PmVector u_sum;
    VecVecAdd(u, v, &u_sum);
    return VecMagSq(&u_sum) < threshold_sq;
}

bool VecHasNAN(const PmVector * const v)
{
    int i;
    bool has_nan = false;
    for (i = 0; i < PM_VECTOR_SIZE; ++i) {
        has_nan |= isnan(v->ax[i]);
    }
    return has_nan;
}

double VecVLimit(PmVector const * const uVec, double v_target, double v_limit_linear, double v_limit_angular)
{
    PmCartesian xyz, abc, uvw;
    VecToCart(uVec, &xyz, &abc, &uvw);
    double m_xyz;
    pmCartMag(&xyz, &m_xyz);
    double m_abc;
    pmCartMag(&abc, &m_abc);
    double m_uvw;
    pmCartMag(&uvw, &m_uvw);

    double u_linear = fmax(m_xyz, m_uvw);
    if (u_linear > 1e-12) {
        return fmin(v_target, v_limit_linear / u_linear);
    } else if (m_abc > 1e-12) {
        return fmin(v_target, v_limit_angular / m_abc);
    } else {
        return 0.0;
    }
}
