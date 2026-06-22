/*
 * emcmodule.cc - Gutted linuxcnc Python module
 *
 * This module previously provided stat, command, error_channel, and
 * positionlogger types via NML/shared memory. Those are now provided
 * by the GMI web services (import gmi).
 *
 * What remains:
 * - linuxcnc.ini (trivial INI file parser wrapper)
 * - Integer constants (task modes, interp states, etc.)
 * - OpenGL draw helpers (draw_lines, draw_dwells, line9, vertex9)
 *
 * Instantiating stat(), command(), error_channel(), or positionlogger()
 * raises linuxcnc.error directing users to the gmi module.
 *
 * Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
 *                             Chris Radek <chris@timeguy.com>
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — GMI port
 * License: GPL Version 2
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include "config.h"
#include "inifile.hh"

#include <epoxy/gl.h>

/* Local constants kept for backward compatibility */
#define LOCAL_SPINDLE_FORWARD (1)
#define LOCAL_SPINDLE_REVERSE (-1)
#define LOCAL_SPINDLE_OFF (0)
#define LOCAL_SPINDLE_INCREASE (10)
#define LOCAL_SPINDLE_DECREASE (11)
#define LOCAL_SPINDLE_CONSTANT (12)

#define LOCAL_MIST_ON (1)
#define LOCAL_MIST_OFF (0)

#define LOCAL_FLOOD_ON (1)
#define LOCAL_FLOOD_OFF (0)

#define LOCAL_BRAKE_ENGAGE (1)
#define LOCAL_BRAKE_RELEASE (0)

#define LOCAL_JOG_STOP (0)
#define LOCAL_JOG_CONTINUOUS (1)
#define LOCAL_JOG_INCREMENT (2)

#define LOCAL_AUTO_RUN (0)
#define LOCAL_AUTO_PAUSE (1)
#define LOCAL_AUTO_RESUME (2)
#define LOCAL_AUTO_STEP (3)
#define LOCAL_AUTO_REVERSE (4)
#define LOCAL_AUTO_FORWARD (5)

/* EMC constants - previously from emc.hh/nml headers */
#define EMC_LINEAR 1
#define EMC_ANGULAR 2

#define EMC_TASK_INTERP_IDLE 1
#define EMC_TASK_INTERP_READING 2
#define EMC_TASK_INTERP_PAUSED 3
#define EMC_TASK_INTERP_WAITING 4

#define EMC_TASK_MODE_MDI 3
#define EMC_TASK_MODE_MANUAL 1
#define EMC_TASK_MODE_AUTO 2

#define EMC_TASK_STATE_OFF 1
#define EMC_TASK_STATE_ON 2
#define EMC_TASK_STATE_ESTOP 3
#define EMC_TASK_STATE_ESTOP_RESET 4

#define EMC_TRAJ_MODE_FREE 1
#define EMC_TRAJ_MODE_COORD 2
#define EMC_TRAJ_MODE_TELEOP 3

#define EMC_MOTION_TYPE_TRAVERSE 1
#define EMC_MOTION_TYPE_FEED 2
#define EMC_MOTION_TYPE_ARC 3
#define EMC_MOTION_TYPE_TOOLCHANGE 4
#define EMC_MOTION_TYPE_PROBING 5
#define EMC_MOTION_TYPE_INDEXROTARY 6

#define KINEMATICS_IDENTITY 1
#define KINEMATICS_FORWARD_ONLY 2
#define KINEMATICS_INVERSE_ONLY 3
#define KINEMATICS_BOTH 4

#define EMC_DEBUG_CONFIG          0x00000002
#define EMC_DEBUG_VERSIONS        0x00000008
#define EMC_DEBUG_TASK_ISSUE      0x00000010
#define EMC_DEBUG_NML             0x00000040
#define EMC_DEBUG_MOTION_TIME     0x00000080
#define EMC_DEBUG_INTERP          0x00000100
#define EMC_DEBUG_RCS             0x00000200
#define EMC_DEBUG_INTERP_LIST     0x00000800
#define EMC_DEBUG_OWORD           0x00002000
#define EMC_DEBUG_REMAP           0x00004000
#define EMC_DEBUG_PYTHON          0x00008000
#define EMC_DEBUG_STATE_TAGS      0x00080000

#define EMC_TASK_EXEC_ERROR 1
#define EMC_TASK_EXEC_DONE 2
#define EMC_TASK_EXEC_WAITING_FOR_MOTION 3
#define EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE 4
#define EMC_TASK_EXEC_WAITING_FOR_IO 5
#define EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO 6
#define EMC_TASK_EXEC_WAITING_FOR_DELAY 7
#define EMC_TASK_EXEC_WAITING_FOR_MCODE_HANDLER 8
#define EMC_TASK_EXEC_WAITING_FOR_SPINDLE_ORIENTED 9

#define EMCMOT_MAX_JOINTS 16
#define EMCMOT_MAX_AXIS 9

#define RCS_DONE 1
#define RCS_EXEC 2
#define RCS_ERROR 3

#define EMC_OPERATOR_ERROR_TYPE 11
#define EMC_OPERATOR_TEXT_TYPE 12
#define EMC_OPERATOR_DISPLAY_TYPE 13
#define NML_ERROR_TYPE 1
#define NML_TEXT_TYPE 2
#define NML_DISPLAY_TYPE 3

static PyObject *error = NULL;

/* ========== INI file type (kept functional) ========== */

struct pyIniFile {
    PyObject_HEAD
    IniFile *i;
};

static int Ini_init(pyIniFile *self, PyObject *a, PyObject *) {
    char *inifile;
    if(!PyArg_ParseTuple(a, "s", &inifile)) return -1;
    self->i = new IniFile();
    if (!self->i->Open(inifile)) {
        PyErr_Format(error, "inifile.open(%s) failed", inifile);
        return -1;
    }
    return 0;
}

static PyObject *Ini_find(pyIniFile *self, PyObject *args) {
    const char *tag, *section=NULL;
    int num = 1;
    if(!PyArg_ParseTuple(args, "ss|i", &section, &tag, &num)) return NULL;
    const char *result = self->i->Find(tag, section, num);
    if(!result) Py_RETURN_NONE;
    return PyUnicode_FromString(result);
}

static PyObject *Ini_findall(pyIniFile *self, PyObject *args) {
    const char *tag, *section=NULL;
    if(!PyArg_ParseTuple(args, "ss", &section, &tag)) return NULL;
    PyObject *result = PyList_New(0);
    for(int i=1; ; i++) {
        const char *val = self->i->Find(tag, section, i);
        if(!val) break;
        PyList_Append(result, PyUnicode_FromString(val));
    }
    return result;
}

static void Ini_dealloc(pyIniFile *self) {
    if(self->i) { delete self->i; self->i = NULL; }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMethodDef Ini_methods[] = {
    {"find", (PyCFunction)Ini_find, METH_VARARGS,
        "Find value in inifile as string."},
    {"findall", (PyCFunction)Ini_findall, METH_VARARGS,
        "Find all values for key in section as a list."},
    {NULL}
};

static PyTypeObject Ini_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "linuxcnc.ini",            /* tp_name */
    sizeof(pyIniFile),         /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Ini_dealloc,   /* tp_dealloc */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "INI file parser",         /* tp_doc */
    0,0,0,0,0,0,
    Ini_methods,               /* tp_methods */
    0,0,0,0,0,0,0,
    (initproc)Ini_init,        /* tp_init */
    0,
    PyType_GenericNew,         /* tp_new */
};

/* ========== Deprecated type stubs ========== */

static const char *DEPRECATED_MSG =
    "linuxcnc.%s is removed. Use the 'gmi' module instead "
    "(import gmi; see gmi documentation).";

static int deprecated_init(PyObject *, PyObject *, PyObject *) {
    PyErr_Format(error, DEPRECATED_MSG, "deprecated");
    return -1;
}

static PyObject *Stat_new(PyTypeObject *, PyObject *, PyObject *) {
    PyErr_Format(error, DEPRECATED_MSG, "stat");
    return NULL;
}

static PyTypeObject Stat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "linuxcnc.stat",           /* tp_name */
    sizeof(PyObject),          /* tp_basicsize */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    Py_TPFLAGS_DEFAULT,
    "Removed. Use gmi module.",
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    (initproc)deprecated_init,
    0,
    Stat_new,
};

static PyObject *Command_new(PyTypeObject *, PyObject *, PyObject *) {
    PyErr_Format(error, DEPRECATED_MSG, "command");
    return NULL;
}

static PyTypeObject Command_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "linuxcnc.command",        /* tp_name */
    sizeof(PyObject),          /* tp_basicsize */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    Py_TPFLAGS_DEFAULT,
    "Removed. Use gmi module.",
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    (initproc)deprecated_init,
    0,
    Command_new,
};

static PyObject *Error_new(PyTypeObject *, PyObject *, PyObject *) {
    PyErr_Format(error, DEPRECATED_MSG, "error_channel");
    return NULL;
}

static PyTypeObject Error_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "linuxcnc.error_channel",  /* tp_name */
    sizeof(PyObject),          /* tp_basicsize */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    Py_TPFLAGS_DEFAULT,
    "Removed. Use gmi module.",
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    (initproc)deprecated_init,
    0,
    Error_new,
};

static PyObject *PositionLogger_new(PyTypeObject *, PyObject *, PyObject *) {
    PyErr_Format(error, DEPRECATED_MSG, "positionlogger");
    return NULL;
}

static PyTypeObject PositionLoggerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "linuxcnc.positionlogger", /* tp_name */
    sizeof(PyObject),          /* tp_basicsize */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    Py_TPFLAGS_DEFAULT,
    "Removed. Use gmi.positionlogger.",
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    (initproc)deprecated_init,
    0,
    PositionLogger_new,
};

/* ========== OpenGL drawing helpers (kept for gremlin) ========== */

static PyObject *pydraw_lines(PyObject *, PyObject *args) {
    PyObject *o;
    int for_selection = 0;
    if(!PyArg_ParseTuple(args, "O|i", &o, &for_selection)) return NULL;

    PyObject *iterator = PyObject_GetIter(o);
    if(!iterator) return NULL;

    PyObject *item;
    while((item = PyIter_Next(iterator))) {
        PyObject *fast = PySequence_Fast(item, "Expected sequence");
        Py_DECREF(item);
        if(!fast) { Py_DECREF(iterator); return NULL; }

        Py_ssize_t len = PySequence_Fast_GET_SIZE(fast);
        if(len < 7) { Py_DECREF(fast); continue; }

        PyObject **items = PySequence_Fast_ITEMS(fast);
        int line_number = PyLong_AsLong(items[0]);
        float x1 = PyFloat_AsDouble(items[1]);
        float y1 = PyFloat_AsDouble(items[2]);
        float z1 = PyFloat_AsDouble(items[3]);
        float x2 = PyFloat_AsDouble(items[4]);
        float y2 = PyFloat_AsDouble(items[5]);
        float z2 = PyFloat_AsDouble(items[6]);

        if(for_selection) glLoadName(line_number);
        glVertex3f(x1, y1, z1);
        glVertex3f(x2, y2, z2);

        Py_DECREF(fast);
    }
    Py_DECREF(iterator);
    if(PyErr_Occurred()) return NULL;
    Py_RETURN_NONE;
}

static PyObject *pydraw_dwells(PyObject *, PyObject *args) {
    PyObject *o;
    int for_selection = 0;
    float alpha = 1.0;
    if(!PyArg_ParseTuple(args, "O|if", &o, &for_selection, &alpha)) return NULL;

    PyObject *iterator = PyObject_GetIter(o);
    if(!iterator) return NULL;

    PyObject *item;
    while((item = PyIter_Next(iterator))) {
        PyObject *fast = PySequence_Fast(item, "Expected sequence");
        Py_DECREF(item);
        if(!fast) { Py_DECREF(iterator); return NULL; }

        Py_ssize_t len = PySequence_Fast_GET_SIZE(fast);
        if(len < 7) { Py_DECREF(fast); continue; }

        PyObject **items = PySequence_Fast_ITEMS(fast);
        int line_number = PyLong_AsLong(items[0]);
        int color_idx = PyLong_AsLong(items[1]);
        float x = PyFloat_AsDouble(items[2]);
        float y = PyFloat_AsDouble(items[3]);
        float z = PyFloat_AsDouble(items[4]);
        (void)color_idx;

        if(for_selection) glLoadName(line_number);
        glVertex3f(x, y, z);

        Py_DECREF(fast);
    }
    Py_DECREF(iterator);
    if(PyErr_Occurred()) return NULL;
    Py_RETURN_NONE;
}

static PyObject *pyline9(PyObject *, PyObject *args) {
    double x1, y1, z1, a1, b1, c1, u1, v1, w1;
    double x2, y2, z2, a2, b2, c2, u2, v2, w2;
    if(!PyArg_ParseTuple(args, "(ddddddddd)(ddddddddd)",
            &x1, &y1, &z1, &a1, &b1, &c1, &u1, &v1, &w1,
            &x2, &y2, &z2, &a2, &b2, &c2, &u2, &v2, &w2)) return NULL;
    glVertex3d(x1, y1, z1);
    glVertex3d(x2, y2, z2);
    Py_RETURN_NONE;
}

static PyObject *pyvertex9(PyObject *, PyObject *args) {
    double x, y, z, a, b, c, u, v, w;
    if(!PyArg_ParseTuple(args, "ddddddddd",
            &x, &y, &z, &a, &b, &c, &u, &v, &w)) return NULL;
    return Py_BuildValue("(ddd)", x, y, z);
}

static PyObject *pygui_rot_offsets(PyObject *, PyObject *args) {
    /* Stub - rotation offsets for backplot, kept for API compat */
    Py_RETURN_NONE;
}

static PyObject *pygui_respect_offsets(PyObject *, PyObject *args) {
    /* Stub - kept for API compat */
    Py_RETURN_NONE;
}

/* ========== Module definition ========== */

static PyMethodDef emc_methods[] = {
    {"draw_lines", (PyCFunction)pydraw_lines, METH_VARARGS,
        "Draw lines in rs274.glcanon format"},
    {"draw_dwells", (PyCFunction)pydraw_dwells, METH_VARARGS,
        "Draw dwell positions in rs274.glcanon format"},
    {"line9", (PyCFunction)pyline9, METH_VARARGS,
        "Draw a single line from 9d points"},
    {"vertex9", (PyCFunction)pyvertex9, METH_VARARGS,
        "Get 3d location for a 9d point"},
    {"gui_rot_offsets", (PyCFunction)pygui_rot_offsets, METH_VARARGS,
        "Set rotation offsets (deprecated)"},
    {"gui_respect_offsets", (PyCFunction)pygui_respect_offsets, METH_VARARGS,
        "Enable rotation offsets (deprecated)"},
    {NULL}
};

static struct PyModuleDef linuxcnc_moduledef = {
    PyModuleDef_HEAD_INIT,
    "linuxcnc",
    "LinuxCNC interface module (legacy - use gmi for new code)",
    -1,
    emc_methods
};

PyMODINIT_FUNC PyInit_linuxcnc(void);
PyMODINIT_FUNC PyInit_linuxcnc(void)
{
    PyType_Ready(&Stat_Type);
    PyType_Ready(&Command_Type);
    PyType_Ready(&Error_Type);
    PyType_Ready(&Ini_Type);
    PyType_Ready(&PositionLoggerType);

    error = PyErr_NewException((char*)"linuxcnc.error", PyExc_RuntimeError, NULL);

    PyObject *m = PyModule_Create(&linuxcnc_moduledef);
    PyModule_AddObject(m, "stat", (PyObject*)&Stat_Type);
    PyModule_AddObject(m, "command", (PyObject*)&Command_Type);
    PyModule_AddObject(m, "error_channel", (PyObject*)&Error_Type);
    PyModule_AddObject(m, "ini", (PyObject*)&Ini_Type);
    PyModule_AddObject(m, "error", error);
    PyModule_AddObject(m, "positionlogger", (PyObject*)&PositionLoggerType);

    PyModule_AddStringConstant(m, "PREFIX", EMC2_HOME);
    PyModule_AddStringConstant(m, "SHARE", EMC2_HOME "/share");
    PyModule_AddStringConstant(m, "nmlfile", "");
    PyModule_AddStringConstant(m, "version", PACKAGE_VERSION);

#define ENUM(e) PyModule_AddIntConstant(m, #e, e)

    ENUM(EMC_LINEAR);
    ENUM(EMC_ANGULAR);

    PyModule_AddIntConstant(m, "TASK_INTERP_IDLE", EMC_TASK_INTERP_IDLE);
    PyModule_AddIntConstant(m, "TASK_INTERP_READING", EMC_TASK_INTERP_READING);
    PyModule_AddIntConstant(m, "TASK_INTERP_PAUSED", EMC_TASK_INTERP_PAUSED);
    PyModule_AddIntConstant(m, "TASK_INTERP_WAITING", EMC_TASK_INTERP_WAITING);

    PyModule_AddIntConstant(m, "TASK_MODE_MDI", EMC_TASK_MODE_MDI);
    PyModule_AddIntConstant(m, "TASK_MODE_MANUAL", EMC_TASK_MODE_MANUAL);
    PyModule_AddIntConstant(m, "TASK_MODE_AUTO", EMC_TASK_MODE_AUTO);

    PyModule_AddIntConstant(m, "TASK_STATE_OFF", EMC_TASK_STATE_OFF);
    PyModule_AddIntConstant(m, "TASK_STATE_ON", EMC_TASK_STATE_ON);
    PyModule_AddIntConstant(m, "TASK_STATE_ESTOP", EMC_TASK_STATE_ESTOP);
    PyModule_AddIntConstant(m, "TASK_STATE_ESTOP_RESET", EMC_TASK_STATE_ESTOP_RESET);

    ENUM(LOCAL_SPINDLE_FORWARD);
    ENUM(LOCAL_SPINDLE_REVERSE);
    ENUM(LOCAL_SPINDLE_OFF);
    ENUM(LOCAL_SPINDLE_INCREASE);
    ENUM(LOCAL_SPINDLE_DECREASE);
    ENUM(LOCAL_SPINDLE_CONSTANT);

    ENUM(LOCAL_MIST_ON);
    ENUM(LOCAL_MIST_OFF);
    ENUM(LOCAL_FLOOD_ON);
    ENUM(LOCAL_FLOOD_OFF);

    ENUM(LOCAL_BRAKE_ENGAGE);
    ENUM(LOCAL_BRAKE_RELEASE);

    ENUM(LOCAL_JOG_STOP);
    ENUM(LOCAL_JOG_CONTINUOUS);
    ENUM(LOCAL_JOG_INCREMENT);

    ENUM(LOCAL_AUTO_RUN);
    ENUM(LOCAL_AUTO_PAUSE);
    ENUM(LOCAL_AUTO_RESUME);
    ENUM(LOCAL_AUTO_STEP);
    ENUM(LOCAL_AUTO_REVERSE);
    ENUM(LOCAL_AUTO_FORWARD);

    PyModule_AddIntConstant(m, "TRAJ_MODE_FREE", EMC_TRAJ_MODE_FREE);
    PyModule_AddIntConstant(m, "TRAJ_MODE_COORD", EMC_TRAJ_MODE_COORD);
    PyModule_AddIntConstant(m, "TRAJ_MODE_TELEOP", EMC_TRAJ_MODE_TELEOP);

    PyModule_AddIntConstant(m, "MOTION_TYPE_TRAVERSE", EMC_MOTION_TYPE_TRAVERSE);
    PyModule_AddIntConstant(m, "MOTION_TYPE_FEED", EMC_MOTION_TYPE_FEED);
    PyModule_AddIntConstant(m, "MOTION_TYPE_ARC", EMC_MOTION_TYPE_ARC);
    PyModule_AddIntConstant(m, "MOTION_TYPE_TOOLCHANGE", EMC_MOTION_TYPE_TOOLCHANGE);
    PyModule_AddIntConstant(m, "MOTION_TYPE_PROBING", EMC_MOTION_TYPE_PROBING);
    PyModule_AddIntConstant(m, "MOTION_TYPE_INDEXROTARY", EMC_MOTION_TYPE_INDEXROTARY);

    ENUM(KINEMATICS_IDENTITY);
    ENUM(KINEMATICS_FORWARD_ONLY);
    ENUM(KINEMATICS_INVERSE_ONLY);
    ENUM(KINEMATICS_BOTH);

    PyModule_AddIntConstant(m, "DEBUG_CONFIG", EMC_DEBUG_CONFIG);
    PyModule_AddIntConstant(m, "DEBUG_VERSIONS", EMC_DEBUG_VERSIONS);
    PyModule_AddIntConstant(m, "DEBUG_TASK_ISSUE", EMC_DEBUG_TASK_ISSUE);
    PyModule_AddIntConstant(m, "DEBUG_NML", EMC_DEBUG_NML);
    PyModule_AddIntConstant(m, "DEBUG_MOTION_TIME", EMC_DEBUG_MOTION_TIME);
    PyModule_AddIntConstant(m, "DEBUG_INTERP", EMC_DEBUG_INTERP);
    PyModule_AddIntConstant(m, "DEBUG_RCS", EMC_DEBUG_RCS);
    PyModule_AddIntConstant(m, "DEBUG_INTERP_LIST", EMC_DEBUG_INTERP_LIST);
    PyModule_AddIntConstant(m, "DEBUG_OWORD", EMC_DEBUG_OWORD);
    PyModule_AddIntConstant(m, "DEBUG_REMAP", EMC_DEBUG_REMAP);
    PyModule_AddIntConstant(m, "DEBUG_PYTHON", EMC_DEBUG_PYTHON);
    PyModule_AddIntConstant(m, "DEBUG_STATE_TAGS", EMC_DEBUG_STATE_TAGS);

    PyModule_AddIntConstant(m, "TASK_EXEC_ERROR", EMC_TASK_EXEC_ERROR);
    PyModule_AddIntConstant(m, "TASK_EXEC_DONE", EMC_TASK_EXEC_DONE);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_MOTION", EMC_TASK_EXEC_WAITING_FOR_MOTION);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_MOTION_QUEUE", EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_IO", EMC_TASK_EXEC_WAITING_FOR_IO);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_MOTION_AND_IO", EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_DELAY", EMC_TASK_EXEC_WAITING_FOR_DELAY);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_MCODE_HANDLER", EMC_TASK_EXEC_WAITING_FOR_MCODE_HANDLER);
    PyModule_AddIntConstant(m, "TASK_EXEC_WAITING_FOR_SPINDLE_ORIENTED", EMC_TASK_EXEC_WAITING_FOR_SPINDLE_ORIENTED);

    PyModule_AddIntConstant(m, "MAX_JOINTS", EMCMOT_MAX_JOINTS);
    PyModule_AddIntConstant(m, "MAX_AXIS", EMCMOT_MAX_AXIS);

    ENUM(RCS_DONE);
    ENUM(RCS_EXEC);
    ENUM(RCS_ERROR);

    PyModule_AddIntConstant(m, "OPERATOR_ERROR", EMC_OPERATOR_ERROR_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_TEXT", EMC_OPERATOR_TEXT_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_DISPLAY", EMC_OPERATOR_DISPLAY_TYPE);
    PyModule_AddIntConstant(m, "NML_ERROR", NML_ERROR_TYPE);
    PyModule_AddIntConstant(m, "NML_TEXT", NML_TEXT_TYPE);
    PyModule_AddIntConstant(m, "NML_DISPLAY", NML_DISPLAY_TYPE);

#undef ENUM
    return m;
}
