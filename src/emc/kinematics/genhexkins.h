/********************************************************************
* Description: genhexkins.h
*   Kinematics for a generalised hexapod machine
*
*   Derived from a work by R. Brian Register
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*******************************************************************

  This is the header file to accompany genhexkins.c.  This header file is used
  to configure genhexkins.c to solve the kinematics for a particular Stewart
  Platform configuration.

  Defined are the parameters necessary to configure the functions to solve
  several different Stewart Platform configurations.  To choose a particular
  configuration #define the configuration you are interested in and
  comment out any others.

  */

#include "posemath.h"		/* PmCartesian */

/* genhexSetParams lets you set the Cartesian coords of the base and platform,
   overriding the defaults set in the subsequent #defines */
extern int genhexSetParams(const PmCartesian base[], const PmCartesian platform[]);
extern int genhexGetParams(PmCartesian base[], PmCartesian platform[]);
extern int genhexKinematicsForwardIterations(void);

#define MINI_TETRA

#define NUM_STRUTS 6

#ifdef INGERSOLL_HEXAPOD

/* Define position of base strut ends in base (world) coordinates for */
/* Ingersoll Hexapod */
#define BASE_0_X  1.7580
#define BASE_1_X  1.6021
#define BASE_2_X -1.7580
#define BASE_3_X -1.6021
#define BASE_4_X  0.0
#define BASE_5_X  0.0

#define BASE_0_Y  2.8
#define BASE_1_Y  3.07
#define BASE_2_Y  2.8
#define BASE_3_Y  3.07
#define BASE_4_Y  2.8
#define BASE_5_Y  3.07

#define BASE_0_Z -1.015
#define BASE_1_Z -0.925
#define BASE_2_Z -1.015
#define BASE_3_Z -0.925
#define BASE_4_Z  2.03
#define BASE_5_Z  1.85

/* Define position of platform strut end in platform coordinate system */
/* for Ingersoll Hexapod  */
#define PLATFORM_0_X  0.225
#define PLATFORM_1_X  0.1125
#define PLATFORM_2_X -0.1125
#define PLATFORM_3_X -0.225
#define PLATFORM_4_X -0.1125
#define PLATFORM_5_X  0.1125

#define PLATFORM_0_Y  0.0
#define PLATFORM_1_Y  0.1949
#define PLATFORM_2_Y  0.1949
#define PLATFORM_3_Y  0.0
#define PLATFORM_4_Y -0.1949
#define PLATFORM_5_Y -0.1949

#define PLATFORM_0_Z -0.228
#define PLATFORM_1_Z -0.228
#define PLATFORM_2_Z -0.228
#define PLATFORM_3_Z -0.228
#define PLATFORM_4_Z -0.228
#define PLATFORM_5_Z -0.228

#endif

#ifdef UF_HEXAPOD

/* Define position of base strut ends in base (world) coordinates for */
/* UF Hexapod */
#define BASE_0_X -16.00
#define BASE_1_X -10.00
#define BASE_2_X  16.00
#define BASE_3_X  13.00
#define BASE_4_X   0.00
#define BASE_5_X  -3.00

#define BASE_0_Y  -9.24
#define BASE_1_Y  -9.24
#define BASE_2_Y  -9.24
#define BASE_3_Y  -4.04
#define BASE_4_Y  18.48
#define BASE_5_Y  13.28

#define BASE_0_Z  18.00
#define BASE_1_Z  18.00
#define BASE_2_Z  18.00
#define BASE_3_Z  18.00
#define BASE_4_Z  18.00
#define BASE_5_Z  18.00

/* Define position of platform strut end in platform coordinate system */
/* for UF Hexapod  */
#define PLATFORM_0_X  -9.00
#define PLATFORM_1_X   0.00
#define PLATFORM_2_X   3.00
#define PLATFORM_3_X  12.00
#define PLATFORM_4_X   6.00
#define PLATFORM_5_X -12.00

#define PLATFORM_0_Y   1.73
#define PLATFORM_1_Y -13.86
#define PLATFORM_2_Y  -8.66
#define PLATFORM_3_Y   6.93
#define PLATFORM_4_Y   6.93
#define PLATFORM_5_Y   6.93

#define PLATFORM_0_Z   0.00
#define PLATFORM_1_Z   0.00
#define PLATFORM_2_Z   0.00
#define PLATFORM_3_Z   0.00
#define PLATFORM_4_Z   0.00
#define PLATFORM_5_Z   0.00

#endif

#ifdef MINI_TETRA

/* Define position of base strut ends in base (world) coordinates for */
/* mini tetra */
#define BASE_0_X -22.950
#define BASE_0_Y 13.250

#define BASE_1_X 22.950
#define BASE_1_Y 13.250

#define BASE_2_X 22.950
#define BASE_2_Y 13.250

#define BASE_3_X 0
#define BASE_3_Y -26.5

#define BASE_4_X 0
#define BASE_4_Y -26.5

#define BASE_5_X -22.950
#define BASE_5_Y 13.250

#define BASE_0_Z  0
#define BASE_1_Z  0
#define BASE_2_Z  0
#define BASE_3_Z  0
#define BASE_4_Z  0
#define BASE_5_Z  0

/* Define position of platform strut end in platform coordinate system */
/* for mini tetra */
#define PLATFORM_0_X -1
#define PLATFORM_0_Y 11.5

#define PLATFORM_1_X 1
#define PLATFORM_1_Y 11.5

#define PLATFORM_2_X 10.459
#define PLATFORM_2_Y -4.884

#define PLATFORM_3_X 9.459
#define PLATFORM_3_Y -6.616

#define PLATFORM_4_X -9.459
#define PLATFORM_4_Y -6.616

#define PLATFORM_5_X -10.459
#define PLATFORM_5_Y -4.884

#define PLATFORM_0_Z  0
#define PLATFORM_1_Z  0
#define PLATFORM_2_Z  0
#define PLATFORM_3_Z  0
#define PLATFORM_4_Z  0
#define PLATFORM_5_Z  0

#endif

#ifdef OCTA_TETRA

/* Define position of base strut ends in base (world) coordinates for
   minitetra with octahedral outside frame. Let A be side length of
   base triangle. Here A = 39 inches. */
#define BASE_0_X -19.5          /* -A/2 */
#define BASE_0_Y 11.258         /* A cos 30 * 1/3 */

#define BASE_1_X 19.5           /* A/2 */
#define BASE_1_Y 11.258         /* A cos 30 * 1/3 */

#define BASE_2_X 19.5           /* same as BASE_1_X */
#define BASE_2_Y 11.258         /* same as BASE_1_Y */

#define BASE_3_X 0              /* 0 */
#define BASE_3_Y -22.517        /* -A cos 30 * 2/3 */

#define BASE_4_X 0              /* same as BASE_3_X */
#define BASE_4_Y -22.517        /* same as BASE_3_Y */

#define BASE_5_X -19.5          /* same as BASE_0_X */
#define BASE_5_Y 11.258         /* same as BASE_0_Y */

#define BASE_0_Z  0             /* all 0 */
#define BASE_1_Z  0
#define BASE_2_Z  0
#define BASE_3_Z  0
#define BASE_4_Z  0
#define BASE_5_Z  0

/* Define position of platform strut end in platform coordinate system
   for minitetra with octahedral outside frame */
#define PLATFORM_0_X -1
#define PLATFORM_0_Y 11.5

#define PLATFORM_1_X 1
#define PLATFORM_1_Y 11.5

#define PLATFORM_2_X 10.459
#define PLATFORM_2_Y -4.884

#define PLATFORM_3_X 9.459
#define PLATFORM_3_Y -6.616

#define PLATFORM_4_X -9.459
#define PLATFORM_4_Y -6.616

#define PLATFORM_5_X -10.459
#define PLATFORM_5_Y -4.884

#define PLATFORM_0_Z  0
#define PLATFORM_1_Z  0
#define PLATFORM_2_Z  0
#define PLATFORM_3_Z  0
#define PLATFORM_4_Z  0
#define PLATFORM_5_Z  0

#endif

#ifdef GEN_TETRA

/*
  GEN_TETRA uses 4 parameters for the side and corner lengths of the
  base and platform. Set these and recompile, and you don't have to
  figure out all the vertex coordinates yourself.
*/

#define BASE_LENGTH 40.0
#define BASE_CORNER 1.75
#define PLATFORM_LENGTH 18.9
#define PLATFORM_CORNER 0.5
#define Q 0.2886751346          /* 1/3 cos 30 */

#define BASE_0_X (0.5 * BASE_CORNER)
#define BASE_0_Y (Q * (2.0 * BASE_LENGTH + BASE_CORNER))

#define BASE_1_X (0.5 * (BASE_LENGTH + BASE_CORNER))
#define BASE_1_Y (Q * (BASE_CORNER - BASE_LENGTH))

#define BASE_2_X (0.5 * BASE_LENGTH)
#define BASE_2_Y (-Q * (BASE_LENGTH + 2.0 * BASE_CORNER))

#define BASE_3_X -BASE_2_X
#define BASE_3_Y BASE_2_Y

#define BASE_4_X -BASE_1_X
#define BASE_4_Y BASE_1_Y

#define BASE_5_X -BASE_0_X
#define BASE_5_Y BASE_0_Y

#define BASE_0_Z 0              /* all 0 */
#define BASE_1_Z 0
#define BASE_2_Z 0
#define BASE_3_Z 0
#define BASE_4_Z 0
#define BASE_5_Z 0

/* Define position of platform strut end in platform coordinate system
   for minitetra with octahedral outside frame */
#define PLATFORM_0_X (0.5 * PLATFORM_LENGTH)
#define PLATFORM_0_Y (Q * (PLATFORM_LENGTH + 2.0 * PLATFORM_CORNER))

#define PLATFORM_1_X (0.5 * (PLATFORM_LENGTH + PLATFORM_CORNER))
#define PLATFORM_1_Y (Q * (PLATFORM_LENGTH - PLATFORM_CORNER))

#define PLATFORM_2_X (0.5 * PLATFORM_CORNER)
#define PLATFORM_2_Y (-Q * (2.0 * PLATFORM_LENGTH + PLATFORM_CORNER))

#define PLATFORM_3_X -PLATFORM_2_X
#define PLATFORM_3_Y PLATFORM_2_Y

#define PLATFORM_4_X -PLATFORM_1_X
#define PLATFORM_4_Y PLATFORM_1_Y

#define PLATFORM_5_X -PLATFORM_0_X
#define PLATFORM_5_Y PLATFORM_0_Y

#define PLATFORM_0_Z 0
#define PLATFORM_1_Z 0
#define PLATFORM_2_Z 0
#define PLATFORM_3_Z 0
#define PLATFORM_4_Z 0
#define PLATFORM_5_Z 0

#endif
