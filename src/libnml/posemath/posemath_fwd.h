#ifndef POSEMATH_FWD_H
#define POSEMATH_FWD_H

struct PmCartesian;
struct PmCylindrical;
struct PmEuler_ZYX;
struct PmEuler_ZYZ;
struct PmHomogeneous;
struct PmPose;
struct PmQuaternion;
struct PmRotationMatrix;
struct PmRotationVector;
struct PmRPY;
struct PmSpherical;

typedef struct PmCartesian PmCartesian;
typedef struct PmCylindrical PmCylindrical;
typedef struct PmEuler_ZYX PmEuler_ZYX;
typedef struct PmEuler_ZYZ PmEuler_ZYZ;
typedef struct PmHomogeneous PmHomogeneous;
typedef struct PmPose PmPose;
typedef struct PmQuaternion PmQuaternion;
typedef struct PmRotationMatrix PmRotationMatrix;
typedef struct PmRotationVector PmRotationVector;
typedef struct PmRPY PmRPY;
typedef struct PmSpherical PmSpherical;

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
/* how close sin(a/2) is to 0 to be zero rotat */

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


#endif // POSEMATH_FWD_H
