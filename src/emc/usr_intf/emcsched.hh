/********************************************************************
* Description: emcsched.hh
*   Headers for common program scheduling calls
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich, Alex Joni
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2009 All rights reserved.
*
* Last change:
********************************************************************/

#ifndef EMCSCHED_HH
#define EMCSCHED_HH

#include "emc_nml.hh"
#include "nml_oi.hh"            // NML_ERROR_LEN
#include <string>

using namespace std;

typedef enum {
  qsStop, qsRun, qsPause, qsResume, qsError, qsUnknown} queueStatusType;

typedef struct {  
    int priority;
    int tagId;
    float xpos, ypos, zpos;
    int zone;
    char fileName[255];
    float feedOverride;
    float spindleOverride;
    int tool;
    } qRecType;

extern int addProgram(int pri, int tag, float x, float y, float z, int azone, string progName, float feedOvr, float spindleOvr, int toolNum);
extern void updateQueue();
extern int getQueueSize();
extern void clearQueue();
extern int getStatus();
extern void queueStart();
extern void queueStop();
extern void queuePause();
extern int getProgramById(int id, qRecType *qRec);
extern int getProgramByIndex(int idx, qRecType *qRec);
extern int getQueueCRC();
extern int getFirstTagId();
extern int getLastTagId();
extern int deleteProgramById(int id);
extern int deleteProgramByIndex(int idx);
extern int changePriorityById(int id, int newPriority);
extern int changePriorityByIndex(int idx, int newPriority);
extern int getPriorityById(int id, int &pri);
extern int getPriorityByIndex(int idx, int &pri);
extern int getNextTagId();
extern void resetTagIds(int startId);
extern void schedInit();

#endif				/* ifndef SHCOM_HH */
