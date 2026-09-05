/********************************************************************
* Description: emcpose.h
*
*   The EmcPose type and the operations on it.  EmcPose is a pose in the
*   nine coordinates a machine can have, built out of the pose math types,
*   and is not an NML message, which is where it used to live.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef __LINUXCNC_EMCPOSE_H
#define __LINUXCNC_EMCPOSE_H

#include "posemath.h"		/* PmCartesian */

typedef struct EmcPose {
    PmCartesian tran;
    double a, b, c;
    double u, v, w;
} EmcPose;

#define ZERO_EMC_POSE(pos) do { \
(pos).tran.x = 0.0;             \
(pos).tran.y = 0.0;             \
(pos).tran.z = 0.0;             \
(pos).a = 0.0;                  \
(pos).b = 0.0;                  \
(pos).c = 0.0;                  \
(pos).u = 0.0;                  \
(pos).v = 0.0;                  \
(pos).w = 0.0; } while(0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EMCPOSE_ERR_OK = 0,
    EMCPOSE_ERR_FAIL = -1,
    EMCPOSE_ERR_INPUT_MISSING = -2,
    EMCPOSE_ERR_OUTPUT_MISSING = -3,
    EMCPOSE_ERR_ALL
} EmcPoseErr;

void emcPoseZero(EmcPose * const pos);

int emcPoseAdd(EmcPose const * const p1, EmcPose const * const p2, EmcPose * const out);
int emcPoseSub(EmcPose const * const p1, EmcPose const * const p2, EmcPose * const out);

int emcPoseToPmCartesian(EmcPose const * const pose,
        PmCartesian * const xyz, PmCartesian * const abc, PmCartesian * const uvw);
int pmCartesianToEmcPose(PmCartesian const * const xyz,
        PmCartesian const * const abc, PmCartesian const * const uvw, EmcPose * const pose);

int emcPoseSelfAdd(EmcPose * const self, EmcPose const * const p2);
int emcPoseSelfSub(EmcPose * const self, EmcPose const * const p2);

int emcPoseSetXYZ(PmCartesian const * const xyz, EmcPose * const pose);
int emcPoseSetABC(PmCartesian const * const abc, EmcPose * const pose);
int emcPoseSetUVW(PmCartesian const * const uvw, EmcPose * const pose);

int emcPoseGetXYZ(EmcPose const * const pose, PmCartesian * const xyz);
int emcPoseGetABC(EmcPose const * const pose, PmCartesian * const abc);
int emcPoseGetUVW(EmcPose const * const pose, PmCartesian * const uvw);

int emcPoseMagnitude(EmcPose const * const pose, double * const out);

int emcPoseValid(EmcPose const * const pose);

#ifdef __cplusplus
}				/* matches extern "C" for C++ */
#endif

#endif
