/********************************************************************
* Description: emcsched.cc
*   Common functions for program scheduling calls
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <list>
#include <stdint.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "nml_oi.hh"            // nmlErrorFormat, NML_ERROR, etc
#include "rcs_print.hh"
#include "timer.hh"             // esleep
#include "shcom.hh"             // Common NML communications functions
#include "emcsched.hh"          // Common scheduling functions

#define MAX_PRIORITY 0x80000000
#define POLYNOMIAL 0xD8  /* 11011 followed by 0's */
#define WIDTH  (8 * sizeof(crc))
#define TOPBIT (1 << (WIDTH - 1))

typedef uint32_t crc;
crc crcTable[256];
crc crcResult;
int autoTagId = 0;
queueStatusType queueStatus = qsStop;

class SchedEntry {
    int priority;
    int tagId;
    float xpos, ypos, zpos;
    int zone;
    string programName;
    string fileName;
    float feedOverride;
    float spindleOverride;
    int tool;

  public:
    SchedEntry();
    ~SchedEntry();

    int getPriority() const;
    void setPriority(int pri);
    int getTagId() const;
    void setTagId(int tag);
    void getOffsets(float &x, float &y, float &z);
    void setOffsets(float x, float y, float z);
    int getZone() const;
    void setZone(int z);
    string getFileName() const;
    void setFileName(string s);
    string getProgramName() const;
    void setProgramName(string s);
    float getFeedOverride() const;
    void setFeedOverride(float f);
    float getSpindleOverride() const;
    void setSpindleOverride(float s);
    int getTool() const;
    void setTool(int t);
  };

SchedEntry::SchedEntry() {
    priority = 0;
    tagId = 0;
    xpos = 0.0;
    ypos = 0.0;
    zpos = 0.0;
    zone = 0;
    fileName = "";
    feedOverride = 100.0;
    spindleOverride = 100.0;
    tool = 1;
  }

list<SchedEntry> q;

bool operator<(const SchedEntry &a, const SchedEntry &b) {
  return a.getPriority() < b.getPriority();
  }

bool operator==(const SchedEntry &a, const SchedEntry &b) {
  return a.getPriority() == b.getPriority();
  }

SchedEntry::~SchedEntry() {
  }

int SchedEntry::getPriority() const {
  return priority;
  }

void SchedEntry::setPriority(int pri) {
  priority = pri;
  }

int SchedEntry::getTagId() const {
  return tagId;
  }

void SchedEntry::setTagId(int tag) {
  tagId = tag;
  }

void SchedEntry::getOffsets(float &x, float &y, float &z) {
  x = xpos;
  y = ypos;
  z = zpos;
  }

void SchedEntry::setOffsets(float x, float y, float z) {
  xpos = x;
  ypos = y;
  zpos = z;
  }

int SchedEntry::getZone() const {
  return zone;
  }

void SchedEntry::setZone(int z) {
  zone = z;
  }

string SchedEntry::getFileName() const {
  return fileName;
  }

void SchedEntry::setFileName(string s) {
  fileName = s;
  }

string SchedEntry::getProgramName() const {
  return programName;
  }

void SchedEntry::setProgramName(string s) {
  programName = s;
  }

float SchedEntry::getFeedOverride() const {
  return feedOverride;
  }

void SchedEntry::setFeedOverride(float f) {
  feedOverride = f;
  }

float SchedEntry::getSpindleOverride() const {
  return spindleOverride;
  }

void SchedEntry::setSpindleOverride(float s) {
  spindleOverride = s;
  }

int SchedEntry::getTool() const {
  return tool;
  }

void SchedEntry::setTool(int t) {
  tool = t;
  }

static void crcInit() {
  crc rmdr;
  int i;
  uint32_t bit;

  for (i = 0; i < 256; ++i)
    {
      rmdr = i << (WIDTH - 8);
      for (bit = 8; bit > 0; --bit)
        {
          if (rmdr & TOPBIT)
            {
              rmdr = (rmdr << 1) ^ POLYNOMIAL;
            }
          else
            {
              rmdr = (rmdr << 1);
            }
        }
      crcTable[i] = rmdr;
    }
  }

static void crcFast(uint32_t value) {

  uint32_t data;

  data = value ^ (crcResult >> (WIDTH - 8));
  crcResult = crcTable[data] ^ (crcResult << 8);
  }

static queueStatusType getQueueStatus() {
  return queueStatus;
}

static void setQueueStatus(queueStatusType qs) {
  if ((qs == qsResume) && (queueStatus == qsPause)) queueStatus = qsRun;
  else queueStatus = qs;
  }

bool isIdle()
{
   return ((emcStatus->task.interpState != EMC_TASK_INTERP_READING) && 
          (emcStatus->task.interpState != EMC_TASK_INTERP_WAITING) &&
          (emcStatus->task.interpState != EMC_TASK_INTERP_PAUSED));
}

bool interlocksOk() {
  if (emcStatus->task.state == EMC_TASK_STATE_ESTOP) {
    return false;
    }

  if (emcStatus->task.state != EMC_TASK_STATE_ON) {
    return false;
    }
  return true;
}

void updateQueue() {
  char fileStr[255];
  float x, y, z;
  char cmd[80];

  if (queueStatus == qsRun) {
    if (isIdle() && q.empty()) {
      queueStatus = qsStop;
      return;
      }
    if (!q.empty()) {
      if (isIdle()) {
        q.front().setPriority(MAX_PRIORITY); // Lock job as first job
        if (interlocksOk()) {
          sendFeedOverride(((double) q.front().getFeedOverride()) / 100.0);
          sendSpindleOverride(0, ((double) q.front().getSpindleOverride()) / 100.0);
          sendMdi();
          sendMdiCmd("G92.1\n");
          if (emcCommandWaitDone() != 0) {
            queueStatus = qsError;
            return;
            }
          if (q.front().getZone() != 0) {
            switch (q.front().getZone()) {
              case 1: sendMdiCmd("G54\n"); break;
              case 2: sendMdiCmd("G55\n"); break;
              case 3: sendMdiCmd("G56\n"); break;
              case 4: sendMdiCmd("G57\n"); break;
              case 5: sendMdiCmd("G58\n"); break;
              case 6: sendMdiCmd("G59\n"); break;
              case 7: sendMdiCmd("G59.1\n"); break;
              case 8: sendMdiCmd("G59.2\n"); break;
              case 9: sendMdiCmd("G59.3\n");
              }
            if (emcCommandWaitDone() != 0) {
              queueStatus = qsError;
              return;
              }
            }
          else {
             q.front().getOffsets(x, y, z);
             sprintf(cmd, "G0 X%f Y%f Z%f\n", x, y, z); 
             sendMdiCmd(cmd);
             if (emcCommandWaitDone() != 0) {
               queueStatus = qsError;
               return;
               }
             sendMdiCmd("G92 X0 Y0 Z0\n");
             if (emcCommandWaitDone() != 0) {
               queueStatus = qsError;
               return;
               }
             }
          if (sendTaskPlanInit() != 0) {
            queueStatus = qsError;
            }
          sendAuto();
          strcpy(fileStr, defaultPath);
          strcat(fileStr, q.front().getFileName().c_str());
          if (sendProgramOpen(fileStr) != 0) {
            queueStatus = qsError;
            return;
            }
          if (sendProgramRun(0) != 0) {
            queueStatus = qsError;
            }
          q.remove(q.front());
          }
        else queueStatus = qsError;
        } 
      }
    }
  }

int addProgram(int pri, int tag, float x, float y, float z, int azone, string progName, float feedOvr, float spindleOvr, int toolNum) {
  SchedEntry p;

  p.setPriority(pri);
  p.setTagId(tag);
  if (azone == 0) 
    p.setOffsets(x, y, z);
  else p.setZone(azone);
  p.setFileName(progName);
  p.setFeedOverride(feedOvr);
  p.setSpindleOverride(spindleOvr);
  p.setTool(toolNum);
  q.push_back(p);
  q.sort();
  return q.size();
  }

int getQueueSize() {
  return q.size();
  }

void clearQueue() {
  while (!q.empty()) {
    q.remove(q.front());
    }
  }

int getStatus() {
  return (int) getQueueStatus();
  }

void queueStart() {
  setQueueStatus(qsRun);
  }

void queueStop() {
  setQueueStatus(qsStop);
  clearQueue();
  }

void queuePause() {
  setQueueStatus(qsPause);
  }

int getProgramById(int id, qRecType *qRec) {
  list<SchedEntry>::iterator i;
  
  if (q.size() == 0) return -1;
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) break;
    }
  if (i->getTagId() != id) return -1;
  qRec->priority = i->getPriority();
  qRec->tagId = i->getTagId();
  i->getOffsets(qRec->xpos, qRec->ypos, qRec->zpos);
  qRec->zone = i->getZone();
  strcpy(qRec->fileName,  i->getFileName().c_str());
  qRec->feedOverride = i->getFeedOverride();
  qRec->spindleOverride = i->getSpindleOverride();
  qRec->tool = i->getTool();

  return 0;
  }

int getProgramByIndex(int idx, qRecType *qRec) {
  list<SchedEntry>::iterator i;
  int index;

  if ((unsigned int) idx >= q.size()) return -1;
  index = 0;
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) break;
    index++;
    }
  qRec->priority = i->getPriority();
  qRec->tagId = i->getTagId();
  i->getOffsets(qRec->xpos, qRec->ypos, qRec->zpos);
  qRec->zone = i->getZone();
  strcpy(qRec->fileName,  i->getFileName().c_str());
  qRec->feedOverride = i->getFeedOverride();
  qRec->spindleOverride = i->getSpindleOverride();
  qRec->tool = i->getTool();

  return 0;
 }

int getQueueCRC() {
  list<SchedEntry>::iterator i;
  
  crcResult = 0;
  for (i=q.begin(); i!=q.end(); ++i) {
    crcFast(i->getTagId());
    }
  return crcResult;
  }

int getFirstTagId() {
  if (q.empty()) return 0;
  return q.front().getTagId();
  }

int getLastTagId() {
  if (q.empty()) return 0;
  return q.back().getTagId();
  }

int deleteProgramById(int id) {
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) {
      if (i->getPriority() == (int)MAX_PRIORITY) return -1;
      i = q.erase(i);
      return 0;
      }
    }
  return -1;
  }

int deleteProgramByIndex(int idx) {
  list<SchedEntry>::iterator i;
  int index = 0;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) {
      if (i->getPriority() == (int)MAX_PRIORITY) return -1;
      i = q.erase(i);
      return 0;
      }
    index++;
    }
  return -1;
  }

int changePriorityById(int id, int newPriority) {
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) {
      if (i->getPriority() == (int)MAX_PRIORITY) return -1;
      i->setPriority(newPriority);
      q.sort();
      return 0;
      }
    }
  return -1;
  }

int changePriorityByIndex(int idx, int newPriority) {
  list<SchedEntry>::iterator i;
  int index = 0;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) {
      if (i->getPriority() == (int)MAX_PRIORITY) return -1;
      i->setPriority(newPriority);
      q.sort();
      return 0;
      }
    index++;
    }
  return -1;
  }

int getPriorityById(int id, int &pri) {
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) {
      pri = i->getPriority();
      return 0;
      }
    }
  return -1;
  }

int getPriorityByIndex(int idx, int &pri) {
  list<SchedEntry>::iterator i;
  int index = 0;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) {
      pri = i->getPriority();
      return 0;
      }
    index++;
    }
  return -1;
  }

int getNextTagId() {
  return autoTagId++;
  }

void resetTagIds(int startId) {
  autoTagId = startId;
  }

void schedInit() {
  crcInit();
  }
