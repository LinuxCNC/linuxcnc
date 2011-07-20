

#include "python_plugin.hh"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <exception>

#define MAX_ERRMSG_SIZE 200

#define ERRMSG(fmt, args...)					\
    do {							\
        char msgbuf[MAX_ERRMSG_SIZE];				\
        snprintf(msgbuf, sizeof(msgbuf) -1,  fmt, ##args);	\
        error_msg = std::string(msgbuf);			\
    } while(0)


#define PYCHK(bad, fmt, ...)					       \
    do {							       \
	if (bad) {						       \
	    fprintf(stderr,fmt, ## __VA_ARGS__);		       \
	    fprintf(stderr,"\n");				       \
	    ERRMSG(fmt, ## __VA_ARGS__);			       \
	    return PLUGIN_ERROR;				       \
	}							       \
    } while(0)

#define logPP(fmt, ...)  fprintf(stderr, fmt, ## __VA_ARGS__)

PythonPlugin::PythonPlugin(int loglevel)
{
    module_status = PYMOD_NONE;
    log_level = loglevel;
}

PythonPlugin::~PythonPlugin()
{
    // boost.python dont support PyFinalize()
}


int PythonPlugin::setup(const char *modpath, const char *modname , bool reload_if_changed)
{
    reload_on_change = reload_if_changed;
    PYCHK((modname == NULL), "initialize: no module defined");
    module = modname; // ???
    if (modpath) {
	strcpy(module_path, modpath);
	strcat(module_path,"/");
    } else {
	module_path[0] = '\0';
    }
    strcat(module_path, modname);
    strcat(module_path,".py");

    char path[PATH_MAX];
    PYCHK(((realpath(module_path, path)) == NULL),
	  "setup: cant resolve path to '%s'", module_path);

    // record timestamp
    struct stat st;
    PYCHK(stat(module_path, &st),
	  "setup(): stat(%s) returns %s", module_path, strerror(errno));
    module_mtime = st.st_mtime;
    Py_SetProgramName(module_path);
    return PLUGIN_OK;
}

int PythonPlugin::add_inittab_entry(const char *mod_name, void (*mod_init)())
{
    PYCHK(PyImport_AppendInittab( (char *) mod_name , mod_init),
	  "cant extend inittab with module '%s'", mod_name);
    modules.push_back(mod_name);
    return PLUGIN_OK;
}

int PythonPlugin::initialize(bool reload)
{
    std::string msg;

    if (!reload) {
	Py_Initialize();
	if (module_path[0]) {
	    char pathcmd[PATH_MAX];
	    sprintf(pathcmd, "import sys\nsys.path.append(\"%s\")", module);
	    PYCHK(PyRun_SimpleString(pathcmd),
		  "exeception running '%s'", pathcmd);
	}
    }
    try {
	bp::object module = bp::import("__main__");
	module_namespace = module.attr("__dict__");

	for(unsigned i = 0; i < modules.size(); i++) {
	    module_namespace[modules[i]] = bp::import(modules[i].c_str());
	}
	// FIXME
	// the null deallocator avoids destroying the Interp instance on leaving scope or shutdown
	// bp::scope(interp_module).attr("interp") =
	// 	interp_ptr(this, interpDeallocFunc);
	bp::object result = bp::exec_file(module_path,
					  module_namespace,
					  module_namespace);
	module_status = PYMOD_OK;
    }
    catch (bp::error_already_set) {
	python_exception = true;
	if (PyErr_Occurred()) {
	    exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	bp::handle_exception();
	module_status = PYMOD_FAILED;
	PyErr_Clear();
    }
    PYCHK(python_exception, "initialize: module '%s' init failed: \n%s",
	  module_path, exception_msg.c_str());
    return PLUGIN_OK;
}


int PythonPlugin::run_string(const char *cmd, bp::object &retval)
{
    if (reload_on_change)
	reload();
    try {
	retval = bp::exec(cmd, module_namespace, module_namespace);
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	   exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	python_exception = true;
	bp::handle_exception();
	PyErr_Clear();
    }
    PYCHK(python_exception, "run_string(%s): \n%s",
	  cmd, exception_msg.c_str());
    return PLUGIN_OK;
}

int PythonPlugin::call(const char *module, const char *callable,
		       bp::object tupleargs, bp::object kwargs, bp::object &retval)
{
    bp::object function;

    if (reload_on_change)
	reload();
    if ((module_status != PYMOD_OK) ||
	(callable == NULL))
	return PLUGIN_ERROR;

    try {
	if (module == NULL) {  // default to function in toplevel module
	    function = module_namespace[callable];
	} else {
	    bp::object submod =  module_namespace[module];
	    bp::object submod_namespace = submod.attr("__dict__");
	    function = submod_namespace[callable];
	}
	retval = function(tupleargs,kwargs);
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	   exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	python_exception = true;
	bp::handle_exception();
	PyErr_Clear();
    }
    PYCHK(python_exception, "call(%s.%s): \n%s",
	  module, callable, exception_msg.c_str());

    return PLUGIN_OK;
}

bool PythonPlugin::is_callable(const char *module,
			   const char *funcname)
{
    bool unexpected = false;
    bool result = false;
    bp::object function;

    if (reload_on_change)
	reload();
    if ((module_status != PYMOD_OK) ||
	(funcname == NULL)) {
	return false;
    }
    try {
	if (module == NULL) {  // default to function in toplevel module
	   function = module_namespace[funcname];
	} else {
	    bp::object submod =  module_namespace[module];
	    bp::object submod_namespace = submod.attr("__dict__");
	    function = submod_namespace[funcname];
	}
	result = PyCallable_Check(function.ptr());
    }
    catch (bp::error_already_set) {
	// KeyError expected if not callable
	if (!PyErr_ExceptionMatches(PyExc_KeyError)) {
	    // something else, strange
	    exception_msg = handle_pyerror();
	    unexpected = true;
	}
	result = false;
	PyErr_Clear();
    }
    if (unexpected)
	logPP("is_pycallable(%s.%s): unexpected exception:\n%s",module,funcname,exception_msg.c_str());

    if (log_level)
	logPP("is_pycallable(%s.%s) = %s", module ? module : "",funcname,result ? "TRUE":"FALSE");
    return result;


    return false;
}

int PythonPlugin::reload()
{
    struct stat st;
    if (module == NULL)
	return PLUGIN_OK;

    if (stat(module_path, &st)) {
	logPP("reload: stat(%s) returned %s", module_path, strerror(errno));
	return PLUGIN_ERROR;
    }
    if (st.st_mtime > module_mtime) {
	module_mtime = st.st_mtime;
	int status;
	if ((status = initialize(true)) != PLUGIN_OK) {
	    // // init_python() set the error text already
	    // char err_msg[LINELEN+1];
	    // error_text(status, err_msg, sizeof(err_msg));
	    // logPP("reload(%s): %s",  module_path, err_msg);
	    return PLUGIN_ERROR;
	} else
	    logPP("reload(): module %s reloaded", module);
    }
    return PLUGIN_OK;
}

int PythonPlugin::plugin_status()
{
    return PLUGIN_OK;

}

std::string PythonPlugin::last_exception()
{
    return exception_msg;
}

std::string PythonPlugin::last_errmsg()
{
    return error_msg;
}


// decode a Python exception into a string.
std::string PythonPlugin::handle_pyerror()
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
