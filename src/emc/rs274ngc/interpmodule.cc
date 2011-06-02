

#include <boost/python.hpp>

namespace bp = boost::python;

#include <stdio.h>
#include <string.h>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"
#include "interpmodule.hh"

#define IS_STRING(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyString_Type))
#define IS_INT(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyInt_Type))


static void wrap_setError(Interp &interp, const char *s)
{
    setup *settings  = get_setup(&interp);

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

static bp::object wrap_find_tool_pocket(Interp &interp, int toolno)
{
    int status, pocket;
    setup *settings  =  get_setup(&interp); //&interp._setup;

    status = interp.find_tool_pocket(settings, toolno, &pocket);
    return bp::make_tuple(status, pocket);
}

// access to named and numbered parameters via a pseudo-dictionary
struct ParamClass
{
    double getitem( bp::object sub)
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

static ParamClass params;

struct WrapRemap : public remap {
};

struct BlockWrap : public block {// ,boost::python::wrapper<block> {

    char const *get_comment() const { return comment; }
    char const *get_o_name() const { return o_name; }

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

    WrapRemap *get_remap() {
	return (WrapRemap *) executing_remap;
    }
};


struct ContextWrap : public context {
};

struct ContextArray {

    ContextWrap *getitem(bp::object sub) {
	if (IS_INT(sub)) {
	    int index = bp::extract < int > (sub);
	    return (ContextWrap *) &current_setup->sub_context[index];
	} else
	    throw std::runtime_error("sub_context subscript type must be integer");
    }

    long len() {
	return current_setup->call_level;
    }
};
static struct ContextArray sub_context_array;


struct BlockArray {
    BlockWrap *getitem(bp::object sub) {
	int index = bp::extract < int > (sub);
	return (BlockWrap *) &current_setup->blocks[index];
    }

    long len() {
	return current_setup->remap_level;
    }
};
static struct BlockArray block_array;



BOOST_PYTHON_MODULE(InterpMod) {
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

    class_ <ContextWrap, noncopyable>("Context",no_init)
	.def_readwrite("position",&ContextWrap::position)
	.def_readwrite("sequence_number",&ContextWrap::sequence_number)
	.def_readwrite("filename",  &ContextWrap::filename)
	.def_readwrite("subname",  &ContextWrap::subName)
	// need wrapper for saved_params
	.def_readwrite("context_status", &ContextWrap::context_status)
	;

    class_ <ContextArray, noncopyable>("ContextArray","Interpreter call stack",no_init)
	.def("__getitem__", &ContextArray::getitem,
	     return_value_policy<reference_existing_object>())
	.def("__len__", &ContextArray::len)
	;

    class_ <BlockArray, noncopyable>("BlockArray","Interpreter block stack",no_init)
	.def("__getitem__", &BlockArray::getitem,
	     return_value_policy<reference_existing_object>())
	.def("__len__", &BlockArray::len)
	;

    class_ <WrapRemap,noncopyable>("Remap",no_init)
	.def_readwrite("name",&WrapRemap::name)
	.def_readwrite("argspec",&WrapRemap::argspec)
	.def_readwrite("modal_group",&WrapRemap::modal_group)
	.def_readwrite("prolog_func",&WrapRemap::prolog_func)
	.def_readwrite("remap_py",&WrapRemap::remap_py)
	.def_readwrite("remap_ngc",&WrapRemap::remap_ngc)
	.def_readwrite("epilog_func",&WrapRemap::epilog_func)
	;


    class_ <BlockWrap, noncopyable>("Block",no_init)
	.def_readwrite("f_flag",&BlockWrap::f_flag)
	.def_readwrite("p_flag",&BlockWrap::p_flag)
	.def_readwrite("p_number",&BlockWrap::p_number)
	.def_readwrite("a_flag",&BlockWrap::a_flag)
	.def_readwrite("a_number",&BlockWrap::a_number)
	.def_readwrite("b_flag",&BlockWrap::b_flag)
	.def_readwrite("b_number",&BlockWrap::b_number)
	.def_readwrite("c_flag",&BlockWrap::c_flag)
	.def_readwrite("c_number",&BlockWrap::c_number)
	.def_readwrite("d_number_float",&BlockWrap::d_number_float)
	.def_readwrite("d_flag",&BlockWrap::d_flag)
	.def_readwrite("e_flag",&BlockWrap::e_flag)
	.def_readwrite("e_number",&BlockWrap::e_number)
	.def_readwrite("f_flag",&BlockWrap::f_flag)
	.def_readwrite("f_number",&BlockWrap::f_number)
	.def_readwrite("h_flag",&BlockWrap::h_flag)
	.def_readwrite("h_number",&BlockWrap::h_number)
	.def_readwrite("i_flag",&BlockWrap::i_flag)
	.def_readwrite("i_number",&BlockWrap::i_number)
	.def_readwrite("j_flag",&BlockWrap::j_flag)
	.def_readwrite("j_number",&BlockWrap::j_number)
	.def_readwrite("k_flag",&BlockWrap::k_flag)
	.def_readwrite("k_number",&BlockWrap::k_number)
	.def_readwrite("l_number",&BlockWrap::l_number)
	.def_readwrite("l_flag",&BlockWrap::l_flag)
	.def_readwrite("line_number",&BlockWrap::line_number)
	.def_readwrite("n_number",&BlockWrap::n_number)
	.def_readwrite("motion_to_be",&BlockWrap::motion_to_be)
	.def_readwrite("m_count",&BlockWrap::m_count)
	.def_readwrite("user_m",&BlockWrap::user_m)
	.def_readwrite("p_number",&BlockWrap::p_number)
	.def_readwrite("p_flag",&BlockWrap::p_flag)
	.def_readwrite("q_number",&BlockWrap::q_number)
	.def_readwrite("q_flag",&BlockWrap::q_flag)
	.def_readwrite("r_flag",&BlockWrap::r_flag)
	.def_readwrite("r_number",&BlockWrap::r_number)
	.def_readwrite("s_flag",&BlockWrap::s_flag)
	.def_readwrite("s_number",&BlockWrap::s_number)
	.def_readwrite("t_flag",&BlockWrap::t_flag)
	.def_readwrite("t_number",&BlockWrap::t_number)
	.def_readwrite("u_flag",&BlockWrap::u_flag)
	.def_readwrite("u_number",&BlockWrap::u_number)
	.def_readwrite("v_flag",&BlockWrap::v_flag)
	.def_readwrite("v_number",&BlockWrap::v_number)
	.def_readwrite("w_flag",&BlockWrap::w_flag)
	.def_readwrite("w_number",&BlockWrap::w_number)
	.def_readwrite("x_flag",&BlockWrap::x_flag)
	.def_readwrite("x_number",&BlockWrap::x_number)
	.def_readwrite("y_flag",&BlockWrap::y_flag)
	.def_readwrite("y_number",&BlockWrap::y_number)
	.def_readwrite("z_flag",&BlockWrap::z_flag)
	.def_readwrite("z_number",&BlockWrap::z_number)
	.def_readwrite("radius_flag",&BlockWrap::radius_flag)
	.def_readwrite("radius",&BlockWrap::radius)
	.def_readwrite("theta_flag",&BlockWrap::theta_flag)
	.def_readwrite("theta",&BlockWrap::theta)

	.def_readwrite("offset",&BlockWrap::offset)
	.def_readwrite("o_type",&BlockWrap::o_type)

	//  read-only
	.add_property("comment", &BlockWrap::get_comment)
	.add_property("o_name", &BlockWrap::get_o_name)
	.add_property("g_modes", &BlockWrap::get_g_modes)
	.add_property("m_modes", &BlockWrap::get_m_modes)
	.add_property("params", &BlockWrap::get_params)

	.add_property("executing_remap",
		      make_function(&BlockWrap::get_remap,
				    return_value_policy<reference_existing_object>()))
	;

    class_<ParamClass, noncopyable>("Params","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
	.def("__setitem__", &ParamClass::setitem)
	;

    class_<PmCartesian, noncopyable>("PmCartesian","EMC cartesian postition",no_init)
	.def_readwrite("x",&PmCartesian::x)
	.def_readwrite("y",&PmCartesian::y)
	.def_readwrite("z",&PmCartesian::z)
	;

    class_<EmcPose, noncopyable>("EmcPose","EMC pose",no_init)
	.def_readwrite("tran",&EmcPose::tran)
	.def_readwrite("a",&EmcPose::a)
	.def_readwrite("b",&EmcPose::b)
	.def_readwrite("c",&EmcPose::c)
	.def_readwrite("u",&EmcPose::u)
	.def_readwrite("v",&EmcPose::v)
	.def_readwrite("w",&EmcPose::w)
	;

    scope interp_class(
		       class_< Interp, interp_ptr,
		       noncopyable >("Interp",no_init)

		       .def("find_tool_pocket", &wrap_find_tool_pocket)

		       .def("load_tool_table", &Interp::load_tool_table)
		       .def("set_errormsg", &wrap_setError)
		       .def("set_tool_parameters", &Interp::set_tool_parameters)
		       .def("sequence_number", &Interp::sequence_number)
		       .def("synch", &Interp::synch)

		       .def_readwrite("AA_axis_offset", &Interp::_setup.AA_axis_offset)
		       .def_readwrite("BB_axis_offset", &Interp::_setup.BB_axis_offset)
		       .def_readwrite("CC_axis_offset", &Interp::_setup.CC_axis_offset)

		       .def_readwrite("AA_current",     &Interp::_setup.AA_current)
		       .def_readwrite("BB_current",     &Interp::_setup.BB_current)
		       .def_readwrite("CC_current",     &Interp::_setup.CC_current)

		       .def_readwrite("AA_origin_offset", &Interp::_setup.AA_origin_offset)
		       .def_readwrite("BB_origin_offset", &Interp::_setup.BB_origin_offset)
		       .def_readwrite("CC_origin_offset", &Interp::_setup.CC_origin_offset)

		       .def_readwrite("u_axis_offset", &Interp::_setup.u_axis_offset)
		       .def_readwrite("u_current", &Interp::_setup.u_current)
		       .def_readwrite("u_origin_offset", &Interp::_setup.u_origin_offset)

		       .def_readwrite("v_axis_offset", &Interp::_setup.v_axis_offset)
		       .def_readwrite("v_current", &Interp::_setup.v_current)
		       .def_readwrite("v_origin_offset", &Interp::_setup.v_origin_offset)

		       .def_readwrite("w_axis_offset", &Interp::_setup.w_axis_offset)
		       .def_readwrite("w_current", &Interp::_setup.w_current)
		       .def_readwrite("w_origin_offset", &Interp::_setup.w_origin_offset)

		       // // // active m/g codes, settings missing

		       .def_readwrite("arc_not_allowed", &Interp::_setup.arc_not_allowed)

		       .def_readwrite("axis_offset_x", &Interp::_setup.axis_offset_x)
		       .def_readwrite("axis_offset_y", &Interp::_setup.axis_offset_y)
		       .def_readwrite("axis_offset_z", &Interp::_setup.axis_offset_z)

		       .def_readwrite("current_x", &Interp::_setup.current_x)
		       .def_readwrite("current_y", &Interp::_setup.current_y)
		       .def_readwrite("current_z", &Interp::_setup.current_z)


		       // .add_static_property("cutter_comp_side",
		       // 			     make_getter( &Interp::_setup.cutter_comp_side),
		       // 			     make_setter( &Interp::_setup.cutter_comp_side))

// init_python: module '/home/mah/emc2-tc/configs/sim/pysubs/pythonsubs.py' init failed: ['ArgumentError: Python argument types in    None.None(Boost.Python.StaticProperty) did not match C++ signature:    None(int)']
// init_python: module '/home/mah/emc2-tc/configs/sim/pysubs/pythonsubs.py' init failed: ['ArgumentError: Python argument types in\n    None.None(Boost.Python.StaticProperty)\ndid not match C++ signature:\n    None(int)\n']


		       .def_readwrite("cycle_cc", &Interp::_setup.cycle_cc)
		       .def_readwrite("cycle_i", &Interp::_setup.cycle_i)
		       .def_readwrite("cycle_j", &Interp::_setup.cycle_j)
		       .def_readwrite("cycle_k", &Interp::_setup.cycle_k)
		       .def_readwrite("cycle_l", &Interp::_setup.cycle_l)
		       .def_readwrite("cycle_p", &Interp::_setup.cycle_p)
		       .def_readwrite("cycle_q", &Interp::_setup.cycle_q)
		       .def_readwrite("cycle_r", &Interp::_setup.cycle_r)
		       .def_readwrite("cycle_il", &Interp::_setup.cycle_il)
		       .def_readwrite("cycle_il_flag", &Interp::_setup.cycle_il_flag)


		       .def_readwrite("feed_mode", (int *)&Interp::_setup.feed_mode)
		       .def_readwrite("feed_override", (int *)&Interp::_setup.feed_override)
		       .def_readwrite("feed_rate", (int *)&Interp::_setup.feed_rate)

		       .def_readwrite("filename", (char *)&Interp::_setup.filename)
		       .def_readwrite("tool_offset_index",&Interp::_setup.tool_offset_index)
		       .def_readwrite("length_units", &Interp::_setup.length_units)

		       .def_readwrite("linetext", (char *)&Interp::_setup.linetext)
		       .def_readwrite("mist", &Interp::_setup.mist)
		       .def_readwrite("motion_mode", &Interp::_setup.motion_mode)
		       .def_readwrite("origin_index", &Interp::_setup.origin_index)
		       .def_readwrite("origin_offset_x", &Interp::_setup.origin_offset_x)
		       .def_readwrite("origin_offset_y", &Interp::_setup.origin_offset_y)
		       .def_readwrite("origin_offset_z", &Interp::_setup.origin_offset_z)
		       .def_readwrite("rotation_xy", &Interp::_setup.rotation_xy)



		       // .def_readwrite("distance_mode", (int *)&Interp::_setup.distance_mode)
		       //  .def_readwrite("ijk_distance_mode", (int *)&Interp::_setup.ijk_distance_mode)

		       // .def_readwrite("cutter_comp_radius", &Interp::_setup.cutter_comp_radius)
		       // .def_readwrite("cutter_comp_orientation", &Interp::_setup.cutter_comp_orientation)
		       // .def_readwrite("cutter_comp_side", &Interp::_setup.cutter_comp_side)  //<<<!!!

		       .add_property( "cutter_comp_side",
					     make_getter(&Interp::_setup.cutter_comp_side),
					     make_setter(&Interp::_setup.cutter_comp_side))

		       // .def_readwrite("percent_flag", &Interp::_setup.percent_flag)
		       // .def_readwrite("plane", &Interp::_setup.plane)

		       // .def_readwrite("input_index", &Interp::_setup.input_index) // <<<<<!!!


		       // .def_readwrite("input_digital", &Interp::_setup.input_digital)
		       // .def_readwrite("cutter_comp_firstmove", &Interp::_setup.cutter_comp_firstmove)

		       .def_readwrite("program_x", &Interp::_setup.program_x)
		       .def_readwrite("program_y", &Interp::_setup.program_y)
		       .def_readwrite("program_z", &Interp::_setup.program_z)
		       .def_readwrite("retract_mode", (int *) &Interp::_setup.retract_mode)
		       .def_readwrite("random_toolchanger", &Interp::_setup.random_toolchanger)
		       .def_readwrite("speed", &Interp::_setup.speed)
		       .def_readwrite("spindle_mode", (int *) &Interp::_setup.spindle_mode)
		       .def_readwrite("speed_feed_mode", (int *)&Interp::_setup.speed_feed_mode)
		       .def_readwrite("speed_override", &Interp::_setup.speed_override)
		       .def_readwrite("spindle_turning", (int *) &Interp::_setup.spindle_turning)


		       .def_readwrite("pockets_max", &Interp::_setup.pockets_max)

		       // // // CANON_TOOL_TABLE missing

		       .def_readwrite("traverse_rate", &Interp::_setup.traverse_rate)

		       // .add_static_property( "program_x",
		       // 			     make_getter(&Interp::_setup.program_x),
		       // 			     make_setter(&Interp::_setup.program_x))
		       //scope(interp_class).attr("tool_offset") = Interp::_setup.tool_offset;

		       .def_readwrite("tool_offset", &Interp::_setup.tool_offset)

		       // .add_static_property( "tool_offset",
		       // 			     make_getter( &Interp::_setup.tool_offset), //, return_value_policy<return_by_value>()),
		       // 			     make_setter( &Interp::_setup.tool_offset)) //, return_value_policy<return_internal_reference>() ) )


		       .def_readwrite("blocktext", (char *) &Interp::_setup.blocktext)
		       .def_readwrite("call_level", &Interp::_setup.call_level)
		       .def_readwrite("current_pocket", &Interp::_setup.current_pocket)
		       .def_readwrite("current_tool", &Interp::_setup.tool_table[0].toolno)
		       .def_readwrite("cutter_comp_side", &Interp::_setup.cutter_comp_side)
		       .def_readwrite("debugmask", &Interp::_setup.debugmask)
		       .def_readwrite("input_digital", &Interp::_setup.input_digital)
		       .def_readwrite("input_flag", &Interp::_setup.input_flag)
		       .def_readwrite("input_index", &Interp::_setup.input_index)
		       .def_readwrite("mdi_interrupt", &Interp::_setup.mdi_interrupt)
		       .def_readwrite("probe_flag", &Interp::_setup.probe_flag)
		       .def_readwrite("remap_level", &Interp::_setup.remap_level)
		       .def_readwrite("return_value", &Interp::_setup.return_value)
		       .def_readwrite("selected_pocket", &Interp::_setup.selected_pocket)
		       .def_readwrite("toolchange_flag", &Interp::_setup.toolchange_flag)
		       .def_readwrite("reload_on_change", &Interp::_setup.py_reload_on_change)
		       );

    scope(interp_class).attr("params") = ptr(&params);
    scope(interp_class).attr("sub_context") = ptr(&sub_context_array);
    scope(interp_class).attr("blocks") = ptr(&block_array);


}
