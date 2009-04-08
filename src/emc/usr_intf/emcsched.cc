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

#define POLYNOMIAL 0x8005
#define INITIAL_VALUE 0xFFFF

union crcbuftype {
  unsigned long whole;
  struct {
    unsigned char data;
    unsigned int remainder;
    unsigned char head;
    } part;
  } crcBuffer;

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

static void putCRC(unsigned char b) {
  unsigned char i;

  crcBuffer.part.data = b;
  for (i=0; i<8; i++) {
    crcBuffer.whole = crcBuffer.whole << 1;
    if (crcBuffer.part.head & 0x01)
      crcBuffer.part.remainder ^= POLYNOMIAL;
    }
  }

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

list<SchedEntry> q;
queueStatusType queueStatus = qsStop;
int autoTagId = 0;

bool operator<(const SchedEntry &a, const SchedEntry &b) {
  return a.getPriority() < b.getPriority();
  }

bool operator==(const SchedEntry &a, const SchedEntry &b) {
  return a.getPriority() == b.getPriority();
  }

static queueStatusType getQueueStatus() {
  return queueStatus;
}

static void setQueueStatus(queueStatusType qs) {
  if ((qs == qsResume) && (queueStatus == qsPause)) queueStatus = qsRun;
  else queueStatus = qs;
  }

void updateQueue() {
  char fileStr[255];
  float x, y, z;
  char cmd[80];

  if (queueStatus == qsRun)
    if (!q.empty()) 
      if ((emcStatus->task.interpState == EMC_TASK_INTERP_READING) || (emcStatus->task.interpState == EMC_TASK_INTERP_READING)) {
        q.remove(q.front());
        if (!q.empty()) {
//          strcpy(fileStr, "../../nc_files/");
          strcpy(fileStr, defaultPath);
          strcat(fileStr, q.front().getFileName().c_str());
          if (sendProgramOpen(fileStr) != 0) queueStatus = qsError;
          sendMdi();
          sendMdiCmd("G92.1\n");
          if (q.front().getZone() == 0) {
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
            }
          else {
             q.front().getOffsets(x, y, z);
             sprintf(cmd, "G0 X%f Y%f Z%f\n", x, y, z); 
             sendMdiCmd(cmd);
             sendMdiCmd("G92 X0 Y0 Z0\n");
             sendFeedOverride(((double) q.front().getFeedOverride()) / 100.0);
             sendSpindleOverride(((double) q.front().getSpindleOverride()) / 100.0);             
             }
          if (sendTaskPlanInit() != 0) queueStatus = qsError;
          if (sendProgramRun(0) != 0) queueStatus = qsError;
          }
        } 
  }

int addProgram(int pri, int tag, float x, float y, float z, int azone, string progName, float feedOvr, float spindleOvr, int toolNum) {
  SchedEntry p;

  p.setPriority(pri);
  p.setTagId(tag);
  if (azone > 0) 
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

qRecType *getProgramById(int id) {
  static qRecType qRec;
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) break;
    if (i == q.end()) return NULL;
    }
  qRec.priority = i->getPriority();
  qRec.tagId = i->getTagId();
  i->getOffsets(qRec.xpos, qRec.ypos, qRec.zpos);
  qRec.zone = i->getZone();
  strcpy(qRec.fileName,  i->getFileName().c_str());
  qRec.feedOverride = i->getFeedOverride();
  qRec.spindleOverride = i->getSpindleOverride();
  qRec.tool = i->getTool();

  return &qRec;
  }

qRecType *getProgramByIndex(int idx) {
  static qRecType qRec;
  list<SchedEntry>::iterator i;
  int index;

  index = 0;
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) break;
    index++;
    if (i == q.end()) return NULL;
    }
  qRec.priority = i->getPriority();
  qRec.tagId = i->getTagId();
  i->getOffsets(qRec.xpos, qRec.ypos, qRec.zpos);
  qRec.zone = i->getZone();
  strcpy(qRec.fileName,  i->getFileName().c_str());
  qRec.feedOverride = i->getFeedOverride();
  qRec.spindleOverride = i->getSpindleOverride();
  qRec.tool = i->getTool();

  return &qRec;
 }

int getQueueCRC() {
  list<SchedEntry>::iterator i;
  
  crcBuffer.part.remainder = INITIAL_VALUE;
  for (i=q.begin(); i!=q.end(); ++i) {
    putCRC((unsigned char)(i->getTagId() >> 8));
    putCRC((unsigned char)(i->getTagId() & 0xFF));
    }
  putCRC(0);
  putCRC(0);
  return crcBuffer.part.remainder;
  }

int getFirstTagId() {
  return q.front().getTagId();
  }

int getLastTagId() {
  return q.back().getTagId();
  }

int deleteProgramById(int id) {
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) {
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
      i->setPriority(newPriority);
      q.sort();
      return 0;
      }
    index++;
    }
  return -1;
  }

int getPriorityById(int id) {
  list<SchedEntry>::iterator i;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (i->getTagId() == id) {
      return i->getPriority();
      }
    }
  return -1;
  }

int getPriorityByIndex(int idx) {
  list<SchedEntry>::iterator i;
  int index = 0;
  
  for (i=q.begin(); i!=q.end(); ++i) {
    if (index == idx) {
      return i->getPriority();
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
