// this is a slide-in replacement for the functions in iotaskintf.cc
// iotaskintf functions are made into class methods and are the default
// methods of TaskClass which may be overridden by Python methods


/********************************************************************
* Description: iotaskintf.cc
*   NML interface functions for IO
*
*   Based on a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include <math.h>		// fabs()
#include <float.h>		// DBL_MAX
#include <string.h>		// memcpy() strncpy()
#include <stdlib.h>		// malloc()
#include <sys/wait.h>

#include "rcs.hh"		// RCS_CMD_CHANNEL, etc.
#include "rcs_print.hh"
#include "timer.hh"             // esleep, etc.
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "emcglb.h"		// EMC_INIFILE

#include "initool.hh"

#include "python_plugin.hh"
#include "taskclass.hh"

// Python plugin interface
#define TASK_MODULE "task"
#define TASK_INIT "task_init"
#define PLUGIN_CALL "plugin_call"

extern PythonPlugin *python_plugin;  // exported by python_plugin.cc
#define PYUSABLE (((python_plugin) != NULL) && (python_plugin->usable()))
static int emcPythonReturnValue(const char *funcname, bp::object &retval);
Task *task_methods;
static bool fake_iostat;

// IO INTERFACE

// the NML channels to the EMCIO controller
static RCS_CMD_CHANNEL *emcIoCommandBuffer = 0;
static RCS_STAT_CHANNEL *emcIoStatusBuffer = 0;

// global status structure
EMC_IO_STAT *emcIoStatus = 0;

// serial number for communication
static int emcIoCommandSerialNumber = 0;
static double EMCIO_BUFFER_GET_TIMEOUT = 5.0;

static int forceCommand(RCS_CMD_MSG *msg);

static int emcioNmlGet()
{
    int retval = 0;
    double start_time;
    RCS_PRINT_DESTINATION_TYPE orig_dest;
    if (emcIoCommandBuffer == 0) {
	orig_dest = get_rcs_print_destination();
	set_rcs_print_destination(RCS_PRINT_TO_NULL);
	start_time = etime();
	while (start_time - etime() < EMCIO_BUFFER_GET_TIMEOUT) {
	    emcIoCommandBuffer =
		new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc",
				    EMC_NMLFILE);
	    if (!emcIoCommandBuffer->valid()) {
		delete emcIoCommandBuffer;
		emcIoCommandBuffer = 0;
	    } else {
		break;
	    }
	    esleep(0.1);
	}
	set_rcs_print_destination(orig_dest);
    }

    if (emcIoCommandBuffer == 0) {
	emcIoCommandBuffer =
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", EMC_NMLFILE);
	if (!emcIoCommandBuffer->valid()) {
	    delete emcIoCommandBuffer;
	    emcIoCommandBuffer = 0;
	    retval = -1;
	}
    }

    if (emcIoStatusBuffer == 0) {
	orig_dest = get_rcs_print_destination();
	set_rcs_print_destination(RCS_PRINT_TO_NULL);
	start_time = etime();
	while (start_time - etime() < EMCIO_BUFFER_GET_TIMEOUT) {
	    emcIoStatusBuffer =
		new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc",
				     EMC_NMLFILE);
	    if (!emcIoStatusBuffer->valid()) {
		delete emcIoStatusBuffer;
		emcIoStatusBuffer = 0;
	    } else {
		emcIoStatus =
		    (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
		// capture serial number for next send
		emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
		break;
	    }
	    esleep(0.1);
	}
	set_rcs_print_destination(orig_dest);
    }

    if (emcIoStatusBuffer == 0) {
	emcIoStatusBuffer =
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc", EMC_NMLFILE);
	if (!emcIoStatusBuffer->valid()
	    || EMC_IO_STAT_TYPE != emcIoStatusBuffer->peek()) {
	    delete emcIoStatusBuffer;
	    emcIoStatusBuffer = 0;
	    emcIoStatus = 0;
	    retval = -1;
	} else {
	    emcIoStatus = (EMC_IO_STAT *) emcIoStatusBuffer->get_address();
	    // capture serial number for next send
	    emcIoCommandSerialNumber = emcIoStatus->echo_serial_number;
	}
    }

    return retval;
}

static RCS_CMD_MSG *last_io_command = 0;
static long largest_io_command_size = 0;

/*
  sendCommand() waits until any currently executing command has finished,
  then writes the given command.*/
/*! \todo
  FIXME: Not very RCS-like to wait for status done here. (wps)
*/
static int sendCommand(RCS_CMD_MSG * msg)
{
    // need command buffer to be there
    if (0 == emcIoCommandBuffer) {
	return -1;
    }
    // need status buffer also, to check for command received
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }

    // always force-queue an abort
    if (msg->type == EMC_TOOL_ABORT_TYPE) {
	// just queue the abort and call it a day
	int rc = forceCommand(msg);
	if (rc) {
	    rcs_print_error("forceCommand(EMC_TOOL_ABORT) returned %d\n", rc);
	}
	return 0;
    }

    double send_command_timeout = etime() + 5.0;

    // check if we're executing, and wait until we're done
    while (etime() < send_command_timeout) {
	emcIoStatusBuffer->peek();
	if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
	    emcIoStatus->status == RCS_EXEC) {
	    esleep(0.001);
	    continue;
	} else {
	    break;
	}
    }

    if (emcIoStatus->echo_serial_number != emcIoCommandSerialNumber ||
	emcIoStatus->status == RCS_EXEC) {
	// Still not done, must have timed out.
	rcs_print_error
	    ("Command to IO level (%s:%s) timed out waiting for last command done. \n",
	     emcSymbolLookup(msg->type), emcIoCommandBuffer->msg2str(msg));
	rcs_print_error
	    ("emcIoStatus->echo_serial_number=%d, emcIoCommandSerialNumber=%d, emcIoStatus->status=%d\n",
	     emcIoStatus->echo_serial_number, emcIoCommandSerialNumber,
	     emcIoStatus->status);
	if (0 != last_io_command) {
	    rcs_print_error("Last command sent to IO level was (%s:%s)\n",
			    emcSymbolLookup(last_io_command->type),
			    emcIoCommandBuffer->msg2str(last_io_command));
	}
	return -1;
    }
    // now we can send
    msg->serial_number = ++emcIoCommandSerialNumber;
    if (0 != emcIoCommandBuffer->write(msg)) {
	rcs_print_error("Failed to send command to  IO level (%s:%s)\n",
			emcSymbolLookup(msg->type),
			emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (largest_io_command_size < msg->size) {
	largest_io_command_size = std::max<long>(msg->size, 4096);
	last_io_command = (RCS_CMD_MSG *) realloc(last_io_command, largest_io_command_size);
    }

    if (0 != last_io_command) {
	memcpy(last_io_command, msg, msg->size);
    }

    return 0;
}

/*
  forceCommand() writes the given command regardless of the executing
  status of any previous command.
*/
static int forceCommand(RCS_CMD_MSG * msg)
{
    // need command buffer to be there
    if (0 == emcIoCommandBuffer) {
	return -1;
    }
    // need status buffer also, to check for command received
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }
    // send it immediately
    msg->serial_number = ++emcIoCommandSerialNumber;
    if (0 != emcIoCommandBuffer->write(msg)) {
	rcs_print_error("Failed to send command to  IO level (%s:%s)\n",
			emcSymbolLookup(msg->type),
			emcIoCommandBuffer->msg2str(msg));
	return -1;
    }

    if (largest_io_command_size < msg->size) {
	largest_io_command_size = std::max<long>(msg->size, 4096);
	last_io_command = (RCS_CMD_MSG *) realloc(last_io_command, largest_io_command_size);
    }

    if (0 != last_io_command) {
	memcpy(last_io_command, msg, msg->size);
    }

    return 0;
}

// glue

int emcIoInit() { return task_methods->emcIoInit(); }
int emcIoHalt() { return task_methods->emcIoHalt(); }
int emcIoAbort(int reason) { return task_methods->emcIoAbort(reason); }
int emcIoSetDebug(int debug) { return task_methods->emcIoSetDebug(debug); }
int emcAuxEstopOn()  { return task_methods->emcAuxEstopOn(); }
int emcAuxEstopOff() { return task_methods->emcAuxEstopOff(); }
int emcCoolantMistOn() { return task_methods->emcCoolantMistOn(); }
int emcCoolantMistOff() { return task_methods->emcCoolantMistOff(); }
int emcCoolantFloodOn() { return task_methods->emcCoolantFloodOn(); }
int emcCoolantFloodOff() { return task_methods->emcCoolantFloodOff(); }
int emcLubeOn() { return task_methods->emcLubeOn(); }
int emcLubeOff() { return task_methods->emcLubeOff(); }
int emcToolPrepare(int p, int tool) { return task_methods->emcToolPrepare(p, tool); }
int emcToolStartChange() { return task_methods->emcToolStartChange(); }
int emcToolLoad() { return task_methods->emcToolLoad(); }
int emcToolUnload()  { return task_methods->emcToolUnload(); }
int emcToolLoadToolTable(const char *file) { return task_methods->emcToolLoadToolTable(file); }
int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
                     double frontangle, double backangle, int orientation) {
    return task_methods->emcToolSetOffset( pocket,  toolno,  offset,  diameter,
					   frontangle,  backangle,  orientation); }
int emcToolSetNumber(int number) { return task_methods->emcToolSetNumber(number); }
int emcIoUpdate(EMC_IO_STAT * stat) { return task_methods->emcIoUpdate(stat); }

int emcTaskOnce(const char *filename, const char *python_taskinit)
{
    bp::object retval;
    bp::tuple arg;
    bp::dict kwarg;
    int plugin_status;

    extern Interp interp;

    // initialize the Python plugin singleton
    // Interp is already instantiated but not yet fully configured
    // both Task and Interp use it - first to call configure() instantiates the Python part
    extern struct _inittab builtin_modules[];
    if (PythonPlugin::configure(filename, "PYTHON",  builtin_modules, &interp) != NULL) {
	printf("Python plugin configured");
    } else {
	printf("no Python plugin available");
	task_methods = new Task();
	return 0;
    }

    if (PYUSABLE && python_plugin->is_callable(TASK_MODULE, TASK_INIT)) {
	python_plugin->call(TASK_MODULE, TASK_INIT, arg, kwarg, retval);
	plugin_status = emcPythonReturnValue(TASK_INIT, retval);
	if (plugin_status != PLUGIN_OK)  // FIXME - NO_CALLABLE?
	    printf("plugin_status = %d '%s'\n'%s'\n", plugin_status,
		   python_plugin->last_errmsg().c_str(),
		   python_plugin->last_exception().c_str());
	if (PYUSABLE && (python_taskinit != NULL)) {
	    try {
		static bp::object result = bp::eval(python_taskinit,
						    python_plugin->main_namespace,
						    python_plugin->main_namespace);
		bp::extract<Task *> typetest(result);
		if (typetest.check()) {
		    task_methods = bp::extract< Task * >(result);
		} else {
		    printf("cant extract a Task instance out of '%s'\n", python_taskinit);
		    task_methods = NULL;
		}
	    } catch( bp::error_already_set ) {
		std::string msg = handle_pyerror();
		printf("eval(%s): %s\n", python_taskinit,msg.c_str());
		PyErr_Clear();
	    }
	    if (task_methods == NULL) {
		printf("no Python Task() instance available\n");
	    }
	}
    } else {
	printf("python_plugin not available\n");
    }
    if (task_methods == NULL)
	task_methods = new Task();
    return 0;
}

int emcRunHalFiles(const char *filename)
{
    IniFile inifile;
    const char *inistring;
    int lineno,status;
    int n = 1;
    pid_t pid;

    if (inifile.Open(filename) == false) {
	return -1;
    }
    while (NULL != (inistring = inifile.Find("POSTTASK_HALFILE", "HAL",
					     n, &lineno))) {
	if ((pid = vfork()) < 0)
	    perror("vfork()");
	else if (pid == 0) {
	    execlp("halcmd", "halcmd","-i",filename,"-f",inistring, NULL);
	    perror("execlp halcmd");
	} else {
	    if ((waitpid (pid, &status, 0) == pid) &&  WEXITSTATUS(status))
		printf("'halcmd -i %s -f %s' exited with  %d\n",
		       filename, inistring, WEXITSTATUS(status));
	}
	n++;
    }
    return 0;
}

// task callables are expected to return an int.
// extract it, and return that
// else complain.
// Also fail with an operator error if we caused an exception.
static int emcPythonReturnValue(const char *funcname, bp::object &retval)
{
    int status = python_plugin->plugin_status();

    if (status == PLUGIN_EXCEPTION) {
	emcOperatorError(status,"emcPythonReturnValue(%s): %s",
			 funcname, python_plugin->last_exception().c_str());
	return -1;
    }
    if ((retval.ptr() != Py_None) &&
	(PyInt_Check(retval.ptr()))) {
	return  bp::extract<int>(retval);
    } else {
	PyObject *res_str = PyObject_Str(retval.ptr());
	emcOperatorError(0, "emcPythonReturnValue(%s): expected int return value, got '%s' (%s)",
			 funcname,
			 PyString_AsString(res_str),
			 retval.ptr()->ob_type->tp_name);
	Py_XDECREF(res_str);
	return -1;
    }
}

int emcPluginCall(EMC_EXEC_PLUGIN_CALL *call_msg)
{
    if (PYUSABLE) {
	bp::object retval;
	bp::object arg = bp::make_tuple(bp::object(call_msg->call));
	bp::dict kwarg;

	python_plugin->call(TASK_MODULE, PLUGIN_CALL, arg, kwarg, retval);
	return emcPythonReturnValue(PLUGIN_CALL, retval);

    } else {
	emcOperatorError(0, "emcPluginCall: Python plugin not initialized");
	return -1;
    }
}

// int emcAbortCleanup(int reason, const char *message)
// {
//     int status = interp.on_abort(reason,message);
//     if (status > INTERP_MIN_ERROR)
// 	print_interp_error(status);
//     return status;
// }

extern "C" void initemctask();
extern "C" void initinterpreter();
extern "C" void initemccanon();
struct _inittab builtin_modules[] = {
    { (char *) "interpreter", initinterpreter },
    { (char *) "emccanon", initemccanon },
    { (char *) "emctask", initemctask },
    // any others...
    { NULL, NULL }
};



Task::Task() {

    IniFile inifile;

    if (inifile.Open(EMC_INIFILE)) {
	use_iocontrol = (inifile.Find("EMCIO", "EMCIO") != NULL);
	use_legacy_tooltable = (inifile.Find("EMCIO", "TOOL_TABLE") != NULL);
	inifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");
    }
    // if (use_iocontrol) {
    // 	for(int i = 0; i < CANON_POCKETS_MAX; i++) {
    // 	    ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
    // 	}
    // 	if(!random_toolchanger) {
    // 	    emcIoStatus->tool.toolTable[0].toolno = -1;
    // 	    ZERO_EMC_POSE(emcIoStatus->tool.toolTable[0].offset);
    // 	    emcIoStatus->tool.toolTable[0].diameter = 0.0;
    // 	    emcIoStatus->tool.toolTable[0].frontangle = 0.0;
    // 	    emcIoStatus->tool.toolTable[0].backangle = 0.0;
    // 	    emcIoStatus->tool.toolTable[0].orientation = 0;
    // 	    fms[0] = 0;
    // 	    ttcomments[0][0] = '\0';
    // 	}

    // 	if (0 != loadToolTable(TOOL_TABLE_FILE, emcIoStatus->tool.toolTable,
    // 			       fms, ttcomments, random_toolchanger)) {
    // 	    rcs_print_error("%s: can't load tool table.\n",progname);
    // 	}
    // 	/* set status values to 'normal' */
    // 	// emcIoStatus->aux.estop = 1; // estop=1 means to emc that ESTOP condition is met
    // 	// emcIoStatus->tool.pocketPrepped = -1;
    // 	// emcIoStatus->tool.toolInSpindle = 0;

    // 	// emcIoStatus->coolant.mist = 0;
    // 	// emcIoStatus->coolant.flood = 0;
    // 	// emcIoStatus->lube.on = 0;
    // 	// emcIoStatus->lube.level = 1;
    // }
};


Task::~Task() {};

// NML commands

int Task::emcIoInit()
{
    EMC_TOOL_INIT ioInitMsg;

    // get NML buffer to emcio
    if (0 != emcioNmlGet()) {
	rcs_print_error("emcioNmlGet() failed.\n");
	return -1;
    }

    if (0 != iniTool(EMC_INIFILE)) {
	return -1;
    }
    // send init command to emcio
    if (forceCommand(&ioInitMsg)) {
	rcs_print_error("Can't forceCommand(ioInitMsg)\n");
	return -1;
    }

    return 0;
}

int Task::emcIoHalt()
{
    EMC_TOOL_HALT ioHaltMsg;

    // send halt command to emcio
    if (emcIoCommandBuffer != 0) {
	forceCommand(&ioHaltMsg);
    }
    // clear out the buffers

    if (emcIoStatusBuffer != 0) {
	delete emcIoStatusBuffer;
	emcIoStatusBuffer = 0;
	emcIoStatus = 0;
    }

    if (emcIoCommandBuffer != 0) {
	delete emcIoCommandBuffer;
	emcIoCommandBuffer = 0;
    }

    if (last_io_command) {
        free(last_io_command);
        last_io_command = 0;
    }

    return 0;
}

int Task::emcIoAbort(int reason)
{
    EMC_TOOL_ABORT ioAbortMsg;

    ioAbortMsg.reason = reason;
    // send abort command to emcio
    sendCommand(&ioAbortMsg);

    // call abort o-word sub handler if defined
    emcAbortCleanup(reason);

    return 0;
}

int Task::emcIoSetDebug(int debug)
{
    EMC_SET_DEBUG ioDebugMsg;

    ioDebugMsg.debug = debug;

    return sendCommand(&ioDebugMsg);
}

int Task::emcAuxEstopOn()
{
    EMC_AUX_ESTOP_ON estopOnMsg;

    return forceCommand(&estopOnMsg);
}

int Task::emcAuxEstopOff()
{
    EMC_AUX_ESTOP_OFF estopOffMsg;

    return forceCommand(&estopOffMsg); //force the EstopOff message
}

int Task::emcCoolantMistOn()
{
    EMC_COOLANT_MIST_ON mistOnMsg;

    sendCommand(&mistOnMsg);

    return 0;
}

int Task::emcCoolantMistOff()
{
    EMC_COOLANT_MIST_OFF mistOffMsg;

    sendCommand(&mistOffMsg);

    return 0;
}

int Task::emcCoolantFloodOn()
{
    EMC_COOLANT_FLOOD_ON floodOnMsg;

    sendCommand(&floodOnMsg);

    return 0;
}

int Task::emcCoolantFloodOff()
{
    EMC_COOLANT_FLOOD_OFF floodOffMsg;

    sendCommand(&floodOffMsg);

    return 0;
}

int Task::emcLubeOn()
{
    EMC_LUBE_ON lubeOnMsg;

    sendCommand(&lubeOnMsg);

    return 0;
}

int Task::emcLubeOff()
{
    EMC_LUBE_OFF lubeOffMsg;

    sendCommand(&lubeOffMsg);

    return 0;
}

int Task::emcToolPrepare(int p, int tool)
{
    EMC_TOOL_PREPARE toolPrepareMsg;

    toolPrepareMsg.pocket = p;
    toolPrepareMsg.tool = tool;
    sendCommand(&toolPrepareMsg);

    return 0;
}


int Task::emcToolStartChange()
{
    EMC_TOOL_START_CHANGE toolStartChangeMsg;

    sendCommand(&toolStartChangeMsg);

    return 0;
}


int Task::emcToolLoad()
{
    EMC_TOOL_LOAD toolLoadMsg;

    sendCommand(&toolLoadMsg);

    return 0;
}

int Task::emcToolUnload()
{
    EMC_TOOL_UNLOAD toolUnloadMsg;

    sendCommand(&toolUnloadMsg);

    return 0;
}

int Task::emcToolLoadToolTable(const char *file)
{
    EMC_TOOL_LOAD_TOOL_TABLE toolLoadToolTableMsg;

    strcpy(toolLoadToolTableMsg.file, file);

    sendCommand(&toolLoadToolTableMsg);

    return 0;
}

int Task::emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
                     double frontangle, double backangle, int orientation)
{
    EMC_TOOL_SET_OFFSET toolSetOffsetMsg;

    toolSetOffsetMsg.pocket = pocket;
    toolSetOffsetMsg.toolno = toolno;
    toolSetOffsetMsg.offset = offset;
    toolSetOffsetMsg.diameter = diameter;
    toolSetOffsetMsg.frontangle = frontangle;
    toolSetOffsetMsg.backangle = backangle;
    toolSetOffsetMsg.orientation = orientation;

    sendCommand(&toolSetOffsetMsg);

    return 0;
}

int Task::emcToolSetNumber(int number)
{
    EMC_TOOL_SET_NUMBER toolSetNumberMsg;

    toolSetNumberMsg.tool = number;

    sendCommand(&toolSetNumberMsg);

    return 0;
}

// Status functions

int Task::emcIoUpdate(EMC_IO_STAT * stat)
{
    if (!use_iocontrol) {
	// there's no message to copy - Python directly operates on emcStatus and its io member
	return 0;
    }
    if (0 == emcIoStatusBuffer || !emcIoStatusBuffer->valid()) {
	return -1;
    }

    switch (emcIoStatusBuffer->peek()) {
    case -1:
	// error on CMS channel
	return -1;
	break;

    case 0:			// nothing new
    case EMC_IO_STAT_TYPE:	// something new
	// drop out to copy
	break;

    default:
	// something else is in there
	return -1;
	break;
    }

    // copy status
    *stat = *emcIoStatus;

    /*
       We need to check that the RCS_DONE isn't left over from the previous
       command, by comparing the command number we sent with the command
       number that emcio echoes. If they're different, then the command
       hasn't been acknowledged yet and the state should be forced to be
       RCS_EXEC. */
    if (stat->echo_serial_number != emcIoCommandSerialNumber) {
	stat->status = RCS_EXEC;
    }
    //commented out because it keeps resetting the spindle speed to some odd value
    //the speed gets set by the IO controller, no need to override it here (io takes care of increase/decrease speed too)
    // stat->spindle.speed = spindleSpeed;

    return 0;
}

int Task::emcIoPluginCall(int len, const char *msg)
{
    printf("iocontrol task: emcIoPluginCall(%d,%s)\n",len,msg);
    return 0;
}


// --------------------------------------------------------------------------------------





// int Task::emcToolPrepare(int p, int tool)
// {
//     // it doesn't make sense to prep the spindle pocket
//     if (random_toolchanger && p == 0)
// 	return 0;

//     /* set tool number first */
//     int prep_number;
//     //*(iocontrol_data->tool_prep_pocket) = p;
//     if (!random_toolchanger && p == 0) {
// 	//	*(iocontrol_data->tool_prep_number) = 0;
// 	prep_number = 0;
//     } else {
// 	// *(iocontrol_data->tool_prep_number) = emcIoStatus->tool.toolTable[p].toolno;
// 	prep_number = emcIoStatus->tool.toolTable[p].toolno;
//     }
//     fprintf(stderr,"emcToolPrepare: raise prepare, prep_number=%d, wait for prepared\n",prep_number);

//     emcIoStatus->tool.pocketPrepped = p; // *(iocontrol_data->tool_prep_pocket); //check if tool has been prepared
//     emcIoStatus->status = RCS_DONE;

//     // *(iocontrol_data->tool_prepare) = 0;

//     // if ((proto > V1) && *(iocontrol_data->toolchanger_faulted)) { // informational
//     // 	rtapi_print_msg(RTAPI_MSG_DBG, "%s: prepare: toolchanger faulted (reason=%d), next M6 will %s\n",
//     // 			progname,toolchanger_reason,
//     // 			toolchanger_reason > 0 ? "set fault code and reason" : "abort program");
//     // }
//     // // then set the prepare pin to tell external logic to get started
//     // *(iocontrol_data->tool_prepare) = 1;
//     // *(iocontrol_data->state) = ST_PREPARING;

//     // // delay fetching the next message until prepare done
//     // if (!(input_status & TI_PREPARE_COMPLETE)) {
//     // 	emcIoStatus->status = RCS_EXEC;
//     // }


//     return 0;
// }


  // virtual int emcToolLoad();
  //   virtual int emcToolUnload();
  //   virtual int emcToolSetNumber(int number);

// int Task::emcToolLoad()
// {

//     fprintf(stderr,"emcToolLoad()\n");

//     // it doesn't make sense to load a tool from the spindle pocket
//     if (random_toolchanger && emcIoStatus->tool.pocketPrepped == 0) {
// 	return 0;
//     }

//     // it's not necessary to load the tool already in the spindle
//     if (!random_toolchanger && emcIoStatus->tool.pocketPrepped > 0 &&
// 	emcIoStatus->tool.toolInSpindle == emcIoStatus->tool.toolTable[emcIoStatus->tool.pocketPrepped].toolno) {
// 		return 0;
//     }

//     if (emcIoStatus->tool.pocketPrepped != -1) {
// 	fprintf(stderr,"emcToolLoad() raise change, wait for changed\n");

// 	// Assume changed=true:
//         if(!random_toolchanger && emcIoStatus->tool.pocketPrepped == 0) {
//             emcIoStatus->tool.toolInSpindle = 0;
//         } else {
//             // the tool now in the spindle is the one that was prepared
//             emcIoStatus->tool.toolInSpindle = emcIoStatus->tool.toolTable[emcIoStatus->tool.pocketPrepped].toolno;
//         }
// 	// *(iocontrol_data->tool_number) = emcIoStatus->tool.toolInSpindle; //likewise in HAL
// 	load_tool(emcIoStatus->tool.pocketPrepped);
// 	emcIoStatus->tool.pocketPrepped = -1; //reset the tool preped number, -1 to permit tool 0 to be loaded
// 	// *(iocontrol_data->tool_prep_number) = 0; //likewise in HAL
// 	// *(iocontrol_data->tool_prep_pocket) = 0; //likewise in HAL
// 	// *(iocontrol_data->tool_change) = 0; //also reset the tool change signal
// 	emcIoStatus->status = RCS_DONE;	// we finally finished to do tool-changing, signal task with RCS_DONE
// 	emcIoStatus->tool.pocketPrepped = -1; // reset the tool prepped number, -1 to permit tool 0 to be loaded

//     } else {
// 	fprintf(stderr,"emcToolLoad() no pocket prepped, failing\n");
// 	emcIoStatus->status = RCS_ERROR;

//     }
//     return 0;
// }

// int Task::emcToolUnload()
// {

//     fprintf(stderr,"emcToolUnLoad()\n");
//     emcIoStatus->status = RCS_DONE;

//     return 0;
// }

// int Task::emcToolSetNumber(int number)
// {
//     fprintf(stderr,"emcToolSetNumber(%d)\n",number);
//     if (number == 0) {
// 	emcIoStatus->tool.toolInSpindle = 0;
// 	emcIoStatus->tool.pocketPrepped = -1; //????? reset the tool prepped number, -1 to permit tool 0 to be loaded
//     } else {
// 	emcIoStatus->tool.toolInSpindle = emcIoStatus->tool.toolTable[number].toolno;
//     }
//     load_tool(number);
//     emcIoStatus->status = RCS_DONE;
//     return 0;
// }

void Task::load_tool(int pocket) {
    if(random_toolchanger) {
	// swap the tools between the desired pocket and the spindle pocket
	CANON_TOOL_TABLE temp;
	char *comment_temp;

	temp = emcIoStatus->tool.toolTable[0];
	emcIoStatus->tool.toolTable[0] = emcIoStatus->tool.toolTable[pocket];
	emcIoStatus->tool.toolTable[pocket] = temp;

	comment_temp = ttcomments[0];
	ttcomments[0] = ttcomments[pocket];
	ttcomments[pocket] = comment_temp;

	if (0 != saveToolTable(TOOL_TABLE_FILE, emcIoStatus->tool.toolTable))
	    emcIoStatus->status = RCS_ERROR;
    } else if (pocket == 0) {
	// magic T0 = pocket 0 = no tool
	emcIoStatus->tool.toolTable[0].toolno = -1;
	ZERO_EMC_POSE(emcIoStatus->tool.toolTable[0].offset);
	emcIoStatus->tool.toolTable[0].diameter = 0.0;
	emcIoStatus->tool.toolTable[0].frontangle = 0.0;
	emcIoStatus->tool.toolTable[0].backangle = 0.0;
	emcIoStatus->tool.toolTable[0].orientation = 0;
    } else {
	// just copy the desired tool to the spindle
	emcIoStatus->tool.toolTable[0] = emcIoStatus->tool.toolTable[pocket];
    }
}

/********************************************************************
 *
 * Description: saveToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
 *		Saves the tool table from toolTable[] array into file filename.
 *		  Array is CANON_TOOL_MAX + 1 entries, since 0 is included.
 *
 * Return Value: Zero on success or -1 if file not found.
 *
 * Side Effects: Default setting used if the parameter not found in
 *		the ini file.
 *
 * Called By: main()
 *
 ********************************************************************/
int Task::saveToolTable(const char *filename,
			 CANON_TOOL_TABLE toolTable[])
{
    int pocket;
    FILE *fp;
    const char *name;
    int start_pocket;

    // check filename
    if (filename[0] == 0) {
	name = TOOL_TABLE_FILE;
    } else {
	// point to name provided
	name = filename;
    }

    // open tool table file
    if (NULL == (fp = fopen(name, "w"))) {
	// can't open file
	return -1;
    }

    if(random_toolchanger) {
	start_pocket = 0;
    } else {
	start_pocket = 1;
    }
    for (pocket = start_pocket; pocket < CANON_POCKETS_MAX; pocket++) {
	if (toolTable[pocket].toolno != -1) {
	    fprintf(fp, "T%d P%d", toolTable[pocket].toolno, random_toolchanger? pocket: fms[pocket]);
	    if (toolTable[pocket].diameter) fprintf(fp, " D%f", toolTable[pocket].diameter);
	    if (toolTable[pocket].offset.tran.x) fprintf(fp, " X%+f", toolTable[pocket].offset.tran.x);
	    if (toolTable[pocket].offset.tran.y) fprintf(fp, " Y%+f", toolTable[pocket].offset.tran.y);
	    if (toolTable[pocket].offset.tran.z) fprintf(fp, " Z%+f", toolTable[pocket].offset.tran.z);
	    if (toolTable[pocket].offset.a) fprintf(fp, " A%+f", toolTable[pocket].offset.a);
	    if (toolTable[pocket].offset.b) fprintf(fp, " B%+f", toolTable[pocket].offset.b);
	    if (toolTable[pocket].offset.c) fprintf(fp, " C%+f", toolTable[pocket].offset.c);
	    if (toolTable[pocket].offset.u) fprintf(fp, " U%+f", toolTable[pocket].offset.u);
	    if (toolTable[pocket].offset.v) fprintf(fp, " V%+f", toolTable[pocket].offset.v);
	    if (toolTable[pocket].offset.w) fprintf(fp, " W%+f", toolTable[pocket].offset.w);
	    if (toolTable[pocket].frontangle) fprintf(fp, " I%+f", toolTable[pocket].frontangle);
	    if (toolTable[pocket].backangle) fprintf(fp, " J%+f", toolTable[pocket].backangle);
	    if (toolTable[pocket].orientation) fprintf(fp, " Q%d", toolTable[pocket].orientation);
	    // FIXME   fprintf(fp, " ;%s\n", ttcomments[pocket]);
	    fprintf(fp, "\n");// , ttcomments[pocket]);
	}
    }

    fclose(fp);
    return 0;
}
