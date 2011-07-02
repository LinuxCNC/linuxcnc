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
#include <unistd.h>
#include <exception>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"
#include "interpmodule.hh"

#define PYTHONPATH "PYTHONPATH"

static bool useGIL = false;     // not sure if this is needed

extern "C" void initCanonMod();
extern "C" void initInterpMod();

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

static  char module_path[PATH_MAX];

int Interp::init_python(setup_pointer settings, bool reload)
{
    std::string msg;
    bool py_exception = false;
    PyGILState_STATE gstate;

    static const char *orig_python_path;

    if (orig_python_path == NULL) { // record original PYTHONPATH first time around
	orig_python_path = getenv(PYTHONPATH);
	if (orig_python_path == NULL)
	    orig_python_path = "";
    }

    if ((settings->py_module_stat != PYMOD_NONE) && !reload) {
	logPy("init_python RE-INIT %d pid=%d",settings->py_module_stat,getpid());
	return INTERP_OK;  // already done, or failed
    }

    // this crap has to go.
    current_setup = get_setup(this);
    current_interp = this;

    logPy("init_python(this=%lx  pid=%d py_module_stat=%d PyInited=%d reload=%d",
	  (unsigned long)this, getpid(), settings->py_module_stat, Py_IsInitialized(),reload);

    PYCHK((settings->py_module == NULL), "init_python: no module defined");
    // PYCHK(((realpath(settings->py_module, path)) == NULL),
    // 	  "init_python: cant resolve path to '%s'", settings->py_module);

    strcpy(module_path, settings->py_path);
    strcat(module_path,"/");
    strcat(module_path, settings->py_module);
    strcat(module_path,".py");

    // record timestamp first time around
    if (settings->module_mtime == 0) {
	struct stat sub_stat;
	if (stat(module_path, &sub_stat)) {
	    logPy("init_python(): stat(%s) returns %s",
		  module_path, sys_errlist[errno]);
	} else {
	    settings->module_mtime = sub_stat.st_mtime;
	}
    }

    if (!reload) {

	//     char *newpath = (char *) malloc(strlen(orig_python_path) + strlen(settings->py_path)
	// 			   + 20 + strlen(PYTHONPATH));
	//     sprintf(newpath, "%s=%s", PYTHONPATH, settings->py_path);
	//     // if (strlen(orig_python_path)) {
	//     // 	strcat(newpath,":");
	//     // 	strcat(newpath,orig_python_path);
	//     // }
	//     printf("putenv(%s)\n",newpath);
	//     putenv(newpath);
	//     free(newpath);
	// }
	Py_SetProgramName(module_path);
	PyImport_AppendInittab( (char *) "CanonMod", &initCanonMod);
	PyImport_AppendInittab( (char *) "InterpMod", &initInterpMod);
	Py_Initialize();
	if (settings->py_path) {
	    char pathcmd[PATH_MAX];
	    sprintf(pathcmd, "import sys\nsys.path.append(\"%s\")", settings->py_path);
	    PyRun_SimpleString(pathcmd);
	}
    }

    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	bp::object module = bp::import("__main__");
	settings->module_namespace = module.attr("__dict__");
	if (!reload) {
	    bp::object interp_module = bp::import("InterpMod");
	    bp::object canon_module = bp::import("CanonMod");
	    settings->module_namespace["InterpMod"] = interp_module;
	    settings->module_namespace["CanonMod"] = canon_module;
	    // the null deallocator avoids destroying the Interp instance on leaving scope or shutdown
	    bp::scope(interp_module).attr("interp") =
		interp_ptr(this, interpDeallocFunc);
	}
	bp::object result = bp::exec_file(module_path,
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
    PYCHK(py_exception, "init_python: module '%s' init failed: \n%s",
	  module_path, msg.c_str());

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

    if (stat(module_path, &current_stat)) {
	logPy("py_reload_on_change: stat(%s) returned %s",
	      module_path,
	      sys_errlist[errno]);
	return INTERP_ERROR;
    }
    if (current_stat.st_mtime > settings->module_mtime) {
	settings->module_mtime = current_stat.st_mtime;
	int status;
	if ((status = init_python(settings,true)) != INTERP_OK) {
	    // init_python() set the error text already
	    char err_msg[LINELEN+1];
	    error_text(status, err_msg, sizeof(err_msg));
	    logPy("py_reload_on_change(%s): %s",
		  module_path, err_msg);
	    return INTERP_ERROR;
	} else
	    logPy("py_reload_on_change(): module %s reloaded",
		  settings->py_module);
    }
    return INTERP_OK;
}

// determine wether module.funcname is callable
bool Interp::is_pycallable(setup_pointer settings,
			   const char *module,
			   const char *funcname)
{
    bool result = false;
    bool py_unexpected = false;
    std::string msg;
    bp::object function;

    if (settings->py_reload_on_change)
	py_reload_on_change(settings);
    if ((settings->py_module_stat != PYMOD_OK) ||
	(funcname == NULL)) {
	return false;
    }
    try {
	if (module == NULL) {  // default to function in toplevel module
	   function = settings->module_namespace[funcname];
	} else {
	    bp::object submod =  settings->module_namespace[module];
	    bp::object submod_namespace = submod.attr("__dict__");
	    function = submod_namespace[funcname];
	}
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
	logPy("is_pycallable(%s.%s): unexpected exception:\n%s",module,funcname,msg.c_str());

    if (_setup.loggingLevel > 5)
	logPy("py_iscallable(%s.%s) = %s",module ? module : "",funcname,result ? "TRUE":"FALSE");
    return result;
}

int Interp::pycall(setup_pointer settings,
		   block_pointer block,
		   const char *module,
		   const char *funcname,
		   int calltype)
{
    bp::object retval, function;
    PyGILState_STATE gstate;
    std::string msg;
    bool py_exception = false;
    int status = INTERP_OK;
    PyObject *res_str;

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
	    (settings->py_module_stat == PYMOD_FAILED) ?
	    "initialization failed" : " not initialized");
    }
    if (useGIL)
	gstate = PyGILState_Ensure();

    try {
	switch (calltype) {

	case PY_EXECUTE:
	    retval = bp::exec(funcname,
			      _setup.module_namespace,
			      _setup.module_namespace);
	    break;

	default:
	    if (module == NULL) {
		function = settings->module_namespace[funcname];
	    } else {
		bp::object submod =  settings->module_namespace[module];
		bp::object submod_namespace = submod.attr("__dict__");
		function = submod_namespace[funcname];
	    }
	    retval = function(*block->tupleargs,**block->kwargs);
	    break;

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
	ERM("pycall: %s:\n%s", funcname, msg.c_str());
	goto done;
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
		    logPy("pycall: O <%s> call returned %f", funcname, block->py_returned_value);
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
	    } else {
		logPy("pycall(%s):  PY_INTERNAL: expected an int return code", funcname);
		res_str = PyObject_Str(retval.ptr());
		Py_XDECREF(res_str);
		ERM("Python internal function '%s' expected tuple or int return value, got '%s' (%s)",
		    funcname,
		    PyString_AsString(res_str),
		    retval.ptr()->ob_type->tp_name);
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
    if (useGIL)
	PyGILState_Release(gstate);
    return status;
}

// called by  (py, ....) comments
int Interp::py_execute(const char *cmd)
{
    std::string msg;
    PyGILState_STATE gstate;
    bool py_exception = false;

    // bp::object main_module = bp::import("__main__");
    // bp::object main_namespace = main_module.attr("__dict__");

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

    return INTERP_OK;
}


// // we borrow the interp Python environment for task-time calls
// int Interp::plugin_call(const char *name, const char *call)
// {
//     std::string msg;
//     PyGILState_STATE gstate;
//     bool py_exception = false;

//     logPy("plugin_call(%s)",call);

//     if (_setup.py_reload_on_change)
// 	py_reload_on_change(&_setup);

//     if (useGIL)
// 	gstate = PyGILState_Ensure();

//     bp::object retval, function;

//     try {
// 	bp::object args(call);
// 	function = _setup.module_namespace[name];
// 	retval = function(args);
//     }
//     catch (bp::error_already_set) {
//         if (PyErr_Occurred())  {
// 	    py_exception = true;
// 	    msg = handle_pyerror();
// 	}
//     	bp::handle_exception();
// 	PyErr_Clear();
//     }
//     if (useGIL)
// 	PyGILState_Release(gstate);

//     if (py_exception)
// 	logPy("plugin_call(%s):  %s", call, msg.c_str());


//     // must have returned an int
//     if ((retval.ptr() != Py_None) &&
// 	(PyInt_Check(retval.ptr()))) {
// 	return  bp::extract<int>(retval);
//     } else {
// 	logPy("plugin_call(%s):  expected an int return code", call);
// 	// the EMC_TASK_EXEC_xxx codes are all > 0
// 	return -1;
//     }
// }

// we borrow the interp Python environment for task-time calls
int Interp::plugin_call(const char *module, const char *name, const char *call)
{
    int status;
    block b;

    logPy("plugin_call(%s)",call);
    init_block(&b);
    if (call != NULL)
	b.tupleargs = bp::list(bp::object(call));

    if (is_pycallable(&_setup, module, name)) {
	status = pycall(&_setup, &b, module, name, PY_PLUGIN_CALL);
	if (status == INTERP_OK)
	    return b.py_returned_status;
	else
	    return INTERP_ERROR;
    } else
	return INTERP_OK;

}

int Interp::task_init()
{
    int status;
    block b;

    logPy("task_init()");
    init_block(&b);

    if (is_pycallable(&_setup, TASK_MODULE, TASK_INIT)) {
	status = pycall(&_setup, &b,TASK_MODULE,  TASK_INIT, PY_PLUGIN_CALL);
	if (status == INTERP_OK)
	    return b.py_returned_status;
	else
	    return INTERP_ERROR;
    } else
	return INTERP_OK;

}
