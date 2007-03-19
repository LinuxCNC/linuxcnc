/*
Copyright 2005 Jeff Epler
All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, 

JEFF EPLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL JEFF EPLER
BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <Python.h>

#include <tk.h>
extern Tk_Window TkpGetWrapperWindow(Tk_Window);

Tcl_Interp *get_interpreter(PyObject *tkapp) {
    long interpaddr;
    PyObject *interpaddrobj = PyObject_CallMethod(tkapp, "interpaddr", NULL);
    if(interpaddrobj == NULL) { return NULL; }
    interpaddr = PyInt_AsLong(interpaddrobj);
    Py_DECREF(interpaddrobj);
    if(interpaddr == -1) { return NULL; }
    return (Tcl_Interp*)interpaddr;
}

static Atom NET_WM_ICON, NET_WM_NAME, CARDINAL, UTF8_STRING;
PyObject *setname(PyObject *s, PyObject *o) {
    Tcl_Interp *trp;
    Tk_Window tkwin, tkwrap;
    PyObject *win, *app, *path;
    char *title=0;
    int sz=0;

    if(!PyArg_ParseTuple(o, "Oes#", &win, "utf-8", &title, &sz))
        return NULL;

    printf("title[%d] %s\n", sz, title);
    app = PyObject_GetAttrString(win, "tk");
    if(!app) goto OUT_NULL; 
    path = PyObject_GetAttrString(win, "_w");
    if(!path) goto OUT_NULL;
    if(!PyString_Check(path)) {
        PyErr_SetString(PyExc_ValueError, "Not a widget?");
    }

    trp = get_interpreter(app);    
    if(!PyString_Check(path)) {
        PyErr_SetString(PyExc_ValueError, "Not a widget?");
        goto OUT_NULL;
    }

    tkwin = Tk_NameToWindow(trp, PyString_AsString(path), Tk_MainWindow(trp));
    Tk_MakeWindowExist(tkwin);
    tkwrap = TkpGetWrapperWindow(tkwin);
    if(!tkwrap) {
        PyErr_SetString(PyExc_ValueError, "No wrapper widget?");
        goto OUT_NULL;
    }
    Tk_MakeWindowExist(tkwrap);

    if(!NET_WM_NAME) {
        NET_WM_NAME = XInternAtom(Tk_Display(tkwin), "_NET_WM_NAME", True);
    }
    if(!UTF8_STRING) {
        UTF8_STRING = XInternAtom(Tk_Display(tkwin), "UTF8_STRING", True);
    }

    XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwrap), NET_WM_NAME,
        UTF8_STRING, 8, PropModeReplace, (unsigned char *)title, sz);
    XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), NET_WM_NAME,
        UTF8_STRING, 8, PropModeReplace, (unsigned char *)title, sz);

    PyMem_Free(title);

    Py_INCREF(Py_None);
    return Py_None;

OUT_NULL:
    if(title) PyMem_Free(title);
    return NULL;
}


PyObject *seticon(PyObject *s, PyObject *o) {
    Tcl_Interp *trp;
    Tk_Window tkwin, tkwrap;
    PyObject *win, *app, *path;
    char *icon;
    int sz;

    if(!PyArg_ParseTuple(o, "Os#", &win, &icon, &sz)) return NULL;

    app = PyObject_GetAttrString(win, "tk");
    if(!app) return NULL;
    path = PyObject_GetAttrString(win, "_w");
    if(!path) return NULL;
    if(!PyString_Check(path)) {
        PyErr_SetString(PyExc_ValueError, "Not a widget?");
    }

    trp = get_interpreter(app);    
    if(!PyString_Check(path)) {
        PyErr_SetString(PyExc_ValueError, "Not a widget?");
        return NULL;
    }

    tkwin = Tk_NameToWindow(trp, PyString_AsString(path), Tk_MainWindow(trp));
    Tk_MakeWindowExist(tkwin);
    tkwrap = TkpGetWrapperWindow(tkwin);
    if(!tkwrap) {
        PyErr_SetString(PyExc_ValueError, "No wrapper widget?");
        return NULL;
    }
    Tk_MakeWindowExist(tkwrap);

    if(!NET_WM_ICON) {
        NET_WM_ICON = XInternAtom(Tk_Display(tkwin), "_NET_WM_ICON", True);
    }
    if(!CARDINAL) {
        CARDINAL = XInternAtom(Tk_Display(tkwin), "CARDINAL", True);
    }

    if(NET_WM_ICON && CARDINAL) {
	XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwrap), NET_WM_ICON,
	    CARDINAL, 32, PropModeReplace, (unsigned char *)icon, sz/4);
	XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), NET_WM_ICON,
	    CARDINAL, 32, PropModeReplace, (unsigned char *)icon, sz/4);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef methods[] = {
    {"seticon", (PyCFunction)seticon, METH_VARARGS, "Set the NET_WM_ICON"},
    {"setname", (PyCFunction)setname, METH_VARARGS, "Set the NET_WM_NAME"},
    {NULL}
};

PyMODINIT_FUNC
init_tk_seticon(void) {
    Py_InitModule3("_tk_seticon", methods, "Set the NET_WM_ICON");
}

