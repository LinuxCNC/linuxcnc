/*    This is a component of LinuxCNC
 *    Copyright 2011, 2012, 2014 Jeff Epler <jepler@unpythonic.net>,
 *    Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
// TODO: reuse interp converters

#define BOOST_PYTHON_MAX_ARITY 7
#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/implicit.hpp>
#include <boost/python/module.hpp>
#include <boost/python/overloads.hpp>
#include <boost/python/scope.hpp>
#include "python_plugin.hh"
#include "rs274ngc.hh"
#include "interp_internal.hh"
#include "taskclass.hh"
#include "initool.hh"
#include "emcglb.h"		// EMC_INIFILE

namespace bp = boost::python;

#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;

#include "interp_array_types.hh"  // import activeMCodes,activeGCodes,activeSettings, toolTable

#include "rcs.hh"		// NML classes, nmlErrorFormat()
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"

extern void emctask_quit(int sig);
extern EMC_STAT *emcStatus;
typedef boost::shared_ptr< EMC_STAT > emcstatus_ptr;
extern int return_int(const char *funcname, PyObject *retval);


// man, is this ugly. I'm taking suggestions to make this better

static int handle_exception(const char *name)
{
    std::string msg = handle_pyerror();
    printf("%s(): %s\n", name, msg.c_str());
    PyErr_Clear();
    return -1;
}

#define EXPAND(method)						\
    int method() {						\
	if (bp::override f = this->get_override(#method)) {	\
	    try {						\
		return f();					\
	    }							\
	    catch( bp::error_already_set ) {			\
		return handle_exception(#method);		\
	    }							\
	}							\
	else							\
	    return  Task::method();				\
    }


#define EXPAND1(method,type,name)					\
    int method(type name) {						\
	if (bp::override f = this->get_override(#method)) {		\
	    try {							\
		return f(name);						\
	    }								\
	    catch( bp::error_already_set ) {				\
		return handle_exception(#method);			\
	    }								\
	} else								\
	    return  Task::method(name);					\
    }


#define EXPAND2(method,type,name,type2,name2)				\
    int method(type name,type2 name2) {					\
	if (bp::override f = this->get_override(#method)) {		\
	    try {							\
		return f(name,name2);					\
	    }								\
	    catch( bp::error_already_set ) {				\
		return handle_exception(#method);			\
	    }								\
	} else								\
	    return  Task::method(name,name2);				\
    }

struct TaskWrap : public Task, public bp::wrapper<Task> {

    TaskWrap() : Task() {}

    EXPAND(emcIoInit)
    EXPAND(emcIoHalt)
    EXPAND1(emcIoAbort,int,reason)

    EXPAND(emcToolStartChange)
    EXPAND(emcAuxEstopOn)
    EXPAND(emcAuxEstopOff)
    EXPAND(emcCoolantMistOn)
    EXPAND(emcCoolantMistOff)
    EXPAND(emcCoolantFloodOn)
    EXPAND(emcCoolantFloodOff)
    EXPAND(emcLubeOn)
    EXPAND(emcLubeOff)
    EXPAND1(emcIoSetDebug,int,debug)

    EXPAND2(emcToolPrepare,int, p, int, tool)
    EXPAND(emcToolLoad)
    EXPAND1(emcToolLoadToolTable, const char *, file)
    EXPAND(emcToolUnload)
    EXPAND1(emcToolSetNumber,int,number)

    int emcIoPluginCall(int len,const char *msg) {
	if (bp::override f = this->get_override("emcIoPluginCall")) {
	    try {
		// binary picklings may contain zeroes
		std::string buffer(msg,len);
		return f(len,buffer);
	    }
	    catch( bp::error_already_set ) {
		return handle_exception("emcIoPluginCall");
	    }
	} else
	    return  Task::emcIoPluginCall(len,msg);
    }

    int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
			 double frontangle, double backangle, int orientation) {
	if (bp::override f = this->get_override("emcToolSetOffset"))
	    try {
		return f(pocket,toolno,offset,diameter,frontangle,backangle,orientation);
	    }
	    catch( bp::error_already_set ) {
		return handle_exception("emcToolSetOffset");
	    }
	else
	    return  Task::emcToolSetOffset(pocket,toolno,offset,diameter,frontangle,backangle,orientation);
    }

    int emcIoUpdate(EMC_IO_STAT * stat) {
	if (bp::override f = this->get_override("emcIoUpdate"))
	    try {
		return f(); /// bug in Boost.Python, fixed in 1.44 I guess: return_int("foo",f());
	    }
	    catch( bp::error_already_set ) {
		return handle_exception("emcIoUpdate");
	    }
	else
	    return  Task::emcIoUpdate(stat);
    }

};

typedef pp::array_1_t< EMC_AXIS_STAT, EMCMOT_MAX_AXIS> axis_array, (*axis_w)( EMC_MOTION_STAT &m );
typedef pp::array_1_t< EMC_SPINDLE_STAT, EMCMOT_MAX_SPINDLES> spindle_array, (*spindle_w)( EMC_MOTION_STAT &m );
typedef pp::array_1_t< int, EMCMOT_MAX_DIO> synch_dio_array, (*synch_dio_w)( EMC_MOTION_STAT &m );
typedef pp::array_1_t< double, EMCMOT_MAX_AIO> analog_io_array, (*analog_io_w)( EMC_MOTION_STAT &m );

typedef pp::array_1_t< int, ACTIVE_G_CODES> active_g_codes_array, (*active_g_codes_tw)( EMC_TASK_STAT &t );
typedef pp::array_1_t< int, ACTIVE_M_CODES> active_m_codes_array, (*active_m_codes_tw)( EMC_TASK_STAT &t );
typedef pp::array_1_t< double, ACTIVE_SETTINGS> active_settings_array, (*active_settings_tw)( EMC_TASK_STAT &t );

typedef pp::array_1_t< CANON_TOOL_TABLE, CANON_POCKETS_MAX> tool_array, (*tool_w)( EMC_TOOL_STAT &t );

static  tool_array tool_wrapper ( EMC_TOOL_STAT & t) {
    return tool_array(t.toolTable);
}

static  axis_array axis_wrapper ( EMC_MOTION_STAT & m) {
    return axis_array(m.axis);
}

static  spindle_array spindle_wrapper ( EMC_MOTION_STAT & m) {
    return spindle_array(m.spindle);
}

static  synch_dio_array synch_di_wrapper ( EMC_MOTION_STAT & m) {
    return synch_dio_array(m.synch_di);
}

static  synch_dio_array synch_do_wrapper ( EMC_MOTION_STAT & m) {
    return synch_dio_array(m.synch_do);
}

static  analog_io_array analog_input_wrapper ( EMC_MOTION_STAT & m) {
    return analog_io_array(m.analog_input);
}

static  analog_io_array analog_output_wrapper ( EMC_MOTION_STAT & m) {
    return analog_io_array(m.analog_output);
}

static  active_g_codes_array activeGCodes_wrapper (  EMC_TASK_STAT & m) {
    return active_g_codes_array(m.activeGCodes);
}

static  active_m_codes_array activeMCodes_wrapper ( EMC_TASK_STAT & m) {
    return active_m_codes_array(m.activeMCodes);
}

static  active_settings_array activeSettings_wrapper ( EMC_TASK_STAT & m) {
    return active_settings_array(m.activeSettings);
}

static const char *get_file( EMC_TASK_STAT &t) { return t.file; }
static const char *get_command( EMC_TASK_STAT &t) { return t.command; }

static void operator_error(const char *message, int id = 0) {
    emcOperatorError(id,"%s",message);
}
static void operator_text(const char *message, int id = 0) {
    emcOperatorText(id,"%s",message);
}
static void operator_display(const char *message, int id = 0) {
    emcOperatorDisplay(id,"%s",message);
}


#pragma GCC diagnostic push
#if defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7)))
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
BOOST_PYTHON_FUNCTION_OVERLOADS(operator_error_overloads, operator_error, 1,2)
BOOST_PYTHON_FUNCTION_OVERLOADS(operator_text_overloads, operator_text, 1,2)
BOOST_PYTHON_FUNCTION_OVERLOADS(operator_display_overloads, operator_display, 1,2)
#pragma GCC diagnostic pop


static const char *ini_filename() { return emc_inifile; }

BOOST_PYTHON_MODULE(emctask) {
    using namespace boost::python;
    using namespace boost;

    scope().attr("__doc__") =
        "Task introspection\n"
        ;

    def("emctask_quit", emctask_quit);
    def("ini_filename", ini_filename);
    // def("iniTool", iniTool);

    def("operator_error",
	operator_error,
	operator_error_overloads ( args("id"),
				   "send an error message to the operator screen with an optional message id"  ));
    def("operator_text",
	operator_text,
	operator_text_overloads ( args("id"),
				   "send a informational message to the operator screen"  ));
    def("operator_display",
	operator_display,
	operator_display_overloads ( args("id"),
				   "send a message to the operator display"  ));


#define VAL(X)  .value(#X, X)

    enum_<RCS_STATUS>("RCS_STATUS")
	VAL(RCS_EXEC)
	VAL(RCS_DONE)
	VAL(RCS_ERROR)
	;

    enum_<EMC_TASK_MODE_ENUM>("EMC_TASK_MODE")
	VAL(EMC_TASK_MODE_MANUAL)
	VAL(EMC_TASK_MODE_AUTO)
	VAL(EMC_TASK_MODE_MDI)
	;

    enum_<EMC_TASK_STATE_ENUM>("EMC_TASK_STATE")
	VAL(EMC_TASK_STATE_ESTOP)
	VAL(EMC_TASK_STATE_ESTOP_RESET)
	VAL(EMC_TASK_STATE_OFF)
	VAL(EMC_TASK_STATE_ON)
	;

    enum_<EMC_TASK_EXEC_ENUM>("EMC_TASK_EXEC")
	VAL(EMC_TASK_EXEC_ERROR)
	VAL(EMC_TASK_EXEC_DONE)
	VAL(EMC_TASK_EXEC_WAITING_FOR_MOTION)
	VAL(EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE)
	VAL(EMC_TASK_EXEC_WAITING_FOR_IO)
	VAL(EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO)
	VAL(EMC_TASK_EXEC_WAITING_FOR_DELAY)
	VAL(EMC_TASK_EXEC_WAITING_FOR_SYSTEM_CMD)
	;

    enum_<EMC_TASK_INTERP_ENUM>("EMC_TASK_INTERP")
	VAL(EMC_TASK_INTERP_IDLE)
	VAL(EMC_TASK_INTERP_READING)
	VAL(EMC_TASK_INTERP_PAUSED)
	VAL(EMC_TASK_INTERP_WAITING)
	;


    enum_<EMC_IO_ABORT_REASON_ENUM>("EMC_IO_ABORT_REASON")
	VAL(EMC_ABORT_TASK_EXEC_ERROR)
	VAL(EMC_ABORT_AUX_ESTOP)
	VAL(EMC_ABORT_MOTION_OR_IO_RCS_ERROR)
	VAL(EMC_ABORT_TASK_STATE_OFF)
	VAL(EMC_ABORT_TASK_STATE_ESTOP_RESET)
	VAL(EMC_ABORT_TASK_STATE_ESTOP)
	VAL(EMC_ABORT_TASK_STATE_NOT_ON)
	VAL(EMC_ABORT_TASK_ABORT)
	VAL(EMC_ABORT_USER)
	;

    class_<TaskWrap, shared_ptr<TaskWrap>, noncopyable >("Task")

	.def_readonly("use_iocontrol", &Task::use_iocontrol)
	.def_readonly("random_toolchanger", &Task::random_toolchanger)
	.def_readonly("tooltable_filename", &Task::tooltable_filename)
	;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    class_ <EMC_TRAJ_STAT, noncopyable>("EMC_TRAJ_STAT",no_init)
	.def_readwrite("linearUnits", &EMC_TRAJ_STAT::linearUnits )
	.def_readwrite("angularUnits", &EMC_TRAJ_STAT::angularUnits )
	.def_readwrite("cycleTime", &EMC_TRAJ_STAT::cycleTime )
	.def_readwrite("axes", &EMC_TRAJ_STAT::axes )
	.def_readwrite("axis_mask", &EMC_TRAJ_STAT::axis_mask )
	.def_readwrite("mode", &EMC_TRAJ_STAT::mode )
	.def_readwrite("enabled", &EMC_TRAJ_STAT::enabled )
	.def_readwrite("inpos", &EMC_TRAJ_STAT::inpos )
	.def_readwrite("queue", &EMC_TRAJ_STAT::queue )
	.def_readwrite("activeQueue", &EMC_TRAJ_STAT::activeQueue )
	.def_readwrite("queueFull", &EMC_TRAJ_STAT::queueFull )
	.def_readwrite("id", &EMC_TRAJ_STAT::id )
	.def_readwrite("paused", &EMC_TRAJ_STAT::paused )
	.def_readwrite("scale", &EMC_TRAJ_STAT::scale )
	.def_readwrite("position", &EMC_TRAJ_STAT::position )
	.def_readwrite("actualPosition", &EMC_TRAJ_STAT::actualPosition )
	.def_readwrite("velocity", &EMC_TRAJ_STAT::velocity )
	.def_readwrite("acceleration", &EMC_TRAJ_STAT::acceleration)
	.def_readwrite("maxVelocity", &EMC_TRAJ_STAT::maxVelocity )
	.def_readwrite("maxAcceleration", &EMC_TRAJ_STAT::maxAcceleration )
	.def_readwrite("probedPosition", &EMC_TRAJ_STAT::probedPosition )
	.def_readwrite("probe_tripped", &EMC_TRAJ_STAT::probe_tripped )
	.def_readwrite("probing", &EMC_TRAJ_STAT::probing )
	.def_readwrite("probeval", &EMC_TRAJ_STAT::probeval )
	.def_readwrite("kinematics_type", &EMC_TRAJ_STAT::kinematics_type )
	.def_readwrite("motion_type", &EMC_TRAJ_STAT::motion_type )
	.def_readwrite("distance_to_go", &EMC_TRAJ_STAT::distance_to_go )
	.def_readwrite("dtg", &EMC_TRAJ_STAT::dtg )
	.def_readwrite("current_vel", &EMC_TRAJ_STAT::current_vel )
	.def_readwrite("feed_override_enabled", &EMC_TRAJ_STAT::feed_override_enabled )
	.def_readwrite("adaptive_feed_enabled", &EMC_TRAJ_STAT::adaptive_feed_enabled )
	.def_readwrite("feed_hold_enabled", &EMC_TRAJ_STAT::feed_hold_enabled )
	;
#pragma GCC diagnostic pop
    class_ <EMC_JOINT_STAT, noncopyable>("EMC_JOINT_STAT",no_init)
	.def_readwrite("units", &EMC_JOINT_STAT::units)
	.def_readwrite("backlash", &EMC_JOINT_STAT::backlash)
	.def_readwrite("minPositionLimit", &EMC_JOINT_STAT::minPositionLimit)
	.def_readwrite("maxPositionLimit" ,&EMC_JOINT_STAT::maxPositionLimit)
	.def_readwrite("maxFerror", &EMC_JOINT_STAT::maxFerror)
	.def_readwrite("minFerror", &EMC_JOINT_STAT::minFerror)
	.def_readwrite("ferrorCurrent", &EMC_JOINT_STAT::ferrorCurrent)
	.def_readwrite("ferrorHighMark", &EMC_JOINT_STAT::ferrorHighMark)
	.def_readwrite("output", &EMC_JOINT_STAT::output)
	.def_readwrite("input", &EMC_JOINT_STAT::input)
	.def_readwrite("velocity", &EMC_JOINT_STAT::velocity)
	.def_readwrite("inpos",  &EMC_JOINT_STAT::inpos)
	.def_readwrite("homing",  &EMC_JOINT_STAT::homing)
	.def_readwrite("homed",  &EMC_JOINT_STAT::homed)
	.def_readwrite("fault",  &EMC_JOINT_STAT::fault)
	.def_readwrite("enabled",  &EMC_JOINT_STAT::enabled)
	.def_readwrite("minSoftLimit",  &EMC_JOINT_STAT::minSoftLimit)
	.def_readwrite("maxSoftLimit",  &EMC_JOINT_STAT::maxSoftLimit)
	.def_readwrite("minHardLimit",  &EMC_JOINT_STAT::minHardLimit)
	.def_readwrite("maxHardLimit",  &EMC_JOINT_STAT::maxHardLimit)
	.def_readwrite("overrideLimits",  &EMC_JOINT_STAT::overrideLimits)
	;

    class_ <EMC_SPINDLE_STAT, noncopyable>("EMC_SPINDLE_STAT",no_init)
	.def_readwrite("speed", &EMC_SPINDLE_STAT::speed )
	.def_readwrite("direction", &EMC_SPINDLE_STAT::direction )
	.def_readwrite("brake", &EMC_SPINDLE_STAT::brake )
	.def_readwrite("increasing", &EMC_SPINDLE_STAT::increasing )
	.def_readwrite("enabled", &EMC_SPINDLE_STAT::enabled )
	.def_readwrite("spindle_override_enabled", &EMC_SPINDLE_STAT::spindle_override_enabled )
	.def_readwrite("spindle_scale", &EMC_SPINDLE_STAT::spindle_scale )
	.def_readwrite("spindle_orient_state", &EMC_SPINDLE_STAT::orient_state )
	.def_readwrite("spindle_orient_fault", &EMC_SPINDLE_STAT::orient_fault )
	;

    class_ <EMC_COOLANT_STAT , noncopyable>("EMC_COOLANT_STAT ",no_init)
	.def_readwrite("mist", &EMC_COOLANT_STAT::mist )
	.def_readwrite("flood", &EMC_COOLANT_STAT::flood )
	;

    class_ <EMC_LUBE_STAT, noncopyable>("EMC_LUBE_STAT",no_init)
	.def_readwrite("on", &EMC_LUBE_STAT::on )
	.def_readwrite("level", &EMC_LUBE_STAT::level )
	;

    class_ <EMC_MOTION_STAT, noncopyable>("EMC_MOTION_STAT",no_init)
	.def_readwrite("traj", &EMC_MOTION_STAT::traj)
	.add_property( "axis",
		       bp::make_function( axis_w(&axis_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "spindle",
			   bp::make_function( spindle_w(&spindle_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "synch_di",
		       bp::make_function( synch_dio_w(&synch_di_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "synch_do",
		       bp::make_function( synch_dio_w(&synch_do_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "analog_input",
		       bp::make_function( analog_io_w(&analog_input_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "analog_output",
		       bp::make_function( analog_io_w(&analog_output_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	;


    class_ <EMC_TASK_STAT, noncopyable>("EMC_TASK_STAT",no_init)
	.def_readwrite("mode",  &EMC_TASK_STAT::mode)
	.def_readwrite("state",  &EMC_TASK_STAT::state)
	.def_readwrite("execState",  &EMC_TASK_STAT::execState)
	.def_readwrite("interpState",  &EMC_TASK_STAT::interpState)
	.def_readwrite("motionLine", &EMC_TASK_STAT::motionLine)
	.def_readwrite("currentLine", &EMC_TASK_STAT::currentLine)
	.def_readwrite("readLine", &EMC_TASK_STAT::readLine)
	.def_readwrite("optional_stop_state", &EMC_TASK_STAT::optional_stop_state)
	.def_readwrite("block_delete_state", &EMC_TASK_STAT::block_delete_state)
	.def_readwrite("input_timeout", &EMC_TASK_STAT::input_timeout)

	//  read-only
	.add_property("file",  &get_file)
	.add_property("command",   &get_command)

	.def_readwrite("g5x_offset", &EMC_TASK_STAT::g5x_offset)
	.def_readwrite("g5x_index", &EMC_TASK_STAT::g5x_index)
	.def_readwrite("g92_offset", &EMC_TASK_STAT::g92_offset)
	.def_readwrite("rotation_xy", &EMC_TASK_STAT::rotation_xy)
	.def_readwrite("toolOffset", &EMC_TASK_STAT::toolOffset)
	.add_property( "activeGCodes",
		       bp::make_function( active_g_codes_tw(&activeGCodes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "activeMCodes",
		       bp::make_function( active_m_codes_tw(&activeMCodes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "activeSettings",
		       bp::make_function( active_settings_tw(&activeSettings_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.def_readwrite("programUnits", &EMC_TASK_STAT::programUnits)
	.def_readwrite("interpreter_errcode", &EMC_TASK_STAT::interpreter_errcode)
	.def_readwrite("task_paused", &EMC_TASK_STAT::task_paused)
	.def_readwrite("delayLeft", &EMC_TASK_STAT::delayLeft)
	;

    class_ <EMC_TOOL_STAT, noncopyable>("EMC_TOOL_STAT",no_init)
	.def_readwrite("pocketPrepped", &EMC_TOOL_STAT::pocketPrepped )
	.def_readwrite("toolInSpindle", &EMC_TOOL_STAT::toolInSpindle )
	.add_property( "toolTable",
		       bp::make_function( tool_w(&tool_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	;

    class_ <EMC_AUX_STAT, noncopyable>("EMC_AUX_STAT",no_init)
	.def_readwrite("estop", &EMC_AUX_STAT::estop)
	;

    class_ <EMC_IO_STAT, noncopyable>("EMC_IO_STAT",no_init)
	.def_readwrite("cycleTime", &EMC_IO_STAT::cycleTime )
	.def_readwrite("debug", &EMC_IO_STAT::debug )
	.def_readwrite("reason", &EMC_IO_STAT::reason )
	.def_readwrite("fault", &EMC_IO_STAT::fault )
	.def_readwrite("tool", &EMC_IO_STAT::tool)
	.def_readwrite("aux", &EMC_IO_STAT::aux)
	.def_readwrite("coolant", &EMC_IO_STAT::coolant)
	.def_readwrite("lube", &EMC_IO_STAT::lube)
	.def_readwrite("status", &EMC_IO_STAT::status)
	;


    class_ <EMC_STAT, emcstatus_ptr, noncopyable>("EMC_STAT",no_init)
	.def_readwrite("task", &EMC_STAT::task)
	.def_readwrite("motion", &EMC_STAT::motion)
	.def_readwrite("io", &EMC_STAT::io)
	.def_readwrite("debug", &EMC_STAT::debug)

	;



    // this assumes that at module init time emcStatus is valid (non-NULL)
    scope().attr("emcstat") = emcstatus_ptr(emcStatus);

    implicitly_convertible<EMC_TASK_STATE_ENUM, int>();

    pp::register_array_1< double, EMCMOT_MAX_AIO> ("AnalogIoArray");
    pp::register_array_1< int, EMCMOT_MAX_DIO> ("DigitalIoArray");
    pp::register_array_1< EMC_AXIS_STAT,EMCMOT_MAX_AXIS,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("AxisArray");
}


