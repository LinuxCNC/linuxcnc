// Interpreter internals - Python bindings
// Michael Haberler 7/2011
//

#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <map>

namespace bp = boost::python;

#include <stdio.h>
#include <string.h>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"
#include "interpmodule.hh"
#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;
#include "interp_array_types.hh"

#define IS_STRING(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyString_Type))
#define IS_INT(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyInt_Type))

static  active_g_codes_array active_g_codes_wrapper ( Interp & inst) {
    return active_g_codes_array(inst._setup.active_g_codes);
}
static  active_m_codes_array active_m_codes_wrapper ( Interp & inst) {
    return active_m_codes_array(inst._setup.active_m_codes);
}

static  active_settings_array active_settings_wrapper ( Interp & inst) {
    return active_settings_array(inst._setup.active_settings);
}

static  blocks_array blocks_wrapper ( Interp & inst) {
    return blocks_array(inst._setup.blocks);
}

static  parameters_array parameters_wrapper ( Interp & inst) {
    return parameters_array(inst._setup.parameters);
}

static  tool_table_array tool_table_wrapper ( Interp & inst) {
    return tool_table_array(inst._setup.tool_table);
}

static  sub_context_array sub_context_wrapper ( Interp & inst) {
    return sub_context_array(inst._setup.sub_context);
}

static  g_modes_array g_modes_wrapper ( block & b) {
    return g_modes_array(b.g_modes);
}

static  m_modes_array m_modes_wrapper ( block & b) {
    return m_modes_array(b.m_modes);
}

static params_array saved_params_wrapper ( context &c) {
    return params_array(c.saved_params);
}

static params_array params_wrapper ( block & b) {
    return params_array(b.params);
}

static  active_g_codes_array saved_g_codes_wrapper ( context &c) {
    return active_g_codes_array(c.saved_g_codes);
}

static  active_m_codes_array saved_m_codes_wrapper ( context &c) {
    return active_m_codes_array(c.saved_m_codes);
}

static  active_settings_array saved_settings_wrapper ( context &c) {
    return active_settings_array(c.saved_settings);
}

#pragma GCC diagnostic ignored "-Wformat-security"
static void wrap_setError(Interp &interp, const char *s)
{
    setup *settings  = &interp._setup;

    if ((s == NULL) && !strlen(s))
	s = "###";
    interp.setError (s);
    settings->stack_index = 0;
    strncpy(settings->stack[settings->stack_index],
	    "Python", STACK_ENTRY_LEN);
    settings->stack[settings->stack_index][STACK_ENTRY_LEN-1] = 0;
    settings->stack_index++;
    settings->stack[settings->stack_index][0] = 0;
}
#pragma GCC diagnostic warning "-Wformat-security"

static bp::object wrap_find_tool_pocket(Interp &interp, int toolno)
{
    int status, pocket;
    setup *settings  =  &interp._setup;

    status = interp.find_tool_pocket(settings, toolno, &pocket);
    return bp::make_tuple(status, pocket);
}

// access to named and numbered parameters via a pseudo-dictionary
// either params["paramname"] or params[5400] is valid
struct ParamClass {

    Interp &interp;

    ParamClass(Interp &i) : interp(i) {};
    double getitem( bp::object sub)
    {
	double retval = 0;
	if (IS_STRING(sub)) {
	    char const* varname = bp::extract < char const* > (sub);
	    int status;
	    interp.find_named_param(varname, &status, &retval);
	    if (!status)
		throw std::runtime_error("parameter does not exist: " + std::string(varname));
	} else
	    if (IS_INT(sub)) {
		int index = bp::extract < int > (sub);
		retval = interp._setup.parameters[index];
	    } else {
		throw std::runtime_error("params subscript type must be integer or string");
	    }
	return retval;
    }

    double setitem(bp::object sub, double dvalue)
    {
	if (IS_STRING(sub)) {
	    char const* varname = bp::extract < char const* > (sub);
	    int status = interp.add_named_param(varname, varname[0] == '_' ? PA_GLOBAL :0);
	    status = interp.store_named_param(&interp._setup,varname, dvalue, 0);
	    if (status != INTERP_OK)
		throw std::runtime_error("cant assign value to parameter: " + std::string(varname));

	} else
	    if (IS_INT(sub)) {
		int index = bp::extract < int > (sub);
		if ((index < 0) || (index > RS274NGC_MAX_PARAMETERS -1)) {
		    std::stringstream sstr;
		    sstr << "params subscript out of range : " << index << " - must be between 0 and " << RS274NGC_MAX_PARAMETERS;
		    throw std::runtime_error(sstr.str());
		}
		interp._setup.parameters[index] = dvalue;
		return dvalue;
	    } else
		throw std::runtime_error("params subscript type must be integer or string");
	return dvalue;
    }

    bp::list namelist(context &c) const {
	bp::list result;
	for(parameter_map::iterator it = c.named_params.begin(); it != c.named_params.end(); ++it) {
	    result.append( it->first);
	}
	return result;
    }

    bp::list locals() {
	return namelist(interp._setup.sub_context[interp._setup.call_level]);
    }

    bp::list globals() {
	return namelist(interp._setup.sub_context[0]);
    }

    bp::list operator()() const
    {
	bp::list result = namelist(interp._setup.sub_context[interp._setup.call_level]);
	result.extend(namelist(interp._setup.sub_context[0]));
	return result;
    };

    int length() { return RS274NGC_MAX_PARAMETERS;}
};

// FIXME not sure if this is really needed
static  ParamClass param_wrapper ( Interp & inst) {
    return ParamClass(inst);
}

static bp::object emcpose_2_obj ( EmcPose &p) {
    return  bp::object("x=%.4f y=%.4f z=%.4f a=%.4f b=%.4f c=%.4f u=%.4f v=%.4f w=%.4f" %
	bp::make_tuple(p.tran.x,p.tran.y,p.tran.z,
		       p.a,p.b,p.c,p.u,p.v,p.w));
}
static bp::object emcpose_str( EmcPose &p) {
    return  bp::object("EmcPose(" + emcpose_2_obj(p) + ")");
}

static bp::object tool_str( CANON_TOOL_TABLE &t) {
    return  bp::object("Tool(T%d D%.4f I%.4f J%.4f Q%d offset: " %
		       bp::make_tuple(t.toolno,  t.diameter,
				      t.frontangle,t.backangle, t.orientation) +
		       emcpose_2_obj(t.offset) + ")");
}

static void tool_zero( CANON_TOOL_TABLE &t) {
	t.toolno = -1;
        ZERO_EMC_POSE(t.offset);
	t.diameter = 0.0;
        t.frontangle = 0.0;
        t.backangle = 0.0;
        t.orientation = 0;
}

static bp::object pmcartesian_str( PmCartesian &c) {
    return  bp::object("PmCartesian(x=%.4f y=%.4f z=%.4f)" %
		       bp::make_tuple(c.x,c.y,c.z));
}

static const char *get_comment(block &b) { return b.comment; };
static const char *get_o_name(block &b) { return b.o_name; };

static void  set_x(EmcPose &p, double value) { p.tran.x = value; }
static void  set_y(EmcPose &p, double value) { p.tran.y = value; }
static void  set_z(EmcPose &p, double value) { p.tran.z = value; }
static double get_x(EmcPose &p) { return p.tran.x; }
static double get_y(EmcPose &p) { return p.tran.y; }
static double get_z(EmcPose &p) { return p.tran.z; }

// those are exposed here because they look useful for regression testing
static bool equal(double a, double b) { return fabs(a - b) < TOLERANCE_EQUAL; }
// see interp_convert.cc
static bool is_near_int(double value) {
    int i = (int)(value + .5);
    return fabs(i - value) < .0001;
}
static int nearest_int(double value) { return (int)(value + .5); }


BOOST_PYTHON_MODULE(interpreter) {
    using namespace boost::python;
    using namespace boost;

    scope().attr("__doc__") =
        "Interpreter introspection\n"
        ;

    scope().attr("INTERP_OK") = INTERP_OK;
    scope().attr("INTERP_EXIT") = INTERP_EXIT;
    scope().attr("INTERP_EXECUTE_FINISH") = INTERP_EXECUTE_FINISH;
    scope().attr("INTERP_ENDFILE") = INTERP_ENDFILE;
    scope().attr("INTERP_FILE_NOT_OPEN") = INTERP_FILE_NOT_OPEN;
    scope().attr("INTERP_ERROR") = INTERP_ERROR;
    scope().attr("TOLERANCE_EQUAL") = TOLERANCE_EQUAL;

    def("equal", &equal);  // EMC's perception of equality of doubles
    def("is_near_int", &is_near_int);  // EMC's perception of closeness to an int
    def("nearest_int", &nearest_int);


    class_<PmCartesian, noncopyable>("PmCartesian","EMC cartesian postition",no_init)
	.def_readwrite("x",&PmCartesian::x)
	.def_readwrite("y",&PmCartesian::y)
	.def_readwrite("z",&PmCartesian::z)
	.def("__str__", &pmcartesian_str)
	;


    // leave EmcPose copyable/assignable because it's used as a parameter value (eg emcSetToolOffset)
    class_<EmcPose>("EmcPose","EMC pose",no_init)
	.def_readwrite("tran",&EmcPose::tran)
	.add_property("x", &get_x, &set_x)
	.add_property("y", &get_y, &set_y)
	.add_property("z", &get_z, &set_z)
	.def_readwrite("a",&EmcPose::a)
	.def_readwrite("b",&EmcPose::b)
	.def_readwrite("c",&EmcPose::c)
	.def_readwrite("u",&EmcPose::u)
	.def_readwrite("v",&EmcPose::v)
	.def_readwrite("w",&EmcPose::w)
	.def("__str__", &emcpose_str)
	;

    // leave CANON_TOOL_TABLE copyable/assignable because assignment is used a lot when fiddling
    // with tooltable entries
    class_<CANON_TOOL_TABLE >("CANON_TOOL_TABLE","Tool description" ,no_init)
	.def_readwrite("toolno", &CANON_TOOL_TABLE::toolno)
	.def_readwrite("offset", &CANON_TOOL_TABLE::offset)
	.def_readwrite("diameter", &CANON_TOOL_TABLE::diameter)
	.def_readwrite("frontangle", &CANON_TOOL_TABLE::frontangle)
	.def_readwrite("backangle", &CANON_TOOL_TABLE::backangle)
	.def_readwrite("orientation", &CANON_TOOL_TABLE::orientation)
	.def("__str__", &tool_str)
	.def("zero", &tool_zero)
	;


    class_<parameter_value_struct /*,noncopyable */>("ParameterValue") // ,no_init)
	.def_readwrite("attr",&parameter_value_struct::attr)
	.def_readwrite("value",&parameter_value_struct::value)
	;

    class_<parameter_map,noncopyable>("ParameterMap",no_init)
        .def(map_indexing_suite<parameter_map>())
	;

    // FIXME expose properly. RVP? stupid allocation
    // class_<remap_type,noncopyable>("RemapMap",no_init)
    //     .def(map_indexing_suite<remap_type>())
    // 	;

    // FIXME make noncopyable: class_<ParamClass, noncopyable>("Params","Interpreter parameters",no_init)
    class_<ParamClass>("Params","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
        .def("__setitem__", &ParamClass::setitem)
        .def("__len__", &ParamClass::length)
        .def("globals", &ParamClass::globals)
        .def("locals", &ParamClass::locals)
	.def("__call__", &ParamClass::operator());
	;

    class_ <context, noncopyable>("Context",no_init)
	.def_readwrite("position",&context::position)
	.def_readwrite("sequence_number",&context::sequence_number)
	.def_readwrite("filename",  &context::filename)
	.def_readwrite("subname",  &context::subName)
	.add_property( "saved_params",
		       bp::make_function( saved_params_w(&saved_params_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_g_codes",
		       bp::make_function( active_g_codes_w(&saved_g_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_m_codes",
		       bp::make_function( active_m_codes_w(&saved_m_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_settings",
		       bp::make_function( active_settings_w(&saved_settings_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.def_readwrite("context_status", &context::context_status)
	.def_readwrite("named_params",  &context::named_params)

	;

    class_ <remap,noncopyable>("Remap",no_init)
	.def_readwrite("name",&remap::name)
	.def_readwrite("argspec",&remap::argspec)
	.def_readwrite("modal_group",&remap::modal_group)
	.def_readwrite("prolog_func",&remap::prolog_func)
	.def_readwrite("remap_py",&remap::remap_py)
	.def_readwrite("remap_ngc",&remap::remap_ngc)
	.def_readwrite("epilog_func",&remap::epilog_func)
	;

    class_ <block, noncopyable>("Block",no_init)
	.def_readwrite("f_flag",&block::f_flag)
	.def_readwrite("p_flag",&block::p_flag)
	.def_readwrite("p_number",&block::p_number)
	.def_readwrite("a_flag",&block::a_flag)
	.def_readwrite("a_number",&block::a_number)
	.def_readwrite("b_flag",&block::b_flag)
	.def_readwrite("b_number",&block::b_number)
	.def_readwrite("c_flag",&block::c_flag)
	.def_readwrite("c_number",&block::c_number)
	.def_readwrite("d_number_float",&block::d_number_float)
	.def_readwrite("d_flag",&block::d_flag)
	.def_readwrite("e_flag",&block::e_flag)
	.def_readwrite("e_number",&block::e_number)
	.def_readwrite("f_flag",&block::f_flag)
	.def_readwrite("f_number",&block::f_number)
	.def_readwrite("h_flag",&block::h_flag)
	.def_readwrite("h_number",&block::h_number)
	.def_readwrite("i_flag",&block::i_flag)
	.def_readwrite("i_number",&block::i_number)
	.def_readwrite("j_flag",&block::j_flag)
	.def_readwrite("j_number",&block::j_number)
	.def_readwrite("k_flag",&block::k_flag)
	.def_readwrite("k_number",&block::k_number)
	.def_readwrite("l_number",&block::l_number)
	.def_readwrite("l_flag",&block::l_flag)
	.def_readwrite("line_number",&block::line_number)
	.def_readwrite("n_number",&block::n_number)
	.def_readwrite("motion_to_be",&block::motion_to_be)
	.def_readwrite("m_count",&block::m_count)
	.def_readwrite("user_m",&block::user_m)
	.def_readwrite("p_number",&block::p_number)
	.def_readwrite("p_flag",&block::p_flag)
	.def_readwrite("q_number",&block::q_number)
	.def_readwrite("q_flag",&block::q_flag)
	.def_readwrite("r_flag",&block::r_flag)
	.def_readwrite("r_number",&block::r_number)
	.def_readwrite("s_flag",&block::s_flag)
	.def_readwrite("s_number",&block::s_number)
	.def_readwrite("t_flag",&block::t_flag)
	.def_readwrite("t_number",&block::t_number)
	.def_readwrite("u_flag",&block::u_flag)
	.def_readwrite("u_number",&block::u_number)
	.def_readwrite("v_flag",&block::v_flag)
	.def_readwrite("v_number",&block::v_number)
	.def_readwrite("w_flag",&block::w_flag)
	.def_readwrite("w_number",&block::w_number)
	.def_readwrite("x_flag",&block::x_flag)
	.def_readwrite("x_number",&block::x_number)
	.def_readwrite("y_flag",&block::y_flag)
	.def_readwrite("y_number",&block::y_number)
	.def_readwrite("z_flag",&block::z_flag)
	.def_readwrite("z_number",&block::z_number)
	.def_readwrite("radius_flag",&block::radius_flag)
	.def_readwrite("radius",&block::radius)
	.def_readwrite("theta_flag",&block::theta_flag)
	.def_readwrite("theta",&block::theta)

	.def_readwrite("offset",&block::offset)
	.def_readwrite("o_type",&block::o_type)
	.def_readwrite("executing_remap",&block::executing_remap)

	//  read-only
	.add_property("comment",  &get_comment)
	.add_property("o_name",   &get_o_name)

	.add_property( "params",
		       bp::make_function( params_w(&params_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	// arrays
	.add_property( "m_modes",
		       bp::make_function( m_modes_w(&m_modes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "g_modes",
		       bp::make_function( g_modes_w(&g_modes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))

	;



    class_< Interp,  interp_ptr, noncopyable >("Interp",no_init)

	.def("find_tool_pocket", &wrap_find_tool_pocket)
	.def("load_tool_table", &Interp::load_tool_table)
	.def("set_errormsg", &wrap_setError)
	.def("set_tool_parameters", &Interp::set_tool_parameters)
	.def("synch", &Interp::synch)
	.def("print_named_params", &Interp::print_named_params)
	.def_readonly("filename", (char *) &Interp::_setup.filename)
	.def_readonly("linetext", (char *) &Interp::_setup.linetext)
	.def_readonly("current_tool", &Interp::_setup.tool_table[0].toolno)
	.def_readwrite("AA_axis_offset", &Interp::_setup.AA_axis_offset)
	.def_readwrite("AA_current", &Interp::_setup.AA_current)
	.def_readwrite("AA_origin_offset", &Interp::_setup.AA_origin_offset)
	.def_readwrite("BB_axis_offset", &Interp::_setup.BB_axis_offset)
	.def_readwrite("BB_current", &Interp::_setup.BB_current)
	.def_readwrite("BB_origin_offset", &Interp::_setup.BB_origin_offset)
	.def_readwrite("CC_axis_offset", &Interp::_setup.CC_axis_offset)
	.def_readwrite("CC_current", &Interp::_setup.CC_current)
	.def_readwrite("CC_origin_offset", &Interp::_setup.CC_origin_offset)
	.def_readwrite("arc_not_allowed", &Interp::_setup.arc_not_allowed)
	.def_readwrite("axis_offset_x", &Interp::_setup.axis_offset_x)
	.def_readwrite("axis_offset_y", &Interp::_setup.axis_offset_y)
	.def_readwrite("axis_offset_z", &Interp::_setup.axis_offset_z)
	.def_readwrite("call_level", &Interp::_setup.call_level)
	.def_readwrite("current_pocket", &Interp::_setup.current_pocket)
	.def_readwrite("current_x", &Interp::_setup.current_x)
	.def_readwrite("current_y", &Interp::_setup.current_y)
	.def_readwrite("current_z", &Interp::_setup.current_z)
	.def_readwrite("cutter_comp_firstmove", &Interp::_setup.cutter_comp_firstmove)
	.def_readwrite("cutter_comp_orientation", &Interp::_setup.cutter_comp_orientation)
	.def_readwrite("cutter_comp_radius", &Interp::_setup.cutter_comp_radius)
	.def_readwrite("cutter_comp_side", &Interp::_setup.cutter_comp_side)
	.def_readwrite("cycle_cc", &Interp::_setup.cycle_cc)
	.def_readwrite("cycle_i", &Interp::_setup.cycle_i)
	.def_readwrite("cycle_il", &Interp::_setup.cycle_il)
	.def_readwrite("cycle_il_flag", &Interp::_setup.cycle_il_flag)
	.def_readwrite("cycle_j", &Interp::_setup.cycle_j)
	.def_readwrite("cycle_k", &Interp::_setup.cycle_k)
	.def_readwrite("cycle_l", &Interp::_setup.cycle_l)
	.def_readwrite("cycle_p", &Interp::_setup.cycle_p)
	.def_readwrite("cycle_q", &Interp::_setup.cycle_q)
	.def_readwrite("cycle_r", &Interp::_setup.cycle_r)
	.def_readwrite("debugmask", &Interp::_setup.debugmask)
	.def_readwrite("distance_mode", &Interp::_setup.distance_mode)
	.def_readwrite("feed_mode", &Interp::_setup.feed_mode)
	.def_readwrite("feed_override", &Interp::_setup.feed_override)
	.def_readwrite("feed_rate", &Interp::_setup.feed_rate)
	.def_readwrite("ijk_distance_mode", &Interp::_setup.ijk_distance_mode)
	.def_readwrite("input_digital", &Interp::_setup.input_digital)
	.def_readwrite("input_flag", &Interp::_setup.input_flag)
	.def_readwrite("input_index", &Interp::_setup.input_index)
	.def_readwrite("length_units", &Interp::_setup.length_units)
	.def_readwrite("loggingLevel", &Interp::_setup.loggingLevel)
	.def_readwrite("mdi_interrupt", &Interp::_setup.mdi_interrupt)
	.def_readwrite("mist", &Interp::_setup.mist)
	.def_readwrite("motion_mode", &Interp::_setup.motion_mode)
	.def_readwrite("origin_index", &Interp::_setup.origin_index)
	.def_readwrite("origin_offset_x", &Interp::_setup.origin_offset_x)
	.def_readwrite("origin_offset_y", &Interp::_setup.origin_offset_y)
	.def_readwrite("origin_offset_z", &Interp::_setup.origin_offset_z)
	.def_readwrite("percent_flag", &Interp::_setup.percent_flag)
	.def_readwrite("plane", &Interp::_setup.plane)
	.def_readwrite("pockets_max", &Interp::_setup.pockets_max)
	.def_readwrite("probe_flag", &Interp::_setup.probe_flag)
	.def_readwrite("program_x", &Interp::_setup.program_x)
	.def_readwrite("program_y", &Interp::_setup.program_y)
	.def_readwrite("program_z", &Interp::_setup.program_z)

	//  .def_readwrite("py_reload_on_change", &Interp::_setup.py_reload_on_change)

	.def_readwrite("random_toolchanger", &Interp::_setup.random_toolchanger)
	.def_readwrite("remap_level", &Interp::_setup.remap_level)
	.def_readwrite("retract_mode", &Interp::_setup.retract_mode)
	.def_readwrite("return_value", &Interp::_setup.return_value)
	.def_readwrite("value_returned", &Interp::_setup.value_returned)
	.def_readwrite("rotation_xy", &Interp::_setup.rotation_xy)
	.def_readwrite("selected_pocket", &Interp::_setup.selected_pocket)
	.def_readwrite("selected_tool", &Interp::_setup.selected_tool)
	.def_readwrite("sequence_number", &Interp::sequence_number)
	.def_readwrite("speed", &Interp::_setup.speed)
	.def_readwrite("speed_feed_mode", &Interp::_setup.speed_feed_mode)
	.def_readwrite("speed_override", &Interp::_setup.speed_override)
	.def_readwrite("spindle_mode", &Interp::_setup.spindle_mode)
	.def_readwrite("spindle_turning", &Interp::_setup.spindle_turning)
	.def_readwrite("tool_offset", &Interp::_setup.tool_offset)

	//unused,removed  .def_readwrite("tool_offset_index", &Interp::_setup.tool_offset_index)

	.def_readwrite("toolchange_flag", &Interp::_setup.toolchange_flag)
	.def_readwrite("traverse_rate", &Interp::_setup.traverse_rate)
	.def_readwrite("u_axis_offset", &Interp::_setup.u_axis_offset)
	.def_readwrite("u_current", &Interp::_setup.u_current)
	.def_readwrite("u_origin_offset", &Interp::_setup.u_origin_offset)
	.def_readwrite("v_axis_offset", &Interp::_setup.v_axis_offset)
	.def_readwrite("v_current", &Interp::_setup.v_current)
	.def_readwrite("v_origin_offset", &Interp::_setup.v_origin_offset)
	.def_readwrite("w_axis_offset", &Interp::_setup.w_axis_offset)
	.def_readwrite("w_current", &Interp::_setup.w_current)
	.def_readwrite("w_origin_offset", &Interp::_setup.w_origin_offset)

	.add_property( "params",
		       bp::make_function( &param_wrapper,
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))


	// _setup arrays
	.add_property( "active_g_codes",
		       bp::make_function( active_g_codes_w(&active_g_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "active_m_codes",
		       bp::make_function( active_m_codes_w(&active_m_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "active_settings",
		       bp::make_function( active_settings_w(&active_settings_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "blocks",
		       bp::make_function( blocks_w(&blocks_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "parameters",
		       bp::make_function( parameters_w(&parameters_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "tool_table",
		       bp::make_function( tool_table_w(&tool_table_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "sub_context",
		       bp::make_function( sub_context_w(&sub_context_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	;

    pp::register_array_1< int, ACTIVE_G_CODES> ("ActiveGcodesArray" );
    pp::register_array_1< int, ACTIVE_M_CODES> ("ActiveMcodesArray" );
    pp::register_array_1< double, ACTIVE_SETTINGS> ("ActiveSettingsArray");
    pp::register_array_1< block, MAX_NESTED_REMAPS,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("BlocksArray");
    pp::register_array_1< double, RS274NGC_MAX_PARAMETERS > ("ParametersArray");
    pp::register_array_1< CANON_TOOL_TABLE, CANON_POCKETS_MAX,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("ToolTableArray");
    pp::register_array_1< context, INTERP_SUB_ROUTINE_LEVELS,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("SubcontextArray");
    pp::register_array_1< int, 16> ("GmodesArray");
    pp::register_array_1< int, 11> ("MmodesArray");
    pp::register_array_1< double, INTERP_SUB_PARAMS> ("SubroutineParamsArray");
}
