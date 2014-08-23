/********************************************************************
* Description: emcpos.h
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef EMCPOS_H
#define EMCPOS_H

#include "posemath.h"		/* PmCartesian */

typedef struct EmcPose {
    PmCartesian tran;
    double a, b, c;
    double u, v, w;
} EmcPose;

#define ZERO_EMC_POSE(pos) do { \
pos.tran.x = 0.0;               \
pos.tran.y = 0.0;               \
pos.tran.z = 0.0;               \
pos.a = 0.0;                    \
pos.b = 0.0;                    \
pos.c = 0.0;                    \
pos.u = 0.0;                    \
pos.v = 0.0;                    \
pos.w = 0.0; } while(0)

#endif
