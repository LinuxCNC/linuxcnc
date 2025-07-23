/********************************************************************
* Description: tp.h
*   Trajectory planner based on TC elements
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef TP_H
#define TP_H

#include "posemath.h"
#include "tc_types.h"
#include "tp_types.h"
#include "tp_enums.h"
#include "tcq.h"

int tpCreate(TP_STRUCT * const tp, int _queueSize, TC_STRUCT * const tcSpace);
int tpClear(TP_STRUCT * const tp);
int tpInit(TP_STRUCT * const tp);
int tpClearDIOs(TP_STRUCT * const tp);
int tpSetCycleTime(TP_STRUCT * const tp, double secs);
int tpSetVlimit(TP_STRUCT * const tp, double vLimit, double vLimitAng);
int tpSetId(TP_STRUCT * const tp, int id);
tc_unique_id_t tpGetNextUniqueId(TP_STRUCT * const tp);
void tcSetId(TP_STRUCT * const tp, TC_STRUCT * const tc, struct state_tag_t tag);
int tpGetExecId(TP_STRUCT const * const tp);
int tpGetCompletedId(TP_STRUCT const * const tp);
int tpGetQueuedId(TP_STRUCT const * const tp);
struct state_tag_t tpGetExecTag(const TP_STRUCT * const tp);
int tpGetExecSrcLine(const TP_STRUCT * const tp);
int tpGetNextExecId(TP_STRUCT const * const tp);
int tpSetTermCond(TP_STRUCT * const tp, tc_term_cond_t cond, double tolerance);
int tpSetPos(TP_STRUCT * const tp, EmcPose const * const pos);
int tpAddCurrentPos(TP_STRUCT * const tp, const PmVector * const disp);
int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos);

int tpAddRigidTap(TP_STRUCT * const tp,
        EmcPose end,
        double tap_uu_per_rev,
        double retract_uu_per_rev,
        double ini_maxvel,
        double acc,
        unsigned char enables,
        char atspeed,
        double scale,
        struct state_tag_t tag);

int tpAddLine(TP_STRUCT * const tp,
    EmcPose end,
    EMCMotionTypes canon_motion_type,
    double vel,
    double ini_maxvel,
    double acc,
    unsigned char enables,
    char atspeed,
    IndexRotaryAxis indexrotary,
    struct state_tag_t tag);

int tpAddCircle(TP_STRUCT * const tp,
        EmcPose end,
        PmCartesian center,
        PmCartesian normal,
        int turn, double expected_angle_rad,
        EMCMotionTypes canon_motion_type,
        double vel,
        double ini_maxvel,
        double acc,
        double acc_normal,
        unsigned char enables,
        char atspeed,
        struct state_tag_t tag);

int tpAddDwell(TP_STRUCT * const tp,
    double time_sec, double delta_rpm,
    struct state_tag_t tag);

tp_err_t tpRunCycle(TP_STRUCT * const tp);
int tpPause(TP_STRUCT * const tp);
int tpResume(TP_STRUCT * const tp);
int tpAbort(TP_STRUCT * const tp);
int tpGetPos(TP_STRUCT const  * const tp, EmcPose * const pos);
EmcPose tpGetGoalPos(TP_STRUCT const  * const tp);
int tpIsDone(TP_STRUCT * const tp);
int tpQueueDepth(TP_STRUCT * const tp);
int tpActiveDepth(TP_STRUCT * const tp);
int tpGetMotionType(TP_STRUCT * const tp);
int tpSetSpindleSync(TP_STRUCT * const tp, double sync, int velocity_mode);
void tpToggleDIOs(TC_STRUCT * const tc); //gets called when a new tc is taken from the queue. it checks and toggles all needed DIO's

int tpSetAout(TP_STRUCT * const tp, unsigned char index, double start, double end);
int tpSetDout(TP_STRUCT * const tp, int index, unsigned char start, unsigned char end); //gets called to place DIO toggles on the TC queue

int tpSetRunDir(TP_STRUCT * const tp, tc_direction_t dir);
int tpIsMoving(TP_STRUCT const * const tp);

typedef struct {
    char buf[20];
} LineDescriptor;

void formatLinePrefix(struct state_tag_t const *tag, LineDescriptor *linebuf);
void tpStopWithError(TP_STRUCT * tp, const char *fmt, ...)
    __attribute__((format(printf,2,3)));

#endif				/* TP_H */
