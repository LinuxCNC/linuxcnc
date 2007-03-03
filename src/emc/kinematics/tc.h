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
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef TC_H
#define TC_H

#include "posemath.h"
#include "emcpos.h"

/* values for endFlag */
#define TC_TERM_COND_STOP 1
#define TC_TERM_COND_BLEND 2

#define TC_LINEAR 1
#define TC_CIRCULAR 2
#define TC_RIGIDTAP 3

/* structure for individual trajectory elements */

typedef struct {
    PmLine xyz;
    PmLine abc;
} PmLine6;

typedef struct {
    PmCircle xyz;
    PmLine abc;
} PmCircle6;

typedef enum {
    TAPPING, RETRACTION, FINAL_PLACEMENT
} RIGIDTAP_STATE;

typedef struct {
    PmLine xyz;             // original, but elongated, move down
    PmLine aux_xyz;         // this will be generated on the fly, for the other
                            // two moves: retraction, final placement
    PmCartesian abc;
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
    double feed_override;   // feed override requested by user
    double maxvel;          // max possible vel (feed override stops here)
    double currentvel;      // keep track of current step (vel * cycle_time)
    
    int id;                 // segment's serial number

    union {                 // describes the segment's start and end positions
        PmLine6 line;
        PmCircle6 circle;
        PmRigidTap rigidtap;
    } coords;

    char motion_type;       // TC_LINEAR (coords.line) or 
                            // TC_CIRCULAR (coords.circle) or
                            // TC_RIGIDTAP (coords.rigidtap)
    char active;            // this motion is being executed
    int canon_motion_type;  // this motion is due to which canon function?
    int blend_with_next;    // gcode requests continuous feed at the end of 
                            // this segment (g64 mode)
    int blending;           // segment is being blended into following segment
    double blend_vel;       // velocity below which we should start blending
    double tolerance;       // during the blend at the end of this move, 
                            // stay within this distance from the path.
    int synchronized;       // spindle sync required for this move
    double uu_per_rev;      // for sync, user units per rev (e.g. 0.0625 for 16tpi)
    double vel_at_blend_start;
    unsigned char enables;  // Feed scale, etc, enable bits for this move
} TC_STRUCT;

/* TC_STRUCT functions */

extern EmcPose tcGetPos(TC_STRUCT * tc);
PmCartesian tcGetEndingUnitVector(TC_STRUCT *tc);
PmCartesian tcGetStartingUnitVector(TC_STRUCT *tc);

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
extern int tcqCreate(TC_QUEUE_STRUCT * tcq, int _size,
		     TC_STRUCT * tcSpace);

/* free up queue */
extern int tcqDelete(TC_QUEUE_STRUCT * tcq);

/* reset queue to empty */
extern int tcqInit(TC_QUEUE_STRUCT * tcq);

/* put tc on end */
extern int tcqPut(TC_QUEUE_STRUCT * tcq, TC_STRUCT tc);

/* remove n tcs from front */
extern int tcqRemove(TC_QUEUE_STRUCT * tcq, int n);

/* how many tcs on queue */
extern int tcqLen(TC_QUEUE_STRUCT * tcq);

/* look at nth item, first is 0 */
extern TC_STRUCT *tcqItem(TC_QUEUE_STRUCT * tcq, int n, long period);

/* get full status */
extern int tcqFull(TC_QUEUE_STRUCT * tcq);

#endif				/* TC_H */
