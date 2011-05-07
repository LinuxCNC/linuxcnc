// Support for Python oword subroutines
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

// bp::object identity(bp::object o) { return o; }


BOOST_PYTHON_MODULE(InterpMod) {
    bp::class_<Interp>("Interp")
    .def("load_tool_table",&Interp::load_tool_table);
}


int Interp::init_python(setup_pointer settings)
{
    char cmd[LINELEN];
    char *path;

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
    Py_Initialize();

    try {
	settings->module = bp::import("__main__");
	settings->module_namespace = settings->module.attr("__dict__");

	bp::object o_int10000(10000);
	settings->module.attr("tamtam") = o_int10000; // define a global

	bp::object interp_module((bp::handle<>(PyImport_ImportModule("InterpMod"))) );
	bp::scope(interp_module).attr("interp") = bp::ptr(this);
	settings->module_namespace["InterpMod"] = interp_module;

	bp::exec_file(path,
		      settings->module_namespace,
		      settings->module_namespace);

	settings->pymodule_stat = PYMOD_OK;
	free(path);
    }
    catch (bp::error_already_set) {
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
