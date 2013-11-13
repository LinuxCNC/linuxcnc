/********************************************************************
* Description: tc.h
*   Discriminate-based trajectory planning
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
#ifndef TC_H
#define TC_H

#include "posemath.h"
#include "emcpos.h"
#include "emcmotcfg.h"
#include <stdbool.h>

/* values for endFlag */
#define TC_TERM_COND_NONE 0
#define TC_TERM_COND_STOP 1
#define TC_TERM_COND_PARABOLIC 2
#define TC_TERM_COND_TANGENT 3

#define TC_LINEAR 1
#define TC_CIRCULAR 2
#define TC_RIGIDTAP 3
#define TC_SPHERICAL 4

#define TC_SYNC_NONE 0
#define TC_SYNC_VELOCITY 1
#define TC_SYNC_POSITION 2

#define TC_GET_PROGRESS 0
#define TC_GET_STARTPOINT 1
#define TC_GET_ENDPOINT 2

/* structure for individual trajectory elements */

typedef struct {
    PmCartLine xyz;
    PmCartLine abc;
    PmCartLine uvw;
} PmLine9;

typedef struct {
    PmCircle xyz;
    PmCartLine abc;
    PmCartLine uvw;
} PmCircle9;

typedef enum {
    TAPPING, REVERSING, RETRACTION, FINAL_REVERSAL, FINAL_PLACEMENT
} RIGIDTAP_STATE;

typedef unsigned long long iomask_t; // 64 bits on both x86 and x86_64

typedef struct {
    char anychanged;
    iomask_t dio_mask;
    iomask_t aio_mask;
    signed char dios[EMCMOT_MAX_DIO];
    double aios[EMCMOT_MAX_AIO];
} syncdio_t;

typedef struct {
    PmCartLine xyz;             // original, but elongated, move down
    PmCartLine aux_xyz;         // this will be generated on the fly, for the other
                            // two moves: retraction, final placement
    PmCartesian abc;
    PmCartesian uvw;
    double reversal_target;
    double spindlerevs_at_reversal;
    RIGIDTAP_STATE state;
} PmRigidTap;

typedef struct {
    double cycle_time;
    double progress;        // where are we in the segment?  0..target
    double target;          // segment length
    double reqvel;          // vel requested by F word, calc'd by task
    double maxaccel;        // accel calc'd by task
    double maxvel;          // max possible vel (feed override stops here)
    double currentvel;      // keep track of current step (vel * cycle_time)
    double accel_scale;
    double finalvel;        // velocity to aim for at end of segment
    
    int id;                 // segment's serial number

    union {                 // describes the segment's start and end positions
        PmLine9 line;
        PmCircle9 circle;
        PmRigidTap rigidtap;
    } coords;

    int motion_type;       // TC_LINEAR (coords.line) or 
                            // TC_CIRCULAR (coords.circle) or
                            // TC_RIGIDTAP (coords.rigidtap)
    int active;            // this motion is being executed
    int canon_motion_type;  // this motion is due to which canon function?
    int term_cond;    // gcode requests continuous feed at the end of 
                            // this segment (g64 mode)

    int blending;           // segment is being blended into following segment
    double blend_vel;       // velocity below which we should start blending
    double tolerance;       // during the blend at the end of this move, 
                            // stay within this distance from the path.
    int synchronized;       // spindle sync state
    double uu_per_rev;      // for sync, user units per rev (e.g. 0.0625 for 16tpi)
    double vel_at_blend_start;
    int sync_accel;         // we're accelerating up to sync with the spindle
    unsigned char enables;  // Feed scale, etc, enable bits for this move
    int atspeed;           // wait for the spindle to be at-speed before starting this move
    syncdio_t syncdio;      // synched DIO's for this move. what to turn on/off
    int indexrotary;        // which rotary axis to unlock to make this move, -1 for none
    int atpeak;             //At peak velocity during blends)
    int on_final_decel;
} TC_STRUCT;

/* TC_STRUCT functions */


extern int tcGetEndpoint(TC_STRUCT const * const tc, EmcPose * const out);
extern int tcGetStartpoint(TC_STRUCT const * const tc, EmcPose * const out);
extern int tcGetPos(TC_STRUCT const * const tc,  EmcPose * const out);
int tcGetPosReal(TC_STRUCT const * const tc, int of_endpoint,  EmcPose * const out);
int tcGetEndingUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);
int tcGetStartingUnitVector(TC_STRUCT const * const tc, PmCartesian * const out);

int pmCircleFromPoints(PmCircle * const arc, PmCartesian const * const start,
        PmCartesian const * const middle, PmCartesian const * const end,
        double radius);

/* queue of TC_STRUCT elements*/

typedef struct {
    TC_STRUCT *queue;		/* ptr to the tcs */
    int size;			/* size of queue */
    int _len;			/* number of tcs now in queue */
    int start, end;		/* indices to next to get, next to put */
    int allFull;		/* flag meaning it's actually full */
} TC_QUEUE_STRUCT;

/* TC_QUEUE_STRUCT functions */

/* create queue of _size */
extern int tcqCreate(TC_QUEUE_STRUCT * const tcq, int _size,
		     TC_STRUCT * const tcSpace);

/* free up queue */
extern int tcqDelete(TC_QUEUE_STRUCT * const tcq);

/* reset queue to empty */
extern int tcqInit(TC_QUEUE_STRUCT * const tcq);

/* put tc on end */
extern int tcqPut(TC_QUEUE_STRUCT * const tcq, TC_STRUCT const * const tc);

/* remove a single tc from the back of the queue */
extern int tcqPopBack(TC_QUEUE_STRUCT * const tcq);

/* remove n tcs from front */
extern int tcqRemove(TC_QUEUE_STRUCT * const tcq, int n);

/* how many tcs on queue */
extern int tcqLen(TC_QUEUE_STRUCT const * const tcq);

/* look at nth item, first is 0 */
extern TC_STRUCT * tcqItem(TC_QUEUE_STRUCT const * const tcq, int n);

/**
 * Get the "end" of the queue, the most recently added item.
 */
extern TC_STRUCT * tcqLast(TC_QUEUE_STRUCT const * const tcq);

/* get full status */
extern int tcqFull(TC_QUEUE_STRUCT const * const tcq);

#endif				/* TC_H */
