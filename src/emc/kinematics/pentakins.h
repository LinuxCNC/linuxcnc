/*******************************************************************
* Description: pentakins.h
*
*   Kinematics for a pentapod machine
*
*   Derived from genhexkins
*
* Author: Andrew Kyrychenko
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2016 All rights reserved.
********************************************************************

  This is the header file containing joints coordinates.

  */

#define NUM_STRUTS 5 // number of struts

/* Default position of base strut ends in base (world) coordinates */

#define DEFAULT_BASE_0_X -418.03
#define DEFAULT_BASE_1_X  417.96
#define DEFAULT_BASE_2_X -418.03
#define DEFAULT_BASE_3_X  417.96
#define DEFAULT_BASE_4_X   -0.06

#define DEFAULT_BASE_0_Y  324.56
#define DEFAULT_BASE_1_Y  324.56
#define DEFAULT_BASE_2_Y -325.44
#define DEFAULT_BASE_3_Y -325.44
#define DEFAULT_BASE_4_Y -492.96

#define DEFAULT_BASE_0_Z  895.56
#define DEFAULT_BASE_1_Z  895.56
#define DEFAULT_BASE_2_Z  895.56
#define DEFAULT_BASE_3_Z  895.56
#define DEFAULT_BASE_4_Z  895.56

/* Default radius of effector strut end in platform coordinate system */

#define DEFAULT_EFFECTOR_0_R 80.32
#define DEFAULT_EFFECTOR_1_R 80.32
#define DEFAULT_EFFECTOR_2_R 80.32
#define DEFAULT_EFFECTOR_3_R 80.32
#define DEFAULT_EFFECTOR_4_R 80.32

/* Default position of effector joints along the spindle axis in
   effector coordinate system */

#define DEFAULT_EFFECTOR_0_Z -185.50
#define DEFAULT_EFFECTOR_1_Z -159.50
#define DEFAULT_EFFECTOR_2_Z  -67.50
#define DEFAULT_EFFECTOR_3_Z  -41.50
#define DEFAULT_EFFECTOR_4_Z  -14.00
