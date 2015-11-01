/********************************************************************
* Description: vector6.c
*  Vector structs and functions (generalizes PmCartesian)
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2015 All rights reserved.
********************************************************************/

#include "vector6.h"
#include "posemath.h"
#include "rtapi_math.h"

int VecVecAdd(Vector6 const * v1, Vector6 const * v2, Vector6 * out)
{
    if (!v1 || !v2 || !out) {
        return -1;
    }

    int i;
    for (i = 0; i < VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i] + v2->ax[i];
    }
    return 0;
}


int VecVecAddEq(Vector6 * v1, Vector6 const * v2)
{
    if (!v1 || !v2) {
        return -1;
    }

    int i;
    for (i = 0; i < VECTOR_SIZE; ++i) {
        v1->ax[i] += v2->ax[i];
    }
    return 0;
}


int VecVecSub(Vector6 const * v1, Vector6 const * v2, Vector6 * out)
{
    if (!v1 || !v2 || !out) {
        return -1;
    }
    int i;
    for (i = 0; i < VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i] - v2->ax[i];
    }
    return 0;
}

int VecScalMult(Vector6 const * v1, double s, Vector6 * out)
{
    if (!v1 || !out) {
        return -1;
    }
    int i;
    for (i = 0; i < VECTOR_SIZE; ++i) {
        out->ax[i] = v1->ax[i]*s;
    }
    return 0;
}

int VecVecDot(Vector6 const * v1, Vector6 const * v2, double * out)
{
    if (!v1 || !v2 || !out) {
        return -1;
    }
    int i;
    *out = 0.0;
    for (i = 0; i < VECTOR_SIZE; ++i) {
       *out += v1->ax[i] * v2->ax[i];
    }
    return 0;
}

int VecMagSq(Vector6 const * v1, double * out)
{
    return VecVecDot(v1,v1,out);
}

int VecMag(Vector6 const * v1, double * out)
{
    int res_mag = VecMagSq(v1, out);
    if (res_mag)
        return res_mag;

    *out = pmSqrt(*out);
    return 0;
}

int VecUnit(Vector6 const * v1, Vector6 * out)
{
    double mag = 0.0;
    int res_mag = VecMag(v1, &mag);
    //FIXME hard-coded cutoff
    if (res_mag || fabs(mag) < 1e-9) 
        return res_mag;

    VecScalMult(v1, 1.0 / mag, out);
    return 0;
}

int VecUnitEq(Vector6 * v1)
{
    double mag = 0.0;
    int res_mag = VecMag(v1, &mag);
    //FIXME hard-coded cutoff
    if (res_mag || fabs(mag) < 1e-9) 
        return res_mag;

    VecScalMult(v1, 1.0 / mag, v1);
    return 0;
}

int VecMin(Vector6 const * v1, double * m)
{
    if (!v1 || !m) {
        return -1;
    }

    int i;
    double m_temp = v1->ax[0];
    for (i = 0; i < VECTOR_SIZE; ++i) {
       m_temp = fmin(v1->ax[i], m_temp);
    }
    *m = m_temp;
    return 0;
}


int VecMax(Vector6 const * v1, double * m)
{
    if (!v1 || !m) {
        return -1;
    }

    int i;
    double m_temp = v1->ax[0];
    for (i = 0; i < VECTOR_SIZE; ++i) {
       m_temp = fmax(v1->ax[i], m_temp);
    }
    *m = m_temp;
    return 0;
}

int CartToVec(PmCartesian const * p1, PmCartesian const * p2, Vector6 * out)
{
    if (!p1 || !p2 || !out) {
        return -1;
    }

    // Copy out elements into our array
    out->ax[0] = p1->x;
    out->ax[1] = p1->y;
    out->ax[2] = p1->z;

    out->ax[3] = p2->x;
    out->ax[4] = p2->y;
    out->ax[5] = p2->z;

    return 0;

}
int VecToCart(Vector6 const * vec, PmCartesian * p1, PmCartesian * p2)
{
    if (!vec) {
        return -1;
    }

    if (p1) {
    p1->x = vec->ax[0];
    p1->y = vec->ax[1];
    p1->z = vec->ax[2];
    }

    if (p2) {
    p2->x = vec->ax[3];
    p2->y = vec->ax[4];
    p2->z = vec->ax[5];
    }

    return 0;
}

void VecPrint(Vector6 const * const vec)
{
#ifdef TP_DEBUG_PRINT
    int i;
    for (i = 0; i < 6; ++i) {
        rtapi_print("   %d: %f\n", i, vec->ax[i]);
    }
#endif
}
