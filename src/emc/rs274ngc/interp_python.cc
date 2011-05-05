// Support for Python oword subroutines
// Michael Haberler 4/2011
//
// FIXME mah: proper dereferencing!!
#include <boost/python.hpp>
#include <boost/make_shared.hpp>
using namespace boost::python;

#include "Python.h"
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

#define HANDLENAME "_this"

typedef boost::shared_ptr< Interp > interp_ptr;

#define PYCHK(bad, fmt, ...)				       \
    do {                                                       \
        if (bad) {                                             \
	    ERS(fmt, ## __VA_ARGS__);                          \
	    goto error;					       \
        }                                                      \
    } while(0)

int Interp::init_python(setup_pointer settings)
{
    char cmd[LINELEN];
    PyObject *py_this,*handle,*exc, *val, *tbk;

    //boost::python::object  test = boost::python::object::int_obj(42);

    logPy("init_python(this=%lx) %lx\n",
	      (unsigned long)this, (unsigned long)&this->_setup);
    if (settings->pymodule_ok)
	return INTERP_OK;
    if (settings->pymodule) {
	Py_Initialize();
	PYCHK((PyRun_SimpleString("import sys") < 0),
	      "init_python: 'import sys' failed");
	if (settings->pydir) {
	    snprintf(cmd,sizeof(cmd),"sys.path.append(\"%s\")",
		     settings->pydir);
	    PYCHK((PyRun_SimpleString(cmd) < 0),
		  "init_python: '%s' failed",cmd);
	}

	PYCHK((settings->pymodname = PyString_FromString(settings->pymodule)) == NULL,
	      "init_python: cant  PyString_FromString()");
	PYCHK((settings->pymod = PyImport_Import(settings->pymodname)) == NULL,
	      "init_python: cant import '%s'",settings->pymodule);
	PYCHK((settings->pymdict = PyModule_GetDict(settings->pymod)) == NULL,
	      "init_python: cant PyModule_GetDict");

	// check for HANDLENAME accidentially already defined
	PYCHK(((handle = PyDict_GetItemString(settings->pymdict,HANDLENAME)) != NULL),
	      "init_python: cannot store global '%s' - object already present",HANDLENAME);

	// pass interpreter instance as first argument wrapped in a long
	// so the interpreter can be called from within Python
	PYCHK(((py_this = PyLong_FromVoidPtr((void *)this)) == NULL),
	      "init_python: cannot convert '%s'",HANDLENAME);

	PYCHK(PyDict_SetItemString(settings->pymdict, HANDLENAME, py_this),
	      "init_python: cannot store global '%s' - PyDict_SetItemString failed ",HANDLENAME);


	if (PyErr_Occurred()) {
	    PyErr_Fetch(&exc, &val, &tbk);
	    ERS("init_python(%s): %s",settings->pymodule,PyString_AsString(val));
	    goto error;
	}

	settings->pymodule_ok = true;
    }
    return INTERP_OK;
 error:
    PyErr_Print();
    Py_XDECREF(settings->pymodname);
    Py_XDECREF(settings->pymod);
    Py_XDECREF(settings->pymdict);
    Py_XDECREF(py_this);
    Py_XDECREF(handle);
    Py_XDECREF(exc);
    Py_XDECREF(val);
    Py_XDECREF(tbk);
    PyErr_Clear();
    return INTERP_ERROR;
}

bool Interp::is_pycallable(setup_pointer settings,
			   const char *funcname)
{
    PyObject *func;
    bool result;

    result =  ((settings->pymdict != NULL) &&
	    ((func = PyDict_GetItemString(settings->pymdict, funcname)) != NULL) &&  /* borrowed reference */
	    (PyCallable_Check(func)));
    logPy("py_iscallable(%s) = %s\n",funcname,result ? "TRUE":"FALSE");
    return result;
}


int Interp::pycall(setup_pointer settings,
		   const char *funcname,
		   double params[])
{
    PyObject *func,*result,*tuple,
	*exc, *val, *tbk, *exc_str,
	*res_str = NULL;;

    logPy("pycall %s [%f] [%f] this=%lx\n",
	    funcname,params[0],params[1],(unsigned long)this);

    // borrowed reference
    PYCHK(((func = PyDict_GetItemString(settings->pymdict,
					funcname)) == NULL),
	  "pycall: didnt find '%s' in module '%s'",
	  funcname,settings->pymodule);


    // there must be a better way to do this
    PYCHK(((tuple = Py_BuildValue("([dddddddddddddddddddddddddddddd])",
				 params[0],params[1],params[2],params[3],params[4],params[5],params[6],
				 params[7],params[8],params[9],params[10],params[11],params[12],
				 params[13],params[14],params[15],params[16],params[17],params[18],
				 params[19],params[20],params[21],params[22],params[23],params[24],
				 params[25],params[26],params[27],params[28],params[29])) == NULL),
	  "pycall: cant build positional arguments for  '%s.%s'",
	  settings->pymodule,funcname);
    result = PyObject_Call(func, tuple,settings->kwargs);
    if (PyErr_Occurred()) {
	PyErr_Fetch(&exc, &val, &tbk);
	ERS("exception calling '%s.%s': %s",
	    settings->pymodule,
	    funcname,
	    PyString_AsString(val));
	goto error;
    }

    if ((result != Py_None) && !PyFloat_Check(result)) {
	res_str = PyObject_Str(result);
	ERS("function '%s.%s' returned '%s' - expected float, got %s",
	    settings->pymodule,funcname, PyString_AsString(res_str),
	    result->ob_type->tp_name);
	goto error;
    }
    if (result != Py_None)
	settings->return_value = PyFloat_AsDouble(result);

    return INTERP_OK;

error:
    // PyErr_Print();
    Py_XDECREF(func);
    Py_XDECREF(tuple);
    Py_XDECREF(result);
    Py_XDECREF(res_str);
    Py_XDECREF(exc_str);
    Py_XDECREF(exc);
    Py_XDECREF(val);
    Py_XDECREF(tbk);
    PyErr_Clear();
    return INTERP_ERROR;
}
