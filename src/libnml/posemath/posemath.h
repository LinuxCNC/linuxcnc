/********************************************************************
* Description: posemath.h
*   Declarations for pose math library data types and manipulation
*   functions.
*
*   Data types comprise various representations of translation and
*   rotation quantities, and a 'pose' for representing the location
*   and orientation of a frame in space relative to a base frame.
*   Translation representations include cartesian, spherical, and
*   cylindrical coordinates. All of these contain 3 elements. Rotation
*   representations include rotation vectors, quaternions, rotation
*   matrices, Euler angles, and roll-pitch-yaw. These contain at least
*   3 elements, and may contain more. Only 3 are necessary for the 3
*   degrees of freedom for either translation or rotation, but some
*   data representations use more for computational efficiency or
*   intuition at the expense of storage space.
*
*   Types are abbreviated in function naming with a few letters.
*   Functions exist for conversion between data types, checking for
*   consistency, normalization into consistency, extracting features
*   such as size, and arithmetic operations.
*
*   Names of data representations are in all capitals, prefixed with
*   'PM_'. Names of functions are in mixed case, prefixed with 'pm',
*   with case changes used to indicate new quantities instead of
*   underscores. Function syntax looks like
*    int UmQuatRotConvert(PM_QUATERNION, PM_ROTATION_VECTOR *);
*
*   The return value is an error code, 0 for success, or a non-zero
*   error code for failure, for example:
*
*    #define PM_ERR -1
*    #define PM_IMPL_ERR -2
*
*   The global variable 'pmErrno' is set to this return value.
*
*   C++ classes are used for data types so that operator overloading can
*   be used to reduce the programming labor. Using the overloaded operator
*   version of functions loses the integer error code. The global
*   variable 'pmErrno' can be queried after these operations. This is not
*   thread-safe or reentrant.
*
*   C++ names corresponding to the C structures use case mixing instead
*   of all caps. Thus, a quaternion in C++ is a PmQuaternion.
*
*   The MATH_DEBUG symbol can be defined to include error reporting via
*   printed errors.
*
*   Native efficient C functions exist for the PM_CARTESIAN, PM_QUATERNION,
*   and PM_POSE types. Constructors in all the classes have been defined
*   to convert to/from PM_CARTESIAN and any other translation type, and
*   to convert to/from PM_QUATERNION and any other rotation type. This means
*   that if no explicit C functions exist for another type, conversions
*   to the corresponding native type will occur automatically. If more
*   efficiency is desired for a particular type, C functions to handle the
*   operations should be coded and the overloaded C++ functions or operators
*   should be added.
*
*   NOTE: posemath C / C++ libraries are now split into c/h and cc/hh file pairs to make the bookeeping easier.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

#ifndef POSEMATH_H
#define POSEMATH_H

#include "posemath_fwd.h"
// #include "config.h"

/* now comes the C stuff */

#ifdef __cplusplus
extern "C" {
#endif

/* PmCartesian */

    struct PmCartesian {
	double x, y, z;		/* this.x, etc. */

    };


/* PmSpherical */

    struct  PmSpherical{
	double theta, phi, r;

    };

/* PmCylindrical */

    struct  PmCylindrical {
	double theta, r, z;

    };

/* PmAxis */
    typedef enum { PM_X, PM_Y, PM_Z } PmAxis;

/* PmRotationVector */

    struct PmRotationVector {
	double s, x, y, z;

    };

/* PmRotationMatrix */

    struct PmRotationMatrix{
	PmCartesian x, y, z;

    };

/* PmQuaternion */

    struct PmQuaternion {
	double s, x, y, z;	/* this.s, etc. */

    };

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

    struct PmPose {
	PmCartesian tran;
	PmQuaternion rot;

    };

/* PmCartLine */
    typedef struct {
        PmCartesian start;
        PmCartesian end;
        PmCartesian uVec;
        double tmag;
        int tmag_zero;
    } PmCartLine;

/* Homogeneous transform PmHomogeneous */

    struct PmHomogeneous{
	PmCartesian tran;
	PmRotationMatrix rot;

    };

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
    double height;
    double angle;
	double spiral;

    } PmCircle;

/*
   shorthand types for normal use-- don't define PM_LOOSE_NAMESPACE if these
   names are used by other headers you need to include and you don't want
   these shorthand versions
*/

/* some nice constants */

#define PM_PI      3.14159265358979323846
#define PM_PI_2    1.57079632679489661923
#define PM_PI_4    0.78539816339744830962
#define PM_TAU    6.28318530717958647692

#ifdef PM_LOOSE_NAMESPACE

    typedef PmCartesian VECTOR;
    typedef PmSpherical SPHERICAL;
    typedef PmCylindrical CYLINDRICAL;
    typedef PmQuaternion QUATERNION;
    typedef PmRotationMatrix MATRIX;
    typedef PmEulerZyz ZYZ;
    typedef PmEulerZyx ZYX;
    typedef PmRpy RPY;
    typedef PmPose POSE;
    typedef PmHomogeneous HX;
    typedef PmCircle CIRCLE;
    typedef PmLine LINE;

#define PI                PM_PI
#define PI_2              PM_PI_2
#define PI_4              PM_PI_4
#define TWO_PI            PM_2_PI	/* 2_PI invalid macro name */
#endif

/* quicky macros */

#define pmClose(a, b, eps) ((fabs((a) - (b)) < (eps)) ? 1 : 0)
#define pmSq(x) ((x)*(x))
#define pmCb(x) ((x)*(x)*(x))

#ifdef TO_DEG
#undef TO_DEG
#endif
#define TO_DEG (180./PM_PI)

#ifdef TO_RAD
#undef TO_RAD
#endif
#define TO_RAD (PM_PI/180.)

/*! \todo FIXME-- fix these */


/* debug output printing */
    extern void pmPrintError(const char *fmt, ...) __attribute__((format(printf,1,2)));

/* global error number and errors */
    extern int pmErrno;
    extern void pmPerror(const char *fmt);

    typedef enum {
        PM_DIV_ERR = -4, /* divide by zero error */
        PM_NORM_ERR = -3, /* arg should have been norm */
        PM_IMPL_ERR = -2, /* not implemented */
        PM_ERR = -1, /* unspecified error */
        PM_OK = 0
    } PosemathErrCode;

/* Scalar functions */

    extern double pmSqrt(double x);

/* Translation rep conversion functions */

    extern int pmCartSphConvert(PmCartesian const * const, PmSpherical * const);
    extern int pmCartCylConvert(PmCartesian const * const, PmCylindrical * const);
    extern int pmSphCartConvert(PmSpherical const * const, PmCartesian * const);
    extern int pmSphCylConvert(PmSpherical const * const, PmCylindrical * const);
    extern int pmCylCartConvert(PmCylindrical const * const, PmCartesian * const);
    extern int pmCylSphConvert(PmCylindrical const * const, PmSpherical * const);

/* Rotation rep conversion functions */

    extern int pmAxisAngleQuatConvert(PmAxis, double, PmQuaternion * const);

    extern int pmRotQuatConvert(PmRotationVector const * const, PmQuaternion * const);
    extern int pmRotMatConvert(PmRotationVector const * const, PmRotationMatrix * const);
    extern int pmRotZyzConvert(PmRotationVector const * const, PmEulerZyz * const);
    extern int pmRotZyxConvert(PmRotationVector const * const, PmEulerZyx * const);
    extern int pmRotRpyConvert(PmRotationVector const * const, PmRpy * const);

    extern int pmQuatRotConvert(PmQuaternion const * const, PmRotationVector * const);
    extern int pmQuatMatConvert(PmQuaternion const * const, PmRotationMatrix * const);
    extern int pmQuatZyzConvert(PmQuaternion const * const, PmEulerZyz * const);
    extern int pmQuatZyxConvert(PmQuaternion const * const, PmEulerZyx * const);
    extern int pmQuatRpyConvert(PmQuaternion const * const, PmRpy * const);

    extern int pmMatRotConvert(PmRotationMatrix const * const, PmRotationVector * const);
    extern int pmMatQuatConvert(PmRotationMatrix const * const, PmQuaternion * const);
    extern int pmMatZyzConvert(PmRotationMatrix const * const, PmEulerZyz * const);
    extern int pmMatZyxConvert(PmRotationMatrix const * const, PmEulerZyx * const);
    extern int pmMatRpyConvert(PmRotationMatrix const * const, PmRpy * const);

    extern int pmZyzRotConvert(PmEulerZyz const * const, PmRotationVector * const);
    extern int pmZyzQuatConvert(PmEulerZyz const * const, PmQuaternion * const);
    extern int pmZyzMatConvert(PmEulerZyz const * const, PmRotationMatrix * const);
    extern int pmZyzZyxConvert(PmEulerZyz const * const, PmEulerZyx * const);
    extern int pmZyzRpyConvert(PmEulerZyz const * const, PmRpy * const);

    extern int pmZyxRotConvert(PmEulerZyx const * const, PmRotationVector * const);
    extern int pmZyxQuatConvert(PmEulerZyx const * const, PmQuaternion * const);
    extern int pmZyxMatConvert(PmEulerZyx const * const, PmRotationMatrix * const);
    extern int pmZyxZyzConvert(PmEulerZyx const * const, PmEulerZyz * const);
    extern int pmZyxRpyConvert(PmEulerZyx const * const, PmRpy * const);

    extern int pmRpyRotConvert(PmRpy const * const, PmRotationVector * const);
    extern int pmRpyQuatConvert(PmRpy const * const, PmQuaternion * const);
    extern int pmRpyMatConvert(PmRpy const * const, PmRotationMatrix * const);
    extern int pmRpyZyzConvert(PmRpy const * const, PmEulerZyz * const);
    extern int pmRpyZyxConvert(PmRpy const * const, PmEulerZyx * const);

/* Combined rep conversion functions */

    extern int pmPoseHomConvert(PmPose const * const, PmHomogeneous* const);

    extern int pmHomPoseConvert(PmHomogeneous const * const, PmPose * const);

/* Arithmetic functions

   Note: currently, only functions for PmCartesian, PmQuaternion, and
   PmPose are supported directly. The type conversion functions
   will be used implicitly when applying arithmetic function
   to other types. This will be slower and less accurate. Explicit
   functions can be added incrementally.
*/

/* translation functions */

/* NOTE:  only Cartesian type supported in C now */

    extern int pmCartCartCompare(PmCartesian const * const, PmCartesian const * const);
    extern int pmCartCartDot(PmCartesian const * const, PmCartesian const * const, double * const);
    extern int pmCartCartCross(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartCartMult(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartCartDiv(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartInfNorm(PmCartesian const * v, double * out);
    extern int pmCartMag(PmCartesian const * const, double * const);
    extern int pmCartMagSq(PmCartesian const * const, double * const);
    extern int pmCartCartDisp(PmCartesian const * const v1, PmCartesian const * const v2, double *d);
    extern int pmCartCartAdd(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartCartSub(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartCartElemDivNonZero(
        PmCartesian const * const v,
        PmCartesian const * const divisor,
        PmCartesian * const scale);

    extern int pmCartScalAdd(const PmCartesian * v, double d, PmCartesian * vout);
    extern int pmCartScalSub(const PmCartesian * v, double d, PmCartesian * vout);
    extern int pmCartScalMult(PmCartesian const * const, double, PmCartesian * const);
    extern int pmCartScalDiv(PmCartesian const * const, double, PmCartesian * const);
    extern int pmCartNeg(PmCartesian const * const, PmCartesian * const);
    extern int pmCartUnit(PmCartesian const * const, PmCartesian * const);
    extern int pmCartAbs(PmCartesian const * const, PmCartesian * const);
    extern int pmCartAbsMax(PmCartesian const * const v, double * out);
    // Equivalent of compound operators like +=, -=, etc. Basically, these functions work directly on the first PmCartesian
    extern int pmCartCartAddEq(PmCartesian * const, PmCartesian const * const);
    extern int pmCartCartSubEq(PmCartesian * const, PmCartesian const * const);
    extern int pmCartScalAddEq(PmCartesian * v, double d);
    extern int pmCartScalSubEq(PmCartesian * const v, double d);
    extern int pmCartScalMultEq(PmCartesian * const, double);
    extern int pmCartScalDivEq(PmCartesian * const, double);
    extern int pmCartUnitEq(PmCartesian * const);
    extern int pmCartNegEq(PmCartesian * const);
    extern int pmCartCartDirection(PmCartesian const * const to, PmCartesian const * const, PmCartesian * const);
/*! \todo Another #if 0 */
#if 0
    extern int pmCartNorm(PmCartesian const * const v, PmCartesian * const vout);
#else
// Hopefully guaranteed to cause a compile error when used.
#define pmCartNorm(a,b,c,d,e)  bad{a.b.c.d.e}
#endif

    extern int pmCartCartParallel(PmCartesian const * const u1,
        PmCartesian const * const u2,
        double tol);

    extern int pmCartCartAntiParallel(PmCartesian const * const u1,
        PmCartesian const * const u2,
        double tol);

    extern int pmCartIsNorm(PmCartesian const * const v);
    extern int pmCartInv(PmCartesian const * const, PmCartesian * const);
    extern int pmCartInvEq(PmCartesian * const);
    extern int pmCartCartProj(PmCartesian const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmCartPlaneProj(PmCartesian const * const v, PmCartesian const * const normal,
	PmCartesian * vout);
    extern int pmCartCartOrthonormal(
        PmCartesian const * const a,
        PmCartesian const * const b,
        PmCartesian * const u,
        PmCartesian * const v);
    extern int pmCartCartHarmonicAddSq(
        PmCartesian const * const a,
        PmCartesian const * const b,
        PmCartesian * const out);

/* rotation functions */

/* quaternion functions */

    extern int pmQuatQuatCompare(PmQuaternion const * const, PmQuaternion const * const);
    extern int pmQuatMag(PmQuaternion const * const q, double *d);
    extern int pmQuatNorm(PmQuaternion const * const, PmQuaternion * const);
    extern int pmQuatInv(PmQuaternion const * const, PmQuaternion * const);
    extern int pmQuatIsNorm(PmQuaternion const * const);
    extern int pmQuatScalMult(PmQuaternion const * const q, double s, PmQuaternion * const qout);
    extern int pmQuatScalDiv(PmQuaternion const * const q, double s, PmQuaternion * const qout);
    extern int pmQuatQuatMult(PmQuaternion const * const, PmQuaternion const * const, PmQuaternion * const);
    extern int pmQuatCartMult(PmQuaternion const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmQuatAxisAngleMult(PmQuaternion const * const, PmAxis, double,
	PmQuaternion *);

/* rotation vector functions */

    extern int pmRotScalMult(PmRotationVector const * const, double, PmRotationVector * const);
    extern int pmRotScalDiv(PmRotationVector const * const, double, PmRotationVector * const);
    extern int pmRotIsNorm(PmRotationVector const * const);
    extern int pmRotNorm(PmRotationVector const * const, PmRotationVector * const);

/* rotation matrix functions */

/*        |  m.x.x   m.y.x   m.z.x  |   */
/*   M =  |  m.x.y   m.y.y   m.z.y  |   */
/*        |  m.x.z   m.y.z   m.z.z  |   */

    extern int pmMatNorm(PmRotationMatrix const * const m, PmRotationMatrix * const mout);
    extern int pmMatIsNorm(PmRotationMatrix const * const  m);
    extern int pmMatInv(PmRotationMatrix const * const  m, PmRotationMatrix * const mout);
    extern int pmMatCartMult(PmRotationMatrix const * const  m, PmCartesian const * const  v,
	PmCartesian * const vout);
    extern int pmMatMatMult(PmRotationMatrix const * const  m1, PmRotationMatrix const * const  m2,
	PmRotationMatrix * const mout);

/* pose functions*/

    extern int pmPosePoseCompare(PmPose const * const, PmPose const * const);
    extern int pmPoseInv(PmPose const * const p, PmPose * const);
    extern int pmPoseCartMult(PmPose const * const, PmCartesian const * const, PmCartesian * const);
    extern int pmPosePoseMult(PmPose const * const, PmPose const * const, PmPose * const);

/* homogeneous functions */
    extern int pmHomInv(PmHomogeneous const * const, PmHomogeneous * const);

/* line functions */

    extern int pmLineInit(PmLine * const line, PmPose const * const start, PmPose const * const end);
    extern int pmLinePoint(PmLine const * const line, double len, PmPose * const point);

/* pure cartesian line functions */
    extern int pmCartLineInit(PmCartLine * const line, PmCartesian const * const start, PmCartesian const * const end);
    extern int pmCartLinePoint(PmCartLine const * const line, double len, PmCartesian * const point);
    extern int pmCartLineStretch(PmCartLine * const line, double new_len, int from_end);

/* circle functions */

    extern int pmCircleInit(
        PmCircle * const circle,
        PmCartesian const * const start,
        PmCartesian const * const end,
        PmCartesian const * const center,
        PmCartesian const * const normal,
        int turn,
        double expected_angle_rad);

    extern int pmCirclePoint(PmCircle const * const circle, double angle, PmCartesian * const point);
    extern int pmCircleCut(PmCircle * const circ, double new_angle, int keep_end_pt);
    extern int pmCircleStartPoint(PmCircle const * const circle, PmCartesian * const point);

#ifdef __cplusplus
}				/* matches extern "C" for C++ */
#endif
#endif				/* #ifndef POSEMATH_H */
