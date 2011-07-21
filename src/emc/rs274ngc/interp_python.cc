// Support for embedding Python in the RS274NGC interpreter
// with access to Interp and Canon
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
#include <exception>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"
#include "interpmodule.hh"

// #define PYTHONPATH "PYTHONPATH"


// extern "C" void initCanon();
// extern "C" void initinterpreter();
// extern "C" void initTaskMod();

#define PYCHK(bad, fmt, ...)				       \
    do {                                                       \
        if (bad) {                                             \
	    logPy(fmt, ## __VA_ARGS__);			       \
	    ERM(fmt, ## __VA_ARGS__);                          \
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
setup_pointer current_setup;

// the Interp instance, set on call before handing to Python
Interp *current_interp;


// http://hafizpariabi.blogspot.com/2008/01/using-custom-deallocator-in.html
// reason: avoid segfaults by del(interp_instance) on program exit
// make delete(interp_instance) a noop wrt Interp
// static void interpDeallocFunc(Interp *interp) {}

#define IS_STRING(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyString_Type))
#define IS_INT(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyInt_Type))


// decode a Python exception into a string.
std::string handle_pyerror()
{
    using namespace boost::python;
    using namespace boost;

    PyObject *exc,*val,*tb;
    object formatted_list, formatted;
    PyErr_Fetch(&exc,&val,&tb);
    handle<> hexc(exc),hval(allow_null(val)),htb(allow_null(tb));
    object traceback(import("traceback"));
    if (!tb) {
	object format_exception_only(traceback.attr("format_exception_only"));
	formatted_list = format_exception_only(hexc,hval);
    } else {
	object format_exception(traceback.attr("format_exception"));
	formatted_list = format_exception(hexc,hval,htb);
    }
    formatted = str("\n").join(formatted_list);
    return extract<std::string>(formatted);
}


// determine wether [module.]funcname is callable
bool Interp::is_pycallable(setup_pointer settings,
			   const char *module,
			   const char *funcname)
{
  if (!_setup.pp)
      return false;

    return settings->pp->is_callable(module,funcname);
}

// all parameters to Python calls go through block, which looks a bit awkward
// the reason is not to expose boost.python through the interpreter public interface
int Interp::pycall(setup_pointer settings,
		   block_pointer block,
		   const char *module,
		   const char *funcname,
		   int calltype)
{
    bp::object retval, function;
    std::string msg;
    bool py_exception = false;
    int status = INTERP_OK;
    PyObject *res_str;

    if (_setup.loggingLevel > 4)
	logPy("pycall(%s.%s) \n", module ? module : "", funcname);


    current_setup = get_setup(this);
    current_interp = this;
    block->returned = 0;

    if (settings->pp->plugin_status() < PLUGIN_OK) {
	ERS("function '%s.%s' : plugin status bad=%d",
	    module ? module : "", funcname,
	    settings->pp->plugin_status());
    }

    switch (calltype) {
    case PY_EXECUTE: // just run a string
	_setup.pp->run_string(funcname, retval);
	break;
    default:
	status = _setup.pp->call(module,funcname, block->tupleargs,block->kwargs,retval);
	break;
    }
    try {
	switch  (calltype) {
	case PY_OWORDCALL:
	    // if this was called as "o<pythoncallable> call",
	    // expect either a float or no return value.
	    status = INTERP_OK;
	    if (retval.ptr() != Py_None) {
		if (PyFloat_Check(retval.ptr())) {
		    block->py_returned_value = bp::extract<double>(retval);
		    block->returned[RET_DOUBLE] = 1;
		    logPy("call: O <%s> call returned %f", funcname, block->py_returned_value);
		} else {
		    // not a float, strange
		    PyObject *res_str = PyObject_Str(retval.ptr());
		    Py_XDECREF(res_str);
		    ERM("Python call for 'O<%s> call returned '%s' - expected float value, got %s",
			funcname,
			PyString_AsString(res_str),
			retval.ptr()->ob_type->tp_name);
		    status = INTERP_ERROR;
		}
	    } else {
		// no value was returned by Python callable
	    }
	    break;

	case PY_PROLOG:
	case PY_REMAP:
	case PY_EPILOG:
	    // these may return values in two flavours:
	    // - an int (INTERP_OK, INTERP_ERROR, INTERP_EXECUTE_FINISH...)
	    // - a tuple (returncode, userdata)
	    // - returning no value is considered bad practice and raises an error
	    if (retval.ptr() != Py_None) {
		if (PyTuple_Check(retval.ptr())) {
		    // this will raise a Python exception on type mismatch
		    bp::tuple rtuple = bp::extract<bp::tuple>(retval);
		    status = block->py_returned_status = bp::extract<int>(rtuple[0]);
		    block->returned[RET_STATUS] = 1;
		    block->py_returned_userdata = bp::extract<int>(rtuple[1]);
		    block->returned[RET_USERDATA] = 1;
		} else {
		    status = block->py_returned_status = bp::extract<int>(retval);
		    block->returned[RET_STATUS] = 1;
		}
		goto done;
	    }
	    res_str = PyObject_Str(retval.ptr());
	    Py_XDECREF(res_str);
	    ERM("Python %s function '%s' expected tuple or int return value, got '%s' (%s)",
		(calltype == PY_PROLOG ? "prolog" : (calltype == PY_EPILOG) ? "epilog" : "remap"),
		funcname,
		PyString_AsString(res_str),
		retval.ptr()->ob_type->tp_name);
	    status = INTERP_ERROR;
	    break;

	case PY_INTERNAL:
	case PY_PLUGIN_CALL:
	    // a plain int (INTERP_OK, INTERP_ERROR, INTERP_EXECUTE_FINISH...) is expected
	    // must have returned an int
	    if ((retval.ptr() != Py_None) &&
		(PyInt_Check(retval.ptr()))) {
		status = block->py_returned_status = bp::extract<int>(retval);
		block->returned[RET_STATUS] = 1;
		logPy("pycall(%s):  PY_INTERNAL/PY_PLUGIN_CALL: return code=%d", funcname,status);
	    } else {
		logPy("pycall(%s):  PY_INTERNAL: expected an int return code", funcname);
		res_str = PyObject_Str(retval.ptr());
		ERM("Python internal function '%s' expected tuple or int return value, got '%s' (%s)",
		    funcname,
		    PyString_AsString(res_str),
		    retval.ptr()->ob_type->tp_name);
		Py_XDECREF(res_str);
		status = INTERP_ERROR;
	    }
	    break;
	case PY_EXECUTE:
	    break;

	default: ;
	}
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    msg = handle_pyerror();
	}
	py_exception = true;
	bp::handle_exception();
	PyErr_Clear();
    }
    if (py_exception) {
	ERM("pycall: %s.%s:\n%s", module ? module:"", funcname, msg.c_str());
	status = INTERP_ERROR;
    }
 done:
    return status;
}

// called by  (py, ....) or ';py,...' comments
int Interp::py_execute(const char *cmd, bool as_file)
{
    bp::object retval;

    if (!_setup.pp)
	return INTERP_OK;

    logPy("py_execute(%s)",cmd);

    _setup.pp->run_string(cmd, retval, as_file);
    CHKS((_setup.pp->plugin_status() == PLUGIN_EXCEPTION),
	 "py_execute(%s)%s:\n%s", cmd,
	 as_file ? " as file" : "", _setup.pp->last_exception().c_str());
    return INTERP_OK;
}

