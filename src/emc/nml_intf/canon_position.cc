/********************************************************************
 * Description: canon_position.cc
 *
 *   Derived from a work by Thomas Kramer, implementing a simple 9D position
 *   structure.
 *
 * Author: Robert Ellenberg
 * License: GPL Version 2+
 * System: Linux
 *
 * Copyright (c) 2014 All rights reserved.
 *
 ********************************************************************/

#include "canon_position.hh"
#include "rtapi_math.h"
#include "posemath.h"

static double noElement = 0.0;

CANON_POSITION::CANON_POSITION(double _x, double _y, double _z,
        double _a, double _b, double _c,
        double _u, double _v, double _w) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
    this->a = _a;
    this->b = _b;
    this->c = _c;
    this->u = _u;
    this->v = _v;
    this->w = _w;
}

CANON_POSITION::CANON_POSITION(const EmcPose &_pos) {
    this->x = _pos.tran.x;
    this->y = _pos.tran.y;
    this->z = _pos.tran.z;
    this->a = _pos.a;
    this->b = _pos.b;
    this->c = _pos.c;
    this->u = _pos.u;
    this->v = _pos.v;
    this->w = _pos.w;
}

CANON_POSITION::CANON_POSITION(PM_CARTESIAN const &xyz) :
    a(0.0),
    b(0.0),
    c(0.0),
    u(0.0),
    v(0.0),
    w(0.0)
{
    this->x = xyz.x;
    this->y = xyz.y;
    this->z = xyz.z;
}

CANON_POSITION::CANON_POSITION(PM_CARTESIAN const &xyz, PM_CARTESIAN const &abc) :
    u(0.0),
    v(0.0),
    w(0.0)
{
    this->x = xyz.x;
    this->y = xyz.y;
    this->z = xyz.z;
    this->a = abc.x;
    this->b = abc.y;
    this->c = abc.z;
}

bool CANON_POSITION::operator==(const CANON_POSITION &o) const {
    return(this->x == o.x &&
            this->y == o.y &&
            this->z == o.z &&
            this->a == o.a &&
            this->b == o.b &&
            this->c == o.c &&
            this->u == o.u &&
            this->v == o.v &&
            this->w == o.w);
}
bool CANON_POSITION::operator!=(const CANON_POSITION &o) const {
    return(this->x != o.x ||
            this->y != o.y ||
            this->z != o.z ||
            this->a != o.a ||
            this->b != o.b ||
            this->c != o.c ||
            this->u != o.u ||
            this->v != o.v ||
            this->w != o.w);
}
CANON_POSITION & CANON_POSITION::operator+=(const CANON_POSITION &o) {
    this->x += o.x;
    this->y += o.y;
    this->z += o.z;
    this->a += o.a;
    this->b += o.b;
    this->c += o.c;
    this->u += o.u;
    this->v += o.v;
    this->w += o.w;
    return *this;
}
CANON_POSITION & CANON_POSITION::operator+=(const EmcPose &o) {
    this->x += o.tran.x;
    this->y += o.tran.y;
    this->z += o.tran.z;
    this->a += o.a;
    this->b += o.b;
    this->c += o.c;
    this->u += o.u;
    this->v += o.v;
    this->w += o.w;
    return *this;
}

const CANON_POSITION CANON_POSITION::operator+(const CANON_POSITION &o) const {
    CANON_POSITION result = *this;
    result += o;
    return result;
}

const CANON_POSITION CANON_POSITION::operator+(const EmcPose &o) const {
    CANON_POSITION result = *this;
    result += o;
    return result;
}
CANON_POSITION & CANON_POSITION::operator-=(const CANON_POSITION &o) {
    this->x -= o.x;
    this->y -= o.y;
    this->z -= o.z;
    this->a -= o.a;
    this->b -= o.b;
    this->c -= o.c;
    this->u -= o.u;
    this->v -= o.v;
    this->w -= o.w;
    return *this;
}

CANON_POSITION & CANON_POSITION::operator-=(const EmcPose &o) {
    this->x -= o.tran.x;
    this->y -= o.tran.y;
    this->z -= o.tran.z;
    this->a -= o.a;
    this->b -= o.b;
    this->c -= o.c;
    this->u -= o.u;
    this->v -= o.v;
    this->w -= o.w;
    return *this;
}

const CANON_POSITION CANON_POSITION::operator-(const CANON_POSITION &o) const {
    CANON_POSITION result = *this;
    result -= o;
    return result;
}
const CANON_POSITION CANON_POSITION::operator-(const EmcPose &o) const {
    CANON_POSITION result = *this;
    result -= o;
    return result;
}

const double CANON_POSITION::max() const{
    double res = x;
    res = rtapi_fmax(res, this->y);
    res = rtapi_fmax(res, this->z);
    res = rtapi_fmax(res, this->a);
    res = rtapi_fmax(res, this->b);
    res = rtapi_fmax(res, this->c);
    res = rtapi_fmax(res, this->u);
    res = rtapi_fmax(res, this->v);
    res = rtapi_fmax(res, this->w);
    return res;
}

const CANON_POSITION CANON_POSITION::abs() const{
    CANON_POSITION result;
    result.x = rtapi_fabs(this->x);
    result.y = rtapi_fabs(this->y);
    result.z = rtapi_fabs(this->z);
    result.a = rtapi_fabs(this->a);
    result.b = rtapi_fabs(this->b);
    result.c = rtapi_fabs(this->c);
    result.u = rtapi_fabs(this->u);
    result.v = rtapi_fabs(this->v);
    result.w = rtapi_fabs(this->w);
    return result;
}

const CANON_POSITION CANON_POSITION::absdiff(const CANON_POSITION &o) const {
    CANON_POSITION result = *this;
    result -= o;
    return result.abs();
}

const EmcPose CANON_POSITION::toEmcPose() const {
    EmcPose out;
    out.tran.x = this->x;
    out.tran.y = this->y;
    out.tran.z = this->z;
    out.a = this->a;
    out.b = this->b;
    out.c = this->c;
    out.u = this->u;
    out.v = this->v;
    out.w = this->w;
    return out;
}

const PM_CARTESIAN CANON_POSITION::xyz() const {
    return PM_CARTESIAN(this->x,
            this->y,
            this->z);
}

void CANON_POSITION::set_xyz(const PM_CARTESIAN & xyz)
{
    this->x = xyz.x;
    this->y = xyz.y;
    this->z = xyz.z;
}


const PM_CARTESIAN CANON_POSITION::abc() const {
    return PM_CARTESIAN(this->a,
            this->b,
            this->c);
}

const PM_CARTESIAN CANON_POSITION::uvw() const {
    return PM_CARTESIAN(this->u,
            this->v,
            this->w);
}

void CANON_POSITION::print() const {
    printf("x %g y %g z %g a %g b %g c %g u %g v %g w %g ",
            this->x,
            this->y,
            this->z,
            this->a,
            this->b,
            this->c,
            this->u,
            this->v,
            this->w);
}

double &CANON_POSITION::operator[](int ind){
    switch (ind) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
            return z;
        case 3:
            return a;
        case 4:
            return b;
        case 5:
            return c;
        case 6:
            return u;
        case 7:
            return v;
        case 8:
            return w;
        default:
            printf("noelement!\n");
            return noElement;
    }
}
