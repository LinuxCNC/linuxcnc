/********************************************************************
* Description: posemath.hh
*   The C++ interface of the pose math library: the PM_ classes, the
*   operators over them and the templates that copy between the class
*   and the C representation of a type.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#ifndef __LINUXCNC_POSEMATH_HH
#define __LINUXCNC_POSEMATH_HH

#ifndef __cplusplus
#error posemath.hh is the C++ interface; C code wants posemath.h
#endif

#include "posemath.h"

/* forward declarations-- conversion ctors will need these */

/* translation types */
struct PM_CARTESIAN;		/* Cart */
struct PM_SPHERICAL;		/* Sph */
struct PM_CYLINDRICAL;		/* Cyl */

/* rotation types */
struct PM_ROTATION_VECTOR;	/* Rot */
struct PM_ROTATION_MATRIX;	/* Mat */
struct PM_QUATERNION;		/* Quat */
struct PM_EULER_ZYZ;		/* Zyz */
struct PM_EULER_ZYX;		/* Zyx */
struct PM_RPY;			/* Rpy */

/* pose types */
struct PM_POSE;			/* Pose */
struct PM_HOMOGENEOUS;		/* Hom */

/* PM_CARTESIAN */

struct PM_CARTESIAN {
    /* ctors/dtors */
    PM_CARTESIAN() {
    };
    PM_CARTESIAN(double _x, double _y, double _z);

    PM_CARTESIAN(const PM_CYLINDRICAL & c);	/* conversion */
    PM_CARTESIAN(const PM_SPHERICAL & s);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */
    PM_CARTESIAN & operator += (const PM_CARTESIAN &o);
    PM_CARTESIAN & operator -= (const PM_CARTESIAN &o);

    // Scalar operations
    PM_CARTESIAN & operator *= (double o);
    PM_CARTESIAN & operator /= (double o);

    /* data */
    double x, y, z;		/* this.x, etc. */
};

/* PM_SPHERICAL */

struct PM_SPHERICAL {
    /* ctors/dtors */
    PM_SPHERICAL() {
    };
    PM_SPHERICAL(double _theta, double _phi, double _r);
    PM_SPHERICAL(const PM_CYLINDRICAL & v);	/* conversion */
    PM_SPHERICAL(const PM_CARTESIAN & v);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double theta, phi, r;
};

/* PM_CYLINDRICAL */

struct PM_CYLINDRICAL {
    /* ctors/dtors */
    PM_CYLINDRICAL() {
    };
    PM_CYLINDRICAL(double _theta, double _r, double _z);
    PM_CYLINDRICAL(const PM_CARTESIAN & v);	/* conversion */
    PM_CYLINDRICAL(const PM_SPHERICAL & v);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double theta, r, z;
};

/* PM_ROTATION_VECTOR */

struct PM_ROTATION_VECTOR {
    /* ctors/dtors */
    PM_ROTATION_VECTOR() {
    };
    PM_ROTATION_VECTOR(double _r, double _x, double _y, double _z);
    PM_ROTATION_VECTOR(const PM_QUATERNION & q);	/* conversion
								 */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double s, x, y, z;
};

/* PM_ROTATION_MATRIX */

struct PM_ROTATION_MATRIX {
    /* ctors/dtors */
    PM_ROTATION_MATRIX() {
    };
    PM_ROTATION_MATRIX(double xx, double xy, double xz,
	double yx, double yy, double yz, double zx, double zy, double zz);
    PM_ROTATION_MATRIX(const PM_CARTESIAN& _x, const PM_CARTESIAN& _y, const PM_CARTESIAN& _z);
    PM_ROTATION_MATRIX(const PM_ROTATION_VECTOR & v);	/* conversion
								 */
    PM_ROTATION_MATRIX(const PM_QUATERNION & q);	/* conversion
								 */
    PM_ROTATION_MATRIX(const PM_EULER_ZYZ & zyz);	/* conversion
								 */
    PM_ROTATION_MATRIX(const PM_EULER_ZYX & zyx);	/* conversion
								 */
    PM_ROTATION_MATRIX(const PM_RPY & rpy);	/* conversion */

    /* operators */
    PM_CARTESIAN & operator[](int n);	/* this[n] */

    /* data */
    PM_CARTESIAN x, y, z;
};

/* PM_QUATERNION */
struct PM_QUATERNION {
    /* ctors/dtors */
    PM_QUATERNION() {
    };
    PM_QUATERNION(double _s, double _x, double _y, double _z);
    PM_QUATERNION(const PM_ROTATION_VECTOR & v);	/* conversion
								 */
    PM_QUATERNION(const PM_ROTATION_MATRIX & m);	/* conversion
								 */
    PM_QUATERNION(const PM_EULER_ZYZ & zyz);	/* conversion */
    PM_QUATERNION(const PM_EULER_ZYX & zyx);	/* conversion */
    PM_QUATERNION(const PM_RPY & rpy);	/* conversion */
    PM_QUATERNION(PM_AXIS axis, double angle);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* functions */
    void axisAngleMult(PM_AXIS axis, double angle);

    /* data */
    double s, x, y, z;		/* this.s, etc. */
};

/* PM_EULER_ZYZ */

struct PM_EULER_ZYZ {
    /* ctors/dtors */
    PM_EULER_ZYZ() {
    };
    PM_EULER_ZYZ(double _z, double _y, double _zp);
    PM_EULER_ZYZ(const PM_QUATERNION & q);	/* conversion */
    PM_EULER_ZYZ(const PM_ROTATION_MATRIX & m);	/* conversion */

    /* operators */
    double &operator[] (int n);

    /* data */
    double z, y, zp;
};

/* PM_EULER_ZYX */

struct PM_EULER_ZYX {
    /* ctors/dtors */
    PM_EULER_ZYX() {
    };
    PM_EULER_ZYX(double _z, double _y, double _x);
    PM_EULER_ZYX(const PM_QUATERNION & q);	/* conversion */
    PM_EULER_ZYX(const PM_ROTATION_MATRIX & m);	/* conversion */

    /* operators */
    double &operator[] (int n);

    /* data */
    double z, y, x;
};

/* PM_RPY */

struct PM_RPY {
    /* ctors/dtors */
    PM_RPY() {
    };
    PM_RPY(double _r, double _p, double _y);
    PM_RPY(const PM_QUATERNION & q);	/* conversion */
    PM_RPY(const PM_ROTATION_MATRIX & m);	/* conversion */

    /* operators */
    double &operator[] (int n);

    /* data */
    double r, p, y;
};

/* PM_POSE */

struct PM_POSE {
    /* ctors/dtors */
    PM_POSE() {
    };
    PM_POSE(const PM_CARTESIAN& v, const PM_QUATERNION& q);
    PM_POSE(double x, double y, double z,
	double s, double sx, double sy, double sz);
    PM_POSE(const PM_HOMOGENEOUS & h);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    PM_CARTESIAN tran;
    PM_QUATERNION rot;
};

/* PM_HOMOGENEOUS */

struct PM_HOMOGENEOUS {
    /* ctors/dtors */
    PM_HOMOGENEOUS() {
    };
    PM_HOMOGENEOUS(const PM_CARTESIAN& v, const PM_ROTATION_MATRIX& m);
    PM_HOMOGENEOUS(const PM_POSE & p);	/* conversion */

    /* operators */
    PM_CARTESIAN & operator[](int n);	/* column vector */

    /* data ( [ 0 0 0 1 ] element is manually returned by [] if needed ) */
    PM_CARTESIAN tran;
    PM_ROTATION_MATRIX rot;
};

/* PM_LINE */

struct PM_LINE {
    /* ctors/dtors */
    PM_LINE() {
    };

    /* functions */
    int init(const PM_POSE& start, const PM_POSE& end);
    int point(double len, PM_POSE * point);

    /* data */
    PM_POSE start;		/* where motion was started */
    PM_POSE end;		/* where motion is going */
    PM_CARTESIAN uVec;		/* unit vector from start to end */
};

/* PM_CIRCLE */

struct PM_CIRCLE {
    /* ctors/dtors */
    PM_CIRCLE()
      : radius(0.0),
        angle(0.0),
        spiral(0.0)
    {};

    /* functions */
    int init(const PM_POSE& start, const PM_POSE& end,
	const PM_CARTESIAN& center, const PM_CARTESIAN& normal, int turn);
    int point(double angle, PM_POSE * point);

    /* data */
    PM_CARTESIAN center;
    PM_CARTESIAN normal;
    PM_CARTESIAN rTan;
    PM_CARTESIAN rPerp;
    PM_CARTESIAN rHelix;
    double radius;
    double angle;
    double spiral;
};

/* overloaded external functions */

/* dot */
extern double dot(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* cross */
extern PM_CARTESIAN cross(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* unit */
extern PM_CARTESIAN unit(const PM_CARTESIAN &v);
extern PM_QUATERNION unit(const PM_QUATERNION &q);
extern PM_ROTATION_VECTOR unit(const PM_ROTATION_VECTOR &r);
extern PM_ROTATION_MATRIX unit(const PM_ROTATION_MATRIX &m);

/* isNorm */
extern int isNorm(const PM_CARTESIAN &v);
extern int isNorm(const PM_QUATERNION &q);
extern int isNorm(const PM_ROTATION_VECTOR &r);
extern int isNorm(const PM_ROTATION_MATRIX &m);

/* mag */
extern double mag(const PM_CARTESIAN &v);

/* disp */
extern double disp(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* inv */
extern PM_CARTESIAN inv(const PM_CARTESIAN &v);
extern PM_ROTATION_MATRIX inv(const PM_ROTATION_MATRIX &m);
extern PM_QUATERNION inv(const PM_QUATERNION &q);
extern PM_POSE inv(const PM_POSE &p);
extern PM_HOMOGENEOUS inv(const PM_HOMOGENEOUS &h);

/* project */
extern PM_CARTESIAN proj(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* overloaded arithmetic functions */

/* unary +, - for translation, rotation, pose */
extern PM_CARTESIAN operator + (const PM_CARTESIAN &v);
extern PM_CARTESIAN operator - (const PM_CARTESIAN &v);
extern PM_QUATERNION operator + (const PM_QUATERNION &q);
extern PM_QUATERNION operator - (const PM_QUATERNION &q);
extern PM_POSE operator + (const PM_POSE &p);
extern PM_POSE operator - (const PM_POSE &p);

/* compare operators */
extern int operator == (const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);
extern int operator == (const PM_QUATERNION &q1, const PM_QUATERNION &q2);
extern int operator == (const PM_POSE &p1, const PM_POSE &p2);
extern int operator != (const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);
extern int operator != (const PM_QUATERNION &q1, const PM_QUATERNION &q2);
extern int operator != (const PM_POSE &p1, const PM_POSE &p2);

/* translation +, -, scalar *, - */

/* v + v */
extern PM_CARTESIAN operator + (PM_CARTESIAN v1, const PM_CARTESIAN &v2);
/* v - v */
extern PM_CARTESIAN operator - (PM_CARTESIAN v1, const PM_CARTESIAN &v2);
/* v * s */
extern PM_CARTESIAN operator *(PM_CARTESIAN v, double s);
/* s * v */
extern PM_CARTESIAN operator *(double s, PM_CARTESIAN v);
/* v / s */
extern PM_CARTESIAN operator / (const PM_CARTESIAN &v, double s);

/* rotation * by scalar, translation, and rotation */

/* s * q */
extern PM_QUATERNION operator *(double s, const PM_QUATERNION &q);
/* q * s */
extern PM_QUATERNION operator *(const PM_QUATERNION &q, double s);
/* q / s */
extern PM_QUATERNION operator / (const PM_QUATERNION &q, double s);
/* q * v */
extern PM_CARTESIAN operator *(const PM_QUATERNION &q, const PM_CARTESIAN &v);
/* q * q */
extern PM_QUATERNION operator *(const PM_QUATERNION &q1, const PM_QUATERNION &q2);
/* m * m */
extern PM_ROTATION_MATRIX operator *(const PM_ROTATION_MATRIX &m1,
    const PM_ROTATION_MATRIX &m2);

/* pose operators */

/* q * p */
extern PM_POSE operator *(const PM_QUATERNION &q, const PM_POSE &p);
/* p * p */
extern PM_POSE operator *(const PM_POSE &p1, const PM_POSE &p2);
/* p * v */
extern PM_CARTESIAN operator *(const PM_POSE &p, const PM_CARTESIAN &v);

/* slicky macros for item-by-item copying between C and C++ structs */

template <class A, class B>
void toCart(const A& src, B* dst) {(dst)->x = (src).x; (dst)->y = (src).y; (dst)->z = (src).z;}

template <class A, class B>
void toCyl(const A& src, B* dst) {(dst)->theta = (src).theta; (dst)->r = (src).r; (dst)->z = (src).z;}

template <class A, class B>
void toSph(const A& src, B* dst) {(dst)->theta = (src).theta; (dst)->phi = (src).phi; (dst)->r = (src).r;}

template <class A, class B>
void toQuat(const A& src, B* dst) {(dst)->s = (src).s; (dst)->x = (src).x; (dst)->y = (src).y; (dst)->z = (src).z;}

template <class A, class B>
void toRot(const A& src, B* dst) {(dst)->s = (src).s; (dst)->x = (src).x; (dst)->y = (src).y; (dst)->z = (src).z;}

template <class A, class B>
void toMat(const A& src, B* dst) {toCart((src).x, &((dst)->x)); toCart((src).y, &((dst)->y)); toCart((src).z, &((dst)->z));}

template <class A, class B>
void toEulerZyz(const A& src, B* dst) {(dst)->z = (src).z; (dst)->y = (src).y; (dst)->zp = (src).zp;}

template <class A, class B>
void toEulerZyx(const A& src, B* dst) {(dst)->z = (src).z; (dst)->y = (src).y; (dst)->x = (src).x;}

template <class A, class B>
void toRpy(const A& src, B* dst) {(dst)->r = (src).r; (dst)->p = (src).p; (dst)->y = (src).y;}

template <class A, class B>
void toPose(const A& src, B* dst) {toCart((src).tran, &((dst)->tran)); toQuat((src).rot, &((dst)->rot));}

template <class A, class B>
void toHom(const A& src, B* dst) {toCart((src).tran, &((dst)->tran)); toMat((src).rot, &((dst)->rot));}

template <class A, class B>
void toLine(const A& src, B* dst) {toPose((src).start, &((dst)->start)); toPose((src).end, &((dst)->end)); toCart((src).uVec, &((dst)->uVec));}

template <class A, class B>
void toCircle(const A& src, B* dst) {toCart((src).center, &((dst)->center)); toCart((src).normal, &((dst)->normal)); toCart((src).rTan, &((dst)->rTan)); toCart((src).rPerp, &((dst)->rPerp)); toCart((src).rHelix, &((dst)->rHelix)); (dst)->radius = (src).radius; (dst)->angle = (src).angle; (dst)->spiral = (src).spiral;}

#endif				/* #ifndef __LINUXCNC_POSEMATH_HH */
