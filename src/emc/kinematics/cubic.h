/********************************************************************
* Description: cubic.h
*   Cubic polynomial interpolation code
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef CUBIC_H
#define CUBIC_H

/*
   Coefficients of a cubic polynomial,

   a * x^3 + b * x^2 + c * x + d
*/

typedef struct {
    double a;
    double b;
    double c;
    double d;
} CUBIC_COEFF;

typedef struct {
    int configured;
    double segmentTime;
    int interpolationRate;
    double interpolationTime;
    double interpolationIncrement;
    double x0, x1, x2, x3;
    double wp0, wp1;
    double velp0, velp1;
    int filled;
    int needNextPoint;
    CUBIC_COEFF coeff;
} CUBIC_STRUCT;

extern int cubicInit(CUBIC_STRUCT * ci);
extern int cubicSetSegmentTime(CUBIC_STRUCT * ci, double time);
extern double cubicGetSegmentTime(CUBIC_STRUCT * ci);
extern int cubicSetInterpolationRate(CUBIC_STRUCT * ci, int rate);
extern int cubicGetInterpolationRate(CUBIC_STRUCT * ci);
extern int cubicAddPoint(CUBIC_STRUCT * ci, double point);
extern int cubicOffset(CUBIC_STRUCT * ci, double offset);
extern double cubicGetInterpolationIncrement(CUBIC_STRUCT * ci);
extern CUBIC_COEFF cubicGetCubicCoeff(CUBIC_STRUCT * ci);
extern int cubicFilled(CUBIC_STRUCT * ci);
extern double cubicInterpolate(CUBIC_STRUCT * ci, double *x,	/* same as
								   return val 
								 */
			       double *v,	/* velocity */
			       double *a,	/* accel */
			       double *j);	/* jerk */
extern int cubicNeedNextPoint(CUBIC_STRUCT * ci);
extern int cubicDrain(CUBIC_STRUCT * ci);

#endif				/* CUBIC_H */
