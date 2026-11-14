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

#include "logutil.hh"
#include <math.h>		// fabs()
#include <float.h>		// DBL_MAX
#include <string.h>		// memcpy() strncpy()
#include <stdlib.h>		// malloc()
#include <sys/wait.h>
#include <libintl.h>		// gettext()

#include "rcs_status.hh"		// enum class RCS_STATUS

#include "nml_intf/emcglb.h"		// EMC_INIFILE

#include "pythonplugin/python_plugin.hh"
#include "taskclass.hh"

using namespace linuxcnc;

/********************************************************************
*
* Description: iocontrol_hal_init(void)
*
* Side Effects: Exports HAL pins.
*
* Called By: main
********************************************************************/
#define IOC0 "iocontrol.0"

#define CHK(x) do { \
        int _rv = (x); \
        if(_rv < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, IOC0": cannot create HAL pin\n"); \
            return -1; \
        } \
    } while(0)

int Task::iocontrol_hal_init(void)
{
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->user_enable_out,  0, IOC0".user-enable-out"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN,  &iocontrol_data->emc_enable_in,    0, IOC0".emc-enable-in"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->user_request_enable, 0, IOC0".user-request-enable"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->coolant_mist,     0, IOC0".coolant-mist"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->coolant_flood,    0, IOC0".coolant-flood"));
    CHK(hal_pin_new_sint(comp_id, HAL_OUT, &iocontrol_data->tool_prep_pocket, 0, IOC0".tool-prep-pocket"));
    CHK(hal_pin_new_sint(comp_id, HAL_OUT, &iocontrol_data->tool_from_pocket, 0, IOC0".tool-from-pocket"));
    CHK(hal_pin_new_sint(comp_id, HAL_OUT, &iocontrol_data->tool_prep_number, 0, IOC0".tool-prep-number"));
    CHK(hal_pin_new_sint(comp_id, HAL_OUT, &iocontrol_data->tool_number,      0, IOC0".tool-number"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->tool_prepare,     0, IOC0".tool-prepare"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN,  &iocontrol_data->tool_prepared,    0, IOC0".tool-prepared"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &iocontrol_data->tool_change,      0, IOC0".tool-change"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN,  &iocontrol_data->tool_changed,     0, IOC0".tool-changed"));
    if(hal_ready(comp_id) < 0) {
        hal_exit(comp_id);
        comp_id = -1;
        return -1;
    }
    return 0;
}
#undef CHK

/********************************************************************
*
* Description: hal_init_pins(void)
*
* Side Effects: Sets HAL pins default values.
*
* Called By: main
********************************************************************/
void Task::hal_init_pins(void)
{
    hal_set_bool(iocontrol_data->user_enable_out, 0);    /* output, FALSE when EMC wants stop */
    hal_set_bool(iocontrol_data->user_request_enable, 0);/* output, used to reset HAL latch */
    hal_set_bool(iocontrol_data->coolant_mist, 0);       /* coolant mist output pin */
    hal_set_bool(iocontrol_data->coolant_flood, 0);      /* coolant flood output pin */
    hal_set_bool(iocontrol_data->tool_prepare, 0);       /* output, pin that notifies HAL it needs to prepare a tool */
    hal_set_sint(iocontrol_data->tool_prep_number, 0);   /* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_set_sint(iocontrol_data->tool_prep_pocket, 0);   /* output, pin that holds the pocketno for the tool to be prepared, only valid when tool-prepare=TRUE */
    hal_set_sint(iocontrol_data->tool_from_pocket, 0);   /* output, always 0 at startup */
    hal_set_bool(iocontrol_data->tool_change, 0);        /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_set_sint(iocontrol_data->tool_number, emcioStatus.tool.toolInSpindle);
}

Task *task_methods;

// global status structure
EMC_IO_STAT *emcIoStatus = NULL;

// glue

int emcIoInit() { return task_methods->emcIoInit(); }
int emcIoAbort(EMC_ABORT reason) { return task_methods->emcIoAbort(reason); }
int emcAuxEstopOn()  { return task_methods->emcAuxEstopOn(); }
int emcAuxEstopOff() { return task_methods->emcAuxEstopOff(); }
int emcCoolantMistOn() { return task_methods->emcCoolantMistOn(); }
int emcCoolantMistOff() { return task_methods->emcCoolantMistOff(); }
int emcCoolantFloodOn() { return task_methods->emcCoolantFloodOn(); }
int emcCoolantFloodOff() { return task_methods->emcCoolantFloodOff(); }
int emcToolPrepare(int tool) { return task_methods->emcToolPrepare(tool); }
int emcToolLoad() { return task_methods->emcToolLoad(); }
int emcToolLoadToolTable(const char *file) { return task_methods->emcToolLoadToolTable(file); }
int emcToolSetOffset(int toolno, const EmcPose& offset, double diameter,
                     double frontangle, double backangle, int orientation) {
    return task_methods->emcToolSetOffset( toolno,  offset,  diameter,
					   frontangle,  backangle,  orientation); }
int emcToolSetNumber(int toolno) { return task_methods->emcToolSetNumber(toolno); }

int emcTaskOnce(const char * /*filename*/, EMC_IO_STAT &emcioStatus)
{
    task_methods = new Task(emcioStatus);
    if(task_methods->comp_id < 0) {
        // This happens when init is incomplete
        return -1;
    }
    if (int res = task_methods->iocontrol_hal_init()) {
        return res;
    }
    return 0;
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
    iocontrol_data(nullptr),
    comp_id(-1),
    ini_filename(emc_inifile),
    tool_status(0)
    {

    comp_id = hal_init(IOC0);
    if(comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, IOC0": ERROR: hal_init() failed\n");
        return;  // emcTaskOnce() fails on comp_id
    }
    iocontrol_data = reinterpret_cast<iocontrol_str *>(hal_malloc(sizeof(*iocontrol_data)));
    if(!iocontrol_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, IOC0": ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        comp_id = -1;
        return;  // emcTaskOnce() fails on comp_id
    }

    IniFile inifile(ini_filename);

    if (inifile) {
        if (auto inival = inifile.findBool("RANDOM_TOOLCHANGER", "EMCIO")) {
            random_toolchanger = *inival;
        }
        if (auto t = inifile.findString("TOOL_TABLE", "EMCIO"))
            tooltable_filename = strdup(t->c_str());

        if (auto t = inifile.findString("DB_PROGRAM", "EMCIO")) {
            db_mode = tooldb_t::DB_ACTIVE;
            strncpy(db_program, t->c_str(), LINELEN - 1);
        }

        if (tooltable_filename != NULL && db_program[0] != '\0') {
            fprintf(stderr,"DB_PROGRAM active: IGNORING tool table file %s\n",
                    tooltable_filename);
        }
    }

#ifdef TOOL_NML //{
    tool_nml_register( (CANON_TOOL_TABLE*)&emcStatus->io.tool.toolTable);
#else //}{
    tool_mmap_creator((EMC_TOOL_STAT*)&(emcioStatus.tool), random_toolchanger);
    tool_mmap_user();
    // initialize database tool finder:
#endif //}

    tooldata_init(random_toolchanger);
    if (db_mode == tooldb_t::DB_ACTIVE) {
        if (0 != tooldata_db_init(db_program, random_toolchanger)) {
            log_error("can't initialize DB_PROGRAM.\n");
            db_mode = tooldb_t::DB_NOTUSED;
        }
    }

    // the storage the table is exported to exists now
    tools = new ToolTable(random_toolchanger);
    tools->use_db(db_mode == tooldb_t::DB_ACTIVE);

    emcioStatus.status = RCS_STATUS::DONE;//TODO??
    set_prepped(-1);

    if (0 != tools->read(tooltable_filename)) {
        log_error("can't load tool table.\n");
    }

    if (random_toolchanger) {
        emcioStatus.tool.toolInSpindle = tools->spindle().toolno;
    } else {
        emcioStatus.tool.toolInSpindle = 0;
    }
    tools_changed();
};

Task::~Task() {
    if(comp_id >= 0)
        hal_exit(comp_id);
    delete tools;
}

// Call this after anything that may have changed the table. The status
// carries the spindle's entry, and tells the readers of the exported table
// that it has changed.
void Task::tools_changed() {
    emcioStatus.tool.toolTableCurrent = tools->spindle();
    emcioStatus.tool.toolTableSerial = tools->serial();
}

// set the have_tool_change_position global
static int readToolChange(const IniFile &toolInifile)
{
    int retval = 0;

    auto inistring = toolInifile.findString("TOOL_CHANGE_POSITION", "EMCIO");
    if (inistring) {
        // FIXME: This should really be a LCNC library call written in C++
	/* found an entry */
        if (9 == sscanf(inistring->c_str(), "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
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
        } else if (6 == sscanf(inistring->c_str(), "%lf %lf %lf %lf %lf %lf",
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
        } else if (3 == sscanf(inistring->c_str(), "%lf %lf %lf",
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
	    log_info("bad format for TOOL_CHANGE_POSITION\n");
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
    IniFile toolInifile(filename);

    if (!toolInifile) {
	return -1;
    }
    // read the tool change positions
    if (0 != readToolChange(toolInifile)) {
	retval = -1;
    }

    return retval;
}

// Put tool `toolno` into the spindle; it is in the table. On a non-random
// changer tool 0 empties the spindle.
void Task::load_tool(int toolno) {
    CANON_TOOL_TABLE tdata;
    ToolTable::Where where = tools->locate(toolno, &tdata);
    if (where == ToolTable::NOT_FOUND) {
        UNEXPECTED_MSG; return;
    }
    if(random_toolchanger) {
        // swap the tools between the desired pocket and the spindle pocket
        CANON_TOOL_TABLE tzero = tools->spindle();
        // spindle-->pocket
        tooldata_db_notify(SPINDLE_UNLOAD,tzero.toolno,tdata.pocketno,tzero);
        // pocket-->spindle
        tooldata_db_notify(SPINDLE_LOAD,tdata.toolno,0,tdata);
        tools->load(toolno);
        if (0 != tools->write(tooltable_filename)) {
            emcioStatus.status = RCS_STATUS::ERROR;
        }
    } else if(where == ToolTable::SPINDLE) {
        // on non-random tool-changers tool 0 is "unload the tool from the
        // spindle"
        tools->load(toolno);
        tdata = tools->spindle();
        if (tooldata_db_notify(SPINDLE_UNLOAD,0,0,tdata)) { UNEXPECTED_MSG; }
    } else {
        // just copy the desired tool to the spindle
        tools->load(toolno);
        // notify tool in spindle:
        CANON_TOOL_TABLE temp = tools->spindle();
        if (tooldata_db_notify(SPINDLE_LOAD,temp.toolno,0,temp)) { UNEXPECTED_MSG; }
    }
    tools_changed();
} // load_tool()

// After the tool table was read: a non-random changer's spindle holds the
// loaded tool's entry again.
void Task::reload_tool_number(int toolno) {
    if(random_toolchanger) return; // doesn't need special handling here
    if (tools->reload(toolno)) {
        CANON_TOOL_TABLE temp = tools->spindle();
        if (tooldata_db_notify(SPINDLE_LOAD,temp.toolno,0,temp)) { UNEXPECTED_MSG; }
    }
    tools_changed();
}

// NML commands

int Task::emcIoInit()//EMC_TOOL_INIT
{
    tools->read(tooltable_filename);
    reload_tool_number(emcioStatus.tool.toolInSpindle);
    tools_changed();

    if (0 != iniTool(emc_inifile)) {
	return -1;
    }
    return 0;
}

int Task::emcIoAbort(EMC_ABORT /*reason*/)//EMC_TOOL_ABORT_TYPE
{
    // only used in v2
    // this gets sent on any Task Abort, so it might be safer to stop
    // the spindle  and coolant
    rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_ABORT\n");
    emcioStatus.coolant.mist = 0;
    emcioStatus.coolant.flood = 0;
    hal_set_bool(iocontrol_data->coolant_mist, 0);    /* coolant mist output pin */
    hal_set_bool(iocontrol_data->coolant_flood, 0);   /* coolant flood output pin */
    hal_set_bool(iocontrol_data->tool_change, 0);     /* abort tool change if in progress */
    hal_set_bool(iocontrol_data->tool_prepare, 0);    /* abort tool prepare if in progress */
    // release task wait on pending tool-change/prepare (old NML iocontrol
    // returned RCS_DONE by default; in-process call must do it explicitly)
    emcioStatus.status = RCS_STATUS::DONE;
    return 0;
}

int Task::emcAuxEstopOn()//EMC_AUX_ESTOP_ON_TYPE
{
    /* assert an ESTOP to the outside world (thru HAL) */
    hal_set_bool(iocontrol_data->user_enable_out, 0); //disable on ESTOP_ON
    hal_set_bool(iocontrol_data->user_request_enable, 0);
    hal_init_pins(); //resets all HAL pins to safe valuea
    return 0;
}

int Task::emcAuxEstopOff()
{
    /* remove ESTOP */
    hal_set_bool(iocontrol_data->user_enable_out, 1); //we're good to enable on ESTOP_OFF
    /* generate a rising edge to reset optional HAL latch */
    hal_set_bool(iocontrol_data->user_request_enable, 1);
    emcioStatus.aux.estop = 0;
    return 0;
}

int Task::emcCoolantMistOn()
{
    emcioStatus.coolant.mist = 1;
    hal_set_bool(iocontrol_data->coolant_mist, 1);
    return 0;
}

int Task::emcCoolantMistOff()
{
    emcioStatus.coolant.mist = 0;
    hal_set_bool(iocontrol_data->coolant_mist, 0);
    return 0;
}

int Task::emcCoolantFloodOn()
{
    emcioStatus.coolant.flood = 1;
    hal_set_bool(iocontrol_data->coolant_flood, 1);
    return 0;
}

int Task::emcCoolantFloodOff()
{
    emcioStatus.coolant.flood = 0;
    hal_set_bool(iocontrol_data->coolant_flood, 0);
    return 0;
}

int Task::emcToolPrepare(int toolno)
{
    CANON_TOOL_TABLE tdata;
    ToolTable::Where where = tools->locate(toolno, &tdata);
    // in the spindle: the tool itself, or on a non-random changer tool 0
    bool in_spindle = where == ToolTable::SPINDLE;
    if (where == ToolTable::NOT_FOUND) {
        prep_tool = -1;
        set_prepped(-1);
    } else {
        rtapi_print_msg(RTAPI_MSG_DBG, "EMC_TOOL_PREPARE tool=%d pocket=%d\n",
                        toolno, in_spindle ? 0 : tdata.pocketno);

        prep_tool = toolno; // any type of changer

        // Note: some of the following logic could be simplified
        //       but is maintained to preserve runtests expectations
        // Set HAL pins for tool number and pocket.
        if (random_toolchanger) {
            // RANDOM_TOOLCHANGER
            hal_set_sint(iocontrol_data->tool_prep_number, tdata.toolno);
            if (in_spindle) {
                set_prepped(toolno);
                hal_set_sint(iocontrol_data->tool_prep_pocket, 0);
                return 0;
            }
            hal_set_sint(iocontrol_data->tool_prep_pocket, tdata.pocketno);
        } else {
            // NON_RANDOM_TOOLCHANGER
            if (in_spindle) {
                set_prepped(0);
                hal_set_sint(iocontrol_data->tool_prep_number, 0);
                hal_set_sint(iocontrol_data->tool_prep_pocket, 0);
            } else {
                hal_set_sint(iocontrol_data->tool_prep_number, tdata.toolno);
                hal_set_sint(iocontrol_data->tool_prep_pocket, tdata.pocketno);
            }
        }
        // it doesn't make sense to prep the spindle pocket
        if (random_toolchanger && in_spindle) {
            set_prepped(toolno);
            return 0;
        }
    }

    /* then set the prepare pin to tell external logic to get started */
    hal_set_bool(iocontrol_data->tool_prepare, 1);
    // the feedback logic is done inside read_hal_inputs()
    // we only need to set RCS_EXEC if RCS_DONE is not already set by the above logic
    if (tool_status != 10) //set above to 10 in case PREP already finished (HAL loopback machine)
        emcioStatus.status = RCS_STATUS::EXEC;
    return 0;
}

// Record the prepared tool, and its pocket for the status (pocketPrepped,
// toolPrepped). -1 is none.
void Task::set_prepped(int toolno)
{
    prepped_tool = toolno;
    emcioStatus.tool.toolPrepped = toolno;
    CANON_TOOL_TABLE tdata;
    switch (tools->locate(toolno, &tdata)) {
    case ToolTable::NOT_FOUND:
        emcioStatus.tool.pocketPrepped = -1;
        break;
    case ToolTable::SPINDLE:
        // a random changer's tool in the spindle, or T0, no tool
        emcioStatus.tool.pocketPrepped = 0;
        break;
    case ToolTable::POCKET:
        emcioStatus.tool.pocketPrepped = tdata.pocketno;
        break;
    }
}

int Task::emcToolLoad()//EMC_TOOL_LOAD_TYPE
{
    // the prepared tool is looked up now: the tool table may have been
    // reloaded since it was prepared
    ToolTable::Where where = tools->locate(prepped_tool);
    if (where == ToolTable::NOT_FOUND && prepped_tool >= 0) {
        emcOperatorError(gettext("M6: prepared tool %d is no longer in the tool table"),
                         prepped_tool);
        set_prepped(-1);
        return -1;
    }

    // it doesn't make sense to load a tool from the spindle pocket
    if (random_toolchanger && where == ToolTable::SPINDLE) {
        return 0;
    }

    // it's not necessary to load the tool already in the spindle
    if (!random_toolchanger && where == ToolTable::POCKET &&
        (emcioStatus.tool.toolInSpindle == prepped_tool) ) {
        return 0;
    }

    if (where != ToolTable::NOT_FOUND) {
        //notify HW for toolchange
        hal_set_bool(iocontrol_data->tool_change, 1);
        // the feedback logic is done inside read_hal_inputs() we only
        // need to set RCS_EXEC if RCS_DONE is not already set by the
        // above logic
        if (tool_status != 11)
            // set above to 11 in case LOAD already finished (HAL
            // loopback machine)
            emcioStatus.status = RCS_STATUS::EXEC;
    }
    return 0;
}

int Task::emcToolLoadToolTable(const char *file)//EMC_TOOL_LOAD_TOOL_TABLE_TYPE
{
    if(!strlen(file)) file = tooltable_filename;//use filename from ini if none is provided
    tools->read(file);
    reload_tool_number(emcioStatus.tool.toolInSpindle);
    tools_changed();
    return 0;
}

int Task::emcToolSetOffset(int toolno, const EmcPose& offset, double diameter,
                     double frontangle, double backangle, int orientation)//EMC_TOOL_SET_OFFSET
{
    rtapi_print_msg(RTAPI_MSG_DBG,
            "EMC_TOOL_SET_OFFSET toolno=%d zoffset=%lf, "
            "xoffset=%lf, diameter=%lf, "
            "frontangle=%lf, backangle=%lf, orientation=%d\n",
            toolno, offset.tran.z, offset.tran.x, diameter, frontangle,
            backangle, orientation);
    // A non-random changer keeps the loaded tool's data in the spindle's
    // entry too, which is what is in effect. Tool 0 there is no tool.
    CANON_TOOL_TABLE tdata;
    if (!tools->set_offset(toolno, emcioStatus.tool.toolInSpindle, offset,
                           diameter, frontangle, backangle, orientation,
                           &tdata)) {
        emcOperatorError(gettext("Tool %d not in the tool table"), toolno);
        return -1;
    }
    tools_changed();

    if (0 != tools->write(tooltable_filename)) {
        emcioStatus.status = RCS_STATUS::ERROR;
    }

    if (db_mode == tooldb_t::DB_ACTIVE) {
        // the tool table file has no tool table entries in db mode,
        // so the db_program must be notified to persist the new offsets
        if (tooldata_db_notify(TOOL_OFFSET,toolno,tdata.pocketno,tdata)) {
            UNEXPECTED_MSG;
        }
    }

    return 0;
}

int Task::emcToolSetNumber(int toolno)//EMC_TOOL_SET_NUMBER
{
    if (tools->locate(toolno) == ToolTable::NOT_FOUND) {
        emcOperatorError(gettext("M61: tool %d not in the tool table"), toolno);
        return -1;
    }
    load_tool(toolno);

    // update spindle (fix legacy behavior)
    CANON_TOOL_TABLE tdata = tools->spindle();
    rtapi_print_msg(RTAPI_MSG_DBG,
            "EMC_TOOL_SET_NUMBER old_loaded_tool=%d new_tool=%d\n"
            , emcioStatus.tool.toolInSpindle, tdata.toolno);
    emcioStatus.tool.toolInSpindle = tdata.toolno;
    //likewise in HAL
    hal_set_sint(iocontrol_data->tool_number, emcioStatus.tool.toolInSpindle);
    if (emcioStatus.tool.toolInSpindle == 0) {
        emcioStatus.tool.toolFromPocket = hal_set_sint(iocontrol_data->tool_from_pocket, 0); // no tool in spindle
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
    if (hal_get_bool(iocontrol_data->tool_prepare) && hal_get_bool(iocontrol_data->tool_prepared)) {
        set_prepped(prep_tool);
        hal_set_bool(iocontrol_data->tool_prepare, 0);
        emcioStatus.status = RCS_STATUS::DONE;  // we finally finished to do tool-changing, signal task with RCS_DONE
        return 10; //prepped finished
    }

    if (hal_get_bool(iocontrol_data->tool_change) && hal_get_bool(iocontrol_data->tool_changed)) {
        CANON_TOOL_TABLE tdata;
        ToolTable::Where where = tools->locate(prepped_tool, &tdata);
        if (where == ToolTable::NOT_FOUND) {
            // the tool table lost the tool while it was being changed
            emcOperatorError(gettext("M6: prepared tool %d is no longer in the tool table"),
                             prepped_tool);
            set_prepped(-1);
            prep_tool = -1;
            hal_set_bool(iocontrol_data->tool_change, 0);
            emcioStatus.status = RCS_STATUS::ERROR;
            return -1;
        }
        if(!random_toolchanger && where == ToolTable::SPINDLE) {
            emcioStatus.tool.toolInSpindle = 0;
            emcioStatus.tool.toolFromPocket = hal_set_sint(iocontrol_data->tool_from_pocket, 0);
        } else {
            // the tool now in the spindle is the one that was prepared
            emcioStatus.tool.toolInSpindle = tdata.toolno;
            emcioStatus.tool.toolFromPocket = hal_set_sint(iocontrol_data->tool_from_pocket, tdata.pocketno);
        }
        if (emcioStatus.tool.toolInSpindle == 0) {
             emcioStatus.tool.toolFromPocket = hal_set_sint(iocontrol_data->tool_from_pocket, 0);
        }
        hal_set_sint(iocontrol_data->tool_number, emcioStatus.tool.toolInSpindle); //likewise in HAL
        load_tool(prepped_tool);
        set_prepped(-1); // -1 to permit tool 0 to be loaded
        hal_set_sint(iocontrol_data->tool_prep_number, 0); //likewise in HAL
        hal_set_sint(iocontrol_data->tool_prep_pocket, 0); //likewise in HAL
        prep_tool = -1;
        hal_set_bool(iocontrol_data->tool_change, 0); //also reset the tool change signal
        emcioStatus.status = RCS_STATUS::DONE;        // we finally finished to do tool-changing, signal task with RCS_DONE
        return 11; //change finished
    }
    return 0;
}

void Task::run(){ // called periodically from emctaskmain.cc
    tool_status = read_tool_inputs();
    if (!hal_get_bool(iocontrol_data->emc_enable_in)) //check for estop from HW
        emcioStatus.aux.estop = 1;
    else {
        emcioStatus.aux.estop = 0;
        if (hal_get_bool(iocontrol_data->user_request_enable)) {
            hal_set_bool(iocontrol_data->user_request_enable, 0);
        }
    }
}
