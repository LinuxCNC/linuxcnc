#ifndef CUBIC_H
#define CUBIC_H

/*
   cubic.h

   Cubic polynomial interpolation code

   Modification history:

   5-Jan-2004 MGS used this file to build a motion module for emc2.
   13-Mar-2000 WPS added unused attribute to cubic_h to avoid 'defined but not used' compiler warning.
   2-Aug-1999  FMP added cubicOffset()
   22-Oct-1998  FMP removed '_' from _filled and _needNextPoint
   18-Dec-1997  FMP took out C++ interface
   16-Jun-1997  FMP added cubicDrain()
   16-Apr-1997  FMP created from C and C++ separate versions
*/

/*
   Coefficients of a cubic polynomial,

   a * x^3 + b * x^2 + c * x + d
*/

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) cubic_h[] =
    "$Id$";

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
    double *v,			/* velocity */
    double *a,			/* accel */
    double *j);			/* jerk */
extern int cubicNeedNextPoint(CUBIC_STRUCT * ci);
extern int cubicDrain(CUBIC_STRUCT * ci);

#endif /* CUBIC_H */
