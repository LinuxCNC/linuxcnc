/********************************************************************
* Description: emctask.cc
*   Mode and state management for EMC_TASK class
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

#include <stdlib.h>
#include <rtapi_string.h>	// rtapi_strlcpy()
#include <sys/stat.h>		// struct stat
#include <unistd.h>		// stat()
#include <limits.h>		// PATH_MAX
#include <dlfcn.h>
#include <memory>

#include "rcs.hh"		// INIFILE
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "emcglb.h"		// EMC_INIFILE
#include "interpl.hh"		// NML_INTERP_LIST, interp_list
#include "canon.hh"		// CANON_VECTOR, GET_PROGRAM_ORIGIN()
#include "rs274ngc_interp.hh"	// the interpreter
#include "interp_return.hh"	// INTERP_FILE_NOT_OPEN
#include "inifile.hh"
#include "rcs_print.hh"
#include "task.hh"		// emcTaskCommand etc
#include "taskclass.hh"
#include "motion.h"

#define USER_DEFINED_FUNCTION_MAX_DIRS 5
#define MAX_M_DIRS (USER_DEFINED_FUNCTION_MAX_DIRS+1)
//note:the +1 is for the PROGRAM_PREFIX or default directory==nc_files

/* flag for how we want to interpret traj coord mode, as mdi or auto */
static EMC_TASK_MODE mdiOrAuto = EMC_TASK_MODE::AUTO;

InterpBase *pinterp=0;
#define interp (*pinterp)
setup_pointer _is = 0; // helper for gdb hardware watchpoints FIXME


// Print error messages thrown by interpreter
static char interp_error_text_buf[LINELEN];
static char interp_stack_buf[LINELEN];

static void print_interp_error(int retval)
{
    int index = 0;
    if (retval == 0) {
	return;
    }

    if (0 != emcStatus) {
	emcStatus->task.interpreter_errcode = retval;
    }

    interp_error_text_buf[0] = 0;
    interp.error_text(retval, interp_error_text_buf, LINELEN);
    if (0 != interp_error_text_buf[0]) {
	rcs_print_error("interp_error: %s\n", interp_error_text_buf);
    }
    emcOperatorError("%s", interp_error_text_buf);
    index = 0;
    if (emc_debug & EMC_DEBUG_INTERP) {
	rcs_print("Interpreter stack: \t");
	while (index < 5) {
	    interp_stack_buf[0] = 0;
	    interp.stack_name(index, interp_stack_buf, LINELEN);
	    if (0 == interp_stack_buf[0]) {
		break;
	    }
	    rcs_print(" - %s ", interp_stack_buf);
	    index++;
	}
	rcs_print("\n");
    }
}

/*
  format string for user-defined programs, e.g., "programs/M1%02d" means
  user-defined programs are in the programs/ directory and are named
  M1XX, where XX is a two-digit string.
*/
static char user_defined_fmt[MAX_M_DIRS][EMC_SYSTEM_CMD_LEN]; // ex: "dirname/M1%02d"

// index to directory for each user defined function:
static int user_defined_function_dirindex[USER_DEFINED_FUNCTION_NUM];

static void user_defined_add_m_code(int num, double arg1, double arg2)
{
    // num      is the m_code number, typically 00-99 corresponding to M100-M199
    char fmt[EMC_SYSTEM_CMD_LEN];
    auto system_cmd = std::make_unique<EMC_SYSTEM_CMD>();

    //we call FINISH() to flush any linked motions before the M1xx call, 
    //otherwise they would mix badly
    FINISH();
    rtapi_strxcpy(fmt, user_defined_fmt[user_defined_function_dirindex[num]]);
    rtapi_strxcat(fmt, " %f %f");
    snprintf(system_cmd->string, sizeof(system_cmd->string), fmt, num, arg1, arg2);
    interp_list.append(std::move(system_cmd));
}

int emcTaskInit()
{
    char mdir[MAX_M_DIRS][PATH_MAX];
    int num,dct,dmax;
    char path[EMC_SYSTEM_CMD_LEN];
    struct stat buf;
    IniFile inifile;
    std::optional<const char*> inistring;
    ZERO_EMC_POSE(emcStatus->task.toolOffset);

    inifile.Open(emc_inifile);

    // Identify user_defined_function directories
    if ((inistring = inifile.Find("PROGRAM_PREFIX", "DISPLAY"))) {
        if (strlen(*inistring) >= sizeof(mdir[0])) {
            rcs_print("[DISPLAY]PROGRAM_PREFIX too long (max len %zu)\n", sizeof(mdir[0]));
            return -1;
        }
        strncpy(mdir[0], *inistring, sizeof(mdir[0]));
    } else {
        // default dir if no PROGRAM_PREFIX
        rtapi_strlcpy(mdir[0], "nc_files", sizeof(mdir[0]));
    }
    dmax = 1; //one directory mdir[0],  USER_M_PATH specifies additional dirs

    // user can specify a list of directories for user defined functions
    // with a colon (:) separated list
    if ((inistring = inifile.Find("USER_M_PATH", "RS274NGC"))) {
        char* nextdir;
        char tmpdirs[PATH_MAX];

        for (dct=1; dct < MAX_M_DIRS; dct++) mdir[dct][0] = 0;

        if (strlen(*inistring) >= sizeof(tmpdirs)) {
            rcs_print("[RS274NGC]USER_M_PATH too long (max len %zu)\n", sizeof(tmpdirs));
            return -1;
        }
        strncpy(tmpdirs, *inistring, sizeof(tmpdirs));

        nextdir = strtok(tmpdirs,":");  // first token
        dct = 1;
        while (dct < MAX_M_DIRS) {
            if (nextdir == NULL) break; // no more tokens
            if (strlen(nextdir) >= sizeof(mdir[dct])) {
                rcs_print("[RS274NGC]USER_M_PATH component (%s) too long (max len %zu)\n",
                          nextdir, sizeof(mdir[dct]));
                return -1;
            }
            strncpy(mdir[dct], nextdir, sizeof(mdir[dct]));
            nextdir = strtok(NULL,":");
            dct++;
        }
        dmax=dct;
    }
    inifile.Close();

    /* check for programs named programs/M100 .. programs/M199 and add
       any to the user defined functions list */
    for (num = 0; num < USER_DEFINED_FUNCTION_NUM; num++) {
	for (dct=0; dct < dmax; dct++) {
            char expanddir[LINELEN];
	    if (!mdir[dct][0]) continue;
            if (inifile.TildeExpansion(mdir[dct],expanddir,sizeof(expanddir))) {
		rcs_print("emcTaskInit: TildeExpansion failed for %s, ignoring\n",
			 mdir[dct]);
            }
	    size_t ret = snprintf(path, sizeof(path), "%s/M1%02d",expanddir,num);
	    if (ret < sizeof(path) && 0 == stat(path, &buf)) {
	        if (buf.st_mode & S_IXUSR) {
		    // set the user_defined_fmt string with dirname
		    // note the %%02d means 2 digits after the M code
		    // and we need two % to get the literal %
		    ret = snprintf(user_defined_fmt[dct], sizeof(user_defined_fmt[dct]),
			     "%s/M1%%02d", expanddir); // update global
		    if(ret >= sizeof(user_defined_fmt[0])){
			return -EMSGSIZE; // name truncated
		    } else {
		    USER_DEFINED_FUNCTION_ADD(user_defined_add_m_code,num);
		    if (emc_debug & EMC_DEBUG_CONFIG) {
		        rcs_print("emcTaskInit: adding user-defined function %s\n",
			     path);
		    }
	            user_defined_function_dirindex[num] = dct;
	            break; // use first occurrence found for num
		    }
	        } else {
		    if (emc_debug & EMC_DEBUG_CONFIG) {
		        rcs_print("emcTaskInit: user-defined function %s found, but not executable, so ignoring\n",
			     path);
		    }
	        }
	    }
	}
    }

    return 0;
}

int emcTaskHalt()
{
    return 0;
}

int emcTaskStateRestore()
{
    int res = 0;
    // Do NOT restore on MDI command
    if (emcStatus->task.mode == EMC_TASK_MODE::AUTO) {
        // Validity of state tag checked within restore function
        res = pinterp->restore_from_tag(emcStatus->motion.traj.tag);
    }
    return res;
}

void emcTaskQueueTaskPlanSynchCmd()
{
    auto taskPlanSynchCmd = std::make_unique<EMC_TASK_PLAN_SYNCH>();
    emcTaskQueueCommand(std::move(taskPlanSynchCmd));
}

int emcTaskAbort()
{
    emcMotionAbort();

    // clear out the pending command
    emcTaskCommand = 0;
    interp_list.clear();

    // clear out the interpreter state
    emcStatus->task.interpState = EMC_TASK_INTERP::IDLE;
    emcStatus->task.execState = EMC_TASK_EXEC::DONE;
    emcStatus->task.task_paused = 0;
    emcStatus->task.motionLine = 0;
    emcStatus->task.readLine = 0;
    emcStatus->task.command[0] = 0;
    emcStatus->task.callLevel = 0;

    stepping = 0;
    steppingWait = 0;

    // now queue up command to resynch interpreter
    emcTaskQueueTaskPlanSynchCmd();

    // without emcTaskPlanClose(), a new run command resumes at
    // aborted line-- feature that may be considered later
    {
	int was_open = taskplanopen;
	emcTaskPlanClose();
        emcTaskPlanReset();  // Flush any unflushed segments
	if (emc_debug & EMC_DEBUG_INTERP && was_open) {
	    rcs_print("emcTaskPlanClose() called at %s:%d\n", __FILE__,
		      __LINE__);
	}
    }

    return 0;
}

int emcTaskSetMode(EMC_TASK_MODE mode)
{
    int retval = 0;

    if (jogging_is_active()) {
        emcOperatorError("Ignoring task mode change while jogging");
        return 0;
    }

    switch (mode) {
    case EMC_TASK_MODE::MANUAL:
	// go to manual mode
        if (all_homed()) {
            emcTrajSetMode(EMC_TRAJ_MODE::TELEOP);
        } else {
            emcTrajSetMode(EMC_TRAJ_MODE::FREE);
        }
	mdiOrAuto = EMC_TASK_MODE::AUTO;	// we'll default back to here
	break;

    case EMC_TASK_MODE::MDI:
	// go to mdi mode
	emcTrajSetMode(EMC_TRAJ_MODE::COORD);
	emcTaskAbort();
	emcTaskPlanSynch();
	mdiOrAuto = EMC_TASK_MODE::MDI;
	break;

    case EMC_TASK_MODE::AUTO:
	// go to auto mode
	emcTrajSetMode(EMC_TRAJ_MODE::COORD);
	emcTaskAbort();
	emcTaskPlanSynch();
	mdiOrAuto = EMC_TASK_MODE::AUTO;
	break;

    default:
	retval = -1;
	break;
    }

    return retval;
}

int emcTaskSetState(EMC_TASK_STATE state)
{
    int t;
    int retval = 0;

    switch (state) {
    case EMC_TASK_STATE::OFF:
        emcMotionAbort();
	// turn the machine servos off-- go into READY state
    for (t = 0; t < emcStatus->motion.traj.spindles; t++)  emcSpindleAbort(t);
	emcTrajDisable();
	emcIoAbort(EMC_ABORT::TASK_STATE_OFF);
    emcCoolantFloodOff();//TODO: race here
	emcTaskAbort();
    emcJointUnhome(-2); // only those joints which are volatile_home
	emcAbortCleanup(EMC_ABORT::TASK_STATE_OFF);
	emcTaskPlanSynch();
	break;

    case EMC_TASK_STATE::ON:
	// turn the machine servos on
	emcTrajEnable();
    emcCoolantFloodOff();//TODO: race here
	break;

    case EMC_TASK_STATE::ESTOP_RESET:
	// reset the estop
	emcAuxEstopOff();
	emcCoolantFloodOff();//TODO: race here
	emcTaskAbort();
        emcIoAbort(EMC_ABORT::TASK_STATE_ESTOP_RESET);
    for (t = 0; t < emcStatus->motion.traj.spindles; t++) emcSpindleAbort(t);
	emcAbortCleanup(EMC_ABORT::TASK_STATE_ESTOP_RESET);
	emcTaskPlanSynch();
	break;

    case EMC_TASK_STATE::ESTOP:
        emcMotionAbort();
	for (t = 0; t < emcStatus->motion.traj.spindles; t++) emcSpindleAbort(t);
	// go into estop-- do both IO estop and machine servos off
	emcAuxEstopOn();
	emcTrajDisable();
    emcCoolantFloodOff();//TODO: race here
	emcTaskAbort();
        emcIoAbort(EMC_ABORT::TASK_STATE_ESTOP);
	for (t = 0; t < emcStatus->motion.traj.spindles; t++) emcSpindleAbort(t);
        emcJointUnhome(-2); // only those joints which are volatile_home
	emcAbortCleanup(EMC_ABORT::TASK_STATE_ESTOP);
	emcTaskPlanSynch();
	break;

    default:
	retval = -1;
	break;
    }

    return retval;
}

// WM access functions

/*
  determineMode()

  Looks at mode of subsystems, and returns associated mode

  Depends on traj mode, and mdiOrAuto flag

  traj mode   mdiOrAuto     task mode
  ---------   ---------     ---------
  FREE        XXX           MANUAL
  TELEOP      XXX           MANUAL
  COORD       MDI           MDI
  COORD       AUTO          AUTO
  */
EMC_TASK_MODE determineMode()
{
    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::FREE) {
        return EMC_TASK_MODE::MANUAL;
    }
    if (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) {
        return EMC_TASK_MODE::MANUAL;
    }
    // for EMC_TRAJ_MODE::COORD
    return mdiOrAuto;
}

/*
  determineState()

  Looks at state of subsystems, and returns associated state

  Depends on traj enabled, io estop, and desired task state

  traj enabled   io estop      state
  ------------   --------      -----
  DISABLED       ESTOP         ESTOP
  ENABLED        ESTOP         ESTOP
  DISABLED       OUT OF ESTOP  ESTOP_RESET
  ENABLED        OUT OF ESTOP  ON
  */
static EMC_TASK_STATE determineState()
{
    if (emcStatus->io.aux.estop) {
	return EMC_TASK_STATE::ESTOP;
    }

    if (!emcStatus->motion.traj.enabled) {
	return EMC_TASK_STATE::ESTOP_RESET;
    }

    return EMC_TASK_STATE::ON;
}

static int waitFlag = 0;

int emcTaskPlanInit()
{
    if(!pinterp) {
	IniFile inifile;
	std::optional<const char*> inistring;
	inifile.Open(emc_inifile);
	if((inistring = inifile.Find("INTERPRETER", "TASK"))) {
	    pinterp = interp_from_shlib(*inistring);
	    fprintf(stderr, "interp_from_shlib() -> %p\n", pinterp);
            if (!pinterp) {
                fprintf(stderr, "failed to load [TASK]INTERPRETER (%s)\n", *inistring);
                return -1;
            }
	}
        inifile.Close();
    }
    if(!pinterp) {
        pinterp = new Interp;
    }

    Interp *i = dynamic_cast<Interp*>(pinterp);
    if(i) _is = &i->_setup; // FIXME
    else  _is = 0;
    interp.ini_load(emc_inifile);
    waitFlag = 0;

    int retval = interp.init();
    // In task, enable M99 main program endless looping
    interp.set_loop_on_main_m99(true);
    if (retval > INTERP_MIN_ERROR) {  // I'd think this should be fatal.
	print_interp_error(retval);
    } else {
	if (0 != rs274ngc_startup_code[0]) {
	    retval = interp.execute(rs274ngc_startup_code);
	    while (retval == INTERP_EXECUTE_FINISH) {
		retval = interp.execute(0);
	    }
	    if (retval > INTERP_MIN_ERROR) {
		print_interp_error(retval);
	    }
	}
    }

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanInit() returned %d\n", retval);
    }

    return retval;
}

int emcTaskPlanSetWait()
{
    waitFlag = 1;

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanSetWait() called\n");
    }

    return 0;
}

int emcTaskPlanIsWait()
{
    return waitFlag;
}

int emcTaskPlanClearWait()
{
    waitFlag = 0;

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanClearWait() called\n");
    }

    return 0;
}

int emcTaskPlanSetOptionalStop(bool state)
{
    SET_OPTIONAL_PROGRAM_STOP(state);
    return 0;
}

int emcTaskPlanSetBlockDelete(bool state)
{
    SET_BLOCK_DELETE(state);
    return 0;
}


int emcTaskPlanSynch()
{
    int retval = interp.synch();
    if (retval == INTERP_ERROR) {
        emcTaskAbort();
    }

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanSynch() returned %d\n", retval);
    }

    return retval;
}

void emcTaskPlanExit()
{
    if (pinterp != NULL) {
        interp.exit();
    }
}

int emcTaskPlanOpen(const char *file)
{
    if (emcStatus != 0) {
	emcStatus->task.motionLine = 0;
	emcStatus->task.currentLine = 0;
	emcStatus->task.readLine = 0;
    }

    int retval = interp.open(file);
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
	return retval;
    }
    taskplanopen = 1;

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanOpen(%s) returned %d\n", file, retval);
    }

    return retval;
}


int emcTaskPlanRead()
{
    int retval = interp.read();
    if (retval == INTERP_FILE_NOT_OPEN) {
	if (emcStatus->task.file[0] != 0) {
	    retval = interp.open(emcStatus->task.file);
	    if (retval > INTERP_MIN_ERROR) {
		print_interp_error(retval);
	    }
	    retval = interp.read();
	}
    }
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }
    
    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanRead() returned %d\n", retval);
    }
    
    return retval;
}

int emcTaskPlanExecute(const char *command)
{
    int inpos = emcStatus->motion.traj.inpos;	// 1 if in position, 0 if not.

    if (command != 0) {		// Command is 0 if in AUTO mode, non-null if in MDI mode.
	// Don't sync if not in position.
	if ((*command != 0) && (inpos)) {
	    interp.synch();
	}
    }
    int retval = interp.execute(command);
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }
    if(command != 0) {
	FINISH();
    }

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanExecute(0) return %d\n", retval);
    }

    return retval;
}

int emcTaskPlanExecute(const char *command, int line_number)
{
    int retval = interp.execute(command, line_number);
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }
    if(command != 0) { // this means MDI
	FINISH();
    }

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanExecute(%s) returned %d\n", command, retval);
    }

    return retval;
}

int emcTaskPlanClose()
{
    int retval = interp.close();
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }

    taskplanopen = 0;
    return retval;
}

int emcTaskPlanReset()
{
    int retval = interp.reset();
    if (retval > INTERP_MIN_ERROR) {
	print_interp_error(retval);
    }

    return retval;
}

int emcTaskPlanLine()
{
    int retval = interp.line();
    
    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanLine() returned %d\n", retval);
    }

    return retval;
}

int emcTaskPlanLevel()
{
    int retval = interp.call_level();

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanLevel() returned %d\n", retval);
    }

    return retval;
}

int emcTaskPlanCommand(char *cmd)
{
    char buf[LINELEN];

    strcpy(cmd, interp.command(buf, LINELEN));

    if (emc_debug & EMC_DEBUG_INTERP) {
        rcs_print("emcTaskPlanCommand(%s) called. (line_number=%d)\n",
          cmd, emcStatus->task.readLine);
    }

    return 0;
}

int emcTaskUpdate(EMC_TASK_STAT * stat)
{
    stat->mode = determineMode();
    EMC_TASK_STATE oldstate = stat->state;
    stat->state = determineState();

    if(oldstate == EMC_TASK_STATE::ON && oldstate != stat->state) {
	emcTaskAbort();
    for (int s = 0; s < emcStatus->motion.traj.spindles; s++) emcSpindleAbort(s);
        emcIoAbort(EMC_ABORT::TASK_STATE_NOT_ON);
        emcAbortCleanup(EMC_ABORT::TASK_STATE_NOT_ON);
    }

    // execState set in main
    // interpState set in main
    if (emcStatus->motion.traj.id > 0) {
	stat->motionLine = emcStatus->motion.traj.id;
    }
    // currentLine set in main
    // readLine set in main

    char buf[LINELEN];
    rtapi_strxcpy(stat->file, interp.file(buf, LINELEN));
    // command set in main

    // update active G and M codes
    // Start by assuming that we can't unpack a state tag from motion
    int res_state = INTERP_ERROR;
    if (emcStatus->task.interpState != EMC_TASK_INTERP::IDLE) {
        res_state = interp.active_modes(&stat->activeGCodes[0],
					&stat->activeMCodes[0],
					&stat->activeSettings[0],
					emcStatus->motion.traj.tag);
	if (res_state != INTERP_ERROR) {
            rtapi_strxcpy(stat->file, emcStatus->motion.traj.tag.filename);
            stat->currentLine = emcStatus->motion.traj.tag.fields[0];
	}
    } 
    // If we get an error from trying to unpack from the motion state, always
    // use interp's internal state, so the active state is never out of date
    if (emcStatus->task.mode != EMC_TASK_MODE::AUTO ||
	res_state == INTERP_ERROR) {
        interp.active_g_codes(&stat->activeGCodes[0]);
        interp.active_m_codes(&stat->activeMCodes[0]);
        interp.active_settings(&stat->activeSettings[0]);
    }

    //update state of optional stop
    stat->optional_stop_state = GET_OPTIONAL_PROGRAM_STOP();
    
    //update state of block delete
    stat->block_delete_state = GET_BLOCK_DELETE();

    return 0;
}

int emcAbortCleanup(EMC_ABORT reason, const char *message)
{
    int status = interp.on_abort((int)reason,message);
    if (status > INTERP_MIN_ERROR)
	print_interp_error(status);
    return status;
}

