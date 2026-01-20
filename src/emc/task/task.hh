//    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef EMC_TASK_HH
#define EMC_TASK_HH
#include "taskclass.hh"
#include "emc_nml.hh"
#include <memory>

extern std::unique_ptr<NMLmsg> emcTaskCommand;
extern int stepping;
extern int steppingWait;
extern int emcTaskQueueCommand(std::unique_ptr<NMLmsg> &&cmd);
extern int emcTaskOnce(const char *inifile, EMC_IO_STAT &emcioStatus);

// Returns 0 if all joints are homed, 1 if any joints are un-homed.
int all_homed(void);
bool jogging_is_active(void);

int emcTaskInit();
int emcTaskHalt();
int emcTaskStateRestore();
int emcTaskAbort();
int emcTaskSetMode(EMC_TASK_MODE mode);
int emcTaskSetState(EMC_TASK_STATE state);
int emcTaskPlanInit();
int emcTaskPlanSetWait();
int emcTaskPlanIsWait();
int emcTaskPlanClearWait();
int emcTaskPlanSynch();
int emcTaskPlanSetOptionalStop(bool state);
int emcTaskPlanSetBlockDelete(bool state);
void emcTaskPlanExit();
int emcTaskPlanOpen(const char *file);
int emcTaskPlanRead();
int emcTaskPlanExecute(const char *command);
int emcTaskPlanExecute(const char *command, int line_number); //used in case of MDI to pass the pseudo line number to interp
int emcTaskPlanPause();
int emcTaskPlanResume();
int emcTaskPlanClose();
int emcTaskPlanReset();

int emcTaskPlanLine();
int emcTaskPlanLevel();
int emcTaskPlanCommand(char *cmd);

int emcTaskUpdate(EMC_TASK_STAT * stat);

#endif

