//
// tkdar - Tk/Tkinter Detectable Auto Repeat for Python
// Copyright 2026 B.Stultiens
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

//
// Switch the X server's detectable auto repeat feature (if supported).
// Normal auto repeat sends a KeyRelease/KeyPress event sequence
// resulting in:
//     press - release, press - release, press - ... - release
//
// Detectable auto repeat modifies the event sequence into:
//     press - press - press - ... - release
//
// The first KeyPress event is the initial press of the button and the
// final KeyRelease event is the actual physical release of the button.
//
// This code was inspired by the example found at:
//     https://wiki.tcl-lang.org/page/Disable+autorepeat+under+X11
//
//
// Usage in Python/Tkinter:
/*
import tkinter
import tkdar	# exposes tkdar.enable() and tkdar.disable()

pressed_keys = []	# Current list if pressed keys

def keypress(event):
	if event.keysym in pressed_keys:
		return	# already pressed, ignore repeats
	pressed_keys.append(event.keysym)
	print("Press  ", event.keysym)

def keyrelease(event):
    # KeyRelease without KeyPress may happen when a modifier is active when
    # the key is pressed without a KeyPress handler. No KeyPress event is
    # generated, but releasing the actual key while still holding the modifier
    # generates a KeyRelease event that may be handled if there is a handler
    # installed. Therefore, we test the list to prevent an exception.
    if ev.keysym in pressed_keys:
        pressed_keys.remove(event.keysym)
        print("Release", event.keysym)

rootwin = tkinter.Tk(className="KeyRepeater")
rootwin.title = "Key-repeat tester"
rootwin.minsize(640, 400);

tkdar.enable(rootwin)	# Set detectable auto repeat

for key in ["Up", "Down", "Left", "Right"]:
	rootwin.bind("<KeyPress-{}>".format(key), keypress)
	rootwin.bind("<KeyRelease-{}>".format(key), keyrelease)

rootwin.mainloop()
*/

#include <Python.h>
#include <tk.h>
#include <X11/XKBlib.h>

static PyObject *tkdar(PyObject *arg, Bool enable)
{
	// Retrieve the Tcl interpreter instance
	PyObject *interpaddrobj = PyObject_CallMethod(arg, "interpaddr", NULL);
	if(!interpaddrobj) {
		PyErr_SetString(PyExc_TypeError, "get_interpreter: 'interpaddr' call returned NULL");
		return NULL;
	}
	Tcl_Interp *interp = (Tcl_Interp *)PyLong_AsVoidPtr(interpaddrobj);
	Py_DECREF(interpaddrobj);
	if(interp == (void*)-1) {
		PyErr_SetString(PyExc_TypeError, "get_interpreter: 'interpaddrobj' returned NULL");
		return NULL;
	}

	// Get the X server display via the main Tk window of the interpreter
	Tk_Window tkwin = Tk_MainWindow(interp);
	if(!tkwin) {
		PyErr_SetString(PyExc_RuntimeError, "Error while getting Tk_MainWindow");
		return NULL;
	}
	Display *display = Tk_Display(tkwin);
	if(!display) {
		PyErr_SetString(PyExc_RuntimeError, "Error while getting display connection to X server");
		return NULL;
	}

	// Set the intended detectable auto repeat
	Bool supported = 1;
	Bool result = XkbSetDetectableAutoRepeat(display, enable, &supported);
	XFlush(display);

	if(!supported) {
		PyErr_SetString(PyExc_NotImplementedError, "Setting detectable auto repeat not supported by X server");
		return NULL;
	}
	if(enable != result) {
		PyErr_SetString(PyExc_RuntimeError, "Could not set detectable auto repeat");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

// Python function tkdar.enable() handler
static PyObject *tkdar_ena(PyObject *s, PyObject *arg)
{
	(void)s;
	return tkdar(arg, 1);
}

// Python function tkdar.disable() handler
static PyObject *tkdar_dis(PyObject *s, PyObject *arg)
{
	(void)s;
	return tkdar(arg, 0);
}

static PyMethodDef tkdar_methods[] = {
	{"enable",  (PyCFunction)tkdar_ena, METH_O, "Enable detectable auto repeat"},
	{"disable", (PyCFunction)tkdar_dis, METH_O, "Disable detectable auto repeat"},
	{}
};

static struct PyModuleDef tkdar_moduledef = {
	.m_base		= PyModuleDef_HEAD_INIT,
	.m_name		= "tkdar",
	.m_doc		= "Detectable auto repeat extension for Tk/Tkinter",
	.m_size		= -1,
	.m_methods	= tkdar_methods,
};

PyMODINIT_FUNC PyInit_tkdar(void);
PyMODINIT_FUNC PyInit_tkdar(void)
{
	PyObject *m = PyModule_Create(&tkdar_moduledef);
	return m;
}
// vim: ts=4 shiftwidth=4
