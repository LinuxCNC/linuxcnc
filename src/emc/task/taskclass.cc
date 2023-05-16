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
#include "emcglb.h"		// EMC_INIFILE

#include "python_plugin.hh"
#include "taskclass.hh"
#include <rtapi_string.h>

#define BOOST_PYTHON_MAX_ARITY 4
#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/object.hpp>
#include <boost/python/tuple.hpp>
namespace bp = boost::python;

#include "tooldata.hh"
#include "hal.h"

struct iocontrol_str {
    hal_bit_t *user_enable_out;        /* output, TRUE when EMC wants stop */
    hal_bit_t *emc_enable_in;        /* input, TRUE on any external stop */
    hal_bit_t *user_request_enable;        /* output, used to reset ENABLE latch */
    hal_bit_t *coolant_mist;        /* coolant mist output pin */
    hal_bit_t *coolant_flood;        /* coolant flood output pin */
    hal_bit_t *lube;                /* lube output pin */
    hal_bit_t *lube_level;        /* lube level input pin */

    // the following pins are needed for toolchanging
    //tool-prepare
    hal_bit_t *tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    hal_s32_t *tool_prep_pocket;/* output, pin that holds the pocketno for the tool table entry matching the tool to be prepared,
                                   only valid when tool-prepare=TRUE */
    hal_s32_t *tool_from_pocket;/* output, pin indicating pocket current load tool retrieved from*/
    hal_s32_t *tool_prep_index; /* output, pin for internal index (idx) of prepped tool above */
    hal_s32_t *tool_prep_number;/* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_s32_t *tool_number;     /* output, pin that holds the tool number currently in the spindle */
    hal_bit_t *tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    //tool-change
    hal_bit_t *tool_change;        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_bit_t *tool_changed;        /* input, notifies tool has been changed */

    // note: spindle control has been moved to motion
} * iocontrol_data;                        //pointer to the HAL-struct

static int comp_id;                                /* component ID */

/********************************************************************
*
* Description: iocontrol_hal_init(void)
*
* Side Effects: Exports HAL pins.
*
* Called By: main
********************************************************************/
int Task::iocontrol_hal_init(void)
{
    int n = 0, retval;                //n - number of the hal component (only one for iocotrol)

    /* STEP 1: initialise the hal component */
    comp_id = hal_init("iocontrol");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: hal_init() failed\n");
        return -1;
    }

    /* STEP 2: allocate shared memory for iocontrol data */
    iocontrol_data = (iocontrol_str *) hal_malloc(sizeof(iocontrol_str));
    if (iocontrol_data == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "IOCONTROL: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    // user-enable-out
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->user_enable_out), comp_id,
                              "iocontrol.%d.user-enable-out", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-enable-out export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // user-request-enable
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->user_request_enable), comp_id,
                             "iocontrol.%d.user-request-enable", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin user-request-enable export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // coolant-flood
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->coolant_flood), comp_id,
                         "iocontrol.%d.coolant-flood", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-flood export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // coolant-mist
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->coolant_mist), comp_id,
                              "iocontrol.%d.coolant-mist", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin coolant-mist export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // lube
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->lube), comp_id,
                              "iocontrol.%d.lube", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prepare
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->tool_prepare), comp_id,
                              "iocontrol.%d.tool-prepare", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepare export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-number
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_number), comp_id,
                              "iocontrol.%d.tool-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-number export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prep-number
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_number), comp_id,
                              "iocontrol.%d.tool-prep-number", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-number export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    // tool-prep-index (idx)
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_index), comp_id,
                              "iocontrol.%d.tool-prep-index", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-index export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    // tool-prep-pocket
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_prep_pocket), comp_id,
                              "iocontrol.%d.tool-prep-pocket", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prep-pocket export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-from-pocket
    retval = hal_pin_s32_newf(HAL_OUT, &(iocontrol_data->tool_from_pocket), comp_id,
                              "iocontrol.%d.tool-from-pocket", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-from-pocket export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-prepared
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->tool_prepared), comp_id,
                              "iocontrol.%d.tool-prepared", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-prepared export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-change
    retval = hal_pin_bit_newf(HAL_OUT, &(iocontrol_data->tool_change), comp_id,
                              "iocontrol.%d.tool-change", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-change export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // tool-changed
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->tool_changed), comp_id,
                        "iocontrol.%d.tool-changed", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin tool-changed export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    /* STEP 3b: export the in-pin(s) */

    // emc-enable-in
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->emc_enable_in), comp_id,
                             "iocontrol.%d.emc-enable-in", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin emc-enable-in export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }
    // lube_level
    retval = hal_pin_bit_newf(HAL_IN, &(iocontrol_data->lube_level), comp_id,
                             "iocontrol.%d.lube_level", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "IOCONTROL: ERROR: iocontrol %d pin lube_level export failed with err=%i\n",
                        n, retval);
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

/********************************************************************
*
* Description: hal_init_pins(void)
*
* Side Effects: Sets HAL pins default values.
*
* Called By: main
********************************************************************/
static void hal_init_pins(void)
{
    *(iocontrol_data->user_enable_out)=0;    /* output, FALSE when EMC wants stop */
    *(iocontrol_data->user_request_enable)=0;/* output, used to reset HAL latch */
    *(iocontrol_data->coolant_mist)=0;       /* coolant mist output pin */
    *(iocontrol_data->coolant_flood)=0;      /* coolant flood output pin */
    *(iocontrol_data->lube)=0;               /* lube output pin */
    *(iocontrol_data->tool_prepare)=0;       /* output, pin that notifies HAL it needs to prepare a tool */
    *(iocontrol_data->tool_prep_number)=0;   /* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    *(iocontrol_data->tool_prep_pocket)=0;   /* output, pin that holds the pocketno for the tool to be prepared, only valid when tool-prepare=TRUE */
    *(iocontrol_data->tool_from_pocket)=0;   /* output, always 0 at startup */
    *iocontrol_data->tool_prep_index=0;      /* output, pin that holds the internal index (idx) of the tool to be prepared, for debug */
    *(iocontrol_data->tool_change)=0;        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
}


// Python plugin interface
#define TASK_MODULE "task"
#define TASK_VAR "pytask"
#define PLUGIN_CALL "plugin_call"

extern PythonPlugin *python_plugin;  // exported by python_plugin.cc
#define PYUSABLE (((python_plugin) != NULL) && (python_plugin->usable()))
extern int return_int(const char *funcname, bp::object &retval);
Task *task_methods;

// global status structure
EMC_IO_STAT *emcIoStatus = 0;

// glue

int emcIoInit() { return task_methods->emcIoInit(); }

int emcIoHalt() {
    try {
	return task_methods->emcIoHalt();
    } catch( bp::error_already_set &) {
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
int emcToolPrepare(int tool) { return task_methods->emcToolPrepare(tool); }
int emcToolStartChange() { return task_methods->emcToolStartChange(); }
int emcToolLoad() { return task_methods->emcToolLoad(); }
int emcToolUnload()  { return task_methods->emcToolUnload(); }
int emcToolLoadToolTable(const char *file) { return task_methods->emcToolLoadToolTable(file); }
int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
                     double frontangle, double backangle, int orientation) {
    return task_methods->emcToolSetOffset( pocket,  toolno,  offset,  diameter,
					   frontangle,  backangle,  orientation); }
int emcToolSetNumber(int number) { return task_methods->emcToolSetNumber(number); }
int emcIoPluginCall(EMC_IO_PLUGIN_CALL *call_msg) { return task_methods->emcIoPluginCall(call_msg->len,
											   call_msg->call); }
static const char *instance_name = "task_instance";

int emcTaskOnce(const char *filename, EMC_IO_STAT &emcioStatus)
{
    // initialize the Python plugin singleton
    // Interp is already instantiated but not yet fully configured
    // both Task and Interp use it - first to call configure() instantiates the Python part
    // NB: the interpreter.this global will appear only after Interp.init()

    extern struct _inittab builtin_modules[];
    if (!PythonPlugin::instantiate(builtin_modules)) {
	rcs_print("emcTaskOnce: can\'t instantiate Python plugin\n");
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
		rcs_print("can\'t extract a Task instance out of '%s'\n", instance_name);
		task_methods = NULL;
	    }
	} catch(bp::error_already_set &) {
	    std::string msg = handle_pyerror();
	    if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
		// this really just means the task python backend wasn't configured.
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
	task_methods = new Task(emcioStatus);
    if (int res = task_methods->iocontrol_hal_init()) {
        return res;
    }
    *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle;//TODO: move
    }
    return 0;
}

// If using a Python-based HAL module in task, normal HAL_FILE's are run too early.
// Execute those here if specified via POSTTASK_HALFILE in INI.
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
    (PyLong_Check(retval))) {
    return PyLong_AsLong(retval);
    } else {
	emcOperatorError(0, "return_int(%s): expected int return value, got '%s' (%s)",
			 funcname,
            PyBytes_AsString(retval),
            Py_TYPE(retval)->tp_name);
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

extern "C" PyObject* PyInit_interpreter(void);
extern "C" PyObject* PyInit_emccanon(void);
struct _inittab builtin_modules[] = {
    { "interpreter", PyInit_interpreter },
    { "emccanon", PyInit_emccanon },
    // any others...
    { NULL, NULL }
};

Task::Task(EMC_IO_STAT & emcioStatus_in) :
    emcioStatus(emcioStatus_in),
    random_toolchanger(0),
    ini_filename(emc_inifile)
    {

    IniFile inifile;

    ini_filename = emc_inifile;

    if (inifile.Open(ini_filename)) {
	inifile.Find(&random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");
	const char *t;
	if ((t = inifile.Find("TOOL_TABLE", "EMCIO")) != NULL)
	    tooltable_filename = strdup(t);
    }
	tool_mmap_creator((EMC_TOOL_STAT*)&(emcioStatus.tool), random_toolchanger);
#ifdef TOOL_NML //{
    tool_nml_register( (CANON_TOOL_TABLE*)&emcStatus->io.tool.toolTable);
#else //}{
    tool_mmap_user();
    // initialize database tool finder:
#endif //}
    emcioStatus.status = RCS_DONE;//TODO??
    tooldata_init(random_toolchanger);
    emcioStatus.tool.pocketPrepped = -1;
    if(!random_toolchanger) {
        CANON_TOOL_TABLE tdata = tooldata_entry_init();
        tdata.pocketno =  0; //nonrandom init
        tdata.toolno   = -1; //nonrandom init
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }
    }
    if (0 != tooldata_load(tooltable_filename)) {
        rcs_print_error("can't load tool table.\n");
    }

    if (random_toolchanger) {
        CANON_TOOL_TABLE tdata;
        if (tooldata_get(&tdata,0) != IDX_OK) {
            UNEXPECTED_MSG;//todo: handle error
        }
        emcioStatus.tool.toolInSpindle = tdata.toolno;
    } else {
        emcioStatus.tool.toolInSpindle = 0;
    }    
};


Task::~Task() {};

// set the have_tool_change_position global
static int readToolChange(IniFile *toolInifile)
{
    int retval = 0;
    const char *inistring;

    if (NULL !=
	(inistring = toolInifile->Find("TOOL_CHANGE_POSITION", "EMCIO"))) {
	/* found an entry */
        if (9 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
                        &tool_change_position.tran.x,
                        &tool_change_position.tran.y,
                        &tool_change_position.tran.z,
                        &tool_change_position.a,
                        &tool_change_position.b,
                        &tool_change_position.c,
                        &tool_change_position.u,
                        &tool_change_position.v,
                        &tool_change_position.w)) {
            have_tool_change_position=9;
            retval=0;
        } else if (6 == sscanf(inistring, "%lf %lf %lf %lf %lf %lf",
                        &tool_change_position.tran.x,
                        &tool_change_position.tran.y,
                        &tool_change_position.tran.z,
                        &tool_change_position.a,
                        &tool_change_position.b,
                        &tool_change_position.c)) {
	    tool_change_position.u = 0.0;
	    tool_change_position.v = 0.0;
	    tool_change_position.w = 0.0;
            have_tool_change_position = 6;
            retval = 0;
        } else if (3 == sscanf(inistring, "%lf %lf %lf",
                               &tool_change_position.tran.x,
                               &tool_change_position.tran.y,
                               &tool_change_position.tran.z)) {
	    /* read them OK */
	    tool_change_position.a = 0.0;
	    tool_change_position.b = 0.0;
	    tool_change_position.c = 0.0;
	    tool_change_position.u = 0.0;
	    tool_change_position.v = 0.0;
	    tool_change_position.w = 0.0;
	    have_tool_change_position = 3;
	    retval = 0;
	} else {
	    /* bad format */
	    rcs_print("bad format for TOOL_CHANGE_POSITION\n");
	    have_tool_change_position = 0;
	    retval = -1;
	}
    } else {
	/* didn't find an entry */
	have_tool_change_position = 0;
    }
    return retval;
}

static int iniTool(const char *filename)
{
    int retval = 0;
    IniFile toolInifile;

    if (toolInifile.Open(filename) == false) {
	return -1;
    }
    // read the tool change positions
    if (0 != readToolChange(&toolInifile)) {
	retval = -1;
    }
    // close the inifile
    toolInifile.Close();

    return retval;
}

void Task::load_tool(int idx) {
    CANON_TOOL_TABLE tdata;
    if(random_toolchanger) {
        // swap the tools between the desired pocket and the spindle pocket

        CANON_TOOL_TABLE tzero,tpocket;
        if (   tooldata_get(&tzero,0    ) != IDX_OK
            || tooldata_get(&tpocket,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        // spindle-->pocket (specified by idx)
        tooldata_db_notify(SPINDLE_UNLOAD,tzero.toolno,idx,tzero);
        tzero.pocketno = tpocket.pocketno;
        if (tooldata_put(tzero,idx) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        // pocket-->spindle (idx==0)
        tooldata_db_notify(SPINDLE_LOAD,tpocket.toolno,0,tpocket);
        tpocket.pocketno = 0;
        if (tooldata_put(tpocket,0) != IDX_OK) {
            UNEXPECTED_MSG;
        }

        if (0 != tooldata_save(tooltable_filename)) {
            emcioStatus.status = RCS_ERROR;
        }
    } else if(idx == 0) {
        // on non-random tool-changers, asking for pocket 0 is the secret
        // handshake for "unload the tool from the spindle"
        tdata = tooldata_entry_init();
        tdata.toolno   = 0; // nonrandom unload tool from spindle
        tdata.pocketno = 0; // nonrandom unload tool from spindle
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_db_notify(SPINDLE_UNLOAD,0,0,tdata)) { UNEXPECTED_MSG; }
    } else {
        // just copy the desired tool to the spindle
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if (tooldata_put(tdata,0) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        // notify idx==0 tool in spindle:
        CANON_TOOL_TABLE temp;
        if (tooldata_get(&temp,0) != IDX_OK) { UNEXPECTED_MSG; }
        if (tooldata_db_notify(SPINDLE_LOAD,temp.toolno,0,temp)) { UNEXPECTED_MSG; }
    }
} // load_tool()

void Task::reload_tool_number(int toolno) {
    CANON_TOOL_TABLE tdata;
    if(random_toolchanger) return; // doesn't need special handling here
    for(int idx = 1; idx <= tooldata_last_index_get(); idx++) { //note <=
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG; return;
        }
        if(tdata.toolno == toolno) {
            load_tool(idx);
            break;
        }
    }
}

// NML commands

int Task::emcIoInit()//EMC_TOOL_INIT
{
    tooldata_load(tooltable_filename);
    reload_tool_number(emcioStatus.tool.toolInSpindle);

    if (0 != iniTool(emc_inifile)) {
	return -1;
    }
    return 0;
}

int Task::emcIoHalt()
{

    return 0;
}

int Task::emcIoAbort(int reason)//EMC_TOOL_ABORT_TYPE
{
    // only used in v2
    // this gets sent on any Task Abort, so it might be safer to stop
    // the spindle  and coolant
    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
    emcioStatus.coolant.mist = 0;
    emcioStatus.coolant.flood = 0;
    *(iocontrol_data->coolant_mist)=0;                /* coolant mist output pin */
    *(iocontrol_data->coolant_flood)=0;                /* coolant flood output pin */
    *(iocontrol_data->tool_change)=0;                /* abort tool change if in progress */
    *(iocontrol_data->tool_prepare)=0;                /* abort tool prepare if in progress */
    return 0;
}

int Task::emcIoSetDebug(int debug)//EMC_SET_DEBUG
{
    return 0;
}

int Task::emcAuxEstopOn()//EMC_AUX_ESTOP_ON_TYPE
{
    /* assert an ESTOP to the outside world (thru HAL) */
    *(iocontrol_data->user_enable_out) = 0; //disable on ESTOP_ON
    hal_init_pins(); //resets all HAL pins to safe valuea
    return 0;
}

int Task::emcAuxEstopOff()
{
    /* remove ESTOP */
    *(iocontrol_data->user_enable_out) = 1; //we're good to enable on ESTOP_OFF
    /* generate a rising edge to reset optional HAL latch */
    *(iocontrol_data->user_request_enable) = 1;
    emcioStatus.aux.estop = 0;
    return 0;
}

int Task::emcCoolantMistOn()
{
    emcioStatus.coolant.mist = 1;
    *(iocontrol_data->coolant_mist) = 1;
    return 0;
}

int Task::emcCoolantMistOff()
{
    emcioStatus.coolant.mist = 0;
    *(iocontrol_data->coolant_mist) = 0;
    return 0;
}

int Task::emcCoolantFloodOn()
{
    emcioStatus.coolant.flood = 1;
    *(iocontrol_data->coolant_flood) = 1;
    return 0;
}

int Task::emcCoolantFloodOff()
{
    emcioStatus.coolant.flood = 0;
    *(iocontrol_data->coolant_flood) = 0;
    return 0;
}

int Task::emcLubeOn()
{
    emcioStatus.lube.on = 1;
    *(iocontrol_data->lube) = 1;
    return 0;
}

int Task::emcLubeOff()
{
    emcioStatus.lube.on = 0;
    *(iocontrol_data->lube) = 0;
    return 0;
}

int Task::emcToolPrepare(int toolno)
{
    int idx = 0;
    CANON_TOOL_TABLE tdata;
    idx  = tooldata_find_index_for_tool(toolno);
#ifdef TOOL_NML
    if (!random_toolchanger && toolno == 0) { idx = 0; }
#endif
    if (idx == -1) {  // not found
        emcioStatus.tool.pocketPrepped = -1;
    } else {
        if (tooldata_get(&tdata,idx) != IDX_OK) {
            UNEXPECTED_MSG;
        }
        rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d idx=%d\n", toolno, idx);

        *(iocontrol_data->tool_prep_index)  = idx; // any type of changer

        // Note: some of the following logic could be simplified
        //       but is maintained to preserve runtests expectations
        // Set HAL pins for tool number, pocket, and index.
        if (random_toolchanger) {
            // RANDOM_TOOLCHANGER
            *(iocontrol_data->tool_prep_number) = tdata.toolno;
            if (idx == 0) {
                emcioStatus.tool.pocketPrepped      = 0; // pocketPrepped is an idx
                *(iocontrol_data->tool_prep_pocket) = 0;
                return 0;
            }
            *(iocontrol_data->tool_prep_pocket) = tdata.pocketno;
        } else {
            // NON_RANDOM_TOOLCHANGER
            if (idx == 0) {
                emcioStatus.tool.pocketPrepped      = 0; // pocketPrepped is an idx
                *(iocontrol_data->tool_prep_number) = 0;
                *(iocontrol_data->tool_prep_pocket) = 0;
            } else {
                *(iocontrol_data->tool_prep_number) = tdata.toolno;
                *(iocontrol_data->tool_prep_pocket) = tdata.pocketno;
            }
        }
        // it doesn't make sense to prep the spindle pocket
        if (random_toolchanger && idx == 0) {
            emcioStatus.tool.pocketPrepped = 0; // idx
            return 0;
        }
    }

    /* then set the prepare pin to tell external logic to get started */
    *(iocontrol_data->tool_prepare) = 1;
    // the feedback logic is done inside read_hal_inputs()
    // we only need to set RCS_EXEC if RCS_DONE is not already set by the above logic
    if (tool_status != 10) //set above to 10 in case PREP already finished (HAL loopback machine)
        emcioStatus.status = RCS_EXEC;
    return 0;
}


int Task::emcToolStartChange()//EMC_TOOL_START_CHANGE_TYPE
{
    return 0;
}


int Task::emcToolLoad()//EMC_TOOL_LOAD_TYPE
{
    // it doesn't make sense to load a tool from the spindle pocket
    if (random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
        return 0;
    }

    // it's not necessary to load the tool already in the spindle
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata, emcioStatus.tool.pocketPrepped) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    if (!random_toolchanger && (emcioStatus.tool.pocketPrepped > 0) &&
        (emcioStatus.tool.toolInSpindle == tdata.toolno) ) {
        return 0;
    }

    if (emcioStatus.tool.pocketPrepped != -1) {
        //notify HW for toolchange
        *(iocontrol_data->tool_change) = 1;
        // the feedback logic is done inside read_hal_inputs() we only
        // need to set RCS_EXEC if RCS_DONE is not already set by the
        // above logic
        if (tool_status != 11)
            // set above to 11 in case LOAD already finished (HAL
            // loopback machine)
            emcioStatus.status = RCS_EXEC;
    }
    return 0;
}

int Task::emcToolUnload()//EMC_TOOL_UNLOAD_TYPE
{
    emcioStatus.tool.toolInSpindle = 0;
    return 0;
}

int Task::emcToolLoadToolTable(const char *file)//EMC_TOOL_LOAD_TOOL_TABLE_TYPE
{
    //error handler?
    tooldata_load(file);
    reload_tool_number(emcioStatus.tool.toolInSpindle);
    return 0;
}

int Task::emcToolSetOffset(int idx, int toolno, EmcPose offset, double diameter,
                     double frontangle, double backangle, int orientation)//EMC_TOOL_SET_OFFSET
{

    int o;
    double d, f, b;

    d = diameter;
    f = frontangle;
    b = backangle;
    o = orientation;

    rtapi_print_msg(RTAPI_MSG_DBG,
            "EMC_TOOL_SET_OFFSET idx=%d toolno=%d zoffset=%lf, "
            "xoffset=%lf, diameter=%lf, "
            "frontangle=%lf, backangle=%lf, orientation=%d\n",
            idx, toolno, offset.tran.z, offset.tran.x, d, f, b, o);
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    tdata.toolno = toolno;
    tdata.offset = offset;
    tdata.diameter = d;
    tdata.frontangle = f;
    tdata.backangle = b;
    tdata.orientation = o;
    if (tooldata_put(tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    if (0 != tooldata_save(tooltable_filename)) {
        emcioStatus.status = RCS_ERROR;
    }
    //TODO
    // if (io_db_mode == DB_ACTIVE) {
    //     int pno = idx; // for random_toolchanger
    //     if (!random_toolchanger) { pno = tdata.pocketno; }
    //     if (tooldata_db_notify(TOOL_OFFSET,toolno,pno,tdata)) {
    //         UNEXPECTED_MSG;
    //     }
    // }

    return 0;
}

int Task::emcToolSetNumber(int number)//EMC_TOOL_SET_NUMBER
{
    int idx;

    idx = number;//TODO: should be toolno
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    load_tool(idx);

    idx=0; // update spindle (fix legacy behavior)
    if (tooldata_get(&tdata,idx) != IDX_OK) {
        UNEXPECTED_MSG;
    }
    emcioStatus.tool.toolInSpindle = tdata.toolno;
    rtapi_print_msg(RTAPI_MSG_DBG,
            "EMC_TOOL_SET_NUMBER old_loaded_tool=%d new_idx_number=%d new_tool=%d\n"
            , emcioStatus.tool.toolInSpindle, idx, tdata.toolno);
    //likewise in HAL
    *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle;
    if (emcioStatus.tool.toolInSpindle == 0) {
        emcioStatus.tool.toolFromPocket =  *(iocontrol_data->tool_from_pocket) = 0; // no tool in spindle
    }

    return 0;
}

/********************************************************************
*
* Description: read_tool_inputs(void)
*                        Reads the tool-pin values from HAL
*                        this function gets called once per cycle
*                        It sets the values for the emcioStatus.aux.*
*
* Returns:        returns which of the status has changed
*                we then need to update through NML (a bit different as read_hal_inputs)
*
* Side Effects: updates values
*
* Called By: main every CYCLE
********************************************************************/
int Task::read_tool_inputs(void)
{
    if (*iocontrol_data->tool_prepare && *iocontrol_data->tool_prepared) {
        emcioStatus.tool.pocketPrepped = *iocontrol_data->tool_prep_index; //check if tool has been (idx) prepared
        *(iocontrol_data->tool_prepare) = 0;
        emcioStatus.status = RCS_DONE;  // we finally finished to do tool-changing, signal task with RCS_DONE
        return 10; //prepped finished
    }

    if (*iocontrol_data->tool_change && *iocontrol_data->tool_changed) {
        if(!random_toolchanger && emcioStatus.tool.pocketPrepped == 0) {
            emcioStatus.tool.toolInSpindle = 0;
            emcioStatus.tool.toolFromPocket  =  *(iocontrol_data->tool_from_pocket) = 0;
        } else {
            // the tool now in the spindle is the one that was prepared
            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata,emcioStatus.tool.pocketPrepped) != IDX_OK) {
                UNEXPECTED_MSG; return -1;
            }
            emcioStatus.tool.toolInSpindle = tdata.toolno;
            emcioStatus.tool.toolFromPocket = *(iocontrol_data->tool_from_pocket) = tdata.pocketno;
        }
        if (emcioStatus.tool.toolInSpindle == 0) {
             emcioStatus.tool.toolFromPocket =  *(iocontrol_data->tool_from_pocket) = 0;
        }
        *(iocontrol_data->tool_number) = emcioStatus.tool.toolInSpindle; //likewise in HAL
        load_tool(emcioStatus.tool.pocketPrepped);
        emcioStatus.tool.pocketPrepped = -1; //reset the tool preped number, -1 to permit tool 0 to be loaded
        *(iocontrol_data->tool_prep_number) = 0; //likewise in HAL
        *(iocontrol_data->tool_prep_pocket) = 0; //likewise in HAL
        *(iocontrol_data->tool_prep_index)  = 0; //likewise in HAL
        *(iocontrol_data->tool_change) = 0; //also reset the tool change signal
        emcioStatus.status = RCS_DONE;        // we finally finished to do tool-changing, signal task with RCS_DONE
        return 11; //change finished
    }
    return 0;
}

void Task::run(){ // called periodically from emctaskmain.cc
    tool_status = read_tool_inputs();
    if ( *(iocontrol_data->emc_enable_in)==0) //check for estop from HW
        emcioStatus.aux.estop = 1;
    else
        emcioStatus.aux.estop = 0;

    emcioStatus.lube.level = *(iocontrol_data->lube_level);        //check for lube_level from HW
}

int Task::emcIoPluginCall(int len, const char *msg)
{
    if (emc_debug & EMC_DEBUG_PYTHON_TASK) {
	rcs_print("emcIoPluginCall(%d,%s) - no Python handler set\n",len,msg);
    }
    return 0;
}
