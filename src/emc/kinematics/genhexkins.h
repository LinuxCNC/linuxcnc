/*******************************************************************
* Description: genhexkins.h
*
*   Kinematics for a generalised hexapod machine
*
*   Derived from a work by R. Brian Register
*
* Adapting Author: Andrew Kyrychenko
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
********************************************************************

  This is the header file to accompany genhexkins.c.  This header file
  is used to configure genhexkins.c to solve the kinematics for a default
  Stewart Platform configuration.

  */
#ifndef GENHEXKINS_H
#define GENHEXKINS_H

#include "kinematics.h"
#include "hal.h"

#define GENHEX_MAX_JOINTS 6
#define NUM_STRUTS 6 // number of struts, only 6 supported for now

/* Default position of base strut ends in base (world) coordinates */

#define DEFAULT_BASE_0_X -22.950
#define DEFAULT_BASE_1_X  22.950
#define DEFAULT_BASE_2_X  22.950
#define DEFAULT_BASE_3_X   0.000
#define DEFAULT_BASE_4_X   0.000
#define DEFAULT_BASE_5_X -22.950

#define DEFAULT_BASE_0_Y  13.250
#define DEFAULT_BASE_1_Y  13.250
#define DEFAULT_BASE_2_Y  13.250
#define DEFAULT_BASE_3_Y -26.500
#define DEFAULT_BASE_4_Y -26.500
#define DEFAULT_BASE_5_Y  13.250

#define DEFAULT_BASE_0_Z   0.000
#define DEFAULT_BASE_1_Z   0.000
#define DEFAULT_BASE_2_Z   0.000
#define DEFAULT_BASE_3_Z   0.000
#define DEFAULT_BASE_4_Z   0.000
#define DEFAULT_BASE_5_Z   0.000

/* Default position of platform strut end in platform coordinate system */

#define DEFAULT_PLATFORM_0_X  -1.000
#define DEFAULT_PLATFORM_1_X   1.000
#define DEFAULT_PLATFORM_2_X  10.459
#define DEFAULT_PLATFORM_3_X   9.459
#define DEFAULT_PLATFORM_4_X  -9.459
#define DEFAULT_PLATFORM_5_X -10.459

#define DEFAULT_PLATFORM_0_Y  11.500
#define DEFAULT_PLATFORM_1_Y  11.500
#define DEFAULT_PLATFORM_2_Y  -4.884
#define DEFAULT_PLATFORM_3_Y  -6.616
#define DEFAULT_PLATFORM_4_Y  -6.616
#define DEFAULT_PLATFORM_5_Y  -4.884

#define DEFAULT_PLATFORM_0_Z   0.000
#define DEFAULT_PLATFORM_1_Z   0.000
#define DEFAULT_PLATFORM_2_Z   0.000
#define DEFAULT_PLATFORM_3_Z   0.000
#define DEFAULT_PLATFORM_4_Z   0.000
#define DEFAULT_PLATFORM_5_Z   0.000

/* Default unit vectors of base joints axes in world coordinates */

#define DEFAULT_BASE_0_NX  0.707107
#define DEFAULT_BASE_0_NY  0.0
#define DEFAULT_BASE_0_NZ  0.707107

#define DEFAULT_BASE_1_NX  0.0
#define DEFAULT_BASE_1_NY -0.707107
#define DEFAULT_BASE_1_NZ  0.707107

#define DEFAULT_BASE_2_NX -0.707107
#define DEFAULT_BASE_2_NY  0.0
#define DEFAULT_BASE_2_NZ  0.707107

#define DEFAULT_BASE_3_NX -0.707107
#define DEFAULT_BASE_3_NY  0.0
#define DEFAULT_BASE_3_NZ  0.707107

#define DEFAULT_BASE_4_NX  0.0
#define DEFAULT_BASE_4_NY  0.707107
#define DEFAULT_BASE_4_NZ  0.707107

#define DEFAULT_BASE_5_NX  0.707107
#define DEFAULT_BASE_5_NY  0.0
#define DEFAULT_BASE_5_NZ  0.707107

/* Default unit vectors of platform joints axes in platform coordinates */

#define DEFAULT_PLATFORM_0_NX -1.0
#define DEFAULT_PLATFORM_0_NY  0.0
#define DEFAULT_PLATFORM_0_NZ  0.0

#define DEFAULT_PLATFORM_1_NX  0.866025
#define DEFAULT_PLATFORM_1_NY  0.5
#define DEFAULT_PLATFORM_1_NZ  0.0

#define DEFAULT_PLATFORM_2_NX  0.866025
#define DEFAULT_PLATFORM_2_NY  0.5
#define DEFAULT_PLATFORM_2_NZ  0.0

#define DEFAULT_PLATFORM_3_NX  0.866025
#define DEFAULT_PLATFORM_3_NY -0.5
#define DEFAULT_PLATFORM_3_NZ  0.0

#define DEFAULT_PLATFORM_4_NX  0.866025
#define DEFAULT_PLATFORM_4_NY -0.5
#define DEFAULT_PLATFORM_4_NZ  0.0

#define DEFAULT_PLATFORM_5_NX -1.0
#define DEFAULT_PLATFORM_5_NY  0.0
#define DEFAULT_PLATFORM_5_NZ  0.0

/* Default lead of strut actuator screw */

#define DEFAULT_SCREW_LEAD 0.0

#endif
