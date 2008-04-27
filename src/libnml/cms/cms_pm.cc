/********************************************************************
* Description: cms_pm.cc
*   Provides CMS update functions for the POSEMATH classes.*
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

#include "cms.hh"		// class CMS
#include "posemath.h"		// POSEMATH classes
// translation types
CMS_STATUS CMS::update(PM_CARTESIAN & Cart)
{
    update(Cart.x);
    update(Cart.y);
    update(Cart.z);
    return (status);
}

CMS_STATUS CMS::update(PM_CARTESIAN * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_SPHERICAL & Sph)
{
    update(Sph.theta);
    update(Sph.phi);
    update(Sph.r);
    return (status);
}

CMS_STATUS CMS::update(PM_SPHERICAL * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_CYLINDRICAL & Cyl)
{
    update(Cyl.theta);
    update(Cyl.r);
    update(Cyl.z);
    return (status);
}

CMS_STATUS CMS::update(PM_CYLINDRICAL * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

  // rotation types
CMS_STATUS CMS::update(PM_ROTATION_VECTOR & Rot)
{
    update(Rot.s);
    update(Rot.x);
    update(Rot.y);
    update(Rot.z);
    return (status);
}

CMS_STATUS CMS::update(PM_ROTATION_VECTOR * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_ROTATION_MATRIX & Mat)
{
    update(Mat.x);
    update(Mat.y);
    update(Mat.z);
    return (status);
}

CMS_STATUS CMS::update(PM_ROTATION_MATRIX * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_QUATERNION & Quat)
{
    update(Quat.s);
    update(Quat.x);
    update(Quat.y);
    update(Quat.z);
    return (status);
}

CMS_STATUS CMS::update(PM_QUATERNION * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_EULER_ZYZ & Zyz)
{
    update(Zyz.z);
    update(Zyz.y);
    update(Zyz.zp);
    return (status);
}

CMS_STATUS CMS::update(PM_EULER_ZYZ * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_EULER_ZYX & Zyx)
{
    update(Zyx.z);
    update(Zyx.y);
    update(Zyx.x);
    return (status);
}

CMS_STATUS CMS::update(PM_EULER_ZYX * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_RPY & Rpy)
{
    update(Rpy.r);
    update(Rpy.p);
    update(Rpy.y);
    return (status);
}

CMS_STATUS CMS::update(PM_RPY * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

  // pose types
CMS_STATUS CMS::update(PM_POSE & Pose)
{
    update(Pose.tran);
    update(Pose.rot);
    return (status);
}

CMS_STATUS CMS::update(PM_POSE * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}

CMS_STATUS CMS::update(PM_HOMOGENEOUS & Hom)
{
    update(Hom.tran);
    update(Hom.rot);
    return (status);
}

CMS_STATUS CMS::update(PM_HOMOGENEOUS * x, int n)
{
    int i;
    for (i = 0; i < n; i++) {
	update(x[i]);
    }
    return (status);
}
