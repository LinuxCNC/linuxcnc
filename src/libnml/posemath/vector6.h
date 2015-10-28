/********************************************************************
* Description: vector6.h
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
#ifndef VECTOR6_H
#define VECTOR6_H
#include "posemath.h"

typedef struct {
    double ax[6];
} Vector6;

int VecVecAdd(Vector6 const * v1, Vector6 const * v2, Vector6 * out);
int VecVecSub(Vector6 const * v1, Vector6 const * v2, Vector6 * out);
int VecScalMult(Vector6 const * v1, double s, Vector6 * vout);
int VecVecDot(Vector6 const * v1, Vector6 const * v2, double * out);
int VecMag(Vector6 const * v1, double * out);
int VecUnit(Vector6 const * v1, Vector6 * out);

int CartToVec(PmCartesian const * p1, PmCartesian const * p2, Vector6 * out);
int VecToCart(Vector6 const * vec, PmCartesian * p1, PmCartesian * p2);
#endif
