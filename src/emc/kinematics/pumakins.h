/*****************************************************************
* Description: pumakins.h
*   Kinematics for a puma typed robot
*
*   Derived from a work by Fred Proctor
* 
*  rdp added PUMA560_D6 (left the old values which may be used in puma560kins?)
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
*******************************************************************
* This is the header file to accompany pumakins.c.  
*******************************************************************
*/
#ifndef PUMA_H
#define PUMA_H


/* the default values for a PUMA 560 type robot, these can be changed as HAL parameters */
#define DEFAULT_PUMA560_A2 300.0
#define DEFAULT_PUMA560_A3  50.0
#define DEFAULT_PUMA560_D3  70.0
#define DEFAULT_PUMA560_D4 400.0
#define DEFAULT_PUMA560_D6  70.0

#define SINGULAR_FUZZ 0.000001
#define FLAG_FUZZ     0.000001

/* flags for inverse kinematics */
#define PUMA_SHOULDER_RIGHT 0x01
#define PUMA_ELBOW_DOWN     0x02
#define PUMA_WRIST_FLIP     0x04
#define PUMA_SINGULAR       0x08  /* joints at a singularity */

/* flags for forward kinematics */
#define PUMA_REACH          0x01  /* pose out of reach */

#endif /* PUMA_H */
