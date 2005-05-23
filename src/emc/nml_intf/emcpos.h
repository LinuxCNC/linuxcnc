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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef EMCPOS_H
#define EMCPOS_H

#include "posemath.h"		/* PmCartesian */

typedef struct _EmcPose {
    PmCartesian tran;
    double a, b, c;
} EmcPose;

#endif
