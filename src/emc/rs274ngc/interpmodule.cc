

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

struct wrap_remap : public remap {
};

struct wrap_block : public block {// ,boost::python::wrapper<block> {

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

    wrap_remap *get_remap() {
	return (wrap_remap *) executing_remap;
    }
};


struct wrap_context : public context {
};

struct wrap_context_array {

    wrap_context *getitem(bp::object sub) {
	if (IS_INT(sub)) {
	    int index = bp::extract < int > (sub);
	    return (wrap_context *) &current_setup->sub_context[index];
	} else
	    throw std::runtime_error("sub_context subscript type must be integer");
    }

    long len() {
	return current_setup->call_level;
    }
};
static struct wrap_context_array sub_context_array;


struct wrap_block_array {
    wrap_block *getitem(bp::object sub) {
	int index = bp::extract < int > (sub);
	return (wrap_block *) &current_setup->blocks[index];
    }

    long len() {
	return current_setup->remap_level;
    }
};
static struct wrap_block_array block_array;


static wrap_block *get_cblock(Interp *interp)
{
    setup_pointer settings = get_setup(interp);
    return  (wrap_block *)&settings->blocks[settings->remap_level];
}

static wrap_block * get_eblock(Interp *interp)
{
    setup_pointer settings = get_setup(interp);
    return  (wrap_block *)&settings->blocks[0];
}

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

    class_ <wrap_context, noncopyable>("wrap_context",no_init)
	.def_readwrite("position",&wrap_context::position)
	.def_readwrite("sequence_number",&wrap_context::sequence_number)
	.def_readwrite("filename",  &wrap_context::filename)
	.def_readwrite("subname",  &wrap_context::subName)
	// need wrapper for saved_params
	.def_readwrite("context_status", &wrap_context::context_status)
	;

    class_ <wrap_context_array, noncopyable>("wrap_context_array","Interpreter call stack",no_init)
	.def("__getitem__", &wrap_context_array::getitem,
	     return_value_policy<reference_existing_object>())
	.def("__len__", &wrap_context_array::len)
	;

    class_ <wrap_block_array, noncopyable>("wrap_block_array","Interpreter remap stack",no_init)
	.def("__getitem__", &wrap_block_array::getitem,
	     return_value_policy<reference_existing_object>())
	.def("__len__", &wrap_block_array::len)
	;

    class_ <wrap_remap,noncopyable>("wrap_remap",no_init)
	.def_readwrite("name",&wrap_remap::name)
	.def_readwrite("argspec",&wrap_remap::argspec)
	.def_readwrite("modal_group",&wrap_remap::modal_group)
	.def_readwrite("prolog_func",&wrap_remap::prolog_func)
	.def_readwrite("remap_py",&wrap_remap::remap_py)
	.def_readwrite("remap_ngc",&wrap_remap::remap_ngc)
	.def_readwrite("epilog_func",&wrap_remap::epilog_func)
	;

    class_ <wrap_block, noncopyable, std::auto_ptr<wrap_block> >("wrap_block",no_init)
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

	//  read-only
	.add_property("comment", &wrap_block::get_comment)
	.add_property("o_name", &wrap_block::get_o_name)
	.add_property("g_modes", &wrap_block::get_g_modes)
	.add_property("m_modes", &wrap_block::get_m_modes)
	.add_property("params", &wrap_block::get_params)

	.add_property("executing_remap",
		      make_function(&wrap_block::get_remap,
				    return_value_policy<reference_existing_object>()))
	;

    class_<ParamClass, noncopyable>("ParamClass","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
	.def("__setitem__", &ParamClass::setitem)
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

		       .def_readwrite("blocktext", (char *) &current_setup->blocktext)
		       .def_readwrite("call_level", &current_setup->call_level)
		       .def_readwrite("current_pocket", &current_setup->current_pocket)
		       .def_readwrite("current_tool", &current_setup->tool_table[0].toolno)
		       .def_readwrite("cutter_comp_side", &current_setup->cutter_comp_side)
		       .def_readwrite("debugmask", &current_setup->debugmask)
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
		       .def_readwrite("reload_on_change", &current_setup->py_reload_on_change)

		       .add_property("cblock",
				     make_function(&get_cblock,
						   return_value_policy<reference_existing_object>()))
		       .add_property("eblock",
				     make_function(&get_eblock,
						   return_value_policy<reference_existing_object>()))

		       );

    scope(interp_class).attr("params") = ptr(&params);
    scope(interp_class).attr("sub_context") = ptr(&sub_context_array);
    scope(interp_class).attr("blocks") = ptr(&block_array);


   }

