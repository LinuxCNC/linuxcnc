// Support for Python oword subroutines
//
// proof-of-concept for access to Interp and canon
//
// NB: all this is executed at readahead time
//
// Michael Haberler 4/2011

#include <boost/python.hpp>
#include <boost/make_shared.hpp>
#include <boost/python/object.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/python/call.hpp>

namespace bp = boost::python;

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"

#define PYCHK(bad, fmt, ...)				       \
    do {                                                       \
        if (bad) {                                             \
	    ERS(fmt, ## __VA_ARGS__);                          \
	    goto error;					       \
        }                                                      \
    } while(0)

// accessing the private data of Interp is kind of kludgy because
// technically the Python module does not run in a member context
// but 'outside'
// we store references to private data during call setup and
// use it to reference it in the python module

static setup_pointer current_setup;

// accessors for a few _setup members so they can be injected into Python
// this would be much easier with add_static_property but _setup
// would have to be public

int Interp::get_selected_pocket() { return _setup.selected_pocket;}
void Interp::set_selected_pocket(int p) {  _setup.selected_pocket = p;}
int Interp::get_current_pocket() { return _setup.current_pocket;}
void Interp::set_current_pocket(int p) {  _setup.current_pocket = p;}
int Interp::get_toolchange_flag() { return _setup.toolchange_flag;}
void Interp::set_toolchange_flag(bool f) {  _setup.toolchange_flag = f;}
double Interp::get_return_value()  { return _setup.return_value;}
int Interp::get_remap_level()  { return _setup.stack_level;}

//BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(execute_overloads, execute, 1, 2)
int demovar;
int get_demo() {return demovar;}
void set_demo(int i) {demovar = i;}


// http://stackoverflow.com/questions/2348960/how-can-i-use-boostpython-to-add-a-method-to-an-exported-class-without-modifyin
//     print "do = ",InterpMod.interp.do(123)
int do_things_to_a_foo(Interp& self, int a)
{
    //  fprintf(stderr,"doThings %d",self.get_setup().sequence_number);
    return 4711 * a;
}

// struct InterpParameters
// {
//   double val;

//   double getitem(bp::object subscriptionObj)
//   {
//       fprintf(stderr,"getitem %f",this->val);

//     return this->val;
//   }

//   double setitem(bp::object subscriptionObj, double val)
//   {
//       fprintf(stderr,"setitem %f",val);

//       this->val = val;
//       return val *3.14;
//   }
// };

struct Subscriptable
{

    Subscriptable()
    {
	fprintf(stderr,"new\n");
    }
    double val;

    double getitem(bp::object subscriptionObj)
    {
	fprintf(stderr,"getitem\n");

	return this->val;
    }

    double setitem(bp::object subscriptionObj, double val)
    {

	if(PyObject_IsInstance(subscriptionObj.ptr(),
			       (PyObject*)&PyString_Type))  {

	    char const* c_str = bp::extract < char const* > (subscriptionObj);
	    fprintf(stderr,"setitem('%s'), %f\n",c_str,val);

	} else
	    if(PyObject_IsInstance(subscriptionObj.ptr(),
				   (PyObject*)&PyInt_Type)) {
		int ival = bp::extract < int > (subscriptionObj);
		fprintf(stderr,"setitem(%d), %f\n",ival,val);

	    } else
		fprintf(stderr,"setitem BAD TYPE %f\n",val);

	this->val = val;
	return val *2.1718;
    }
};

const char * invite(const Interp& w) {
    return "! Please come soon!";
    //    return w.greet() + "! Please come soon!";
}

const char * blocktext(const Interp& w) {
    return &current_setup->blocktext[0];
}

BOOST_PYTHON_MODULE(InterpMod) {

    // param array access siehe http://www.spieleprogrammierer.de/index.php?page=Thread&postID=185452

    // next milestone:
    // get/set indexed param
    // get/set named param

    // using namespace boost::python;
    // bp::def("foo2",foo2);
    //    bp::def("selected_pocket",&current_setup->selected_pocket);

    // bp::scope().attr("this") = bp::object(bp::ptr(&foo));
    //bp::add_property("demo",get_demo,set_demo)

    // bp::
    // works bp::scope().attr("foo") = bp::object(4711);
    bp::object pdict =  bp::dict();


    bp::scope Interp_class =
    bp::class_<Interp>("Interp")
	.def("load_tool_table",&Interp::load_tool_table)
	.def("synch",&Interp::synch)

	.def("do", &do_things_to_a_foo)

	// the result, btw, currently is wrong.. FIXME mah:
	.def("sequence_number",&Interp::sequence_number)

	// works .setattr("TAIL_BITS", 815)
	.add_property("selected_pocket",
		      &Interp::get_selected_pocket,
		      &Interp::set_selected_pocket)
	.add_property("current_pocket",
		      &Interp::get_current_pocket,
		      &Interp::set_current_pocket)
	.add_property("toolchange_flag",
		      &Interp::get_toolchange_flag,
		      &Interp::set_toolchange_flag)
	.add_property("call_level",
		      &Interp::call_level)
	.add_property("remap_level",
		      &Interp::get_remap_level)
        .def("invite", invite)  // Add invite() as a regular function to the module.
        .add_property("invprop", invite)
        .add_property("blocktext", blocktext)


	// bombs	.def("foo3",foo3)
	//bombs	.def("anobject", bp::object(4711))

	;
	bp::scope().attr("params") = pdict;
	bp::scope().attr("foo4") = bp::object();
	bp::class_<Subscriptable>("ps")
	    .def("__getitem__", &Subscriptable::getitem)
	    .def("__setitem__", &Subscriptable::setitem);

    // bp::class_<InterpParameters>("params")
    // 	.def("__getitem__", &InterpParameters::getitem)
    // 	.def("__setitem__", &InterpParameters::setitem)

    // 	;
	// bp::class_
	// .def("param",
	//      .def("__getitem__", obj_getitem);
	//      .def("__setitem__", obj_setitem);


	// .def("__str__", to_string<eoPop<PyEO> >)

	// this segfaults on initialisation
	 // .add_static_property("itest", &Interp::staticstruct.itest)
	 // .add_static_property("dtest", &Interp::staticstruct.dtest)

	//.add_static_property("name", &Interp::statictest,&Interp::statictest)

	// .def("statictest", &Interp::statictest)
        // .staticmethod("statictest")

	//.def("execute", &Interp::execute,execute_overloads()); //??

	     //	    &(Interp::get_setup().selected_pocket);



	// TBD: how to access _setup et al
	// this probably nees a wrapper which I dont understand just yet
	// worst-case effort is a public member + wrapper

	// .add_static_property("_gees", &Interp._gees)
	// .add_static_property("_ems", &Interp._ems)
	// .add_static_property("_setup", &Interp._setup)
	// .def("f", &foo::f)
	// .staticmethod("f")
	;
}

BOOST_PYTHON_MODULE(CanonMod) {
    // first stab - result of an awk-based mass conversion.
    // just keeping those without compile errors (no missing type converters)
    bp::def("ARC_FEED",&ARC_FEED);
    //bp::def("CANON_AXIS;",&CANON_AXIS);
    //bp::def("CANON_CONTINUOUS.",&CANON_CONTINUOUS);
    //bp::def("CANON_DIRECTION;",&CANON_DIRECTION);
    //bp::def("CANON_FEED_REFERENCE;",&CANON_FEED_REFERENCE);
    //bp::def("CANON_MOTION_MODE;",&CANON_MOTION_MODE);
    //bp::def("CANON_PLANE;",&CANON_PLANE);
    //bp::def("CANON_SIDE;",&CANON_SIDE);
    //bp::def("CANON_SPEED_FEED_MODE;",&CANON_SPEED_FEED_MODE);
    //bp::def("CANON_UNITS;",&CANON_UNITS);
    // bp::def("CANON_UPDATE_END_POINT",&CANON_UPDATE_END_POINT);
    bp::def("CHANGE_TOOL",&CHANGE_TOOL);
    bp::def("CHANGE_TOOL_NUMBER",&CHANGE_TOOL_NUMBER);
    bp::def("CLAMP_AXIS",&CLAMP_AXIS);
    bp::def("CLEAR_AUX_OUTPUT_BIT",&CLEAR_AUX_OUTPUT_BIT);
    bp::def("CLEAR_MOTION_OUTPUT_BIT",&CLEAR_MOTION_OUTPUT_BIT);
    bp::def("COMMENT",&COMMENT);
    bp::def("DISABLE_ADAPTIVE_FEED",&DISABLE_ADAPTIVE_FEED);
    bp::def("DISABLE_FEED_HOLD",&DISABLE_FEED_HOLD);
    bp::def("DISABLE_FEED_OVERRIDE",&DISABLE_FEED_OVERRIDE);
    bp::def("DISABLE_SPEED_OVERRIDE",&DISABLE_SPEED_OVERRIDE);
    bp::def("DWELL",&DWELL);
    bp::def("ENABLE_ADAPTIVE_FEED",&ENABLE_ADAPTIVE_FEED);
    bp::def("ENABLE_FEED_HOLD",&ENABLE_FEED_HOLD);
    bp::def("ENABLE_FEED_OVERRIDE",&ENABLE_FEED_OVERRIDE);
    bp::def("ENABLE_SPEED_OVERRIDE",&ENABLE_SPEED_OVERRIDE);
    bp::def("FINISH",&FINISH);
    bp::def("FLOOD_OFF",&FLOOD_OFF);
    bp::def("FLOOD_ON",&FLOOD_ON);
    bp::def("GET_BLOCK_DELETE",&GET_BLOCK_DELETE);
    bp::def("GET_EXTERNAL_ADAPTIVE_FEED_ENABLE",&GET_EXTERNAL_ADAPTIVE_FEED_ENABLE);
    bp::def("GET_EXTERNAL_ANALOG_INPUT",&GET_EXTERNAL_ANALOG_INPUT);
    //    bp::def("GET_EXTERNAL_ANGLE_UNIT_FACTOR",&GET_EXTERNAL_ANGLE_UNIT_FACTOR);
    bp::def("GET_EXTERNAL_ANGLE_UNITS",&GET_EXTERNAL_ANGLE_UNITS);
    bp::def("GET_EXTERNAL_AXIS_MASK",&GET_EXTERNAL_AXIS_MASK);
    bp::def("GET_EXTERNAL_DIGITAL_INPUT",&GET_EXTERNAL_DIGITAL_INPUT);
    bp::def("GET_EXTERNAL_FEED_HOLD_ENABLE",&GET_EXTERNAL_FEED_HOLD_ENABLE);
    bp::def("GET_EXTERNAL_FEED_OVERRIDE_ENABLE",&GET_EXTERNAL_FEED_OVERRIDE_ENABLE);
    bp::def("GET_EXTERNAL_FEED_RATE",&GET_EXTERNAL_FEED_RATE);
    bp::def("GET_EXTERNAL_FLOOD",&GET_EXTERNAL_FLOOD);
    //    bp::def("GET_EXTERNAL_LENGTH_UNIT_FACTOR",&GET_EXTERNAL_LENGTH_UNIT_FACTOR);
    bp::def("GET_EXTERNAL_LENGTH_UNITS",&GET_EXTERNAL_LENGTH_UNITS);
    bp::def("GET_EXTERNAL_MIST",&GET_EXTERNAL_MIST);
    bp::def("GET_EXTERNAL_MOTION_CONTROL_MODE",&GET_EXTERNAL_MOTION_CONTROL_MODE);
    bp::def("GET_EXTERNAL_MOTION_CONTROL_TOLERANCE",&GET_EXTERNAL_MOTION_CONTROL_TOLERANCE);
    // bp::def("GET_EXTERNAL_ORIGIN_A",&GET_EXTERNAL_ORIGIN_A);
    // bp::def("GET_EXTERNAL_ORIGIN_B",&GET_EXTERNAL_ORIGIN_B);
    // bp::def("GET_EXTERNAL_ORIGIN_C",&GET_EXTERNAL_ORIGIN_C);
    // bp::def("GET_EXTERNAL_ORIGIN_X",&GET_EXTERNAL_ORIGIN_X);
    // bp::def("GET_EXTERNAL_ORIGIN_Y",&GET_EXTERNAL_ORIGIN_Y);
    // bp::def("GET_EXTERNAL_ORIGIN_Z",&GET_EXTERNAL_ORIGIN_Z);
    bp::def("GET_EXTERNAL_PARAMETER_FILE_NAME",&GET_EXTERNAL_PARAMETER_FILE_NAME);
    bp::def("GET_EXTERNAL_PLANE",&GET_EXTERNAL_PLANE);
    bp::def("GET_EXTERNAL_POCKETS_MAX",&GET_EXTERNAL_POCKETS_MAX);
    bp::def("GET_EXTERNAL_POSITION_A",&GET_EXTERNAL_POSITION_A);
    bp::def("GET_EXTERNAL_POSITION_B",&GET_EXTERNAL_POSITION_B);
    bp::def("GET_EXTERNAL_POSITION_C",&GET_EXTERNAL_POSITION_C);
    bp::def("GET_EXTERNAL_POSITION_U",&GET_EXTERNAL_POSITION_U);
    bp::def("GET_EXTERNAL_POSITION_V",&GET_EXTERNAL_POSITION_V);
    bp::def("GET_EXTERNAL_POSITION_W",&GET_EXTERNAL_POSITION_W);
    bp::def("GET_EXTERNAL_POSITION_X",&GET_EXTERNAL_POSITION_X);
    bp::def("GET_EXTERNAL_POSITION_Y",&GET_EXTERNAL_POSITION_Y);
    bp::def("GET_EXTERNAL_POSITION_Z",&GET_EXTERNAL_POSITION_Z);
    bp::def("GET_EXTERNAL_PROBE_POSITION_A",&GET_EXTERNAL_PROBE_POSITION_A);
    bp::def("GET_EXTERNAL_PROBE_POSITION_B",&GET_EXTERNAL_PROBE_POSITION_B);
    bp::def("GET_EXTERNAL_PROBE_POSITION_C",&GET_EXTERNAL_PROBE_POSITION_C);
    bp::def("GET_EXTERNAL_PROBE_POSITION_U",&GET_EXTERNAL_PROBE_POSITION_U);
    bp::def("GET_EXTERNAL_PROBE_POSITION_V",&GET_EXTERNAL_PROBE_POSITION_V);
    bp::def("GET_EXTERNAL_PROBE_POSITION_W",&GET_EXTERNAL_PROBE_POSITION_W);
    bp::def("GET_EXTERNAL_PROBE_POSITION_X",&GET_EXTERNAL_PROBE_POSITION_X);
    bp::def("GET_EXTERNAL_PROBE_POSITION_Y",&GET_EXTERNAL_PROBE_POSITION_Y);
    bp::def("GET_EXTERNAL_PROBE_POSITION_Z",&GET_EXTERNAL_PROBE_POSITION_Z);
    bp::def("GET_EXTERNAL_PROBE_TRIPPED_VALUE",&GET_EXTERNAL_PROBE_TRIPPED_VALUE);
    bp::def("GET_EXTERNAL_PROBE_VALUE",&GET_EXTERNAL_PROBE_VALUE);
    bp::def("GET_EXTERNAL_QUEUE_EMPTY",&GET_EXTERNAL_QUEUE_EMPTY);
    bp::def("GET_EXTERNAL_SELECTED_TOOL_SLOT",&GET_EXTERNAL_SELECTED_TOOL_SLOT);
    bp::def("GET_EXTERNAL_SPEED",&GET_EXTERNAL_SPEED);
    bp::def("GET_EXTERNAL_SPINDLE",&GET_EXTERNAL_SPINDLE);
    bp::def("GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE",&GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE);
    bp::def("GET_EXTERNAL_TC_FAULT",&GET_EXTERNAL_TC_FAULT);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_AOFFSET",&GET_EXTERNAL_TOOL_LENGTH_AOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_BOFFSET",&GET_EXTERNAL_TOOL_LENGTH_BOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_COFFSET",&GET_EXTERNAL_TOOL_LENGTH_COFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_UOFFSET",&GET_EXTERNAL_TOOL_LENGTH_UOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_VOFFSET",&GET_EXTERNAL_TOOL_LENGTH_VOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_WOFFSET",&GET_EXTERNAL_TOOL_LENGTH_WOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_XOFFSET",&GET_EXTERNAL_TOOL_LENGTH_XOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_YOFFSET",&GET_EXTERNAL_TOOL_LENGTH_YOFFSET);
    bp::def("GET_EXTERNAL_TOOL_LENGTH_ZOFFSET",&GET_EXTERNAL_TOOL_LENGTH_ZOFFSET);
    bp::def("GET_EXTERNAL_TOOL_SLOT",&GET_EXTERNAL_TOOL_SLOT);
    bp::def("GET_EXTERNAL_TOOL_TABLE",&GET_EXTERNAL_TOOL_TABLE);
    bp::def("GET_EXTERNAL_TRAVERSE_RATE",&GET_EXTERNAL_TRAVERSE_RATE);
    bp::def("GET_OPTIONAL_PROGRAM_STOP",&GET_OPTIONAL_PROGRAM_STOP);
    bp::def("INIT_CANON",&INIT_CANON);
    bp::def("INTERP_ABORT",&INTERP_ABORT);
    bp::def("LOCK_ROTARY",&LOCK_ROTARY);
    //    bp::def("LOCK_SPINDLE_Z",&LOCK_SPINDLE_Z);
    bp::def("LOGAPPEND",&LOGAPPEND);
    bp::def("LOGCLOSE",&LOGCLOSE);
    bp::def("LOG",&LOG);
    bp::def("LOGOPEN",&LOGOPEN);
    bp::def("MESSAGE",&MESSAGE);
    bp::def("MIST_OFF",&MIST_OFF);
    bp::def("MIST_ON",&MIST_ON);
    // bp::def("NURB_CONTROL_POINT",&NURB_CONTROL_POINT);
    //  bp::def("NURB_FEED",&NURB_FEED);
    // bp::def("NURB_KNOT_VECTOR",&NURB_KNOT_VECTOR);
    bp::def("NURBS_FEED",&NURBS_FEED);
    bp::def("OPTIONAL_PROGRAM_STOP",&OPTIONAL_PROGRAM_STOP);
    // bp::def("ORIENT_SPINDLE",&ORIENT_SPINDLE);
    bp::def("PALLET_SHUTTLE",&PALLET_SHUTTLE);
    bp::def("PROGRAM_END",&PROGRAM_END);
    bp::def("PROGRAM_STOP",&PROGRAM_STOP);
    bp::def("RIGID_TAP",&RIGID_TAP);
    bp::def("SELECT_PLANE",&SELECT_PLANE);
    bp::def("SELECT_POCKET",&SELECT_POCKET);
    bp::def("SET_AUX_OUTPUT_BIT",&SET_AUX_OUTPUT_BIT);
    bp::def("SET_AUX_OUTPUT_VALUE",&SET_AUX_OUTPUT_VALUE);
    bp::def("SET_BLOCK_DELETE",&SET_BLOCK_DELETE);
    bp::def("SET_CUTTER_RADIUS_COMPENSATION",&SET_CUTTER_RADIUS_COMPENSATION);
    bp::def("SET_FEED_MODE",&SET_FEED_MODE);
    bp::def("SET_FEED_RATE",&SET_FEED_RATE);
    //    bp::def("SET_FEED_REFERENCE",&SET_FEED_REFERENCE);
    bp::def("SET_G5X_OFFSET",&SET_G5X_OFFSET);
    bp::def("SET_G92_OFFSET",&SET_G92_OFFSET);
    //   bp::def("SET_MOTION_CONTROL_MODE",&SET_MOTION_CONTROL_MODE);
    bp::def("SET_MOTION_OUTPUT_BIT",&SET_MOTION_OUTPUT_BIT);
    bp::def("SET_MOTION_OUTPUT_VALUE",&SET_MOTION_OUTPUT_VALUE);
    bp::def("SET_NAIVECAM_TOLERANCE",&SET_NAIVECAM_TOLERANCE);
    bp::def("SET_OPTIONAL_PROGRAM_STOP",&SET_OPTIONAL_PROGRAM_STOP);
    bp::def("SET_SPINDLE_MODE",&SET_SPINDLE_MODE);
    bp::def("SET_SPINDLE_SPEED",&SET_SPINDLE_SPEED);
    bp::def("SET_TOOL_TABLE_ENTRY",&SET_TOOL_TABLE_ENTRY);
    bp::def("SET_TRAVERSE_RATE",&SET_TRAVERSE_RATE);
    bp::def("SET_XY_ROTATION",&SET_XY_ROTATION);
    bp::def("SPINDLE_RETRACT",&SPINDLE_RETRACT);
    bp::def("SPINDLE_RETRACT_TRAVERSE",&SPINDLE_RETRACT_TRAVERSE);
    bp::def("START_CHANGE",&START_CHANGE);
    bp::def("START_CUTTER_RADIUS_COMPENSATION",&START_CUTTER_RADIUS_COMPENSATION);
    bp::def("START_SPEED_FEED_SYNCH",&START_SPEED_FEED_SYNCH);
    bp::def("START_SPINDLE_CLOCKWISE",&START_SPINDLE_CLOCKWISE);
    bp::def("START_SPINDLE_COUNTERCLOCKWISE",&START_SPINDLE_COUNTERCLOCKWISE);
    bp::def("STOP_CUTTER_RADIUS_COMPENSATION",&STOP_CUTTER_RADIUS_COMPENSATION);
    bp::def("STOP_SPEED_FEED_SYNCH",&STOP_SPEED_FEED_SYNCH);
    bp::def("STOP_SPINDLE_TURNING",&STOP_SPINDLE_TURNING);
    // bp::def("STOP",&STOP);
    bp::def("STRAIGHT_FEED",&STRAIGHT_FEED);
    bp::def("STRAIGHT_PROBE",&STRAIGHT_PROBE);
    bp::def("STRAIGHT_TRAVERSE",&STRAIGHT_TRAVERSE);
    bp::def("TURN_PROBE_OFF",&TURN_PROBE_OFF);
    bp::def("TURN_PROBE_ON",&TURN_PROBE_ON);
    // bp::def("UNCLAMP_AXIS",&UNCLAMP_AXIS);
    bp::def("UNLOCK_ROTARY",&UNLOCK_ROTARY);
    bp::def("USE_LENGTH_UNITS",&USE_LENGTH_UNITS);
    bp::def("USE_NO_SPINDLE_FORCE",&USE_NO_SPINDLE_FORCE);
    // bp::def("USER_DEFINED_FUNCTION_ADD",&USER_DEFINED_FUNCTION_ADD);
    // bp::def("USE_SPINDLE_FORCE",&USE_SPINDLE_FORCE);
    bp::def("USE_TOOL_LENGTH_OFFSET",&USE_TOOL_LENGTH_OFFSET);
    bp::def("WAIT",&WAIT);
    //    bp::def("XYZ",&XYZ);
}

int Interp::init_python(setup_pointer settings)
{
    char cmd[LINELEN];
    char *path;

    //Log("get_setup().selected_pocket=%d",get_setup().selected_pocket);
    current_setup = &this->_setup;

    if (settings->pymodule_stat != PYMOD_NONE)
	return INTERP_OK;  // already done, or failed

    logPy("init_python(this=%lx _setup=%lx pid=%d)",
	  (unsigned long)this, (unsigned long)&this->_setup,getpid());

    PYCHK((settings->pymodule == NULL),
	  "init_python: no module defined");

    if (settings->pydir) {
	snprintf(cmd,sizeof(cmd),"%s/%s", settings->pydir,settings->pymodule);
    } else
	strcpy(cmd,settings->pymodule);

    PYCHK(((path = realpath(cmd,NULL)) == NULL),
	  "init_python: can resolve path to '%s'",cmd);
    logPy("module path='%s'",path);

    Py_SetProgramName(path);
    PyImport_AppendInittab( (char *) "InterpMod", &initInterpMod);
    PyImport_AppendInittab( (char *) "CanonMod", &initCanonMod);
    Py_Initialize();

    try {
	settings->module = bp::import("__main__");
	settings->module_namespace = settings->module.attr("__dict__");

	bp::object interp_module = bp::import("InterpMod");
	bp::scope(interp_module).attr("interp") = bp::ptr(this);
	settings->module_namespace["InterpMod"] = interp_module;

	bp::object canon_module = bp::import("CanonMod");
	settings->module_namespace["CanonMod"] = canon_module;

	bp::exec_file(path,
		      settings->module_namespace,
		      settings->module_namespace);

	settings->pymodule_stat = PYMOD_OK;
	free(path);
    }
    catch (bp::error_already_set) {
	logPy("init_python: Exception");

	PyErr_Print();
	PyErr_Clear();
	settings->pymodule_stat = PYMOD_FAILED;
    }
    return INTERP_OK;

 error:
    return INTERP_ERROR;
}

bool Interp::is_pycallable(setup_pointer settings,
			   const char *funcname)
{
    bool result;
    PyObject *func, *exc, *val, *tbk;

    if ((settings->pymodule_stat != PYMOD_OK) ||
	(funcname == NULL)) {
	return false;
    }

    func = PyDict_GetItemString(settings->module_namespace.ptr(), funcname);
    result = PyCallable_Check(func);
    if (PyErr_Occurred()) {
	PyErr_Print();
	PyErr_Fetch(&exc, &val, &tbk);
	Log("exception calling '%s.%s': %s",
	    settings->pymodule,
	    funcname,
	    PyString_AsString(val));
	PyErr_Clear();
	result = false;
    }
    logPy("py_iscallable(%s) = %s\n",funcname,result ? "TRUE":"FALSE");
    return result;
}

static bp::object callobject(bp::object c, bp::object args, bp::object kwds)
{
    return c(*args, **kwds);
}


int Interp::pycall(setup_pointer settings,
		   const char *funcname,
		   double params[])
{
    PyObject *exc, *val, *tbk;
    bp::object retval;

    logPy("pycall %s [%f] [%f] this=%lx\n",
	    funcname,params[0],params[1],(unsigned long)this);

    current_setup = &this->_setup;

    if (settings->pymodule_stat != PYMOD_OK) {
	ERS("function '%s.%s' : module not initialized",
	    settings->pymodule,funcname);
	return INTERP_OK;
    }

    try {
	bp::object function = settings->module_namespace[funcname];
	bp::list plist;
	for (int i = 0; i < 30; i++) {
	    plist.append(params[i]);
	}
	retval = callobject(function,bp::make_tuple(plist),
				       settings->kwargs);
    }
    catch (bp::error_already_set) {
	PyErr_Print();
	PyErr_Fetch(&exc, &val, &tbk);
	ERS("pycall: exception calling '%s.%s': %s",
	    settings->pymodule,
	    funcname,
	    PyString_AsString(val));
	PyErr_Clear();
	return INTERP_OK;
    }

    if (retval.ptr() != Py_None) {
	if (!PyFloat_Check(retval.ptr())) {
	    PyObject *res_str = PyObject_Str(retval.ptr());
	    ERS("function '%s.%s' returned '%s' - expected float, got %s",
		settings->pymodule,funcname,
		PyString_AsString(res_str),
		retval.ptr()->ob_type->tp_name);
	    Py_XDECREF(res_str);
	    return INTERP_OK;
	} else {
	    settings->return_value = bp::extract<double>(retval);
	    logPy("pycall: '%s' returned %f", funcname,settings->return_value);
	}
    } else {
	logPy("pycall: '%s' returned no value",funcname);
    }
    return INTERP_OK;
}
