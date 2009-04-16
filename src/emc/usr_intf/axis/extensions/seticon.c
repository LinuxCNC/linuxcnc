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

static Tcl_Interp *get_interpreter(PyObject *tkapp) {
    long interpaddr;
    Tcl_Interp *trp;
    PyObject *interpaddrobj = PyObject_CallMethod(tkapp, "interpaddr", NULL);
    if(interpaddrobj == NULL) { return NULL; }
    interpaddr = PyInt_AsLong(interpaddrobj);
    Py_DECREF(interpaddrobj);
    if(interpaddr == -1) { return NULL; }
    trp = (Tcl_Interp*)interpaddr;
    if (Tcl_InitStubs(trp, "8.1", 0) == NULL) 
    {
        PyErr_SetString(PyExc_RuntimeError, "Tcl_InitStubs returned NULL");
        return NULL;
    }
    if (Tk_InitStubs(trp, "8.1", 0) == NULL) 
    {
        PyErr_SetString(PyExc_RuntimeError, "Tk_InitStubs returned NULL");
        return NULL;
    }

    return trp;
}

static Atom NET_WM_ICON=0, NET_WM_NAME=0, CARDINAL=0, UTF8_STRING=0;
static Atom NET_WM_STATE, WM_TRANSIENT_FOR=0;

#define INTERN(v, a) intern(Tk_Display(tkwin), v, &a)

static Atom intern(Display *d, char *atom_name, Atom *atom_value) {
    if(!*atom_value)
        *atom_value = XInternAtom(d, atom_name, True);
    return *atom_value;
}

static int get_windows(PyObject *win, Tk_Window *tkwin, Tk_Window *tkwrap) {
    PyObject *app, *path;
    Tcl_Interp *trp;

    app = PyObject_GetAttrString(win, "tk");
    if(!app) return 0;

    path = PyObject_GetAttrString(win, "_w");
    if(!path) return 0;

    if(!PyString_Check(path)) {
        PyErr_SetString(PyExc_ValueError,
                "Not a widget? (PyString_Check(path) returned false)");
        return 0;
    }

    trp = get_interpreter(app);    
    if(!trp) {
        PyErr_SetString(PyExc_ValueError,
                "Not a widget? (get_interpreter() returned NULL)");
    }

    *tkwin = Tk_NameToWindow(trp, PyString_AsString(path), Tk_MainWindow(trp));
    Tk_MakeWindowExist(*tkwin);
    *tkwrap = TkpGetWrapperWindow(*tkwin);
    if(!*tkwrap) {
        PyErr_SetString(PyExc_ValueError, "No wrapper widget?");
        return 0;
    }
    Tk_MakeWindowExist(*tkwrap);
    return 1;
}

static PyObject *getwrapper(PyObject *s, PyObject *o) {
    Tk_Window tkwin, tkwrap;
    PyObject *win;

    if(!PyArg_ParseTuple(o, "O", &win))
        return NULL;

    if(!get_windows(win, &tkwin, &tkwrap)) return NULL;

    return PyInt_FromLong(Tk_WindowId(tkwrap));
}

static PyObject *setname(PyObject *s, PyObject *o) {
    Tk_Window tkwin, tkwrap;
    PyObject *win;
    char *title=0;
    int sz=0;

    if(!PyArg_ParseTuple(o, "Oes#", &win, "utf-8", &title, &sz))
        return NULL;

    if(!get_windows(win, &tkwin, &tkwrap)) goto OUT_NULL;

    printf("title[%d] %s\n", sz, title);
    INTERN("_NET_WM_NAME", NET_WM_NAME);
    INTERN("UTF8_STRING", UTF8_STRING);

    if(NET_WM_NAME && UTF8_STRING) {
        XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwrap), NET_WM_NAME,
            UTF8_STRING, 8, PropModeReplace, (unsigned char *)title, sz);
        XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), NET_WM_NAME,
            UTF8_STRING, 8, PropModeReplace, (unsigned char *)title, sz);
    }

    PyMem_Free(title);

    Py_INCREF(Py_None);
    return Py_None;

OUT_NULL:
    if(title) PyMem_Free(title);
    return NULL;
}

static PyObject *settransient(PyObject *s, PyObject *o) {
    Tk_Window tkwin, tkwrap;
    Display *d;
    PyObject *win;
    int master;

    if(!PyArg_ParseTuple(o, "Oi", &win, &master)) return NULL;
    if(!get_windows(win, &tkwin, &tkwrap)) return NULL;

    INTERN("WM_TRANSIENT_FOR", WM_TRANSIENT_FOR);

    if(WM_TRANSIENT_FOR) {
        d = Tk_Display(tkwin);
        if(master) {
            XSetTransientForHint(d, Tk_WindowId(tkwrap), master);
            XSetTransientForHint(d, Tk_WindowId(tkwin), master);
        } else {
            XDeleteProperty(d, Tk_WindowId(tkwrap), WM_TRANSIENT_FOR);
            XDeleteProperty(d, Tk_WindowId(tkwin), WM_TRANSIENT_FOR);
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *seticon(PyObject *s, PyObject *o) {
    Tk_Window tkwin, tkwrap;
    PyObject *win;
    char *icon;
    int sz;

    if(!PyArg_ParseTuple(o, "Os#", &win, &icon, &sz)) return NULL;
    if(!get_windows(win, &tkwin, &tkwrap)) return NULL;

    INTERN("_NET_WM_ICON", NET_WM_ICON);
    INTERN("CARDINAL", CARDINAL);

    if(NET_WM_ICON && CARDINAL) {
	XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwrap), NET_WM_ICON,
	    CARDINAL, 32, PropModeReplace, (unsigned char *)icon, sz/4);
	XChangeProperty(Tk_Display(tkwin), Tk_WindowId(tkwin), NET_WM_ICON,
	    CARDINAL, 32, PropModeReplace, (unsigned char *)icon, sz/4);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *setprop(PyObject *s, PyObject *o) {
    Tk_Window tkwin, tkwrap;
    PyObject *win;
    char *prop_suffix;
    char prop_string[32];
    Atom prop=0;
    int action;
    int source = 1;
    XEvent xev;

    if(!PyArg_ParseTuple(o, "Ois|i", &win, &action, &prop_suffix, &source))
        return NULL;
    if(!get_windows(win, &tkwin, &tkwrap)) return NULL;

    if(action < 0 || action > 2) {
        PyErr_SetString(PyExc_ValueError,
                "Action must be 0 (REMOVE), 1 (ADD), or 2 (TOGGLE)");
        return 0;
    }

    INTERN("_NET_WM_STATE", NET_WM_STATE);

    snprintf(prop_string, 32, "_NET_WM_STATE_%s", prop_suffix);
    INTERN(prop_string, prop);

    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.display = Tk_Display(tkwin);
    xev.xclient.message_type = NET_WM_STATE;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = action;
    xev.xclient.data.l[1] = prop;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = source;

    if(NET_WM_STATE && action && prop) {
        xev.xclient.window = Tk_WindowId(tkwrap);
        XSendEvent(Tk_Display(tkwin), DefaultRootWindow(Tk_Display(tkwin)),
                False, SubstructureRedirectMask | SubstructureNotifyMask,
                &xev);
        xev.xclient.window = Tk_WindowId(tkwin);
        XSendEvent(Tk_Display(tkwin), DefaultRootWindow(Tk_Display(tkwin)),
                False, SubstructureRedirectMask | SubstructureNotifyMask,
                &xev);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef methods[] = {
    {"getwrapper", (PyCFunction)getwrapper, METH_VARARGS,
        "Gets the ID of the Tk wrapper window"},
    {"seticon", (PyCFunction)seticon, METH_VARARGS, "Set the NET_WM_ICON"},
    {"setname", (PyCFunction)setname, METH_VARARGS, "Set the NET_WM_NAME"},
    {"setprop", (PyCFunction)setprop, METH_VARARGS, "Set the NET_WM_STATE"},
    {"settransient", (PyCFunction)settransient, METH_VARARGS,
        "Set the WM_TRANSIENT_FOR hint by window ID"},
    {NULL}
};

PyMODINIT_FUNC
init_tk_seticon(void) {
    Py_InitModule3("_tk_seticon", methods,
"Control various X facilities (such as NET_WM_ICON) not directly accessible\n"
"from Tk/Tkinter");
}

