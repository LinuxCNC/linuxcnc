/********************************************************************
* Description: segmentqueue.h
*   Trajectory planner based on linking segments together
*
*   Derived from a work by Rogier Blom
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2010 All rights reserved.
*
********************************************************************/

#ifndef SEGMENTQUEUE_H
#define SEGMENTQUEUE_H

#include <math.h>

#include "posemath.h"
#include "emcpos.h"             /* EmcPose */

#ifndef PI
#define PI 3.141592654
#endif

#define SQ_LINEAR 1
#define SQ_CIRCULAR 2

#define SQ_DEFAULT_LENGTH 20

#define use_sq_planner (1)

/* Structure that contains all the information about a segment */
typedef struct _seg
{
    int ID;                 /* identification number */
    int type;               /* SQ_LINEAR or SQ_CIRCULAR */

    EmcPose start;
    EmcPose end;

    PmLine line;
    PmCircle circle;

    double helixRadius;     /* radius of circle with correction for displacement
                               in normal direction */
    double length;
    double totLength;       /* differs from length when segment consists of
                               multiple linked segments */
    double initInc;
    double maxInc;
    double finalInc;

    double amaxTan;         /* maximum tangential acceleration for this segment
                               the value of this can differ from the general
                               maximum acceleration for non-linear segments */

    /* the next coefficients are for internal use and are calculated at planning
       time */
    int m,p,q;              /* number steps needed for acceleration, cruising
                               and decelerating. */
    int totNumPoints;       /* total number of points on segment (=m+p+q+3) */

    double a1,b1,c1,d1;     /* coefficients for the 3rd polynomial for
                               the speed adjusting in phase 1 */
    double plInitInc;       /* planned init inc */
    double cruiseInc;       /* incrediment for phase 2 */
    double plFinalInc;      /* planned final inc */

    double a3,b3,c3,d3;     /* like the coefficients for phase 1, but then for
                               phase 3 */

    /* pointers to previous and next segments. These are in a sense redundant
       for a ring buffer structure, but it make search actions a litte easier
       to implement (and understand) */
    struct _seg *prevSegment;
    struct _seg *nextSegment;

    /* some flags */
    int planningDone;
    int active;             /* indicates if the last calculated point comes
                               from this segment */
    int linkedToPrevSeg;    /* indicates if this segment is linked to the
                               previous one */

    int numLinkedSegs;      /* this is the number of (next) segments that are
                               linked to this segment. When linkedToPrevSeg
                               equals 1, this variable is always zero */
} SEGMENT ;


/* declaration of the segment queue */
typedef struct
{

    /* Definition of the ring buffer of SEGMENTs */
    SEGMENT *queue;
    int size;               /* the total size of the queue */
    int numSegments;        /* the number of SEGMENTs currently in the queue */
    int start, end;         /* indices to the start (next to get) and the end
                               (last added) of the queue */
    int full;               /* indicates if the queue is full */

    int n;                  /* the current discrete time moment */

    /* some parameters */
    double cycleTime;
    double ctPow2, ctPow3;  /* cycleTime to the second and third order, to speed
                               up calculations */
    double maxAcc;          /* maximum acceleration (both tang and normal) */
    double maxV;            /* the absolute maximum velocity */
    double maxFeedOverrideFactor;         /* the maximum value for the feed
                                             override factor */

    /* feed rate of the next motion(s) to append */
    double feed;

    /* initial position */
    EmcPose initXYZ;

    /* the so far travelled distance along the segment */
    double dist;

    /* last calculated point */
    EmcPose lastPoint;

    /* feed overide factor, 1 is default */
    double feedOverrideFactor;

    /* some flags */
    int done; /* set (=1) when segmentqueue is empty or when done
                 with pause/abort action */
    int paused; /* set when a paused command has been given */
    int stepping; /* set when a sqStep command has been given */
    int feedAdjusting;
    int aborting;

    /* for internal use .... */
    EmcPose base;
    double offset;
    double cumLength;
    SEGMENT *cursor;

    /* ID of the segment on which the last calculated point lays */
    int currentID;

    /* ID of the last appended motion */
    int lastAppMotionID;

    double currentVel;    /* the current increment */
} SEGMENTQUEUE;


/* Interface functions */

/* function to initialize the segment queue. Needs to be called after
   creating an instantiation of the segmentqueue before it can be used */
extern int sqCreate(SEGMENTQUEUE *sq, SEGMENT *first, int size);

/* functions to set some parameters */
extern int sqSetAmax(SEGMENTQUEUE *sq, double amax);
extern int sqSetVmax(SEGMENTQUEUE *sq, double vmax);
extern int sqSetCycleTime(SEGMENTQUEUE *sq, double secs);
extern int sqSetMaxFeedOverride(SEGMENTQUEUE *sq, double mfo);

/* function to specify what the initial/current position is */
extern int sqSetPos(SEGMENTQUEUE *sq, EmcPose pos);

/* function to remove a whole segmentqueue from memory */
extern int sqTrashQueue(SEGMENTQUEUE *sq);

/* function to empty the segmentqueue */
extern int sqClear(SEGMENTQUEUE *sq);


/* Implemented commands */
/* -------------------- */

/* functiosn to add a motion to the queue */
extern int sqAddLine(SEGMENTQUEUE *sq, EmcPose end, int ID);
extern int sqAddCircle(SEGMENTQUEUE *sq, EmcPose end,
                       PmCartesian center, PmCartesian normal,
                       int turn, int ID);

/* function to set the feed rate for the motions appended after this command */
extern int sqSetFeed(SEGMENTQUEUE *sq, double feed);

/* function to change the feed override factor */
extern int sqSetFeedOverride(SEGMENTQUEUE *sq, double factor);

/* function to run a new cycle */
extern int sqRunCycle(SEGMENTQUEUE *sq, long period);

/* function to get a new position */
extern EmcPose sqGetPos(SEGMENTQUEUE *sq);

/* function to pause the system (decelerate to zero velocity) */
extern int sqPause(SEGMENTQUEUE *sq);

/* function to resume with a paused segmentqueue */
extern int sqResume(SEGMENTQUEUE *sq);

/* function to abort */
extern int sqAbort(SEGMENTQUEUE *sq);

/* function to do execute one motion from a stop and stop again */
extern int sqStep(SEGMENTQUEUE *sq);


/* functions to get some status information */
/* ---------------------------------------- */

extern int sqIsDone(SEGMENTQUEUE *sq); /* after pause/abort to see if we're done*/
extern int sqIsPaused(SEGMENTQUEUE *sq);
extern int sqIsStepping(SEGMENTQUEUE *sq);
extern double sqGetVel(SEGMENTQUEUE *sq);
extern double sqGetMaxAcc(SEGMENTQUEUE *sq);
extern int sqQueueDepth(SEGMENTQUEUE *sq);
extern int sqIsQueueFull(SEGMENTQUEUE *sq);
extern int sqGetExecId(SEGMENTQUEUE *sq); /* function the get the ID of the
                                       active (sub)segment */
#endif
