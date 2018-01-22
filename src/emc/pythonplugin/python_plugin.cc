/*    This is a component of LinuxCNC
 *    Copyright 2011, 2012, 2013 Jeff Epler <jepler@dsndata.com>, Michael
 *    Haberler <git@mah.priv.at>, Sebastian Kuzminsky <seb@highlab.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "python_plugin.hh"
#include "inifile.hh"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BOOST_PYTHON_MAX_ARITY 4
#include <boost/python/exec.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/import.hpp>

namespace bp = boost::python;

#define MAX_ERRMSG_SIZE 256

#define ERRMSG(fmt, args...)					\
    do {							\
        char msgbuf[MAX_ERRMSG_SIZE];				\
        snprintf(msgbuf, sizeof(msgbuf) -1,  fmt, ##args);	\
        error_msg = std::string(msgbuf);			\
    } while(0)

#define logPP(level, fmt, ...)						\
    do {								\
        ERRMSG(fmt, ## __VA_ARGS__);					\
	if (log_level >= level) {					\
	    fprintf(stderr, fmt, ## __VA_ARGS__);			\
	    fprintf(stderr,"\n");					\
	}								\
    } while (0)


extern const char *strstore(const char *s);

// boost python versions from 1.58 to 1.61 (the latest at the time of
// writing) all have a bug in boost::python::execfile that results in a
// double free.  Work around it by using the Python implementation of
// execfile instead.
// The bug was introduced at https://github.com/boostorg/python/commit/fe24ab9dd5440562e27422cd38f7de03356bfd16
bp::object working_execfile(const char *filename, bp::object globals, bp::object locals) {
    return bp::import("__builtin__").attr("execfile")(filename, globals, locals);
}

int PythonPlugin::run_string(const char *cmd, bp::object &retval, bool as_file)
{
    reload();
    try {
	if (as_file)
	    retval = working_execfile(cmd, main_namespace, main_namespace);
	else
	    retval = bp::exec(cmd, main_namespace, main_namespace);
	status = PLUGIN_OK;
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	    exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	status = PLUGIN_EXCEPTION;
	bp::handle_exception();
	PyErr_Clear();
    }
    if (status == PLUGIN_EXCEPTION) {
	logPP(0, "run_string(%s): \n%s",
	      cmd, exception_msg.c_str());
    }
    return status;
}

int PythonPlugin::call_method(bp::object method, bp::object &retval) 
{

    logPP(1, "call_method()");
    if (status < PLUGIN_OK)
	return status;

    try {
	retval = method(); 
	status = PLUGIN_OK;
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	   exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	status = PLUGIN_EXCEPTION;
	bp::handle_exception();
	PyErr_Clear();
    }
    if (status == PLUGIN_EXCEPTION) {
	logPP(0, "call_method(): %s", exception_msg.c_str());
    }
    return status;

}

int PythonPlugin::call(const char *module, const char *callable,
		       bp::object tupleargs, bp::object kwargs, bp::object &retval)
{
    bp::object function;

    if (callable == NULL)
	return PLUGIN_NO_CALLABLE;

    reload();

    if (status < PLUGIN_OK)
	return status;

    try {
	if (module == NULL) {  // default to function in toplevel module
	    function = main_namespace[callable];
	} else {
	    bp::object submod =  main_namespace[module];
	    bp::object submod_namespace = submod.attr("__dict__");
	    function = submod_namespace[callable];
	}
	// this wont work with boost-python1.34 - needs 1.40
	//retval = function(*tupleargs, **kwargs);

	// this does
	PyObject *rv = PyObject_Call(function.ptr(), tupleargs.ptr(), kwargs.ptr());
	if (PyErr_Occurred()) 
	    bp::throw_error_already_set();
	if (rv) 
	    retval = bp::object(bp::borrowed(rv));
	else
	    retval = bp::object();
	status = PLUGIN_OK;
    }
    catch (bp::error_already_set) {
	if (PyErr_Occurred()) {
	   exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	status = PLUGIN_EXCEPTION;
	bp::handle_exception();
	PyErr_Clear();
    }
    if (status == PLUGIN_EXCEPTION) {
	logPP(0, "call(%s%s%s): \n%s",
	      module ? module : "",
	      module ? "." : "",
	      callable, exception_msg.c_str());
    }
    return status;
}

bool PythonPlugin::is_callable(const char *module,
			       const char *funcname)
{
    bool unexpected = false;
    bool result = false;
    bp::object function;

    reload();
    if ((status != PLUGIN_OK) ||
	(funcname == NULL)) {
	return false;
    }
    try {
	if (module == NULL) {  // default to function in toplevel module
	   function = main_namespace[funcname];
	} else {
	    bp::object submod =  main_namespace[module];
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
	logPP(0, "is_callable(%s%s%s): unexpected exception:\n%s",
	      module ? module : "", module ? "." : "",
	      funcname,exception_msg.c_str());

    if (log_level)
	logPP(4, "is_callable(%s%s%s) = %s",
	      module ? module : "", module ? "." : "",
	      funcname,result ? "TRUE":"FALSE");
    return result;
}

// this should be moved to an inotify-based solution and be done with it
int PythonPlugin::reload()
{
    struct stat st;
    if (!reload_on_change)
	return PLUGIN_OK;

    if (stat(abs_path, &st)) {
	logPP(0, "reload: stat(%s) returned %s", abs_path, strerror(errno));
	status = PLUGIN_STAT_FAILED;
	return status;
    }
    if (st.st_mtime > module_mtime) {
	module_mtime = st.st_mtime;
	initialize();
	logPP(1, "reload():  %s reloaded, status=%d", toplevel, status);
    } else {
	logPP(5, "reload: no-op");
	status = PLUGIN_OK;
    }
    return status;
}

// decode a Python exception into a string.
// Free function usable without working plugin instance.
std::string handle_pyerror()
{
    PyObject *exc, *val, *tb;
    bp::object formatted_list, formatted;

    PyErr_Fetch(&exc, &val, &tb);
    bp::handle<> hexc(exc), hval(bp::allow_null(val)), htb(bp::allow_null(tb));
    bp::object traceback(bp::import("traceback"));
    if (!tb) {
	bp::object format_exception_only(traceback.attr("format_exception_only"));
	formatted_list = format_exception_only(hexc, hval);
    } else {
	bp::object format_exception(traceback.attr("format_exception"));
	formatted_list = format_exception(hexc, hval, htb);
    }
    formatted = bp::str("\n").join(formatted_list);
    return bp::extract<std::string>(formatted);
}

int PythonPlugin::initialize()
{
    std::string msg;
    if (Py_IsInitialized()) {
	try {
	    bp::object module = bp::import("__main__");
	    main_namespace = module.attr("__dict__");

	    for(unsigned i = 0; i < inittab_entries.size(); i++) {
		main_namespace[inittab_entries[i]] = bp::import(inittab_entries[i].c_str());
	    }
	    if (toplevel) // only execute a file if there's one configured.
		bp::object result = working_execfile(abs_path,
						  main_namespace,
						  main_namespace);
	    status = PLUGIN_OK;
	}
	catch (bp::error_already_set) {
	    if (PyErr_Occurred()) {
		exception_msg = handle_pyerror();
	    } else
		exception_msg = "unknown exception";
	    bp::handle_exception();
	    status = PLUGIN_INIT_EXCEPTION;
	    PyErr_Clear();
	}
	if (status == PLUGIN_INIT_EXCEPTION) {
	    logPP(-1, "initialize: module '%s' init failed: \n%s",
		  abs_path, exception_msg.c_str());
	}
    } else {
	logPP(-1, "initialize: Plugin not initialized");
	status = PLUGIN_PYTHON_NOT_INITIALIZED;
    }
    return status;
}

PythonPlugin::PythonPlugin(struct _inittab *inittab) :
    status(0),
    module_mtime(0),
    reload_on_change(0),
    toplevel(0),
    abs_path(0),
    log_level(0)
{
    Py_SetProgramName((char *) abs_path);

    if ((inittab != NULL) &&
	PyImport_ExtendInittab(inittab)) {
	logPP(-1, "cant extend inittab");
	status = PLUGIN_INITTAB_FAILED;
	return;
    }
    Py_Initialize();
    initialize();
}


int PythonPlugin::configure(const char *iniFilename,
			   const char *section) 
{
    IniFile inifile;
    const char *inistring;

    if (section == NULL) {
	logPP(1, "no section");
	status = PLUGIN_NO_SECTION;
	return status;
    }
    if ((iniFilename == NULL) &&
	((iniFilename = getenv("INI_FILE_NAME")) == NULL)) {
	logPP(-1, "no inifile");
	status = PLUGIN_NO_INIFILE;
	return status;
    }
    if (inifile.Open(iniFilename) == false) {
          logPP(-1, "Unable to open inifile:%s:\n", iniFilename);
	  status = PLUGIN_BAD_INIFILE;
	  return status;
    }

    char real_path[PATH_MAX];
    if ((inistring = inifile.Find("TOPLEVEL", section)) != NULL) {
	toplevel = strstore(inistring);

	if ((inistring = inifile.Find("RELOAD_ON_CHANGE", section)) != NULL)
	    reload_on_change = (atoi(inistring) > 0);

	if (realpath(toplevel, real_path) == NULL) {
	    logPP(-1, "cant resolve path to '%s'", toplevel);
	    status = PLUGIN_BAD_PATH;
	    return status;
	}
	struct stat st;
	if (stat(real_path, &st)) {
	    logPP(1, "stat(%s) returns %s", real_path, strerror(errno));
	    status = PLUGIN_STAT_FAILED;
	    return status;
	}
	abs_path = strstore(real_path);
	module_mtime = st.st_mtime;      // record timestamp

    } else {
        if (getcwd(real_path, PATH_MAX) == NULL) {
            logPP(1, "path too long");
            status = PLUGIN_PATH_TOO_LONG;
            return status;
        }
	abs_path = strstore(real_path);
    }

    if ((inistring = inifile.Find("LOG_LEVEL", section)) != NULL)
	log_level = atoi(inistring);
    else log_level = 0;

    char pycmd[PATH_MAX];
    int n = 1;
    int lineno;
    while (NULL != (inistring = inifile.Find("PATH_PREPEND", "PYTHON",
					     n, &lineno))) {
	sprintf(pycmd, "import sys\nsys.path.insert(0,\"%s\")", inistring);
	logPP(1, "%s:%d: executing '%s'",iniFilename, lineno, pycmd);

	if (PyRun_SimpleString(pycmd)) {
	    logPP(-1, "%s:%d: exception running '%s'",iniFilename, lineno, pycmd);
	    exception_msg = "exception running:" + std::string((const char*)pycmd);
	    status = PLUGIN_EXCEPTION_DURING_PATH_PREPEND;
	    return status;
	}
	n++;
    }
    n = 1;
    while (NULL != (inistring = inifile.Find("PATH_APPEND", "PYTHON",
					     n, &lineno))) {
	sprintf(pycmd, "import sys\nsys.path.append(\"%s\")", inistring);
	logPP(1, "%s:%d: executing '%s'",iniFilename, lineno, pycmd);
	if (PyRun_SimpleString(pycmd)) {
	    logPP(-1, "%s:%d: exception running '%s'",iniFilename, lineno, pycmd);
	    exception_msg = "exception running " + std::string((const char*)pycmd);
	    status = PLUGIN_EXCEPTION_DURING_PATH_APPEND;
	    return status;
	}
	n++;
    }
    logPP(3,"PythonPlugin: Python  '%s'",  Py_GetVersion());
    return initialize();
}

// the externally visible singleton instance
PythonPlugin *python_plugin;


// first caller wins
// this splits instantiation from configuring PYTHONPATH, imports etc
PythonPlugin *PythonPlugin::instantiate(struct _inittab *inittab)
{
    if (python_plugin == NULL) {
	python_plugin = new PythonPlugin(inittab);
    }
    return (python_plugin->usable()) ? python_plugin : NULL;
}

