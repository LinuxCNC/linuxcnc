// signatures of the - formerly exposed extern - methods of the tp
// these are used internally only now, and to populate the tp vtable
// in tpmain.c

#ifndef TP_PRIVATE_H
#define TP_PRIVATE_H

#include "posemath.h"
#include "tc_types.h"
#include "tp_types.h"
#include "tcq.h"

int tpCreate(TP_STRUCT * tp, int _queueSize, TC_STRUCT * tcSpace,  tp_shared_t *shared);

int tpClear(TP_STRUCT * tp);

int tpInit(TP_STRUCT * tp);

int tpClearDIOs(TP_STRUCT * const tp);

int tpSetCycleTime(TP_STRUCT * tp, double secs);

int tpSetVmax(TP_STRUCT * tp, double vmax, double ini_maxvel);

int tpSetVlimit(TP_STRUCT * tp, double limit);

int tpSetAmax(TP_STRUCT * tp, double amax);

int tpSetId(TP_STRUCT * tp, int id);

int tpGetExecId(TP_STRUCT * tp);

struct state_tag_t tpGetExecTag(TP_STRUCT * const tp);

int tpSetTermCond(TP_STRUCT * tp, int cond, double tolerance);

int tpSetPos(TP_STRUCT * tp, EmcPose const * const pos);

int tpAddCurrentPos(TP_STRUCT * const tp, EmcPose const * const disp);

int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos);

int tpAddRigidTap(TP_STRUCT * tp, EmcPose end, double vel, double
		  ini_maxvel, double acc, unsigned char enables,
		  struct state_tag_t tag);

int tpAddLine(TP_STRUCT * tp, EmcPose end, int type, double vel, double
	      ini_maxvel, double acc, unsigned char enables, char atspeed,
	      int indexrotary,struct state_tag_t tag);

int tpAddCircle(TP_STRUCT * tp, EmcPose end, PmCartesian center,
		PmCartesian normal, int turn, int type, double vel, double ini_maxvel,
		double acc, unsigned char enables, char atspeed,struct state_tag_t tag);

int tpRunCycle(TP_STRUCT * tp, long period);

int tpPause(TP_STRUCT * tp);

int tpResume(TP_STRUCT * tp);

int tpAbort(TP_STRUCT * tp);

int tpGetPos(TP_STRUCT const  * const tp, EmcPose * const pos);

int tpIsDone(TP_STRUCT * tp);

int tpQueueDepth(TP_STRUCT * tp);

int tpActiveDepth(TP_STRUCT * tp);

int tpGetMotionType(TP_STRUCT * tp);

int tpSetSpindleSync(TP_STRUCT * tp, double sync, int wait);

void tpToggleDIOs(TP_STRUCT const * const tp,TC_STRUCT * tc); //gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's

int tpSetAout(TP_STRUCT * tp, unsigned int index, double start, double end);

int tpSetDout(TP_STRUCT * tp, unsigned int index, unsigned char start, unsigned char end); //gets called to place DIO toggles on the TC queue

// for jog-while-paused:
int tpIsPaused(TP_STRUCT * tp);

int tpSnapshot(TP_STRUCT * from, TP_STRUCT * to);

#endif // TP_PRIVATE_H
