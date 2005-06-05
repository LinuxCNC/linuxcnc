/********************************************************************
* Description: trajectory.h
*
* Author: Paul Corner
* Created on: Sat Jun  4 16:19:25 BST 2005
* Computer: Babylon.node 
* System: Linux 2.6.10-adeos on i686
*    
* Last change:
* $Revision$
* $Author$
* $Date$
*
* Copyright (c) 2005 Paul Corner  All rights reserved.
*
********************************************************************/
#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include <math.h>
#include "vectorlib.h"

/**
 * Interpolation function
 *
    \f[
        P_(v) = \sum_{k=0}^n
            P_k N_k,_t(v)
    \f]
 *
 * Where:
 *
    \f[
        N_k,_t(v) = ^{if u[k] <=v < u[k+1]} _{0\ otherwise}
    \f]
    \f[
        N_k,_t(v) = \frac{v-u[k]}{u[k+t-1]-u[k]}N_{k,t}(v)
        +\frac{u[k+t]-v}{u[k+t]-u[k+1]}N_{k+1,t}(v)
    \f]
 *
 * The quintic:
 *
    \f[ P(t)={a_0+a_1}{t^2+a_3}{t^3+a_4}{t^4+a_5}t^5 \f]
 *
 * Where
 *
    - \f$ P_0=a_0 \f$

    - \f$ P_f=a_0+a_1t_f+a_2t^2_f+a_3t^3_f +a_4t^4_f+a_5t^5_f \f$

    - \f$ V_0=a_1 \f$

    - \f$ V_f=a_1+2a_2t_f+3a_3t^2_f+4a_4t^3_f+5a_5t^4_f \f$

    - \f$ A_o=2a_2 \f$

    - \f$ A_f=2a_2+6a_3t_f+12a_4t^2_f+20a_5t^3_f \f$
*/

#define ABORT		0xF0000000
#define PAUSED		0x00000001
#define SYNCED		0x00000002
#define EXACT_STOP	0x00000010
#define MIN_BLEND	0x00000020
#define BSPLINE_POINT	0x00000040
#define KNOT_POINT	0x00000080
#define STRAIGHT_LINE	0x00000100
#define CLOCKWISE_ARC	0x00000200
#define ANTICLOCK_ARC	0x00000400
#define PLANE_G17	0x00001000
#define PLANE_G18	0x00002000
#define PLANE_G19	0x00004000

#define WRAP_A		0x00000001
#define WRAP_B		0x00000002
#define WRAP_C		0x00000004

/**
 * Struct describing the end pont of each vector.
 */
typedef struct {
    vector3 pos; /**< Coordinates of the end point */
    vector3 rot; /**< Center point for an arc */
    double vel; /**< Velocity for this segment */
    int flags; /**< @see notes */
    int id; /**< ID of this segment */
} point;

typedef struct {
    vector3 vel; /**< Maximum velocity for each axis */
    vector3 accel; /**< Maximum acceleration for each axis */
    vector3 jerk; /**< Maximum jerk for each axis */
    vector3 fuzz; /**< Fuzz factor to ignore residual values */
    double slice; /**< Granularity of the slicing algorthim */
    double dice; /**< The counterpart to slice */
    int flags; /**< @see notes */
} bounds;


extern void* traj_init(void); // inits queue - may sleep
extern int traj_free(void *queue); // nuff said.
extern int traj_reset(void *queue, bounds *limits); // flushes queue and sets bounds
extern int traj_append(void *queue, point *p); // adds p to the queue
extern vector3 traj_point(void *queue, vector3 pos); // interpolates next point from pos
extern int traj_status(void *queue, int flag); // sets/returns queue status
extern int traj_id(void *queue); // current segment ID

/* FIX-ME wrappers around new functions - Not all are implemented... */
extern int tpFull(TP_STRUCT *tp);
extern int tpAbort(TP_STRUCT *tp);
extern int tpAddCircle(TP_STRUCT *tp, EmcPose end, PmCartesian center, PmCartesian normal, int turn);
extern int tpAddLine(TP_STRUCT *tp, EmcPose end);
extern int tpClear(TP_STRUCT *tp);
extern int tpCreate(TP_STRUCT *tp, int _queueSize, TC_STRUCT *tcSpace);
extern int tpGetExecId(TP_STRUCT *tp);
extern EmcPose tpGetPos(TP_STRUCT *tp);
extern int tpInit(TP_STRUCT *tp);
extern int tpIsDone(TP_STRUCT *tp);
extern int tpPause(TP_STRUCT *tp);
extern int tpResume(TP_STRUCT *tp);
extern int tpRunCycle(TP_STRUCT *tp);
extern int tpSetAmax(TP_STRUCT *tp, double amax);
extern int tpSetAout(TP_STRUCT *tp, unsigned char index, double start, double end);
extern int tpSetCycleTime(TP_STRUCT *tp, double secs);
extern int tpSetDout(TP_STRUCT *tp, int index, unsigned char start, unsigned char end);
extern int tpSetId(TP_STRUCT *tp, int id);
extern int tpSetPos(TP_STRUCT *tp, EmcPose pos);
extern int tpSetTermCond(TP_STRUCT *tp, int cond);
extern int tpSetVlimit(TP_STRUCT *tp, double limit);
extern int tpSetVmax(TP_STRUCT *tp, double vmax);
extern int tpSetVscale(TP_STRUCT *tp, double scale);

#endif
