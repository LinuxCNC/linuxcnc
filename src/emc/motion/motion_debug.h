/********************************************************************
* Description: motion_debug.h
*   A data structure used in only a few places
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved
*
* Last change:
********************************************************************/

#ifndef MOTION_DEBUG_H
#define MOTION_DEBUG_H

/*! \todo needs mot_priv.h, but including here causes conflicts */
#include "tp.h"			/* TP_STRUCT */
#include "tp_shared.h"		// tp_shared_t
#include "tc.h"			/* TC_STRUCT, TC_QUEUE_STRUCT */

/*********************************
        DEBUG STRUCTURE
*********************************/

/* This is the debug structure.  I guess it was intended to make some
   of the motion controller's internal variables visible from user
   space for debugging, but it has evolved into a monster.
   180K last time I checked - most of it (174K or so) is the traj
   planner queues... each entry is 720 bytes, and there are 210
   entries in the main queue.
   I'll figure it out eventually though.
   Low level things will be exported thru the HAL so they can be
   monitored with halscope.  High level things will remain here,
   and things that are internal will be moved to a private structure.
*/

/*! \todo FIXME - this struct is broken into two parts... at the top are
   structure members that I understand, and that are needed for emc2.
   Other structure members follow.  All the later ones need to be
   evaluated - either they move up, or they go away.
*/

/*! \todo FIXME - this has become a dumping ground for all kinds of stuff */

typedef struct emcmot_debug_t {
	unsigned char head;	/* flag count for mutex detect */

/*! \todo FIXME - all structure members beyond this point are in limbo */

	double tMin, tMax, tAvg;	/* trajectory min, max, avg times */
	double sMin, sMax, sAvg;	/* servo min, max, avg times */
	double nMin, nMax, nAvg;	/* min, max, avg times in DISABLED
					   mode */
	double yMin, yMax, yAvg;	/* min, max, avg times cycle times
					   rather than compute */
	double fMin, fMax, fAvg;	/* min, max, avg times frequency */
	double fyMin, fyMax, fyAvg;	/* min, max, avg times frequency
					   cycle times rather than compute */

	EMC_TELEOP_DATA teleop_data;
	int split;		/* number of split command reads */
	/* flag for enabling, disabling watchdog; multiple for down-stepping */
	int wdEnabling;
	int wdEnabled;
	int wdWait;
	int wdCount;
	unsigned char wdToggle;

	/* flag that all active axes are homed */
	unsigned char allHomed;

        tp_shared_t *tps;
	TP_STRUCT tp;	/* coordinated mode planner */

/* space for trajectory planner queues, plus 10 more for safety */
/*! \todo FIXME-- default is used; dynamic is not honored */
	TC_STRUCT queueTcSpace[DEFAULT_TC_QUEUE_SIZE + 10];

	TP_STRUCT altqueue;	/* coordinated mode planner */
	TC_STRUCT altqueueTcSpace[DEFAULT_ALT_TC_QUEUE_SIZE + 10];


	EmcPose oldPos;		/* last position, used for vel differencing */
	EmcPose oldVel, newVel;	/* velocities, used for acc differencing */
	EmcPose newAcc;		/* differenced acc */

	int enabling;		/* starts up disabled */
	int coordinating;	/* starts up in free mode */
	int teleoperating;	/* starts up in free mode */
#if 0
	int wasOnLimit;		/* non-zero means we already aborted
				   everything due to a soft limit, and need
				   not re-abort. It's cleared only when all
				   limits are cleared. */
	int onLimit;		/* non-zero means at least one axis is on a
				   soft limit */
#endif

	int overriding;		/* non-zero means we've initiated an joint
				   move while overriding limits */

	hal_bit_t stepping;
	int idForStep;

/* Joints moved to HAL shared memory */
#if 0 // #ifdef STRUCTS_IN_SHMEM
	emcmot_joint_t joints[EMCMOT_MAX_JOINTS];	/* joint data */
#endif

	double start_time;
	double running_time;
	double cur_time;
	double last_time;
	unsigned char tail;	/* flag count for mutex detect */
    } emcmot_debug_t;

#endif // MOTION_DEBUG_H
