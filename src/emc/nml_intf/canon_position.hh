/********************************************************************
 * Description: canon_position.hh
 *
 *   CANON position class with operators and common functions
 *   Derived from a work by Thomas Kramer
 *
 * Author: Robert W. Ellenberg
 * License: GPL Version 2+
 * System: Linux
 *    
 * Copyright (c) 2014 All rights reserved.
 ********************************************************************/

#ifndef CANON_POSITION_HH
#define CANON_POSITION_HH

#include <stdio.h>		// FILE
#include <vector>

#include "emcpos.h"
#include "emctool.h"
#include "posemath.h"       // For PM_CARTESIAN type

struct CANON_POSITION {
#ifndef JAVA_DIAG_APPLET
    CANON_POSITION() :
        x(0.0),
        y(0.0),
        z(0.0),
        a(0.0),
        b(0.0),
        c(0.0),
        u(0.0),
        v(0.0),
        w(0.0) {}

    CANON_POSITION(double _x, double _y, double _z,
                   double _a, double _b, double _c,
                   double _u, double _v, double _w);
    CANON_POSITION(const EmcPose &_pos);
    CANON_POSITION(PM_CARTESIAN const &xyz);
    CANON_POSITION(PM_CARTESIAN const &xyz, PM_CARTESIAN const &abc);

    bool operator==(const CANON_POSITION &o) const;
    bool operator!=(const CANON_POSITION &o) const;
    CANON_POSITION & operator+=(const CANON_POSITION &o);
    CANON_POSITION & operator+=(const EmcPose &o);

    const CANON_POSITION operator+(const CANON_POSITION &o) const;
    const CANON_POSITION operator+(const EmcPose &o) const;
    CANON_POSITION & operator-=(const CANON_POSITION &o);
    CANON_POSITION & operator-=(const EmcPose &o);

    const CANON_POSITION operator-(const CANON_POSITION &o) const;
    const CANON_POSITION operator-(const EmcPose &o) const;

    double &operator[](const int ind);

    const CANON_POSITION abs() const;
    const CANON_POSITION absdiff(const CANON_POSITION &o) const;
    const double max() const;

    const EmcPose toEmcPose() const;

    const PM_CARTESIAN xyz() const;
    const PM_CARTESIAN abc() const;
    const PM_CARTESIAN uvw() const;

    void set_xyz(const PM_CARTESIAN & xyz);

    void print() const;
#endif

    double x, y, z, a, b, c, u, v, w;
};

#endif				/* ifndef CANON_POSITION_HH */
