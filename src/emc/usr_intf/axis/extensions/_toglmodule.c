#include <Python.h>
#include <emc/usr_intf/axis/extensions/togl.c>
static int first_time = 1;

static Tcl_Interp *get_interpreter(PyObject *tkapp) {
    long interpaddr;
    PyObject *interpaddrobj = PyObject_CallMethod(tkapp, "interpaddr", NULL);
    if(interpaddrobj == NULL) { return NULL; }
    interpaddr = PyInt_AsLong(interpaddrobj);
    Py_DECREF(interpaddrobj);
    if(interpaddr == -1) { return NULL; }
    return (Tcl_Interp*)interpaddr;
}

PyObject *install(PyObject *s, PyObject *arg) {
    Tcl_Interp *trp = get_interpreter(arg);
    if(!trp) {
        PyErr_SetString(PyExc_TypeError, "get_interpreter() returned NULL");
        return NULL;
    }
    if (Tcl_PkgPresent(trp, "Togl", TOGL_VERSION, 0)) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    if (Tcl_PkgProvide(trp, "Togl", TOGL_VERSION) != TCL_OK) {
        PyErr_Format(PyExc_RuntimeError, "Tcl_PkgProvide failed: %s", trp->result);
        return NULL;
    }

    Tcl_CreateCommand(trp, "togl", (Tcl_CmdProc *)Togl_Cmd,
                      (ClientData) Tk_MainWindow(trp), NULL);

    if(first_time) {
        Tcl_InitHashTable(&CommandTable, TCL_STRING_KEYS);
        first_time = 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef togl_methods[] = {
    {"install", (PyCFunction)install, METH_O, "install togl in a tkinter application"},
    {NULL}
};

PyMODINIT_FUNC
init_togl(void) {
    Py_InitModule3("_togl", togl_methods, "togl extension for Tkinter");
}
