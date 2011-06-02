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
#include "interpmodule.hh"


static bool useGIL = false;     // not sure if this is needed

extern "C" void initCanonMod();
extern "C" void initInterpMod();

#define PYCHK(bad, fmt, ...)				       \
    do {                                                       \
        if (bad) {                                             \
	    logPy(fmt, ## __VA_ARGS__);			       \
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
setup_pointer current_setup;

// the Interp instance, set on call before handing to Python
Interp *current_interp;


// http://hafizpariabi.blogspot.com/2008/01/using-custom-deallocator-in.html
// reason: avoid segfaults by del(interp_instance) on program exit
// make delete(interp_instance) a noop wrt Interp
static void interpDeallocFunc(Interp *interp) {}

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
    handle<> hexc(exc),hval(val),htb(allow_null(tb));
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

int Interp::init_python(setup_pointer settings, bool reload)
{
    char path[PATH_MAX];
    std::string msg;
    bool py_exception = false;
    PyGILState_STATE gstate;


    if ((settings->py_module_stat != PYMOD_NONE) && !reload) {
	logPy("init_python RE-INIT %d pid=%d",settings->py_module_stat,getpid());
	return INTERP_OK;  // already done, or failed
    }

    current_setup = get_setup(this); //  &this->_setup;
    current_interp = this;

    logPy("init_python(this=%lx  pid=%d py_module_stat=%d PyInited=%d reload=%d",
	  (unsigned long)this, getpid(), settings->py_module_stat, Py_IsInitialized(),reload);

    PYCHK((settings->py_module == NULL), "init_python: no module defined");
    PYCHK(((realpath(settings->py_module, path)) == NULL),
	  "init_python: cant resolve path to '%s'", settings->py_module);

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
	Py_SetProgramName(path);
	PyImport_AppendInittab( (char *) "CanonMod", &initCanonMod);
	PyImport_AppendInittab( (char *) "InterpMod", &initInterpMod);
	Py_Initialize();
    }

    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	settings->module = bp::import("__main__");
	settings->module_namespace = settings->module.attr("__dict__");
	if (!reload) {
	    bp::object interp_module = bp::import("InterpMod");
	    bp::object canon_module = bp::import("CanonMod");
	    settings->module_namespace["InterpMod"] = interp_module;
	    settings->module_namespace["CanonMod"] = canon_module;
	    // the null deallocator avoids destroying the Interp instance on
	    // leaving scope (or shutdown)
	    bp::scope(interp_module).attr("interp") =
		interp_ptr(this, interpDeallocFunc);

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
	logPy("init_python: module '%s' init failed: \n%s",path,msg.c_str());
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

int Interp::pycall(setup_pointer settings,
		   block_pointer block,
		   const char *funcname)
{
    bp::object retval, function;
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
	ERS("function '%s.%s' : module %s",
	    settings->py_module,funcname,
	    (settings->py_module_stat == PYMOD_FAILED) ? "initialization failed" : " not initialized");
	return INTERP_OK;
    }
    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	function = settings->module_namespace[funcname];
	retval = function(*block->tupleargs,**block->kwargs);
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    msg = handle_pyerror();
	}
	py_exception = true;
	bp::handle_exception();
	PyErr_Clear();
    }
    if (useGIL)
	    PyGILState_Release(gstate);
    if (py_exception) {
	fprintf(stderr,"-->%s\n",msg.c_str());
	ERS("pycall: %s", msg.c_str());
	return INTERP_ERROR;
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


