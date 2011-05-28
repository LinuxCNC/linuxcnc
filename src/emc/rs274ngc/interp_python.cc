// Support for Python plugin
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
#include <unistd.h>
#include <exception>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"


static bool useGIL;     // not sure if this is needed

extern "C" void initCanonMod();

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

// access to named and numbered parameters via a pseudo-dictionary
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

static void wrap_setError(Interp &x, const char *s)
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

// http://developer.valvesoftware.com/wiki/HLGameRules
// #include <boost/python.hpp>
// namespace bp = boost::python;

// CTeam* pyGetTeam(int index)
// {
// 	return GetGlobalTeam(index);
// }

// BOOST_PYTHON_MODULE(HLGameRules)
// {
// 	bp::def("GetTeam", pyGetTeam, bp::return_value_policy<bp::reference_existing_object>());

// 	bp::class_<CTeam, boost::noncopyable>("CTeam", bp::no_init)
// 		.def("GetRoundsWon", &CTeam::GetRoundsWon)
// 		.def("SetRoundsWon", &CTeam::SetRoundsWon)
// 		.def("IncrementRoundsWon", &CTeam::IncrementRoundsWon)
// 		.def("ResetScores", &CTeam::ResetScores)
// 		.def("GetScore", &CTeam::GetScore)
// 		.def("SetScore", &CTeam::SetScore)
// 		.def("AddScore", &CTeam::AddScore)
// 		.def("GetNumPlayers", &CTeam::GetNumPlayers)
// 		.def("GetName", &CTeam::GetName)
// 		.def("GetTeamNumber", &CTeam::GetTeamNumber);
// }
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



    // http://snipplr.com/view/6447/boostpython-sample-code-exposing-classes/
    //class_<block,boost::noncopyable,bases<block>>("block", no_init);

    // this be better an array!
    class_ <wrap_context,noncopyable>("wrap_context",no_init)
	.def_readwrite("position",&current_setup->sub_context[current_setup->call_level].position)
	.def_readwrite("sequence_number",&current_setup->sub_context[current_setup->call_level].sequence_number)
	.def_readwrite("filename",  &current_setup->sub_context[current_setup->call_level].filename)
	.def_readwrite("subname",  &current_setup->sub_context[current_setup->call_level].subName)
	// need wrapper for saved_params
	.def_readwrite("context_status", &current_setup->sub_context[current_setup->call_level].context_status)
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
	.def_readwrite("executing_remap",&wrap_block::executing_remap)

	// currently read-only
	.add_property("comment", &wrap_block::get_comment)
	.add_property("o_name", &wrap_block::get_o_name)
	.add_property("g_modes", &wrap_block::get_g_modes)
	.add_property("m_modes", &wrap_block::get_m_modes)
	.add_property("params", &wrap_block::get_params)

	;

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
	.def_readwrite("callframe",
		       (wrap_context *)&current_setup->sub_context[current_setup->call_level])



	.def_readwrite("cblock", (wrap_block *) &current_setup->blocks[current_setup->remap_level])

	//,     return_value_policy<bp::reference_existing_object>())
	.def_readwrite("current_pocket", &current_setup->current_pocket)
	.def_readwrite("current_tool", &current_setup->tool_table[0].toolno)
	.def_readwrite("cutter_comp_side", &current_setup->cutter_comp_side)
	.def_readwrite("debugmask", &current_setup->debugmask)
	.def_readwrite("eblock",  (wrap_block *)&current_setup->blocks[0])
	//,	       return_value_policy<bp::reference_existing_object>())
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
	// .def_readwrite("remap", (wrap_remap *)&current_setup->blocks[current_setup->remap_level].executing_remap)
	;

    class_<ParamClass,noncopyable>("ParamClass","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
	.def("__setitem__", &ParamClass::setitem);
    ;

    // enum_< theora_colorspace >("theora_colorspace")
    //     .value("OC_CS_UNSPECIFIED", OC_CS_UNSPECIFIED)
    //     .value("OC_CS_ITU_REC_470M", OC_CS_ITU_REC_470M)
    //     .value("OC_CS_ITU_REC_470BG", OC_CS_ITU_REC_470BG)
    // ;

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

int Interp::init_python(setup_pointer settings, bool reload)
{
    char path[PATH_MAX];
    interp_ptr  interp_instance;  // the wrapped instance
    std::string msg;
    bool py_exception = false;
    PyGILState_STATE gstate;

    current_setup = get_setup(this); // // &this->_setup;
    current_interp = this;

    if ((settings->py_module_stat != PYMOD_NONE) && !reload) {
	logPy("init_python RE-INIT %d pid=%d",settings->py_module_stat,getpid());
	return INTERP_OK;  // already done, or failed
    }
    logPy("init_python(this=%lx  pid=%d py_module_stat=%d PyInited=%d reload=%d",
	  (unsigned long)this, getpid(), settings->py_module_stat, Py_IsInitialized(),reload);

    PYCHK((settings->py_module == NULL), "init_python: no module defined");
    PYCHK(((realpath(settings->py_module, path)) == NULL),
	  "init_python: can resolve path to '%s'", settings->py_module);

    // record timestamp first time around
    if (settings->module_mtime == 0) {
	struct stat sub_stat;
	if (stat(path, &sub_stat)) {
	    logPy("init_python(): stat(%s) returns %s",
		  settings->py_module, sys_errlist[errno]);
	} else
	    settings->module_mtime = sub_stat.st_mtime;
    }

    if (!reload) {
	// the null deallocator avoids destroying the Interp instance on
	// leaving scope (or shutdown)
	interp_instance = interp_ptr(this,interpDeallocFunc  );

	Py_SetProgramName(path);
	PyImport_AppendInittab( (char *) "InterpMod", &initInterpMod);
	PyImport_AppendInittab( (char *) "CanonMod", &initCanonMod);
	Py_Initialize();
    }

    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	settings->module = bp::import("__main__");
	settings->module_namespace = settings->module.attr("__dict__");
	if (!reload) {
	    bp::object interp_module = bp::import("InterpMod");
	    bp::scope(interp_module).attr("interp") = interp_instance;
	    bp::scope(interp_module).attr("params") = bp::ptr(&paramclass);
	    settings->module_namespace["InterpMod"] = interp_module;
	    bp::object canon_module = bp::import("CanonMod");
	    settings->module_namespace["CanonMod"] = canon_module;
	}
	bp::object result = bp::exec_file(path,
					  settings->module_namespace,
					  settings->module_namespace);
	settings->py_module_stat = PYMOD_OK;
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    msg = handle_pyerror();
	    py_exception = true;
	}
	bp::handle_exception();
	settings->py_module_stat = PYMOD_FAILED;
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

int Interp::py_reload_on_change(setup_pointer settings)
{
    struct stat current_stat;
    if (!settings->py_module)
	return INTERP_OK;

    if (stat(settings->py_module, &current_stat)) {
	logPy("py_iscallable: stat(%s) returned %s",
	      settings->py_module,
	      sys_errlist[errno]);
	return INTERP_ERROR;
    }
    if (current_stat.st_mtime > settings->module_mtime) {
	int status;
	if ((status = init_python(settings,true)) != INTERP_OK) {
	    // init_python() set the error text already
	    char err_msg[LINELEN+1];
	    error_text(status, err_msg, sizeof(err_msg));
	    logPy("init_python(%s): %s",
		  settings->py_module,
		  err_msg);
	    return INTERP_ERROR;
	} else
	    logPy("init_python(): module %s reloaded",
		  settings->py_module);
    }
    return INTERP_OK;
}


bool Interp::is_pycallable(setup_pointer settings,
			   const char *funcname)
{
    bool result = false;
    bool py_unexpected = false;
    std::string msg;

    if (settings->py_reload_on_change)
	py_reload_on_change(settings);

    if ((settings->py_module_stat != PYMOD_OK) ||
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

    if (settings->py_reload_on_change)
	py_reload_on_change(settings);

    // &this->_setup wont work, _setup is currently private
    current_setup = get_setup(this);
    current_interp = this;
    block->returned = 0;

    if (settings->py_module_stat != PYMOD_OK) {
	ERS("function '%s.%s' : module not initialized",
	    settings->py_module,funcname);
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
	    bp::object item0 = tret[0];
	    if (PyInt_Check(item0.ptr())) {
		block->returned[RET_STATUS] = 1;
		block->py_returned_status = bp::extract<int>(item0);
		logPy("pycall: tuple item 0 - int: (%s)",
		      interp_status(block->py_returned_status));
	    }
	    if (len > 1) {
		bp::object item1 = tret[1];
		if (PyInt_Check(item1.ptr())) {
		    block->py_returned_userdata = bp::extract<int>(item1);
		    block->returned[RET_USERDATA] = 1;
		    logPy("pycall: tuple item 1: int userdata=%d",
			  block->py_returned_userdata);
		}
	    }
	    if (block->returned[RET_STATUS])
		ERP(block->py_returned_status);
	    return INTERP_OK;
	}
	PyObject *res_str = PyObject_Str(retval.ptr());
	ERS("function '%s.%s' returned '%s' - expected float or tuple, got %s",
	    settings->py_module,funcname,
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

    if (_setup.py_reload_on_change)
	py_reload_on_change(&_setup);

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


