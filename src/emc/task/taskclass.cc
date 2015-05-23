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

#include "rtapi_math.h"		// fabs()
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
#define TASK_VAR "pytask"
#define PLUGIN_CALL "plugin_call"

extern PythonPlugin *python_plugin;  // exported by python_plugin.cc
#define PYUSABLE (((python_plugin) != NULL) && (python_plugin->usable()))
extern int return_int(const char *funcname, bp::object &retval);
Task *task_methods;

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
				    emc_nmlfile);
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
	    new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emc", emc_nmlfile);
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
				     emc_nmlfile);
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
	    new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emc", emc_nmlfile);
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

int emcIoHalt() {
    try {
	return task_methods->emcIoHalt();
    } catch( bp::error_already_set ) {
	std::string msg = handle_pyerror();
	rcs_print("emcIoHalt(): %s\n", msg.c_str());
	PyErr_Clear();
	return -1;
    }
}


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
int emcIoPluginCall(EMC_IO_PLUGIN_CALL *call_msg) { return task_methods->emcIoPluginCall(call_msg->len,
											   call_msg->call); }
static const char *instance_name = "task_instance";

int emcTaskOnce(const char *filename)
{
    bp::object retval;
    bp::tuple arg;
    bp::dict kwarg;

    // initialize the Python plugin singleton
    // Interp is already instantiated but not yet fully configured
    // both Task and Interp use it - first to call configure() instantiates the Python part
    // NB: the interpreter.this global will appear only after Interp.init()

    extern struct _inittab builtin_modules[];
    if (!PythonPlugin::instantiate(builtin_modules)) {
	rcs_print("emcTaskOnce: cant instantiate Python plugin\n");
	goto no_pytask;
    }
    if (python_plugin->configure(filename, "PYTHON") == PLUGIN_OK) {
	if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
	    rcs_print("emcTaskOnce: Python plugin configured\n");
	}
    } else {
	goto no_pytask;
    }
    if (PYUSABLE) {
	// extract the instance of Python Task()
	try {
	    bp::object task_namespace =  python_plugin->main_namespace[TASK_MODULE].attr("__dict__");;
	    bp::object result = task_namespace[TASK_VAR];
	    bp::extract<Task *> typetest(result);
	    if (typetest.check()) {
		task_methods = bp::extract< Task * >(result);
	    } else {
		rcs_print("cant extract a Task instance out of '%s'\n", instance_name);
		task_methods = NULL;
	    }
	} catch( bp::error_already_set ) {
	    std::string msg = handle_pyerror();
	    if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
		// this really just means the task python backend wasnt configured.
		rcs_print("emcTaskOnce: extract(%s): %s\n", instance_name, msg.c_str());
	    }
	    PyErr_Clear();
	}
    }
 no_pytask:
    if (task_methods == NULL) {
	if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
	    rcs_print("emcTaskOnce: no Python Task() instance available, using default iocontrol-based task methods\n");
	}
	task_methods = new Task();
    }
    return 0;
}

// if using a Python-based HAL module in task, normal HAL_FILE's are run too early.
// execute those here if specified via POSTTASK_HALFILE in ini ,
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
		rcs_print("'halcmd -i %s -f %s' exited with  %d\n",
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
int return_int(const char *funcname, PyObject *retval)
{
    int status = python_plugin->plugin_status();

    if (status == PLUGIN_EXCEPTION) {
	emcOperatorError(status,"return_int(%s): %s",
			 funcname, python_plugin->last_exception().c_str());
	return -1;
    }
    if ((retval != Py_None) &&
	(PyInt_Check(retval))) {
	return PyInt_AS_LONG(retval);
    } else {
	emcOperatorError(0, "return_int(%s): expected int return value, got '%s' (%s)",
			 funcname,
			 PyString_AsString(retval),
			 retval->ob_type->tp_name);
	Py_XDECREF(retval);
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
	return return_int(PLUGIN_CALL, retval.ptr());

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



Task::Task() : use_iocontrol(0), random_toolchanger(0) {

    IniFile inifile;

    ini_filename = emc_inifile;

    if (inifile.Open(ini_filename)) {
	use_iocontrol = (inifile.Find("EMCIO", "EMCIO") != NULL);
	inifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");
	const char *t;
	if ((t = inifile.Find("TOOL_TABLE", "EMCIO")) != NULL)
	    tooltable_filename = strdup(t);
    }
    if (!use_iocontrol) {
	for(int i = 0; i < CANON_POCKETS_MAX; i++) {
	    ttcomments[i] = (char *)malloc(CANON_TOOL_ENTRY_LEN);
	}
    }

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

    if (0 != iniTool(emc_inifile)) {
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
    if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
	rcs_print("emcIoPluginCall(%d,%s) - no Python handler set\n",len,msg);
    }
    return 0;
}



