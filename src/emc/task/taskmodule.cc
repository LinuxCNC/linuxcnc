    // EMC_AXIS_STAT axis[EMC_AXIS_MAX];
    // int synch_di[EMC_MAX_DIO];  // motion inputs queried by interp
    // int synch_do[EMC_MAX_DIO];  // motion outputs queried by interp
    // double analog_input[EMC_MAX_AIO]; //motion analog inputs queried by interp
    // double analog_output[EMC_MAX_AIO]; //motion analog outputs queried by interp
//task_stat:

// reuse interp converters



#include <boost/python.hpp>
#include "rs274ngc.hh"
#include "interp_internal.hh"

namespace bp = boost::python;

#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;

#include "interp_array_types.hh"  // import activeMCodes,activeGCodes,activeSettings, toolTable

#include "rcs.hh"		// NML classes, nmlErrorFormat()
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"

extern EMC_STAT *emcStatus;

typedef pp::array_1_t< EMC_AXIS_STAT, EMC_AXIS_MAX> axis_array, (*axis_w)( EMC_MOTION_STAT &m );
typedef pp::array_1_t< int, EMC_MAX_DIO> synch_dio_array, (*synch_dio_w)( EMC_MOTION_STAT &m );
typedef pp::array_1_t< double, EMC_MAX_AIO> analog_io_array, (*analog_io_w)( EMC_MOTION_STAT &m );

typedef pp::array_1_t< int, ACTIVE_G_CODES> active_g_codes_array, (*active_g_codes_tw)( EMC_TASK_STAT &t );
typedef pp::array_1_t< int, ACTIVE_M_CODES> active_m_codes_array, (*active_m_codes_tw)( EMC_TASK_STAT &t );
typedef pp::array_1_t< double, ACTIVE_SETTINGS> active_settings_array, (*active_settings_tw)( EMC_TASK_STAT &t );

static  axis_array axis_wrapper ( EMC_MOTION_STAT & m) {
    return axis_array(m.axis);
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



BOOST_PYTHON_MODULE(emctask) {
    using namespace boost::python;
    using namespace boost;

    scope().attr("__doc__") =
        "Task introspection\n"
        ;

    class_<PmCartesian, noncopyable>("PmCartesian","EMC cartesian postition",no_init)
	.def_readwrite("x",&PmCartesian::x)
	.def_readwrite("y",&PmCartesian::y)
	.def_readwrite("z",&PmCartesian::z)
	;

    class_<EmcPose, noncopyable>("EmcPose","EMC pose",no_init)
	.def_readwrite("tran",&EmcPose::tran)
	// .def_readwrite("x",&EmcPose::tran.x)
	// .def_readwrite("y",&EmcPose::tran.y)
	// .def_readwrite("x",&EmcPose::tran.z)
	.def_readwrite("a",&EmcPose::a)
	.def_readwrite("b",&EmcPose::b)
	.def_readwrite("c",&EmcPose::c)
	.def_readwrite("u",&EmcPose::u)
	.def_readwrite("v",&EmcPose::v)
	.def_readwrite("w",&EmcPose::w)
	;

    class_ <EMC_TRAJ_STAT, noncopyable>("EMC_TRAJ_STAT",no_init)
	.def_readwrite("linearUnits", &emcStatus->motion.traj.linearUnits )
	.def_readwrite("angularUnits", &emcStatus->motion.traj.angularUnits )
	.def_readwrite("cycleTime", &emcStatus->motion.traj.cycleTime )
	.def_readwrite("axes", &emcStatus->motion.traj.axes )
	.def_readwrite("axis_mask", &emcStatus->motion.traj.axis_mask )
	.def_readwrite("mode", &emcStatus->motion.traj.mode )
	.def_readwrite("enabled", &emcStatus->motion.traj.enabled )
	.def_readwrite("inpos", &emcStatus->motion.traj.inpos )
	.def_readwrite("queue", &emcStatus->motion.traj.queue )
	.def_readwrite("activeQueue", &emcStatus->motion.traj.activeQueue )
	.def_readwrite("queueFull", &emcStatus->motion.traj.queueFull )
	.def_readwrite("id", &emcStatus->motion.traj.id )
	.def_readwrite("paused", &emcStatus->motion.traj.paused )
	.def_readwrite("scale", &emcStatus->motion.traj.scale )
	.def_readwrite("spindle_scale", &emcStatus->motion.traj.spindle_scale )
	.def_readwrite("position", &emcStatus->motion.traj.position )
	.def_readwrite("actualPosition", &emcStatus->motion.traj.actualPosition )
	.def_readwrite("velocity", &emcStatus->motion.traj.velocity )
	.def_readwrite("acceleration", &emcStatus->motion.traj.acceleration)
	.def_readwrite("maxVelocity", &emcStatus->motion.traj.maxVelocity )
	.def_readwrite("maxAcceleration", &emcStatus->motion.traj.maxAcceleration )
	.def_readwrite("probedPosition", &emcStatus->motion.traj.probedPosition )
	.def_readwrite("probe_tripped", &emcStatus->motion.traj.probe_tripped )
	.def_readwrite("probing", &emcStatus->motion.traj.probing )
	.def_readwrite("probeval", &emcStatus->motion.traj.probeval )
	.def_readwrite("kinematics_type", &emcStatus->motion.traj.kinematics_type )
	.def_readwrite("motion_type", &emcStatus->motion.traj.motion_type )
	.def_readwrite("distance_to_go", &emcStatus->motion.traj.distance_to_go )
	.def_readwrite("dtg", &emcStatus->motion.traj.dtg )
	.def_readwrite("current_vel", &emcStatus->motion.traj.current_vel )
	.def_readwrite("feed_override_enabled", &emcStatus->motion.traj.feed_override_enabled )
	.def_readwrite("spindle_override_enabled", &emcStatus->motion.traj.spindle_override_enabled )
	.def_readwrite("adaptive_feed_enabled", &emcStatus->motion.traj.adaptive_feed_enabled )
	.def_readwrite("feed_hold_enabled", &emcStatus->motion.traj.feed_hold_enabled )
	;

    class_ <EMC_AXIS_STAT, noncopyable>("EMC_AXIS_STAT",no_init)
    // // .def_readwrite("", &EmcStatus-> )

    // 	.def("axisType", &EmcStatus-> ;	// EMC_AXIS_LINEAR, EMC_AXIS_ANGULAR
    // double units;		// units per mm, deg for linear, angular
    // double backlash;
    // double minPositionLimit;
    // double maxPositionLimit;
    // double maxFerror;
    // double minFerror;

    // // dynamic status
    // double ferrorCurrent;	// current following error
    // double ferrorHighMark;	// magnitude of max following error
    // /*! \todo FIXME - is this really position, or the DAC output? */
    // double output;		// commanded output position
    // double input;		// current input position
    // double velocity;		// current velocity
    // unsigned char inpos;	// non-zero means in position
    // unsigned char homing;	// non-zero means homing
    // unsigned char homed;	// non-zero means has been homed
    // unsigned char fault;	// non-zero means axis amp fault
    // unsigned char enabled;	// non-zero means enabled
    // unsigned char minSoftLimit;	// non-zero means min soft limit exceeded
    // unsigned char maxSoftLimit;	// non-zero means max soft limit exceeded
    // unsigned char minHardLimit;	// non-zero means min hard limit exceeded
    // unsigned char maxHardLimit;	// non-zero means max hard limit exceeded
    // unsigned char overrideLimits; // non-zero means limits are overridden

    //
	;
    class_ <EMC_SPINDLE_STAT, noncopyable>("EMC_SPINDLE_STAT",no_init)
	.def_readwrite("speed", &emcStatus->motion.spindle.speed )
	.def_readwrite("direction", &emcStatus->motion.spindle.direction )
	.def_readwrite("brake", &emcStatus->motion.spindle.brake )
	.def_readwrite("increasing", &emcStatus->motion.spindle.increasing )
	.def_readwrite("enabled", &emcStatus->motion.spindle.enabled )
	;

    class_ <EMC_COOLANT_STAT , noncopyable>("EMC_COOLANT_STAT ",no_init)
	.def_readwrite("mist", &emcStatus->motion.coolant.mist )
	.def_readwrite("flood", &emcStatus->motion.coolant.flood )
	;

    class_ <EMC_LUBE_STAT, noncopyable>("EMC_LUBE_STAT",no_init)
	.def_readwrite("on", &emcStatus->motion.lube.on )
	.def_readwrite("level", &emcStatus->motion.lube.level )
	;

    class_ <EMC_MOTION_STAT, noncopyable>("EMC_MOTION_STAT",no_init)
	.def_readwrite("traj", &emcStatus->motion.traj)
	.add_property( "axis",
		       bp::make_function( axis_w(&axis_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))


	// .def_readwrite("spindle", &emcStatus->motion.spindle)
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
	.def_readwrite("estop", &emcStatus->motion.estop)
	.def_readwrite("coolant", &emcStatus->motion.coolant)
	.def_readwrite("lube", &emcStatus->motion.lube)
	.def_readwrite("debug", &emcStatus->motion.debug)
	;

    class_ <EMC_TASK_STAT, noncopyable>("EMC_TASK_STAT",no_init)
	.def_readwrite("mode", (int *) &emcStatus->task.mode)
	.def_readwrite("state", (int *) &emcStatus->task.state)
	.def_readwrite("execState", (int *) &emcStatus->task.execState)
	.def_readwrite("interpState", (int *) &emcStatus->task.interpState)
	.def_readwrite("motionLine", &emcStatus->task.motionLine)
	.def_readwrite("currentLine", &emcStatus->task.currentLine)
	.def_readwrite("readLine", &emcStatus->task.readLine)
	.def_readwrite("optional_stop_state", &emcStatus->task.optional_stop_state)
	.def_readwrite("block_delete_state", &emcStatus->task.block_delete_state)
	.def_readwrite("input_timeout", &emcStatus->task.input_timeout)
	.def_readwrite("file", (char *) &emcStatus->task.file)
	.def_readwrite("command", (char *) &emcStatus->task.command)
	.def_readwrite("g5x_offset", &emcStatus->task.g5x_offset)
	.def_readwrite("g5x_index", &emcStatus->task.g5x_index)
	.def_readwrite("g92_offset", &emcStatus->task.g92_offset)
	.def_readwrite("rotation_xy", &emcStatus->task.rotation_xy)
	.def_readwrite("toolOffset", &emcStatus->task.toolOffset)
	.add_property( "activeGCodes",
		       bp::make_function( active_g_codes_tw(&activeGCodes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "activeMCodes",
		       bp::make_function( active_m_codes_tw(&activeMCodes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "activeSettings",
		       bp::make_function( active_settings_tw(&activeSettings_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.def_readwrite("programUnits", &emcStatus->task.programUnits)
	.def_readwrite("interpreter_errcode", &emcStatus->task.interpreter_errcode)
	.def_readwrite("task_paused", &emcStatus->task.task_paused)
	.def_readwrite("delayLeft", &emcStatus->task.delayLeft)
	;

    class_ <EMC_TOOL_STAT, noncopyable>("EMC_TOOL_STAT",no_init)
	.def_readwrite("pocketPrepped", &emcStatus->io.tool.pocketPrepped )
	.def_readwrite("toolInSpindle", &emcStatus->io.tool.toolInSpindle )
	//	.def_readwrite("toolTable", &emcStatus->io.tool.toolTable )  // FIXME wrap this
	;

    class_ <EMC_AUX_STAT, noncopyable>("EMC_AUX_STAT",no_init)
	// .def_readwrite("", &emcStatus->io.aux. ) // empty NML msg?
	;

   class_ <EMC_IO_STAT, noncopyable>("EMC_IO_STAT",no_init)
	.def_readwrite("cycleTime", &emcStatus->io.cycleTime )
	.def_readwrite("debug", &emcStatus->io.debug )
	.def_readwrite("reason", &emcStatus->io.reason )
	.def_readwrite("fault", &emcStatus->io.fault )
	.def_readwrite("tool", &emcStatus->io.tool)
	.def_readwrite("aux", &emcStatus->io.aux )
	;


    class_ <EMC_STAT, noncopyable>("EMC_STAT",no_init)
	.def_readwrite("task",&emcStatus->task)
	.def_readwrite("motion",&emcStatus->motion)
	.def_readwrite("io",&emcStatus->io)
	.def_readwrite("debug",&emcStatus->debug)
	;


    pp::register_array_1< double, EMC_MAX_AIO> ("AnalogIoArray");
    pp::register_array_1< int, EMC_MAX_DIO> ("DigitalIoArray");
    pp::register_array_1< EMC_AXIS_STAT,EMC_AXIS_MAX,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("AxisArray");
}
