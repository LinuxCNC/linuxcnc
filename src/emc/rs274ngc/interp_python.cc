// FIXME mah - exception handling still a mess.


// Support for Python oword subroutines
//
// proof-of-concept for access to Interp and canon
//
// NB: all this is executed at readahead time
//
// Michael Haberler 4/2011
//
// if you get a segfault like described
// here: https://bugs.launchpad.net/ubuntu/+source/mesa/+bug/259219
// or here: https://www.libavg.de/wiki/LinuxInstallIssues#glibc_invalid_pointer :
//
// try this before starting milltask and axis in emc:
//  LD_PRELOAD=/usr/lib/libstdc++.so.6 $EMCTASK ...
//  LD_PRELOAD=/usr/lib/libstdc++.so.6 $EMCDISPLAY ...
//
// this is actually a bug in libgl1-mesa-dri and it looks
// it has been fixed in mesa - 7.10.1-0ubuntu2

// TODO:
// method to re-import the py module (easier to debug without restarting Axis)
// block access


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
#include <exception>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"

static bool useGIL;     // not sure if this is needed

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
// option 1 is making all member data public
// option 2 is this:
// we store references to private data during call setup and
// use it to reference it in the python module
// NB: this is likely not thread-safe
static setup_pointer current_setup;

// the Interp instance, set on call before handing to Python
static Interp *current_interp;


// http://hafizpariabi.blogspot.com/2008/01/using-custom-deallocator-in.html
// reason: avoid segfaults by del(interp_instance) on program exit
// make delete(interp_instance) a noop wrt Interp
static void interpDeallocFunc(Interp *interp) {}

// the boost-wrapped Interp instance
typedef boost::shared_ptr< Interp > interp_ptr;

typedef boost::shared_ptr< block > block_ptr;



#define IS_STRING(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyString_Type))
#define IS_INT(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyInt_Type))

struct ParamClass
{
    double getitem(bp::object sub)
    {
	double retval = 0;
	if (IS_STRING(sub)) {
	    char const* varname = bp::extract < char const* > (sub);
	    int status;
	    current_interp->find_named_param(varname, &status, &retval);
	    if (!status)
		throw std::runtime_error("parameter does not exist: " + std::string(varname));
	} else
	    if (IS_INT(sub)) {
		int index = bp::extract < int > (sub);
		retval = current_setup->parameters[index];
	    } else {
		throw std::runtime_error("params subscript type must be integer or string");
	    }
	return retval;
    }

    double setitem(bp::object sub, double dvalue)
    {
	if (IS_STRING(sub)) {
	    char const* varname = bp::extract < char const* > (sub);
	    int status = current_interp->add_named_param(varname, varname[0] == '_' ? PA_GLOBAL :0);
	    status = current_interp->store_named_param(current_setup,varname,
							   dvalue, 0);
	    if (status != INTERP_OK)
		throw std::runtime_error("cant assign value to parameter: " + std::string(varname));

	} else
	    if (IS_INT(sub)) {
		int index = bp::extract < int > (sub);
		current_setup->parameters[index] = dvalue;
		return dvalue;
	    } else
		throw std::runtime_error("params subscript type must be integer or string");
	return dvalue;
    }
};

static ParamClass paramclass;

struct wrap_block : public block
{
    char const *get_comment() const { return comment; }
    char const *get_o_name() const { return o_name; }

    //const bp::object get_tuple() const {return bp::make_tuple(123, 'D', "Hello, World", 0.0);}

    //http://mail.python.org/pipermail/cplusplus-sig/2007-January/011602.html
    const bp::list get_g_modes() {
	bp::list g_modes_list;
	for (unsigned int i = 0; i < sizeof(g_modes)/sizeof(g_modes[0]); i++)
	    g_modes_list.append(g_modes[i]);
	return g_modes_list;
    }

    const bp::list get_m_modes() {
	bp::list m_modes_list;
	for (unsigned int i = 0; i < sizeof(m_modes)/sizeof(m_modes[0]); i++)
	    m_modes_list.append(m_modes[i]);
	return m_modes_list;
    }
    const bp::list get_params() {
	bp::list params_list;
	for (unsigned int i = 0; i < sizeof(params)/sizeof(params[0]); i++)
	    params_list.append(params[i]);
	return params_list;
    }
};

struct wrap_remap : public remap {
};

struct wrap_context : public context {
};

static void wrap_canon_error(const char *s)
{
    if ((s == NULL) && !strlen(s))
	return;
    CANON_ERROR(s);
}

static void wrapERS(Interp &x, const char *s)
{
    if ((s == NULL) && !strlen(s))
	s = "###";

    current_interp->setError (s);
    current_setup->stack_index = 0;
    strncpy(current_setup->stack[current_setup->stack_index],
	    "Python", STACK_ENTRY_LEN);
    current_setup->stack[current_setup->stack_index][STACK_ENTRY_LEN-1] = 0;
    current_setup->stack_index++;
    current_setup->stack[current_setup->stack_index][0] = 0;
}

static bp::object wrap_find_tool_pocket(Interp &x, int toolno)
{
    int status, pocket;
    status = current_interp->find_tool_pocket(current_setup, toolno, &pocket);
    return bp::make_tuple(status, pocket);
}

BOOST_PYTHON_MODULE(InterpMod) {
    using namespace boost::python;
    using namespace boost;

    scope().attr("__doc__") =
        "Interpreter introspection\n"
        ;


    // def("remapping", (wrap_remap *)&wrap_remapping);

    // http://snipplr.com/view/6447/boostpython-sample-code-exposing-classes/
    //class_<block,boost::noncopyable,bases<block>>("block", no_init);

    class_ <wrap_context,noncopyable>("wrap_context",no_init)
	.def_readwrite("position", &wrap_context::position)
	.def_readwrite("sequence_number", &wrap_context::sequence_number)
	.def_readwrite("filename",  &wrap_context::filename)
	.def_readwrite("subname",  &wrap_context::subName)
	// need wrapper for saved_params
	//.def_readwrite("kwargs", &wrap_context::kwargs)
    ;

    class_ <wrap_remap,noncopyable>("wrap_remap",no_init)
	.def_readwrite("name",&wrap_remap::name)
	//	.def_readwrite("op",&wrap_remap::op)
	.def_readwrite("modal_group",&wrap_remap::modal_group)
	.def_readwrite("prolog_func",&wrap_remap::prolog_func)
	.def_readwrite("remap_py",&wrap_remap::remap_py)
	.def_readwrite("remap_ngc",&wrap_remap::remap_ngc)
	.def_readwrite("epilog_func",&wrap_remap::epilog_func)
    ;

    class_ <wrap_block,noncopyable>("wrap_block",no_init)
	.def_readwrite("f_flag",&wrap_block::f_flag)
	.def_readwrite("p_flag",&wrap_block::p_flag)
	.def_readwrite("p_number",&wrap_block::p_number)
	.def_readwrite("a_flag",&wrap_block::a_flag)
	.def_readwrite("a_number",&wrap_block::a_number)
	.def_readwrite("b_flag",&wrap_block::b_flag)
	.def_readwrite("b_number",&wrap_block::b_number)
	.def_readwrite("c_flag",&wrap_block::c_flag)
	.def_readwrite("c_number",&wrap_block::c_number)
	.def_readwrite("d_number_float",&wrap_block::d_number_float)
	.def_readwrite("d_flag",&wrap_block::d_flag)
	.def_readwrite("e_flag",&wrap_block::e_flag)
	.def_readwrite("e_number",&wrap_block::e_number)
	.def_readwrite("f_flag",&wrap_block::f_flag)
	.def_readwrite("f_number",&wrap_block::f_number)
	.def_readwrite("h_flag",&wrap_block::h_flag)
	.def_readwrite("h_number",&wrap_block::h_number)
	.def_readwrite("i_flag",&wrap_block::i_flag)
	.def_readwrite("i_number",&wrap_block::i_number)
	.def_readwrite("j_flag",&wrap_block::j_flag)
	.def_readwrite("j_number",&wrap_block::j_number)
	.def_readwrite("k_flag",&wrap_block::k_flag)
	.def_readwrite("k_number",&wrap_block::k_number)
	.def_readwrite("l_number",&wrap_block::l_number)
	.def_readwrite("l_flag",&wrap_block::l_flag)
	.def_readwrite("line_number",&wrap_block::line_number)
	.def_readwrite("n_number",&wrap_block::n_number)
	.def_readwrite("motion_to_be",&wrap_block::motion_to_be)
	.def_readwrite("m_count",&wrap_block::m_count)
	.def_readwrite("user_m",&wrap_block::user_m)
	.def_readwrite("p_number",&wrap_block::p_number)
	.def_readwrite("p_flag",&wrap_block::p_flag)
	.def_readwrite("q_number",&wrap_block::q_number)
	.def_readwrite("q_flag",&wrap_block::q_flag)
	.def_readwrite("r_flag",&wrap_block::r_flag)
	.def_readwrite("r_number",&wrap_block::r_number)
	.def_readwrite("s_flag",&wrap_block::s_flag)
	.def_readwrite("s_number",&wrap_block::s_number)
	.def_readwrite("t_flag",&wrap_block::t_flag)
	.def_readwrite("t_number",&wrap_block::t_number)
	.def_readwrite("u_flag",&wrap_block::u_flag)
	.def_readwrite("u_number",&wrap_block::u_number)
	.def_readwrite("v_flag",&wrap_block::v_flag)
	.def_readwrite("v_number",&wrap_block::v_number)
	.def_readwrite("w_flag",&wrap_block::w_flag)
	.def_readwrite("w_number",&wrap_block::w_number)
	.def_readwrite("x_flag",&wrap_block::x_flag)
	.def_readwrite("x_number",&wrap_block::x_number)
	.def_readwrite("y_flag",&wrap_block::y_flag)
	.def_readwrite("y_number",&wrap_block::y_number)
	.def_readwrite("z_flag",&wrap_block::z_flag)
	.def_readwrite("z_number",&wrap_block::z_number)
	.def_readwrite("radius_flag",&wrap_block::radius_flag)
	.def_readwrite("radius",&wrap_block::radius)
	.def_readwrite("theta_flag",&wrap_block::theta_flag)
	.def_readwrite("theta",&wrap_block::theta)

	//http://mail.python.org/pipermail/cplusplus-sig/2005-July/008840.html
	.def_readwrite("offset",&wrap_block::offset)
	.def_readwrite("o_type",&wrap_block::o_type)

	// currently read-only
	.add_property("comment", &wrap_block::get_comment)
	.add_property("o_name", &wrap_block::get_o_name)
	.add_property("g_modes", &wrap_block::get_g_modes)
	.add_property("m_modes", &wrap_block::get_m_modes)
	.add_property("params", &wrap_block::get_params)
	;

    class_< Interp, interp_ptr,
        noncopyable >("Interp",no_init)

	.def("sequence_number", &Interp::sequence_number)
	.def("load_tool_table", &Interp::load_tool_table)
	.def("synch", &Interp::synch)
	.def("push_errormsg", &wrapERS)

	.def("find_tool_pocket", &wrap_find_tool_pocket)

	// .def("add_named_param", &Interp::add_named_param)
	// .def("store_named_param", &Interp::store_named_param)

	// .def_readonly("name", &Var::name)

	.def_readwrite("blocktext", (char *) &current_setup->blocktext)
	.def_readwrite("call_level", &current_setup->call_level)
	.def_readwrite("callframe",
		       (wrap_context *)&current_setup->sub_context[current_setup->call_level])

	.def_readwrite("cblock", (wrap_block *) &current_setup->blocks[current_setup->remap_level])
	.def_readwrite("current_pocket", &current_setup->current_pocket)
	.def_readwrite("debugmask", &current_setup->debugmask)
	.def_readwrite("eblock",  (wrap_block *)&current_setup->blocks[0])
	.def_readwrite("input_digital", &current_setup->input_digital)
	.def_readwrite("input_flag", &current_setup->input_flag)
	.def_readwrite("input_index", &current_setup->input_index)
	.def_readwrite("mdi_interrupt", &current_setup->mdi_interrupt)
	.def_readwrite("pockets_max", &current_setup->pockets_max)
	.def_readwrite("probe_flag", &current_setup->probe_flag)
	.def_readwrite("remap_level", &current_setup->remap_level)
	.def_readwrite("return_value", &current_setup->return_value)
	.def_readwrite("selected_pocket", &current_setup->selected_pocket)
	.def_readwrite("toolchange_flag", &current_setup->toolchange_flag)

	// still broken
	//.def_readwrite("remap", (wrap_remap *)&current_setup->blocks[0].current_remap)


	;

    class_<ParamClass,noncopyable>("ParamClass","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
	.def("__setitem__", &ParamClass::setitem);
    ;

    //.def("execute", &Interp::execute,execute_overloads()); //??
}

BOOST_PYTHON_MODULE(CanonMod) {
    using namespace boost::python;

    // first stab - result of an awk-based mass conversion.
    // just keeping those without compile errors (no missing type converters)
    def("ARC_FEED",&ARC_FEED);
    //def("CANON_AXIS;",&CANON_AXIS);
    //def("CANON_CONTINUOUS.",&CANON_CONTINUOUS);
    //def("CANON_DIRECTION;",&CANON_DIRECTION);
    def("CANON_ERROR",&wrap_canon_error);
    //def("CANON_FEED_REFERENCE;",&CANON_FEED_REFERENCE);
    //def("CANON_MOTION_MODE;",&CANON_MOTION_MODE);
    //def("CANON_PLANE;",&CANON_PLANE);
    //def("CANON_SIDE;",&CANON_SIDE);
    //def("CANON_SPEED_FEED_MODE;",&CANON_SPEED_FEED_MODE);
    //def("CANON_UNITS;",&CANON_UNITS);
    // def("CANON_UPDATE_END_POINT",&CANON_UPDATE_END_POINT);
    def("CHANGE_TOOL",&CHANGE_TOOL);
    def("CHANGE_TOOL_NUMBER",&CHANGE_TOOL_NUMBER);
    def("CLAMP_AXIS",&CLAMP_AXIS);
    def("CLEAR_AUX_OUTPUT_BIT",&CLEAR_AUX_OUTPUT_BIT);
    def("CLEAR_MOTION_OUTPUT_BIT",&CLEAR_MOTION_OUTPUT_BIT);
    def("COMMENT",&COMMENT);
    def("DISABLE_ADAPTIVE_FEED",&DISABLE_ADAPTIVE_FEED);
    def("DISABLE_FEED_HOLD",&DISABLE_FEED_HOLD);
    def("DISABLE_FEED_OVERRIDE",&DISABLE_FEED_OVERRIDE);
    def("DISABLE_SPEED_OVERRIDE",&DISABLE_SPEED_OVERRIDE);
    def("DWELL",&DWELL);
    def("ENABLE_ADAPTIVE_FEED",&ENABLE_ADAPTIVE_FEED);
    def("ENABLE_FEED_HOLD",&ENABLE_FEED_HOLD);
    def("ENABLE_FEED_OVERRIDE",&ENABLE_FEED_OVERRIDE);
    def("ENABLE_SPEED_OVERRIDE",&ENABLE_SPEED_OVERRIDE);
    def("FINISH",&FINISH);
    def("FLOOD_OFF",&FLOOD_OFF);
    def("FLOOD_ON",&FLOOD_ON);
    def("GET_BLOCK_DELETE",&GET_BLOCK_DELETE);
    def("GET_EXTERNAL_ADAPTIVE_FEED_ENABLE",&GET_EXTERNAL_ADAPTIVE_FEED_ENABLE);
    def("GET_EXTERNAL_ANALOG_INPUT",&GET_EXTERNAL_ANALOG_INPUT);
    //    def("GET_EXTERNAL_ANGLE_UNIT_FACTOR",&GET_EXTERNAL_ANGLE_UNIT_FACTOR);
    def("GET_EXTERNAL_ANGLE_UNITS",&GET_EXTERNAL_ANGLE_UNITS);
    def("GET_EXTERNAL_AXIS_MASK",&GET_EXTERNAL_AXIS_MASK);
    def("GET_EXTERNAL_DIGITAL_INPUT",&GET_EXTERNAL_DIGITAL_INPUT);
    def("GET_EXTERNAL_FEED_HOLD_ENABLE",&GET_EXTERNAL_FEED_HOLD_ENABLE);
    def("GET_EXTERNAL_FEED_OVERRIDE_ENABLE",&GET_EXTERNAL_FEED_OVERRIDE_ENABLE);
    def("GET_EXTERNAL_FEED_RATE",&GET_EXTERNAL_FEED_RATE);
    def("GET_EXTERNAL_FLOOD",&GET_EXTERNAL_FLOOD);
    //    def("GET_EXTERNAL_LENGTH_UNIT_FACTOR",&GET_EXTERNAL_LENGTH_UNIT_FACTOR);
    def("GET_EXTERNAL_LENGTH_UNITS",&GET_EXTERNAL_LENGTH_UNITS);
    def("GET_EXTERNAL_MIST",&GET_EXTERNAL_MIST);
    def("GET_EXTERNAL_MOTION_CONTROL_MODE",&GET_EXTERNAL_MOTION_CONTROL_MODE);
    def("GET_EXTERNAL_MOTION_CONTROL_TOLERANCE",&GET_EXTERNAL_MOTION_CONTROL_TOLERANCE);
    // def("GET_EXTERNAL_ORIGIN_A",&GET_EXTERNAL_ORIGIN_A);
    // def("GET_EXTERNAL_ORIGIN_B",&GET_EXTERNAL_ORIGIN_B);
    // def("GET_EXTERNAL_ORIGIN_C",&GET_EXTERNAL_ORIGIN_C);
    // def("GET_EXTERNAL_ORIGIN_X",&GET_EXTERNAL_ORIGIN_X);
    // def("GET_EXTERNAL_ORIGIN_Y",&GET_EXTERNAL_ORIGIN_Y);
    // def("GET_EXTERNAL_ORIGIN_Z",&GET_EXTERNAL_ORIGIN_Z);
    def("GET_EXTERNAL_PARAMETER_FILE_NAME",&GET_EXTERNAL_PARAMETER_FILE_NAME);
    def("GET_EXTERNAL_PLANE",&GET_EXTERNAL_PLANE);
    def("GET_EXTERNAL_POCKETS_MAX",&GET_EXTERNAL_POCKETS_MAX);
    def("GET_EXTERNAL_POSITION_A",&GET_EXTERNAL_POSITION_A);
    def("GET_EXTERNAL_POSITION_B",&GET_EXTERNAL_POSITION_B);
    def("GET_EXTERNAL_POSITION_C",&GET_EXTERNAL_POSITION_C);
    def("GET_EXTERNAL_POSITION_U",&GET_EXTERNAL_POSITION_U);
    def("GET_EXTERNAL_POSITION_V",&GET_EXTERNAL_POSITION_V);
    def("GET_EXTERNAL_POSITION_W",&GET_EXTERNAL_POSITION_W);
    def("GET_EXTERNAL_POSITION_X",&GET_EXTERNAL_POSITION_X);
    def("GET_EXTERNAL_POSITION_Y",&GET_EXTERNAL_POSITION_Y);
    def("GET_EXTERNAL_POSITION_Z",&GET_EXTERNAL_POSITION_Z);
    def("GET_EXTERNAL_PROBE_POSITION_A",&GET_EXTERNAL_PROBE_POSITION_A);
    def("GET_EXTERNAL_PROBE_POSITION_B",&GET_EXTERNAL_PROBE_POSITION_B);
    def("GET_EXTERNAL_PROBE_POSITION_C",&GET_EXTERNAL_PROBE_POSITION_C);
    def("GET_EXTERNAL_PROBE_POSITION_U",&GET_EXTERNAL_PROBE_POSITION_U);
    def("GET_EXTERNAL_PROBE_POSITION_V",&GET_EXTERNAL_PROBE_POSITION_V);
    def("GET_EXTERNAL_PROBE_POSITION_W",&GET_EXTERNAL_PROBE_POSITION_W);
    def("GET_EXTERNAL_PROBE_POSITION_X",&GET_EXTERNAL_PROBE_POSITION_X);
    def("GET_EXTERNAL_PROBE_POSITION_Y",&GET_EXTERNAL_PROBE_POSITION_Y);
    def("GET_EXTERNAL_PROBE_POSITION_Z",&GET_EXTERNAL_PROBE_POSITION_Z);
    def("GET_EXTERNAL_PROBE_TRIPPED_VALUE",&GET_EXTERNAL_PROBE_TRIPPED_VALUE);
    def("GET_EXTERNAL_PROBE_VALUE",&GET_EXTERNAL_PROBE_VALUE);
    def("GET_EXTERNAL_QUEUE_EMPTY",&GET_EXTERNAL_QUEUE_EMPTY);
    def("GET_EXTERNAL_SELECTED_TOOL_SLOT",&GET_EXTERNAL_SELECTED_TOOL_SLOT);
    def("GET_EXTERNAL_SPEED",&GET_EXTERNAL_SPEED);
    def("GET_EXTERNAL_SPINDLE",&GET_EXTERNAL_SPINDLE);
    def("GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE",&GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE);
    def("GET_EXTERNAL_TC_FAULT",&GET_EXTERNAL_TC_FAULT);
    def("GET_EXTERNAL_TOOL_LENGTH_AOFFSET",&GET_EXTERNAL_TOOL_LENGTH_AOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_BOFFSET",&GET_EXTERNAL_TOOL_LENGTH_BOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_COFFSET",&GET_EXTERNAL_TOOL_LENGTH_COFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_UOFFSET",&GET_EXTERNAL_TOOL_LENGTH_UOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_VOFFSET",&GET_EXTERNAL_TOOL_LENGTH_VOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_WOFFSET",&GET_EXTERNAL_TOOL_LENGTH_WOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_XOFFSET",&GET_EXTERNAL_TOOL_LENGTH_XOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_YOFFSET",&GET_EXTERNAL_TOOL_LENGTH_YOFFSET);
    def("GET_EXTERNAL_TOOL_LENGTH_ZOFFSET",&GET_EXTERNAL_TOOL_LENGTH_ZOFFSET);
    def("GET_EXTERNAL_TOOL_SLOT",&GET_EXTERNAL_TOOL_SLOT);
    def("GET_EXTERNAL_TOOL_TABLE",&GET_EXTERNAL_TOOL_TABLE);
    def("GET_EXTERNAL_TRAVERSE_RATE",&GET_EXTERNAL_TRAVERSE_RATE);
    def("GET_OPTIONAL_PROGRAM_STOP",&GET_OPTIONAL_PROGRAM_STOP);
    def("INIT_CANON",&INIT_CANON);
    def("INTERP_ABORT",&INTERP_ABORT);
    def("LOCK_ROTARY",&LOCK_ROTARY);
    //    def("LOCK_SPINDLE_Z",&LOCK_SPINDLE_Z);
    def("LOGAPPEND",&LOGAPPEND);
    def("LOGCLOSE",&LOGCLOSE);
    def("LOG",&LOG);
    def("LOGOPEN",&LOGOPEN);
    def("MESSAGE",&MESSAGE);
    def("MIST_OFF",&MIST_OFF);
    def("MIST_ON",&MIST_ON);
    // def("NURB_CONTROL_POINT",&NURB_CONTROL_POINT);
    //  def("NURB_FEED",&NURB_FEED);
    // def("NURB_KNOT_VECTOR",&NURB_KNOT_VECTOR);
    def("NURBS_FEED",&NURBS_FEED);
    def("OPTIONAL_PROGRAM_STOP",&OPTIONAL_PROGRAM_STOP);
    // def("ORIENT_SPINDLE",&ORIENT_SPINDLE);
    def("PALLET_SHUTTLE",&PALLET_SHUTTLE);
    def("PROGRAM_END",&PROGRAM_END);
    def("PROGRAM_STOP",&PROGRAM_STOP);
    def("RIGID_TAP",&RIGID_TAP);
    def("SELECT_PLANE",&SELECT_PLANE);
    def("SELECT_POCKET",&SELECT_POCKET);
    def("SET_AUX_OUTPUT_BIT",&SET_AUX_OUTPUT_BIT);
    def("SET_AUX_OUTPUT_VALUE",&SET_AUX_OUTPUT_VALUE);
    def("SET_BLOCK_DELETE",&SET_BLOCK_DELETE);
    def("SET_CUTTER_RADIUS_COMPENSATION",&SET_CUTTER_RADIUS_COMPENSATION);
    def("SET_FEED_MODE",&SET_FEED_MODE);
    def("SET_FEED_RATE",&SET_FEED_RATE);
    //    def("SET_FEED_REFERENCE",&SET_FEED_REFERENCE);
    def("SET_G5X_OFFSET",&SET_G5X_OFFSET);
    def("SET_G92_OFFSET",&SET_G92_OFFSET);
    //   def("SET_MOTION_CONTROL_MODE",&SET_MOTION_CONTROL_MODE);
    def("SET_MOTION_OUTPUT_BIT",&SET_MOTION_OUTPUT_BIT);
    def("SET_MOTION_OUTPUT_VALUE",&SET_MOTION_OUTPUT_VALUE);
    def("SET_NAIVECAM_TOLERANCE",&SET_NAIVECAM_TOLERANCE);
    def("SET_OPTIONAL_PROGRAM_STOP",&SET_OPTIONAL_PROGRAM_STOP);
    def("SET_SPINDLE_MODE",&SET_SPINDLE_MODE);
    def("SET_SPINDLE_SPEED",&SET_SPINDLE_SPEED);
    def("SET_TOOL_TABLE_ENTRY",&SET_TOOL_TABLE_ENTRY);
    def("SET_TRAVERSE_RATE",&SET_TRAVERSE_RATE);
    def("SET_XY_ROTATION",&SET_XY_ROTATION);
    def("SPINDLE_RETRACT",&SPINDLE_RETRACT);
    def("SPINDLE_RETRACT_TRAVERSE",&SPINDLE_RETRACT_TRAVERSE);
    def("START_CHANGE",&START_CHANGE);
    def("START_CUTTER_RADIUS_COMPENSATION",&START_CUTTER_RADIUS_COMPENSATION);
    def("START_SPEED_FEED_SYNCH",&START_SPEED_FEED_SYNCH);
    def("START_SPINDLE_CLOCKWISE",&START_SPINDLE_CLOCKWISE);
    def("START_SPINDLE_COUNTERCLOCKWISE",&START_SPINDLE_COUNTERCLOCKWISE);
    def("STOP_CUTTER_RADIUS_COMPENSATION",&STOP_CUTTER_RADIUS_COMPENSATION);
    def("STOP_SPEED_FEED_SYNCH",&STOP_SPEED_FEED_SYNCH);
    def("STOP_SPINDLE_TURNING",&STOP_SPINDLE_TURNING);
    // def("STOP",&STOP);
    def("STRAIGHT_FEED",&STRAIGHT_FEED);
    def("STRAIGHT_PROBE",&STRAIGHT_PROBE);
    def("STRAIGHT_TRAVERSE",&STRAIGHT_TRAVERSE);
    def("TURN_PROBE_OFF",&TURN_PROBE_OFF);
    def("TURN_PROBE_ON",&TURN_PROBE_ON);
    // def("UNCLAMP_AXIS",&UNCLAMP_AXIS);
    def("UNLOCK_ROTARY",&UNLOCK_ROTARY);
    def("USE_LENGTH_UNITS",&USE_LENGTH_UNITS);
    def("USE_NO_SPINDLE_FORCE",&USE_NO_SPINDLE_FORCE);
    // def("USER_DEFINED_FUNCTION_ADD",&USER_DEFINED_FUNCTION_ADD);
    // def("USE_SPINDLE_FORCE",&USE_SPINDLE_FORCE);
    def("USE_TOOL_LENGTH_OFFSET",&USE_TOOL_LENGTH_OFFSET);
    def("WAIT",&WAIT);
    //    def("XYZ",&XYZ);
}

// decode a Python exception into a string.
std::string handle_pyerror()
{
    using namespace boost::python;
    using namespace boost;

    PyObject *exc,*val,*tb;
    object formatted_list, formatted;
    PyErr_Fetch(&exc,&val,&tb);
    handle<> hexc(exc),hval(val),htb(allow_null(tb));
    object traceback(import("traceback"));
    if (!tb) {
	object format_exception_only(traceback.attr("format_exception_only"));
	formatted_list = format_exception_only(hexc,hval);
	// FIXME mah unsure how to correctly format this:
	formatted = str(formatted_list);
    } else {
	object format_exception(traceback.attr("format_exception"));
	formatted_list = format_exception(hexc,hval,htb);
	formatted = str("\n").join(formatted_list);
    }
    return extract<std::string>(formatted);
}

int Interp::init_python(setup_pointer settings)
{
    char cmd[LINELEN], path[PATH_MAX];
    interp_ptr  interp_instance;  // the wrapped instance
    std::string msg;
    bool py_exception = false;
    PyGILState_STATE gstate;

    current_setup = get_setup(this); // // &this->_setup;
    current_interp = this;

    if (settings->pymodule_stat != PYMOD_NONE) {
	logPy("init_python RE-INIT %d pid=%d",settings->pymodule_stat,getpid());
	return INTERP_OK;  // already done, or failed
    }
    logPy("init_python(this=%lx  pid=%d pymodule_stat=%d PyInited=%d",
	  (unsigned long)this, getpid(), settings->pymodule_stat, Py_IsInitialized());

    // the null deallocator avoids destroying the Interp instance on
    // leaving scope (or shutdown)
    interp_instance = interp_ptr(this,interpDeallocFunc  );

    PYCHK((settings->pymodule == NULL), "init_python: no module defined");
    if (settings->pydir) {
	snprintf(cmd,sizeof(cmd),"%s/%s", settings->pydir,settings->pymodule);
    } else
	strcpy(cmd,settings->pymodule);
    PYCHK(((realpath(cmd,path)) == NULL), "init_python: can resolve path to '%s'",cmd);
    Py_SetProgramName(path);
    PyImport_AppendInittab( (char *) "InterpMod", &initInterpMod);
    PyImport_AppendInittab( (char *) "CanonMod", &initCanonMod);
    Py_Initialize();

    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	settings->module = bp::import("__main__");
	settings->module_namespace = settings->module.attr("__dict__");

	bp::object interp_module = bp::import("InterpMod");

	bp::scope(interp_module).attr("interp") = interp_instance;
	bp::scope(interp_module).attr("params") = bp::ptr(&paramclass);

	settings->module_namespace["InterpMod"] = interp_module;

	bp::object canon_module = bp::import("CanonMod");
	settings->module_namespace["CanonMod"] = canon_module;



	bp::object result = bp::exec_file(path,
					  settings->module_namespace,
					  settings->module_namespace);


	settings->pymodule_stat = PYMOD_OK;
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    msg = handle_pyerror();
	    py_exception = true;
	}
	bp::handle_exception();
	settings->pymodule_stat = PYMOD_FAILED;
	PyErr_Clear();
    }
    if (py_exception) {
	logPy("init_python: module '%s' init failed: %s\n",path,msg.c_str());
	if (useGIL)
	    PyGILState_Release(gstate);
	ERS("init_python: %s",msg.c_str());
    }
    if (useGIL)
	PyGILState_Release(gstate);

    return INTERP_OK;

 error:  // come here if PYCHK() macro fails
    if (useGIL)
	PyGILState_Release(gstate);
    return INTERP_ERROR;
}

bool Interp::is_pycallable(setup_pointer settings,
			   const char *funcname)
{
    bool result = false;
    bool py_unexpected = false;
    std::string msg;

    if ((settings->pymodule_stat != PYMOD_OK) ||
	(funcname == NULL)) {
	return false;
    }

    try {
	bp::object function = settings->module_namespace[funcname];
	result = PyCallable_Check(function.ptr());
    }
    catch (bp::error_already_set) {
	// KeyError expected if not a callable
	if (!PyErr_ExceptionMatches(PyExc_KeyError)) {
	    // something else, strange
	    msg = handle_pyerror();
	    py_unexpected = true;
	}
	result = false;
	PyErr_Clear();
    }
    if (py_unexpected)
	logPy("is_pycallable: %s",msg.c_str());

    if (_setup.loggingLevel > 5)
	logPy("py_iscallable(%s) = %s",funcname,result ? "TRUE":"FALSE");
    return result;
}

static bp::object callobject(bp::object c, bp::object args, bp::object kwds)
{
    return c(*args, **kwds);
}

int Interp::pycall(setup_pointer settings,
		   block_pointer block,
		   const char *funcname)
{
    bp::object retval;
    PyGILState_STATE gstate;
    std::string msg;
    bool py_exception = false;

    if (_setup.loggingLevel > 2)
	logPy("pycall %s \n", funcname);

    // &this->_setup wont work, _setup is currently private
    current_setup = get_setup(this);
    current_interp = this;
    block->returned = 0;

    if (settings->pymodule_stat != PYMOD_OK) {
	ERS("function '%s.%s' : module not initialized",
	    settings->pymodule,funcname);
	return INTERP_OK;
    }
    if (useGIL)
	gstate = PyGILState_Ensure();


    try {
	bp::object function = settings->module_namespace[funcname];
	retval = callobject(function,
			    block->tupleargs,
			    block->kwargs);
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    msg = handle_pyerror();
	    py_exception = true;
	}
	bp::handle_exception();
	PyErr_Clear();
    }
    if (useGIL)
	    PyGILState_Release(gstate);
    if (py_exception) {
	ERS("pycall: %s", msg.c_str());
    }
    if (retval.ptr() != Py_None) {
	if (PyFloat_Check(retval.ptr())) {
	    block->py_returned_value = bp::extract<double>(retval);
	    block->returned[RET_DOUBLE] = 1;
	    logPy("pycall: '%s' returned %f", funcname, block->py_returned_value);
	    return INTERP_OK;
	}
	if (PyTuple_Check(retval.ptr())) {
	    bp::tuple tret = bp::extract<bp::tuple>(retval);
	    int len = bp::len(tret);
	    if (!len)  {
		logPy("pycall: empty tuple detected");
		block->returned[RET_NONE] = 1;
		return INTERP_OK;
	    }
	    logPy("pycall: tuple len %d detected",len);
	    bp::object item0 = tret[0];
	    if (PyInt_Check(item0.ptr())) {
		block->returned[RET_STATUS] = 1;
		block->py_returned_status = bp::extract<int>(item0);
		logPy("pycall: item 0 - int=%d detected",
		      block->py_returned_status);
	    }
	    if (len > 1) {
		bp::object item1 = tret[1];
		if (PyInt_Check(item1.ptr())) {
		    block->py_returned_userdata = bp::extract<int>(item1);
		    block->returned[RET_USERDATA] = 1;
		    logPy("pycall: item 1 - userdata=%d detected",
			  block->py_returned_userdata);
		}
	    }
	    if (block->returned[RET_STATUS])
		ERP(block->py_returned_status);
	    return INTERP_OK;
	}
	PyObject *res_str = PyObject_Str(retval.ptr());
	ERS("function '%s.%s' returned '%s' - expected float or tuple, got %s",
	    settings->pymodule,funcname,
	    PyString_AsString(res_str),
	    retval.ptr()->ob_type->tp_name);
	Py_XDECREF(res_str);
	return INTERP_OK;
    }	else {
	block->returned[RET_NONE] = 1;
	logPy("pycall: '%s' returned no value",funcname);
    }
    return INTERP_OK;
}

// called by  (py, ....) comments
int Interp::py_execute(const char *cmd)
{
    std::string msg;
    PyGILState_STATE gstate;
    bool py_exception = false;

    bp::object main_module = bp::import("__main__");
    bp::object main_namespace = main_module.attr("__dict__");

    logPy("py_execute(%s)",cmd);
    if (useGIL)
	gstate = PyGILState_Ensure();

    bp::object retval;

    try {
	retval = bp::exec(cmd,
			  _setup.module_namespace,
			  _setup.module_namespace);
    }
    catch (bp::error_already_set) {
        if (PyErr_Occurred())  {
	    py_exception = true;
	    msg = handle_pyerror();
	}
	bp::handle_exception();
	PyErr_Clear();
    }
    if (useGIL)
	PyGILState_Release(gstate);

    if (py_exception)
	logPy("py_execute(%s):  %s", cmd, msg.c_str());
    // msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());
    // msg.erase(std::remove(msg.begin(), msg.end(), '\r'), msg.end());
    // if (msg.length())
    // 	MESSAGE((char *) msg.c_str());
    ERP(INTERP_OK);
}


