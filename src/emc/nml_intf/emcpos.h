#ifndef EMCPOS_H
#define EMCPOS_H

#include "posemath.h"		/* PmCartesian */

typedef struct _EmcPose {
    PmCartesian tran;
    double a, b, c;
} EmcPose;

#endif
