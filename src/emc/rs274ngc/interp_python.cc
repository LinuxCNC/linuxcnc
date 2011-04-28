// Support for Python oword subroutines
// Michael Haberler 4/2011
//

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

    fprintf(stderr,"--- init_python(this=%lx) %lx\n",(unsigned long)this,(unsigned long)&this->_setup);
    if (settings->pymodule_ok)
	return INTERP_OK;
    if (settings->pymodule) {
	Py_Initialize();
	PYCHK((PyRun_SimpleString("import sys") < 0),"init_python: 'import sys' failed");
	if (settings->pydir) {
	    snprintf(cmd,sizeof(cmd),"sys.path.append(\"%s\")",settings->pydir);
	    PYCHK((PyRun_SimpleString(cmd) < 0),"init_python: '%s' failed",cmd);
	}

	PYCHK((settings->pymodname = PyString_FromString(settings->pymodule)) == NULL,"init_python: cant  PyString_FromString()");
	PYCHK((settings->pymod = PyImport_Import(settings->pymodname)) == NULL,"init_python: cant import '%s'",settings->pymodule);
	PYCHK((settings->pymdict = PyModule_GetDict(settings->pymod)) == NULL,"init_python: cant PyModule_GetDict");
	settings->pymodule_ok = true;
    }
    return INTERP_OK;
 error:
    PyErr_Print();
    Py_XDECREF(settings->pymodname);
    Py_XDECREF(settings->pymod);
    Py_XDECREF(settings->pymdict);
    PyErr_Clear();
    return INTERP_ERROR;
}

bool Interp::is_pycallable(setup_pointer settings,const char *funcname)
{
    PyObject *func;
    bool result;

    result =  ((settings->pymdict != NULL) &&
	    ((func = PyDict_GetItemString(settings->pymdict, funcname)) != NULL) &&  /* borrowed reference */
	    (PyCallable_Check(func)));
    fprintf(stderr,"---- py_iscallable(%s) return %d\n",funcname,result);
    return result;
}


int Interp::pycall(setup_pointer settings,
		   const char *funcname,
		   double params[])
{
    PyObject *func,*result,*tuple,*py_this,
	*exc, *val, *tbk, *exc_str,
	*res_str = NULL;;

    // pass interpreter instance as first argument wrapped in a PyCObject
    // so the interpreter can be called from within Python
    PYCHK(((py_this = PyCObject_FromVoidPtr((void *)this, NULL)) == NULL),
	  "pycall: calling '%s': cannot convert 'this'",funcname);

    fprintf(stderr,"---- pycall %s [%f] [%f] this=%lx\n",
	    funcname,params[0],params[1],(unsigned long)this);

    // borrowed reference
    PYCHK(((func = PyDict_GetItemString(settings->pymdict,
					funcname)) == NULL),
	  "pycall: didnt find '%s' in module '%s'",
	  funcname,settings->pymodule);


    // there must be a better way to do this
    PYCHK(((tuple = Py_BuildValue("(O,[dddddddddddddddddddddddddddddd])", py_this,
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
	ERS("exception calling '%s.%s': %s",settings->pymodule,funcname,PyString_AsString(val));
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
    Py_XDECREF(py_this);
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
