/********************************************************************
* Description: posemath.cc
*    C++ definitions for pose math library data types and manipulation
*    functions.
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

#include "posemath.h"

#ifdef PM_PRINT_ERROR
#define PM_DEBUG		// need debug with printing
#endif

// place to reference arrays when bounds are exceeded
static double noElement = 0.0;
static PM_CARTESIAN *noCart = 0;


// PM_CARTESIAN class

PM_CARTESIAN::PM_CARTESIAN(double _x, double _y, double _z)
{
    x = _x;
    y = _y;
    z = _z;
}

PM_CARTESIAN::PM_CARTESIAN(PM_CONST PM_CYLINDRICAL PM_REF c)
{
    PmCylindrical cyl;
    PmCartesian cart;

    toCyl(c, &cyl);
    pmCylCartConvert(&cyl, &cart);
    toCart(cart, this);
}

PM_CARTESIAN::PM_CARTESIAN(PM_CONST PM_SPHERICAL PM_REF s)
{
    PmSpherical sph;
    PmCartesian cart;

    toSph(s, &sph);
    pmSphCartConvert(&sph, &cart);
    toCart(cart, this);
}

double &PM_CARTESIAN::operator [] (int n) {
    switch (n) {
    case 0:
	return x;
    case 1:
	return y;
    case 2:
	return z;
    default:
	return noElement;	// need to return a double &
    }
}

PM_CARTESIAN & PM_CARTESIAN::operator -= (const PM_CARTESIAN &o) {
    x-=o.x;
    y-=o.y;
    z-=o.z;
    return *this;
}
PM_CARTESIAN & PM_CARTESIAN::operator += (const PM_CARTESIAN &o) {
    x+=o.x;
    y+=o.y;
    z+=o.z;
    return *this;
}
/*
const PM_CARTESIAN PM_CARTESIAN::operator+(const PM_CARTESIAN &o) const {
    PM_CARTESIAN result = *this;
    result += o;
    return result;
}

const PM_CARTESIAN PM_CARTESIAN::operator-(const PM_CARTESIAN &o) const {
    PM_CARTESIAN result = *this;
    result -= o;
    return result;
}
*/

PM_CARTESIAN & PM_CARTESIAN::operator *= (double o)
{
    x*=o;
    y*=o;
    z*=o;
    return *this;

}

PM_CARTESIAN & PM_CARTESIAN::operator /= (double o)
{
    x/=o;
    y/=o;
    z/=o;
    return *this;
}


#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_CARTESIAN::PM_CARTESIAN(PM_CCONST PM_CARTESIAN & v)
{
    x = v.x;
    y = v.y;
    z = v.z;
}
#endif

// PM_SPHERICAL

PM_SPHERICAL::PM_SPHERICAL(double _theta, double _phi, double _r)
{
    theta = _theta;
    phi = _phi;
    r = _r;
}

PM_SPHERICAL::PM_SPHERICAL(PM_CONST PM_CARTESIAN PM_REF v)
{
    PmCartesian cart;
    PmSpherical sph;

    toCart(v, &cart);
    pmCartSphConvert(&cart, &sph);
    toSph(sph, this);
}

PM_SPHERICAL::PM_SPHERICAL(PM_CONST PM_CYLINDRICAL PM_REF c)
{
    PmCylindrical cyl;
    PmSpherical sph;

    toCyl(c, &cyl);
    pmCylSphConvert(&cyl, &sph);
    toSph(sph, this);
}

double &PM_SPHERICAL::operator [] (int n) {
    switch (n) {
    case 0:
	return theta;
    case 1:
	return phi;
    case 2:
	return r;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_SPHERICAL::PM_SPHERICAL(PM_CCONST PM_SPHERICAL & s)
{
    theta = s.theta;
    phi = s.phi;
    r = s.r;
}
#endif
// PM_CYLINDRICAL

PM_CYLINDRICAL::PM_CYLINDRICAL(double _theta, double _r, double _z)
{
    theta = _theta;
    r = _r;
    z = _z;
}

PM_CYLINDRICAL::PM_CYLINDRICAL(PM_CONST PM_CARTESIAN PM_REF v)
{
    PmCartesian cart;
    PmCylindrical cyl;

    toCart(v, &cart);
    pmCartCylConvert(&cart, &cyl);
    toCyl(cyl, this);
}

PM_CYLINDRICAL::PM_CYLINDRICAL(PM_CONST PM_SPHERICAL PM_REF s)
{
    PmSpherical sph;
    PmCylindrical cyl;

    toSph(s, &sph);
    pmSphCylConvert(&sph, &cyl);
    toCyl(cyl, this);
}

double &PM_CYLINDRICAL::operator [] (int n) {
    switch (n) {
    case 0:
	return theta;
    case 1:
	return r;
    case 2:
	return z;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_CYLINDRICAL::PM_CYLINDRICAL(PM_CCONST PM_CYLINDRICAL & c)
{
    theta = c.theta;
    r = c.r;
    z = c.z;
}
#endif
// PM_ROTATION_VECTOR

PM_ROTATION_VECTOR::PM_ROTATION_VECTOR(double _s, double _x,
    double _y, double _z)
{
    PmRotationVector rv;

    rv.s = _s;
    rv.x = _x;
    rv.y = _y;
    rv.z = _z;

    pmRotNorm(&rv, &rv);
    toRot(rv, this);
}

PM_ROTATION_VECTOR::PM_ROTATION_VECTOR(PM_CONST PM_QUATERNION PM_REF q)
{
    PmQuaternion quat;
    PmRotationVector rv;

    toQuat(q, &quat);
    pmQuatRotConvert(&quat, &rv);
    toRot(rv, this);
}

double &PM_ROTATION_VECTOR::operator [] (int n) {
    switch (n) {
    case 0:
	return s;
    case 1:
	return x;
    case 2:
	return y;
    case 3:
	return z;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_ROTATION_VECTOR::PM_ROTATION_VECTOR(PM_CCONST PM_ROTATION_VECTOR & r)
{
    s = r.s;
    x = r.x;
    y = r.y;
    z = r.z;
}
#endif
// PM_ROTATION_MATRIX class

// ctors/dtors

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(double xx, double xy, double xz,
    double yx, double yy, double yz, double zx, double zy, double zz)
{
    x.x = xx;
    x.y = xy;
    x.z = xz;

    y.x = yx;
    y.y = yy;
    y.z = yz;

    z.x = zx;
    z.y = zy;
    z.z = zz;

    /*! \todo FIXME-- need a matrix orthonormalization function pmMatNorm() */
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CARTESIAN _x, PM_CARTESIAN _y,
    PM_CARTESIAN _z)
{
    x = _x;
    y = _y;
    z = _z;
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CONST PM_ROTATION_VECTOR PM_REF v)
{
    PmRotationVector rv;
    PmRotationMatrix mat;

    toRot(v, &rv);
    pmRotMatConvert(&rv, &mat);
    toMat(mat, this);
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CONST PM_QUATERNION PM_REF q)
{
    PmQuaternion quat;
    PmRotationMatrix mat;

    toQuat(q, &quat);
    pmQuatMatConvert(&quat, &mat);
    toMat(mat, this);
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CONST PM_RPY PM_REF rpy)
{
    PmRpy _rpy;
    PmRotationMatrix mat;

    toRpy(rpy, &_rpy);
    pmRpyMatConvert(&_rpy, &mat);
    toMat(mat, this);
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CONST PM_EULER_ZYZ PM_REF zyz)
{
    PmEulerZyz _zyz;
    PmRotationMatrix mat;

    toEulerZyz(zyz, &_zyz);
    pmZyzMatConvert(&_zyz, &mat);
    toMat(mat, this);
}

PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CONST PM_EULER_ZYX PM_REF zyx)
{
    PmEulerZyx _zyx;
    PmRotationMatrix mat;

    toEulerZyx(zyx, &_zyx);
    pmZyxMatConvert(&_zyx, &mat);
    toMat(mat, this);
}

// operators

PM_CARTESIAN & PM_ROTATION_MATRIX::operator [](int n) {
    switch (n) {
    case 0:
	return x;
    case 1:
	return y;
    case 2:
	return z;
    default:
	if (0 == noCart) {
	    noCart = new PM_CARTESIAN(0.0, 0.0, 0.0);
	}
	return (*noCart);	// need to return a PM_CARTESIAN &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_ROTATION_MATRIX::PM_ROTATION_MATRIX(PM_CCONST PM_ROTATION_MATRIX & m)
{
    x = m.x;
    y = m.y;
    z = m.z;
}
#endif
// PM_QUATERNION class

PM_QUATERNION::PM_QUATERNION(double _s, double _x, double _y, double _z)
{
    PmQuaternion quat;

    quat.s = _s;
    quat.x = _x;
    quat.y = _y;
    quat.z = _z;

    pmQuatNorm(&quat, &quat);

    s = quat.s;
    x = quat.x;
    y = quat.y;
    z = quat.z;
}

PM_QUATERNION::PM_QUATERNION(PM_CONST PM_ROTATION_VECTOR PM_REF v)
{
    PmRotationVector rv;
    PmQuaternion quat;

    toRot(v, &rv);
    pmRotQuatConvert(&rv, &quat);
    toQuat(quat, this);
}

PM_QUATERNION::PM_QUATERNION(PM_CONST PM_ROTATION_MATRIX PM_REF m)
{
    PmRotationMatrix mat;
    PmQuaternion quat;

    toMat(m, &mat);
    pmMatQuatConvert(&mat, &quat);
    toQuat(quat, this);
}

PM_QUATERNION::PM_QUATERNION(PM_CONST PM_EULER_ZYZ PM_REF zyz)
{
    PmEulerZyz _zyz;
    PmQuaternion quat;

    toEulerZyz(zyz, &_zyz);
    pmZyzQuatConvert(&_zyz, &quat);
    toQuat(quat, this);
}

PM_QUATERNION::PM_QUATERNION(PM_CONST PM_EULER_ZYX PM_REF zyx)
{
    PmEulerZyx _zyx;
    PmQuaternion quat;

    toEulerZyx(zyx, &_zyx);
    pmZyxQuatConvert(&_zyx, &quat);
    toQuat(quat, this);
}

PM_QUATERNION::PM_QUATERNION(PM_CONST PM_RPY PM_REF rpy)
{
    PmRpy _rpy;
    PmQuaternion quat;

    toRpy(rpy, &_rpy);
    pmRpyQuatConvert(&_rpy, &quat);
    toQuat(quat, this);
}

PM_QUATERNION::PM_QUATERNION(PM_AXIS _axis, double _angle)
{
    PmQuaternion quat;

    pmAxisAngleQuatConvert((PmAxis) _axis, _angle, &quat);
    toQuat(quat, this);
}

void PM_QUATERNION::axisAngleMult(PM_AXIS _axis, double _angle)
{
    PmQuaternion quat;

    toQuat((*this), &quat);
    pmQuatAxisAngleMult(&quat, (PmAxis) _axis, _angle, &quat);
    toQuat(quat, this);
}

double &PM_QUATERNION::operator [] (int n) {
    switch (n) {
    case 0:
	return s;
    case 1:
	return x;
    case 2:
	return y;
    case 3:
	return z;
    default:
	return noElement;	// need to return a double &
    }
}
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_QUATERNION::PM_QUATERNION(PM_CCONST PM_QUATERNION & q)
{
    s = q.s;
    x = q.x;
    y = q.y;
    z = q.z;
}
#endif

// PM_EULER_ZYZ class

PM_EULER_ZYZ::PM_EULER_ZYZ(double _z, double _y, double _zp)
{
    z = _z;
    y = _y;
    zp = _zp;
}

PM_EULER_ZYZ::PM_EULER_ZYZ(PM_CONST PM_QUATERNION PM_REF q)
{
    PmQuaternion quat;
    PmEulerZyz zyz;

    toQuat(q, &quat);
    pmQuatZyzConvert(&quat, &zyz);
    toEulerZyz(zyz, this);
}

PM_EULER_ZYZ::PM_EULER_ZYZ(PM_CONST PM_ROTATION_MATRIX PM_REF m)
{
    PmRotationMatrix mat;
    PmEulerZyz zyz;

    toMat(m, &mat);
    pmMatZyzConvert(&mat, &zyz);
    toEulerZyz(zyz, this);
}

double &PM_EULER_ZYZ::operator [] (int n) {
    switch (n) {
    case 0:
	return z;
    case 1:
	return y;
    case 2:
	return zp;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_EULER_ZYZ::PM_EULER_ZYZ(PM_CCONST PM_EULER_ZYZ & zyz)
{
    z = zyz.z;
    y = zyz.y;
    zp = zyz.zp;
}
#endif

// PM_EULER_ZYX class

PM_EULER_ZYX::PM_EULER_ZYX(double _z, double _y, double _x)
{
    z = _z;
    y = _y;
    x = _x;
}

PM_EULER_ZYX::PM_EULER_ZYX(PM_CONST PM_QUATERNION PM_REF q)
{
    PmQuaternion quat;
    PmEulerZyx zyx;

    toQuat(q, &quat);
    pmQuatZyxConvert(&quat, &zyx);
    toEulerZyx(zyx, this);
}

PM_EULER_ZYX::PM_EULER_ZYX(PM_CONST PM_ROTATION_MATRIX PM_REF m)
{
    PmRotationMatrix mat;
    PmEulerZyx zyx;

    toMat(m, &mat);
    pmMatZyxConvert(&mat, &zyx);
    toEulerZyx(zyx, this);
}

double &PM_EULER_ZYX::operator [] (int n) {
    switch (n) {
    case 0:
	return z;
    case 1:
	return y;
    case 2:
	return x;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_EULER_ZYX::PM_EULER_ZYX(PM_CCONST PM_EULER_ZYX & zyx)
{
    z = zyx.z;
    y = zyx.y;
    x = zyx.x;
}
#endif
// PM_RPY class

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_RPY::PM_RPY(PM_CCONST PM_RPY & rpy)
{
    r = rpy.r;
    p = rpy.p;
    y = rpy.y;
}
#endif

PM_RPY::PM_RPY(double _r, double _p, double _y)
{
    r = _r;
    p = _p;
    y = _y;
}

PM_RPY::PM_RPY(PM_CONST PM_QUATERNION PM_REF q)
{
    PmQuaternion quat;
    PmRpy rpy;

    toQuat(q, &quat);
    pmQuatRpyConvert(&quat, &rpy);
    toRpy(rpy, this);
}

PM_RPY::PM_RPY(PM_CONST PM_ROTATION_MATRIX PM_REF m)
{
    PmRotationMatrix mat;
    PmRpy rpy;

    toMat(m, &mat);
    pmMatRpyConvert(&mat, &rpy);
    toRpy(rpy, this);
}

double &PM_RPY::operator [] (int n) {
    switch (n) {
    case 0:
	return r;
    case 1:
	return p;
    case 2:
	return y;
    default:
	return noElement;	// need to return a double &
    }
}

// PM_POSE class

PM_POSE::PM_POSE(PM_CARTESIAN v, PM_QUATERNION q)
{
    tran.x = v.x;
    tran.y = v.y;
    tran.z = v.z;
    rot.s = q.s;
    rot.x = q.x;
    rot.y = q.y;
    rot.z = q.z;
}

PM_POSE::PM_POSE(double x, double y, double z,
    double s, double sx, double sy, double sz)
{
    tran.x = x;
    tran.y = y;
    tran.z = z;
    rot.s = s;
    rot.x = sx;
    rot.y = sy;
    rot.z = sz;
}

PM_POSE::PM_POSE(PM_CONST PM_HOMOGENEOUS PM_REF h)
{
    PmHomogeneous hom;
    PmPose pose;

    toHom(h, &hom);
    pmHomPoseConvert(&hom, &pose);
    toPose(pose, this);
}

double &PM_POSE::operator [] (int n) {
    switch (n) {
    case 0:
	return tran.x;
    case 1:
	return tran.y;
    case 2:
	return tran.z;
    case 3:
	return rot.s;
    case 4:
	return rot.x;
    case 5:
	return rot.y;
    case 6:
	return rot.z;
    default:
	return noElement;	// need to return a double &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_POSE::PM_POSE(PM_CCONST PM_POSE & p)
{
    tran = p.tran;
    rot = p.rot;
}
#endif
// PM_HOMOGENEOUS class

PM_HOMOGENEOUS::PM_HOMOGENEOUS(PM_CARTESIAN v, PM_ROTATION_MATRIX m)
{
    tran = v;
    rot = m;
}

PM_HOMOGENEOUS::PM_HOMOGENEOUS(PM_CONST PM_POSE PM_REF p)
{
    PmPose pose;
    PmHomogeneous hom;

    toPose(p, &pose);
    pmPoseHomConvert(&pose, &hom);
    toHom(hom, this);
}

PM_CARTESIAN & PM_HOMOGENEOUS::operator [](int n) {
    // if it is a rotation vector, stuff 0 as default bottom
    // if it is a translation vector, stuff 1 as default bottom

    switch (n) {
    case 0:
	noElement = 0.0;
	return rot.x;
    case 1:
	noElement = 0.0;
	return rot.y;
    case 2:
	noElement = 0.0;
	return rot.z;
    case 3:
	noElement = 1.0;
	return tran;
    default:
	if (0 == noCart) {
	    noCart = new PM_CARTESIAN(0.0, 0.0, 0.0);
	}
	return (*noCart);	// need to return a PM_CARTESIAN &
    }
}

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_HOMOGENEOUS::PM_HOMOGENEOUS(PM_CCONST PM_HOMOGENEOUS & h)
{
    tran = h.tran;
    rot = h.rot;
}
#endif

// PM_LINE class

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_LINE::PM_LINE(PM_CCONST PM_LINE & l)
{
    start = l.start;
    end = l.end;
    uVec = l.uVec;
}
#endif

int PM_LINE::init(PM_POSE start, PM_POSE end)
{
    PmLine _line;
    PmPose _start, _end;
    int retval;

    toPose(start, &_start);
    toPose(end, &_end);

    retval = pmLineInit(&_line, &_start, &_end);

    toLine(_line, this);

    return retval;
}

int PM_LINE::point(double len, PM_POSE * point)
{
    PmLine _line;
    PmPose _point;
    int retval;

    toLine(*this, &_line);

    retval = pmLinePoint(&_line, len, &_point);

    toPose(_point, point);

    return retval;
}

// PM_CIRCLE class

#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
PM_CIRCLE::PM_CIRCLE(PM_CCONST PM_CIRCLE & c)
{
    center = c.center;
    normal = c.normal;
    rTan = c.rTan;
    rPerp = c.rPerp;
    rHelix = c.rHelix;
    radius = c.radius;
    angle = c.angle;
    spiral = c.spiral;
}
#endif

int PM_CIRCLE::init(PM_POSE start, PM_POSE end,
    PM_CARTESIAN center, PM_CARTESIAN normal, int turn)
{
    PmCircle _circle;
    PmPose _start, _end;
    PmCartesian _center, _normal;
    int retval;

    toPose(start, &_start);
    toPose(end, &_end);
    toCart(center, &_center);
    toCart(normal, &_normal);

    retval = pmCircleInit(&_circle, &_start.tran, &_end.tran, &_center, &_normal, turn);

    toCircle(_circle, this);

    return retval;
}

int PM_CIRCLE::point(double angle, PM_POSE * point)
{
    PmCircle _circle;
    PmPose _point;
    int retval;

    toCircle(*this, &_circle);

    retval = pmCirclePoint(&_circle, angle, &_point.tran);

    toPose(_point, point);

    return retval;
}

// overloaded external functions

// dot

double dot(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2)
{
    double d;
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    pmCartCartDot(&_v1, &_v2, &d);

    return d;
}

// cross

PM_CARTESIAN cross(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2)
{
    PM_CARTESIAN ret;
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    pmCartCartCross(&_v1, &_v2, &_v1);

    toCart(_v1, &ret);

    return ret;
}

//unit

PM_CARTESIAN unit(const PM_CARTESIAN &v)
{
    PM_CARTESIAN vout;
    PmCartesian _v;

    toCart(v, &_v);

    pmCartUnitEq(&_v);

    toCart(_v, &vout);

    return vout;
}

/*! \todo Another #if 0 */
#if 0

PM_CARTESIAN norm(PM_CARTESIAN v)
{
    PM_CARTESIAN vout;
    PmCartesian _v;

    toCart(v, &_v);

    pmCartNorm(&_v, &_v);

    toCart(_v, &vout);

    return vout;
}

PM_QUATERNION norm(PM_QUATERNION q)
{
    PM_QUATERNION qout;
    PmQuaternion _q;

    toQuat(q, &_q);
    pmQuatNorm(_q, &_q);

    toQuat(_q, &qout);

    return qout;
}

PM_ROTATION_VECTOR norm(PM_ROTATION_VECTOR r)
{
    PM_ROTATION_VECTOR rout;
    PmRotationVector _r;

    toRot(r, &_r);

    pmRotNorm(_r, &_r);

    toRot(_r, &rout);

    return rout;
}

PM_ROTATION_MATRIX norm(PM_ROTATION_MATRIX m)
{
    PM_ROTATION_MATRIX mout;
    PmRotationMatrix _m;

    toMat(m, &_m);

    pmMatNorm(_m, &_m);

    toMat(_m, &mout);

    return mout;
}
#endif

// isNorm

int isNorm(PM_CARTESIAN v)
{
    PmCartesian _v;

    toCart(v, &_v);

    return pmCartIsNorm(&_v);
}

int isNorm(PM_QUATERNION q)
{
    PmQuaternion _q;

    toQuat(q, &_q);

    return pmQuatIsNorm(&_q);
}

int isNorm(PM_ROTATION_VECTOR r)
{
    PmRotationVector _r;

    toRot(r, &_r);

    return pmRotIsNorm(&_r);
}

int isNorm(PM_ROTATION_MATRIX m)
{
    PmRotationMatrix _m;

    toMat(m, &_m);

    return pmMatIsNorm(&_m);
}

// mag

double mag(const PM_CARTESIAN &v)
{
    double ret;
    PmCartesian _v;

    toCart(v, &_v);

    pmCartMag(&_v, &ret);

    return ret;
}

// disp

double disp(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2)
{
    double ret;
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    pmCartCartDisp(&_v1, &_v2, &ret);

    return ret;
}

// inv

PM_CARTESIAN inv(const PM_CARTESIAN &v)
{
    PM_CARTESIAN ret;
    PmCartesian _v;

    toCart(v, &_v);

    pmCartInv(&_v, &_v);

    toCart(_v, &ret);

    return ret;
}

PM_ROTATION_MATRIX inv(const PM_ROTATION_MATRIX &m)
{
    PM_ROTATION_MATRIX ret;
    PmRotationMatrix _m;

    toMat(m, &_m);

    pmMatInv(&_m, &_m);

    toMat(_m, &ret);

    return ret;
}

PM_QUATERNION inv(const PM_QUATERNION &q)
{
    PM_QUATERNION ret;
    PmQuaternion _q;

    toQuat(q, &_q);

    pmQuatInv(&_q, &_q);

    toQuat(_q, &ret);

    return ret;
}

PM_POSE inv(const PM_POSE &p)
{
    PM_POSE ret;
    PmPose _p;

    toPose(p, &_p);

    pmPoseInv(&_p, &_p);

    toPose(_p, &ret);

    return ret;
}

PM_HOMOGENEOUS inv(const PM_HOMOGENEOUS &h)
{
    PM_HOMOGENEOUS ret;
    PmHomogeneous _h;

    toHom(h, &_h);

    pmHomInv(&_h, &_h);

    toHom(_h, &ret);

    return ret;
}

// project

PM_CARTESIAN proj(const PM_CARTESIAN &v1, PM_CARTESIAN &v2)
{
    PM_CARTESIAN ret;
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    pmCartCartProj(&_v1, &_v2, &_v1);

    toCart(_v1, &ret);

    return ret;
}

// overloaded arithmetic operators

PM_CARTESIAN operator +(const PM_CARTESIAN &v)
{
    return v;
}

PM_CARTESIAN operator -(const PM_CARTESIAN &v)
{
    PM_CARTESIAN ret;

    ret.x = -v.x;
    ret.y = -v.y;
    ret.z = -v.z;

    return ret;
}

PM_QUATERNION operator +(const PM_QUATERNION &q)
{
    return q;
}

PM_QUATERNION operator -(const PM_QUATERNION &q)
{
    PM_QUATERNION ret;
    PmQuaternion _q;

    toQuat(q, &_q);

    pmQuatInv(&_q, &_q);

    toQuat(_q, &ret);

    return ret;
}

PM_POSE operator +(const PM_POSE &p)
{
    return p;
}

PM_POSE operator -(const PM_POSE &p)
{
    PM_POSE ret;
    PmPose _p;

    toPose(p, &_p);

    pmPoseInv(&_p, &_p);

    toPose(_p, &ret);

    return ret;
}

int operator ==(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2)
{
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    return pmCartCartCompare(&_v1, &_v2);
}

int operator ==(const PM_QUATERNION &q1, PM_QUATERNION &q2)
{
    PmQuaternion _q1, _q2;

    toQuat(q1, &_q1);
    toQuat(q2, &_q2);

    return pmQuatQuatCompare(&_q1, &_q2);
}

int operator ==(const PM_POSE &p1, const PM_POSE &p2)
{
    PmPose _p1, _p2;

    toPose(p1, &_p1);
    toPose(p2, &_p2);

    return pmPosePoseCompare(&_p1, &_p2);
}

int operator !=(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2)
{
    PmCartesian _v1, _v2;

    toCart(v1, &_v1);
    toCart(v2, &_v2);

    return !pmCartCartCompare(&_v1, &_v2);
}

int operator !=(const PM_QUATERNION &q1, const PM_QUATERNION &q2)
{
    PmQuaternion _q1, _q2;

    toQuat(q1, &_q1);
    toQuat(q2, &_q2);

    return !pmQuatQuatCompare(&_q1, &_q2);
}

int operator !=(const PM_POSE &p1, const PM_POSE &p2)
{
    PmPose _p1, _p2;

    toPose(p1, &_p1);
    toPose(p2, &_p2);

    return !pmPosePoseCompare(&_p1, &_p2);
}

PM_CARTESIAN operator +(PM_CARTESIAN v1, const PM_CARTESIAN &v2)
{
    v1 += v2;
    return v1;
}

PM_CARTESIAN operator -(PM_CARTESIAN v1, const PM_CARTESIAN &v2)
{
    v1 -= v2;
    return v1;
}

PM_CARTESIAN operator *(PM_CARTESIAN v, double s)
{
    v *= s;
    return v;
}

PM_CARTESIAN operator *(double s, PM_CARTESIAN v)
{
    v *= s;
    return v;
}

PM_CARTESIAN operator /(const PM_CARTESIAN &v, double s)
{
    PM_CARTESIAN ret;

#ifdef PM_DEBUG
    if (s == 0.0) {
#ifdef PM_PRINT_ERROR
	pmPrintError("PM_CARTESIAN::operator / : divide by 0\n");
#endif
	pmErrno = PM_DIV_ERR;
	return ret;
    }
#endif

    ret.x = v.x / s;
    ret.y = v.y / s;
    ret.z = v.z / s;

    return ret;
}

PM_QUATERNION operator *(double s, const PM_QUATERNION &q)
{
    PM_QUATERNION qout;
    PmQuaternion _q;

    toQuat(q, &_q);

    pmQuatScalMult(&_q, s, &_q);

    toQuat(_q, &qout);

    return qout;
}

PM_QUATERNION operator *(const PM_QUATERNION &q, double s)
{
    PM_QUATERNION qout;
    PmQuaternion _q;

    toQuat(q, &_q);

    pmQuatScalMult(&_q, s, &_q);

    toQuat(_q, &qout);

    return qout;
}

PM_QUATERNION operator /(const PM_QUATERNION &q, double s)
{
    PM_QUATERNION qout;
    PmQuaternion _q;

    toQuat(q, &_q);

#ifdef PM_DEBUG
    if (s == 0.0) {
#ifdef PM_PRINT_ERROR
	pmPrintError("Divide by 0 in operator /\n");
#endif
	pmErrno = PM_NORM_ERR;

/*! \todo Another #if 0 */
#if 0
	// g++/gcc versions 2.8.x and 2.9.x
	// will complain that the call to PM_QUATERNION(PM_QUATERNION) is
	// ambigous. (2.7.x and some others allow it)
	return qout =
	    PM_QUATERNION((double) 0, (double) 0, (double) 0, (double) 0);
#else

	PmQuaternion quat;

	quat.s = 0;
	quat.x = 0;
	quat.y = 0;
	quat.z = 0;

	pmQuatNorm(&quat, &quat);

	qout.s = quat.s;
	qout.x = quat.x;
	qout.y = quat.y;
	qout.z = quat.z;
	return qout;
#endif

    }
#endif

    pmQuatScalMult(&_q, 1.0 / s, &_q);
    toQuat(_q, &qout);

    pmErrno = 0;
    return qout;
}

PM_CARTESIAN operator *(const PM_QUATERNION &q, const PM_CARTESIAN &v)
{
    PM_CARTESIAN vout;
    PmQuaternion _q;
    PmCartesian _v;

    toQuat(q, &_q);
    toCart(v, &_v);

    pmQuatCartMult(&_q, &_v, &_v);

    toCart(_v, &vout);

    return vout;
}

PM_QUATERNION operator *(const PM_QUATERNION &q1, const PM_QUATERNION &q2)
{
    PM_QUATERNION ret;
    PmQuaternion _q1, _q2;

    toQuat(q1, &_q1);
    toQuat(q2, &_q2);

    pmQuatQuatMult(&_q1, &_q2, &_q1);

    toQuat(_q1, &ret);

    return ret;
}

PM_ROTATION_MATRIX operator *(const PM_ROTATION_MATRIX &m1, const PM_ROTATION_MATRIX &m2)
{
    PM_ROTATION_MATRIX ret;
    PmRotationMatrix _m1, _m2;

    toMat(m1, &_m1);
    toMat(m2, &_m2);

    pmMatMatMult(&_m1, &_m2, &_m1);

    toMat(_m1, &ret);

    return ret;
}

PM_POSE operator *(const PM_POSE &p1, const PM_POSE &p2)
{
    PM_POSE ret;
    PmPose _p1, _p2;

    toPose(p1, &_p1);
    toPose(p2, &_p2);

    pmPosePoseMult(&_p1, &_p2, &_p1);

    toPose(_p1, &ret);

    return ret;
}

PM_CARTESIAN operator *(const PM_POSE &p, const PM_CARTESIAN &v)
{
    PM_CARTESIAN ret;
    PmPose _p;
    PmCartesian _v;

    toPose(p, &_p);
    toCart(v, &_v);

    pmPoseCartMult(&_p, &_v, &_v);

    toCart(_v, &ret);

    return ret;
}
