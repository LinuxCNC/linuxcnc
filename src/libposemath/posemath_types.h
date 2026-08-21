/********************************************************************
* Description: posemath_types.h
*   Data types and constants of the pose math library.  Included by
*   posemath.h; code that wants only the types may include it directly.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#ifndef __LINUXCNC_POSEMATH_TYPES_H
#define __LINUXCNC_POSEMATH_TYPES_H

/* PmCartesian */

    typedef struct {
	double x, y, z;		/* this.x, etc. */

    } PmCartesian;

/* PmSpherical */

    typedef struct {
	double theta, phi, r;

    } PmSpherical;

/* PmCylindrical */

    typedef struct {
	double theta, r, z;

    } PmCylindrical;
/* PmAxis */
    typedef enum PM_AXIS { PM_X, PM_Y, PM_Z } PmAxis;

/* PmRotationVector */

    typedef struct {
	double s, x, y, z;

    } PmRotationVector;

/* PmRotationMatrix */

    typedef struct {
	PmCartesian x, y, z;

    } PmRotationMatrix;

/* PmQuaternion */

    typedef struct {
	double s, x, y, z;	/* this.s, etc. */

    } PmQuaternion;

/* PmEulerZyz */

    typedef struct {
	double z, y, zp;

    } PmEulerZyz;

/* PmEulerZyx */

    typedef struct {
	double z, y, x;

    } PmEulerZyx;

/* PmRpy */

    typedef struct {
	double r, p, y;

    } PmRpy;

/* PmPose */

    typedef struct {
	PmCartesian tran;
	PmQuaternion rot;

    } PmPose;

/* PmCartLine */
    typedef struct {
        PmCartesian start;
        PmCartesian end;
        PmCartesian uVec;
        double tmag;
        int tmag_zero;
    } PmCartLine;

/* Homogeneous transform PmHomogeneous */

    typedef struct {
	PmCartesian tran;
	PmRotationMatrix rot;

    } PmHomogeneous;

/* line structure */

    typedef struct {
	PmPose start;		/* where motion was started */
	PmPose end;		/* where motion is going */
	PmCartesian uVec;	/* unit vector from start to end */
	PmQuaternion qVec;	/* unit of rotation */
	double tmag;
	double rmag;
	int tmag_zero;
	int rmag_zero;

    } PmLine;

/* Generalized circle structure */

    typedef struct {
	PmCartesian center;
	PmCartesian normal;
	PmCartesian rTan;
	PmCartesian rPerp;
	PmCartesian rHelix;
	double radius;
	double angle;
	double spiral;

    } PmCircle;

/* some nice constants */

#define PM_PI      3.14159265358979323846
#define PM_PI_2    1.57079632679489661923
#define PM_PI_4    0.78539816339744830962
#define PM_2_PI    6.28318530717958647692

#ifdef TO_DEG
#undef TO_DEG
#endif
#define TO_DEG (180./PM_PI)

#ifdef TO_RAD
#undef TO_RAD
#endif
#define TO_RAD (PM_PI/180.)

/*! \todo FIXME-- fix these */

/* DOUBLE_FUZZ is the smallest double, d, such that (1+d != 1) w/o FPC.
   DOUBLECP_FUZZ is the same only with the Floating Point CoProcessor */

#define DOUBLE_FUZZ 2.2204460492503131e-16
#define DOUBLECP_FUZZ 1.0842021724855044e-19


/**
 * FIXME sloppily defined constants here.
 * These constants are quite large compared to the DOUBLE_FUZZ limitation. They
 * seem like an ugly band-aid for floating point problems.
 */

// FIXME setting this to be an order of magnitude smaller than canon's shortest
// allowed segment. This is still larger than TP's smallest position, so it may
// be silently causing trouble.
// andypugh 5/2/22 This seems to be interpreted to be in config units.
#define CART_FUZZ (1.0e-8)
/* how close a cartesian vector's magnitude must be for it to be considered
   a zero vector */

#define Q_FUZZ (1.0e-06)
/* how close elements of a Q must be to be equal */

#define QS_FUZZ (1.0e-6)
/* how close q.s is to 0 to be 180 deg rotation */

#define RS_FUZZ (1.0e-6)
/* how close r.s is for a rotation vector to be considered 0 */

#define QSIN_FUZZ (1.0e-6)
/* how close sin(a/2) is to 0 to be zero rotation */

#define V_FUZZ (1.0e-8)
/* how close elements of a V must be to be equal */

#define SQRT_FUZZ (-1.0e-6)
/* how close to 0 before math_sqrt() is error */

#define UNIT_VEC_FUZZ (1.0e-6)
/* how close mag of vec must be to 1.00 */

#define UNIT_QUAT_FUZZ (1.0e-6)
/* how close mag of quat must be to 1.00 */

#define UNIT_SC_FUZZ (1.0e-6)
/* how close mag of sin, cos must be to 1.00 */

#define E_EPSILON (1.0e-6)
/* how close second ZYZ euler angle must be to 0/PI for degeneration */

#define SINGULAR_EPSILON (1.0e-6)
/* how close to zero the determinate of a matrix must be for singularity */

#define RPY_P_FUZZ (1.0e-6)
/* how close pitch is to zero for RPY to degenerate */

#define ZYZ_Y_FUZZ (1.0e-6)
/* how close Y is to zero for ZYZ Euler to degenerate */

#define ZYX_Y_FUZZ (1.0e-6)
/* how close Y is to zero for ZYX Euler to degenerate */

#define CIRCLE_FUZZ (1.0e-6)
/* Bug fix for the missing circles problem */

#endif				/* #ifndef __LINUXCNC_POSEMATH_TYPES_H */
