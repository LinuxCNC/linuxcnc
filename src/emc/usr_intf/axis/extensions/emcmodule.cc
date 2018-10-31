//    This is a component of AXIS, a front-end for LinuxCNC
//    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
//    Chris Radek <chris@timeguy.com>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#define __STDC_FORMAT_MACROS
#include <Python.h>
#include <structseq.h>
#include <pthread.h>
#include <structmember.h>
#include <inttypes.h>
#include "config.h"
#include "rcs.hh"
#include "emc.hh"
#include "emc_nml.hh"
#include "kinematics.h"
#include "config.h"
#include "inifile.hh"
#include "timer.hh"
#include "nml_oi.hh"
#include "rcs_print.hh"

#include <cmath>

#ifndef T_BOOL
// The C++ standard probably doesn't specify the amount of storage for a 'bool',
// and on some systems it might be more than one byte.  However, on x86 and
// x86-64, sizeof(bool) == 1.  When a counterexample is found, this must be
// replaced with the result of a configure test.
#define T_BOOL T_UBYTE
#endif

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

/* This definition of offsetof avoids the g++ warning
 * 'invalid offsetof from non-POD type'.
 */
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

struct pyIniFile {
    PyObject_HEAD
    IniFile *i;
};

struct pyStatChannel {
    PyObject_HEAD
    RCS_STAT_CHANNEL *c;
    EMC_STAT status;
};

struct pyCommandChannel {
    PyObject_HEAD
    RCS_CMD_CHANNEL *c;
    RCS_STAT_CHANNEL *s;
    int serial;
};

struct pyErrorChannel {
    PyObject_HEAD
    NML *c;
};

static PyObject *m = NULL, *error = NULL;

static int Ini_init(pyIniFile *self, PyObject *a, PyObject *k) {
    char *inifile;
    if(!PyArg_ParseTuple(a, "s", &inifile)) return -1;

    if(!self->i)
        self->i = new IniFile();

    if (!self->i->Open(inifile)) {
        PyErr_Format( error, "inifile.open() failed");
        return -1;
    }
    return 0;
}

static PyObject *Ini_find(pyIniFile *self, PyObject *args) {
    const char *s1, *s2, *out;
    int num = 1;
    if(!PyArg_ParseTuple(args, "ss|i:find", &s1, &s2, &num)) return NULL;

    out = self->i->Find(s2, s1, num);
    if(out == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyString_FromString(const_cast<char*>(out));
}

static PyObject *Ini_findall(pyIniFile *self, PyObject *args) {
    const char *s1, *s2, *out;
    int num = 1;
    if(!PyArg_ParseTuple(args, "ss:findall", &s1, &s2)) return NULL;

    PyObject *result = PyList_New(0);
    while(1) {
        out = self->i->Find(s2, s1, num);
        if(out == NULL) {
            break;
        }
        PyList_Append(result, PyString_FromString(const_cast<char*>(out)));
        num++;
    }
    return result;
}

static void Ini_dealloc(pyIniFile *self) {
    if (self->i)
        self->i->Close();
    delete self->i;
    PyObject_Del(self);
}

static PyMethodDef Ini_methods[] = {
    {"find", (PyCFunction)Ini_find, METH_VARARGS,
        "Find value in inifile as string.  This uses the ConfigParser-style "
        "(section,option) order, not the linuxcnc order."},
    {"findall", (PyCFunction)Ini_findall, METH_VARARGS,
        "Find value in inifile as a list.  This uses the ConfigParser-style "
        "(section,option) order, not the linuxcnc order."},
    {NULL}
};

static PyTypeObject Ini_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "linuxcnc.ini",              /*tp_name*/
    sizeof(pyIniFile),      /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Ini_dealloc,/*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Ini_methods,            /*tp_methods*/
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    (initproc)Ini_init,     /*tp_init*/
    0,                      /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

#define EMC_COMMAND_TIMEOUT 5.0  // how long to wait until timeout
#define EMC_COMMAND_DELAY   0.01 // how long to sleep between checks

static int emcWaitCommandComplete(pyCommandChannel *s, double timeout) {
    double start = etime();

    do {
        double now = etime();
        if(s->s->peek() == EMC_STAT_TYPE) {
           EMC_STAT *stat = (EMC_STAT*)s->s->get_address();
           int serial_diff = stat->echo_serial_number - s->serial;
           if (serial_diff > 0) {
                return RCS_DONE;
           }
           if (serial_diff == 0 &&
               ( stat->status == RCS_DONE || stat->status == RCS_ERROR )) {
                return stat->status;
           }
        }
        esleep(fmin(timeout - (now - start), EMC_COMMAND_DELAY));
    } while (etime() - start < timeout);
    return -1;
}

static int emcSendCommand(pyCommandChannel *s, RCS_CMD_MSG & cmd) {
    if (s->c->write(&cmd)) {
        return -1;
    }
    s->serial = cmd.serial_number;

    double start = etime();
    while (etime() - start < EMC_COMMAND_TIMEOUT) {
        EMC_STAT *stat = (EMC_STAT*)s->s->get_address();
        int serial_diff = stat->echo_serial_number - s->serial;
        if(s->s->peek() == EMC_STAT_TYPE &&
           serial_diff >= 0) {
                return 0;
           }
        esleep(EMC_COMMAND_DELAY);
    }
    return -1;
}

static char *get_nmlfile(void) {
    PyObject *fileobj = PyObject_GetAttrString(m, "nmlfile");
    if(fileobj == NULL) return NULL;
    return PyString_AsString(fileobj);
}

static int Stat_init(pyStatChannel *self, PyObject *a, PyObject *k) {
    char *file = get_nmlfile();
    if(file == NULL) return -1;

    RCS_STAT_CHANNEL *c =
        new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", file);
    if(!c) {
        PyErr_Format( error, "new RCS_STAT_CHANNEL failed");
        return -1;
    }

    self->c = c;
    return 0;
}

static void Stat_dealloc(PyObject *self) {
    delete ((pyStatChannel*)self)->c;
    PyObject_Del(self);
}

static bool check_stat(RCS_STAT_CHANNEL *emcStatusBuffer) {
    if(!emcStatusBuffer->valid()) {
        PyErr_Format( error, "emcStatusBuffer invalid err=%d", emcStatusBuffer->error_type);
        return false;
    }
    return true;
}

static PyObject *poll(pyStatChannel *s, PyObject *o) {
    if(!check_stat(s->c)) return NULL;
    if(s->c->peek() == EMC_STAT_TYPE) {
        EMC_STAT *emcStatus = static_cast<EMC_STAT*>(s->c->get_address());
        memcpy(&s->status, emcStatus, sizeof(EMC_STAT));
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef Stat_methods[] = {
    {"poll", (PyCFunction)poll, METH_NOARGS, "Update current machine state"},
    {NULL}
};

#define O(x) offsetof(pyStatChannel,status.x)
static PyMemberDef Stat_members[] = {
// stat
    {(char*)"echo_serial_number", T_INT, O(echo_serial_number), READONLY},
    {(char*)"echo_serial_number", T_INT, O(echo_serial_number), READONLY},
    {(char*)"state", T_INT, O(status), READONLY},

// task
    {(char*)"task_mode", T_INT, O(task.mode), READONLY},
    {(char*)"task_state", T_INT, O(task.state), READONLY},
    {(char*)"exec_state", T_INT, O(task.execState), READONLY},
    {(char*)"interp_state", T_INT, O(task.interpState), READONLY},
    {(char*)"call_level", T_INT, O(task.callLevel), READONLY},
    {(char*)"read_line", T_INT, O(task.readLine), READONLY},
    {(char*)"motion_line", T_INT, O(task.motionLine), READONLY},
    {(char*)"current_line", T_INT, O(task.currentLine), READONLY},
    {(char*)"file", T_STRING_INPLACE, O(task.file), READONLY},
    {(char*)"command", T_STRING_INPLACE, O(task.command), READONLY},
    {(char*)"program_units", T_INT, O(task.programUnits), READONLY},
    {(char*)"interpreter_errcode", T_INT, O(task.interpreter_errcode), READONLY},
    {(char*)"optional_stop", T_BOOL, O(task.optional_stop_state), READONLY},
    {(char*)"block_delete", T_BOOL, O(task.block_delete_state), READONLY},
    {(char*)"task_paused", T_INT, O(task.task_paused), READONLY},
    {(char*)"input_timeout", T_BOOL, O(task.input_timeout), READONLY},
    {(char*)"rotation_xy", T_DOUBLE, O(task.rotation_xy), READONLY},
    {(char*)"delay_left", T_DOUBLE, O(task.delayLeft), READONLY},
    {(char*)"queued_mdi_commands", T_INT, O(task.queuedMDIcommands), READONLY, (char*)"Number of MDI commands queued waiting to run." },

// motion
//   EMC_TRAJ_STAT traj
    {(char*)"linear_units", T_DOUBLE, O(motion.traj.linearUnits), READONLY},
    {(char*)"angular_units", T_DOUBLE, O(motion.traj.angularUnits), READONLY},
    {(char*)"cycle_time", T_DOUBLE, O(motion.traj.cycleTime), READONLY},
    {(char*)"joints", T_INT, O(motion.traj.joints), READONLY},
    {(char*)"spindles", T_INT, O(motion.traj.spindles), READONLY},
    {(char*)"axis_mask", T_INT, O(motion.traj.axis_mask), READONLY},
    {(char*)"motion_mode", T_INT, O(motion.traj.mode), READONLY, (char*)"The current mode of the Motion controller.  One of TRAJ_MODE_FREE,\n"
        "TRAJ_MODE_COORD, or TRAJ_MODE_TELEOP." },
    {(char*)"enabled", T_BOOL, O(motion.traj.enabled), READONLY},
    {(char*)"inpos", T_BOOL, O(motion.traj.inpos), READONLY},
    {(char*)"queue", T_INT, O(motion.traj.queue), READONLY},
    {(char*)"active_queue", T_INT, O(motion.traj.activeQueue), READONLY},
    {(char*)"queue_full", T_BOOL, O(motion.traj.queueFull), READONLY},
    {(char*)"id", T_INT, O(motion.traj.id), READONLY},
    {(char*)"paused", T_BOOL, O(motion.traj.paused), READONLY},
    {(char*)"feedrate", T_DOUBLE, O(motion.traj.scale), READONLY},
    {(char*)"rapidrate", T_DOUBLE, O(motion.traj.rapid_scale), READONLY},
    {(char*)"velocity", T_DOUBLE, O(motion.traj.velocity), READONLY},
    {(char*)"acceleration", T_DOUBLE, O(motion.traj.acceleration), READONLY},
    {(char*)"max_velocity", T_DOUBLE, O(motion.traj.maxVelocity), READONLY},
    {(char*)"max_acceleration", T_DOUBLE, O(motion.traj.maxAcceleration), READONLY},
    {(char*)"probe_tripped", T_BOOL, O(motion.traj.probe_tripped), READONLY},
    {(char*)"probing", T_BOOL, O(motion.traj.probing), READONLY},
    {(char*)"probe_val", T_INT, O(motion.traj.probeval), READONLY},
    {(char*)"kinematics_type", T_INT, O(motion.traj.kinematics_type), READONLY},
    {(char*)"motion_type", T_INT, O(motion.traj.motion_type), READONLY, (char*)"The type of the currently executing motion (one of MOTION_TYPE_TRAVERSE,\n"
        "MOTION_TYPE_FEED, MOTION_TYPE_ARC, MOTION_TYPE_TOOLCHANGE,\n"
        "MOTION_TYPE_PROBING, or MOTION_TYPE_INDEXROTARY), or 0 if no motion is\n"
        "currently taking place."},
    {(char*)"distance_to_go", T_DOUBLE, O(motion.traj.distance_to_go), READONLY},
    {(char*)"current_vel", T_DOUBLE, O(motion.traj.current_vel), READONLY},
    {(char*)"feed_override_enabled", T_BOOL, O(motion.traj.feed_override_enabled), READONLY},
    {(char*)"adaptive_feed_enabled", T_BOOL, O(motion.traj.adaptive_feed_enabled), READONLY},
    {(char*)"feed_hold_enabled", T_BOOL, O(motion.traj.feed_hold_enabled), READONLY},


// EMC_SPINDLE_STAT motion.spindle
    // MOVED TO THE "spindle" TUPLE OF DICTS

// io
// EMC_TOOL_STAT io.tool
    {(char*)"pocket_prepped", T_INT, O(io.tool.pocketPrepped), READONLY,
        (char*)"The index into the stat.tool_table list of the tool currently prepped for\n"
        "tool change, or -1 no tool is prepped.  On a Random toolchanger this is the\n"
        "same as the tool's pocket number.  On a Non-random toolchanger it's a random\n"
        "small integer."
    },
    {(char*)"tool_in_spindle", T_INT, O(io.tool.toolInSpindle), READONLY,
        (char*)"The tool number of the currently loaded tool, or 0 if no tool is loaded."
    },

// EMC_COOLANT_STAT io.cooland
    {(char*)"mist", T_INT, O(io.coolant.mist), READONLY},
    {(char*)"flood", T_INT, O(io.coolant.flood), READONLY},

// EMC_AUX_STAT     io.aux
    {(char*)"estop", T_INT, O(io.aux.estop), READONLY},

// EMC_LUBE_STAT    io.lube
    {(char*)"lube", T_INT, O(io.lube.on), READONLY},
    {(char*)"lube_level", T_INT, O(io.lube.level), READONLY},

    {(char*)"debug", T_INT, O(debug), READONLY},
    {NULL}
};

static PyObject *int_array(int *arr, int sz) {
    PyObject *res = PyTuple_New(sz);
    for(int i = 0; i < sz; i++) {
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(arr[i]));
    }
    return res;
}

static PyObject *double_array(double *arr, int sz) {
    PyObject *res = PyTuple_New(sz);
    for(int i = 0; i < sz; i++) {
        PyTuple_SET_ITEM(res, i, PyFloat_FromDouble(arr[i]));
    }
    return res;
}

static PyObject *pose(const EmcPose &p) {
    PyObject *res = PyTuple_New(9);
    PyTuple_SET_ITEM(res, 0, PyFloat_FromDouble(p.tran.x));
    PyTuple_SET_ITEM(res, 1, PyFloat_FromDouble(p.tran.y));
    PyTuple_SET_ITEM(res, 2, PyFloat_FromDouble(p.tran.z));
    PyTuple_SET_ITEM(res, 3, PyFloat_FromDouble(p.a));
    PyTuple_SET_ITEM(res, 4, PyFloat_FromDouble(p.b));
    PyTuple_SET_ITEM(res, 5, PyFloat_FromDouble(p.c));
    PyTuple_SET_ITEM(res, 6, PyFloat_FromDouble(p.u));
    PyTuple_SET_ITEM(res, 7, PyFloat_FromDouble(p.v));
    PyTuple_SET_ITEM(res, 8, PyFloat_FromDouble(p.w));
    return res;
}

static PyObject *Stat_g5x_index(pyStatChannel *s) {
    return PyInt_FromLong(s->status.task.g5x_index);
}

static PyObject *Stat_g5x_offset(pyStatChannel *s) {
    return pose(s->status.task.g5x_offset);
}

static PyObject *Stat_g92_offset(pyStatChannel *s) {
    return pose(s->status.task.g92_offset);
}

static PyObject *Stat_tool_offset(pyStatChannel *s) {
    return pose(s->status.task.toolOffset);
}

static PyObject *Stat_position(pyStatChannel *s) {
    return pose(s->status.motion.traj.position);
}

static PyObject *Stat_dtg(pyStatChannel *s) {
    return pose(s->status.motion.traj.dtg);
}

static PyObject *Stat_actual(pyStatChannel *s) {
    return pose(s->status.motion.traj.actualPosition);
}

static PyObject *Stat_joint_position(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_JOINTS);
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
        PyTuple_SetItem(res, i,
                PyFloat_FromDouble(s->status.motion.joint[i].output));
    }
    return res;
}

static PyObject *Stat_joint_actual(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_JOINTS);
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
        PyTuple_SetItem(res, i,
                PyFloat_FromDouble(s->status.motion.joint[i].input));
    }
    return res;
}

static PyObject *Stat_probed(pyStatChannel *s) {
    return pose(s->status.motion.traj.probedPosition);
}

static PyObject *Stat_activegcodes(pyStatChannel *s) {
    return int_array(s->status.task.activeGCodes, ACTIVE_G_CODES);
}

static PyObject *Stat_activemcodes(pyStatChannel *s) {
    return int_array(s->status.task.activeMCodes, ACTIVE_M_CODES);
}

static PyObject *Stat_activesettings(pyStatChannel *s) {
   return double_array(s->status.task.activeSettings, ACTIVE_SETTINGS);
}

static PyObject *Stat_din(pyStatChannel *s) {
    return int_array(s->status.motion.synch_di, EMCMOT_MAX_AIO);
}

static PyObject *Stat_dout(pyStatChannel *s) {
    return int_array(s->status.motion.synch_do, EMCMOT_MAX_AIO);
}

static PyObject *Stat_limit(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_JOINTS);
    for(int i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        int v = 0;
        if(s->status.motion.joint[i].minHardLimit) v |= 1;
        if(s->status.motion.joint[i].maxHardLimit) v |= 2;
        if(s->status.motion.joint[i].minSoftLimit) v |= 4;
        if(s->status.motion.joint[i].maxSoftLimit) v |= 8;
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(v));
    }
    return res;
}

static PyObject *Stat_homed(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_JOINTS);
    for(int i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(s->status.motion.joint[i].homed));
    }
    return res;
}

static PyObject *Stat_ain(pyStatChannel *s) {
    return double_array(s->status.motion.analog_input, EMCMOT_MAX_AIO);
}

static PyObject *Stat_aout(pyStatChannel *s) {
    return double_array(s->status.motion.analog_output, EMCMOT_MAX_AIO);
}

static void dict_add(PyObject *d, const char *name, unsigned char v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyInt_FromLong(v));
    Py_XDECREF(o);
}
static void dict_add(PyObject *d, const char *name, double v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyFloat_FromDouble(v));
    Py_XDECREF(o);
}
static void dict_add(PyObject *d, const char *name, bool v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyBool_FromLong((long)v));
    Py_XDECREF(o);
}
static void dict_add(PyObject *d, const char *name, int v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyLong_FromLong((long)v));
    Py_XDECREF(o);
}
#define F(x) F2(#x, x)
#define F2(y,x) dict_add(res, y, s->status.motion.joint[jointno].x)
static PyObject *Stat_joint_one(pyStatChannel *s, int jointno) {
    PyObject *res = PyDict_New();
    F(jointType);
    F(units);
    F(backlash);
    F2("min_position_limit", minPositionLimit);
    F2("max_position_limit", maxPositionLimit);
    F2("max_ferror", maxFerror);
    F2("min_ferror", minFerror);
    F2("ferror_current", ferrorCurrent);
    F2("ferror_highmark", ferrorHighMark);
    F(output);
    F(input);
    F(velocity);
    F(inpos);
    F(homing);
    F(homed);
    F(fault);
    F(enabled);
    F2("min_soft_limit", minSoftLimit);
    F2("max_soft_limit", maxSoftLimit);
    F2("min_hard_limit", minHardLimit);
    F2("max_hard_limit", maxHardLimit);
    F2("override_limits", overrideLimits);
    return res;
}
#undef F
#undef F2

static PyObject *Stat_joint(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_JOINTS);
    for(int i=0; i<EMCMOT_MAX_JOINTS; i++) {
        PyTuple_SetItem(res, i, Stat_joint_one(s, i));
    }
    return res;
}

#define F(x) F2(#x, x)
#define F2(y,x) dict_add(res, y, s->status.motion.axis[axisno].x)
static PyObject *Stat_axis_one(pyStatChannel *s, int axisno) {
    PyObject *res = PyDict_New();
    F(velocity);
    F2("min_position_limit", minPositionLimit);
    F2("max_position_limit", maxPositionLimit);
    return res;
}

#undef F
#undef F2

static PyObject *Stat_axis(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_AXIS);
    for(int i=0; i<EMCMOT_MAX_AXIS; i++) {
        PyTuple_SetItem(res, i, Stat_axis_one(s, i));
    }
    return res;
}

#define F(x) F2(#x, x)
#define F2(y,x) dict_add(res, y, s->status.motion.spindle[spindleno].x)
static PyObject *Stat_spindle_one(pyStatChannel *s, int spindleno) {
    PyObject *res = PyDict_New();
    F(brake);
    F(direction);
    F(enabled);
    F2("override_enabled", spindle_override_enabled);
    F(speed);
    F2("override", spindle_scale);
    F(homed);
    F(orient_state);
    F(orient_fault);
    return res;
}
#undef F
#undef F2

static PyObject *Stat_spindle(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMCMOT_MAX_SPINDLES);
    for(int i=0; i<EMCMOT_MAX_SPINDLES; i++) {
        PyTuple_SetItem(res, i, Stat_spindle_one(s, i));
    }
    return res;
}

static PyStructSequence_Field tool_fields[] = {
    {(char*)"id", },
    {(char*)"xoffset", },
    {(char*)"yoffset", },
    {(char*)"zoffset", },
    {(char*)"aoffset", },
    {(char*)"boffset", },
    {(char*)"coffset", },
    {(char*)"uoffset", },
    {(char*)"voffset", },
    {(char*)"woffset", },
    {(char*)"diameter", },
    {(char*)"frontangle", },
    {(char*)"backangle", },
    {(char*)"orientation", },
    {0,},
};

static PyStructSequence_Desc tool_result_desc = {
    (char*)"tool_result", /* name */
    (char*)"", /* doc */
    tool_fields,
    14
};

static PyTypeObject ToolResultType;

static PyObject *Stat_tool_table(pyStatChannel *s) {
    PyObject *res = PyTuple_New(CANON_POCKETS_MAX);
    int j=0;
    for(int i=0; i<CANON_POCKETS_MAX; i++) {
        struct CANON_TOOL_TABLE &t = s->status.io.tool.toolTable[i];
        PyObject *tool = PyStructSequence_New(&ToolResultType);
        PyStructSequence_SET_ITEM(tool, 0, PyInt_FromLong(t.toolno));
        PyStructSequence_SET_ITEM(tool, 1, PyFloat_FromDouble(t.offset.tran.x));
        PyStructSequence_SET_ITEM(tool, 2, PyFloat_FromDouble(t.offset.tran.y));
        PyStructSequence_SET_ITEM(tool, 3, PyFloat_FromDouble(t.offset.tran.z));
        PyStructSequence_SET_ITEM(tool, 4, PyFloat_FromDouble(t.offset.a));
        PyStructSequence_SET_ITEM(tool, 5, PyFloat_FromDouble(t.offset.b));
        PyStructSequence_SET_ITEM(tool, 6, PyFloat_FromDouble(t.offset.c));
        PyStructSequence_SET_ITEM(tool, 7, PyFloat_FromDouble(t.offset.u));
        PyStructSequence_SET_ITEM(tool, 8, PyFloat_FromDouble(t.offset.v));
        PyStructSequence_SET_ITEM(tool, 9, PyFloat_FromDouble(t.offset.w));
        PyStructSequence_SET_ITEM(tool, 10, PyFloat_FromDouble(t.diameter));
        PyStructSequence_SET_ITEM(tool, 11, PyFloat_FromDouble(t.frontangle));
        PyStructSequence_SET_ITEM(tool, 12, PyFloat_FromDouble(t.backangle));
        PyStructSequence_SET_ITEM(tool, 13, PyInt_FromLong(t.orientation));
        PyTuple_SetItem(res, j, tool);
        j++;
    }
    _PyTuple_Resize(&res, j);
    return res;
}

static PyObject *Stat_axes(pyStatChannel *s) {
    PyErr_WarnEx(PyExc_DeprecationWarning, "stat.axes is deprecated and will be removed in the future", 0);
    return PyInt_FromLong(s->status.motion.traj.deprecated_axes);
}

// XXX io.tool.toolTable
// XXX EMC_JOINT_STAT motion.joint[]

static PyGetSetDef Stat_getsetlist[] = {
    {(char*)"actual_position", (getter)Stat_actual},
    {(char*)"ain", (getter)Stat_ain},
    {(char*)"aout", (getter)Stat_aout},
    {(char*)"joint", (getter)Stat_joint},
    {(char*)"axis", (getter)Stat_axis},
    {(char*)"spindle", (getter)Stat_spindle},
    {(char*)"din", (getter)Stat_din},
    {(char*)"dout", (getter)Stat_dout},
    {(char*)"gcodes", (getter)Stat_activegcodes},
    {(char*)"homed", (getter)Stat_homed},
    {(char*)"limit", (getter)Stat_limit},
    {(char*)"mcodes", (getter)Stat_activemcodes},
    {(char*)"g5x_offset", (getter)Stat_g5x_offset},
    {(char*)"g5x_index", (getter)Stat_g5x_index},
    {(char*)"g92_offset", (getter)Stat_g92_offset},
    {(char*)"position", (getter)Stat_position},
    {(char*)"dtg", (getter)Stat_dtg},
    {(char*)"joint_position", (getter)Stat_joint_position},
    {(char*)"joint_actual_position", (getter)Stat_joint_actual},
    {(char*)"probed_position", (getter)Stat_probed},
    {(char*)"settings", (getter)Stat_activesettings, (setter)NULL,
        (char*)"This is an array containing the Interp active settings: sequence number,\n"
        "feed rate, and spindle speed."
    },
    {(char*)"tool_offset", (getter)Stat_tool_offset},
    {(char*)"tool_table", (getter)Stat_tool_table, (setter)NULL,
        (char*)"The tooltable, expressed as a list of tools.  Each tool is a dict with the\n"
        "tool id (tool number), diameter, offsets, etc."
    },
    {(char*)"axes", (getter)Stat_axes},
    {NULL}
};

static PyTypeObject Stat_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "linuxcnc.stat",             /*tp_name*/
    sizeof(pyStatChannel),  /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Stat_dealloc, /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Stat_methods,           /*tp_methods*/
    Stat_members,           /*tp_members*/
    Stat_getsetlist,        /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    (initproc)Stat_init,    /*tp_init*/
    0,                      /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static int Command_init(pyCommandChannel *self, PyObject *a, PyObject *k) {
    char *file = get_nmlfile();
    if(file == NULL) return -1;

    RCS_CMD_CHANNEL *c =
        new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", file);
    if(!c) {
        PyErr_Format( error, "new RCS_CMD_CHANNEL failed");
        return -1;
    }
    RCS_STAT_CHANNEL *s =
        new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", file);
    if(!c) {
	delete s;
        PyErr_Format( error, "new RCS_STAT_CHANNEL failed");
        return -1;
    }

    self->s = s;
    self->c = c;
    return 0;
}

static void Command_dealloc(PyObject *self) {
    delete ((pyCommandChannel*)self)->c;
    PyObject_Del(self);
}

static PyObject *block_delete(pyCommandChannel *s, PyObject *o) {
    int t;
    EMC_TASK_PLAN_SET_BLOCK_DELETE m;

    if(!PyArg_ParseTuple(o, "i", &t)) return NULL;
    m.state = t;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *optional_stop(pyCommandChannel *s, PyObject *o) {
    int t;
    EMC_TASK_PLAN_SET_OPTIONAL_STOP m;

    if(!PyArg_ParseTuple(o, "i", &t)) return NULL;
    m.state = t;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *mode(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_SET_MODE m;
    if(!PyArg_ParseTuple(o, "i", &m.mode)) return NULL;
    switch(m.mode) {
        case EMC_TASK_MODE_MDI:
        case EMC_TASK_MODE_MANUAL:
        case EMC_TASK_MODE_AUTO:
            break;
        default:
            PyErr_Format(PyExc_ValueError,"Mode should be MODE_MDI, MODE_MANUAL, or MODE_AUTO");
            return NULL;
    }
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *task_plan_synch(pyCommandChannel *s) {
    EMC_TASK_PLAN_SYNCH synch;
    emcSendCommand(s, synch);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *maxvel(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_MAX_VELOCITY m;
    if(!PyArg_ParseTuple(o, "d", &m.velocity)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *feedrate(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_SCALE m;
    if(!PyArg_ParseTuple(o, "d", &m.scale)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *rapidrate(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_RAPID_SCALE m;
    if(!PyArg_ParseTuple(o, "d", &m.scale)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *spindleoverride(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_SPINDLE_SCALE m;
    m.spindle = 0;
    if(!PyArg_ParseTuple(o, "d|i", &m.scale, &m.spindle)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *spindle(pyCommandChannel *s, PyObject *o) {
    int dir;
    double arg1 = 0,arg2 = 0;
    if(!PyArg_ParseTuple(o, "i|dd", &dir, &arg1, &arg2)) return NULL;
    switch(dir) {
        case LOCAL_SPINDLE_FORWARD:
        case LOCAL_SPINDLE_REVERSE:
        {
            EMC_SPINDLE_ON m;
            m.speed = dir * arg1;
            m.spindle = (int)arg2;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_SPINDLE_INCREASE:
        {
            EMC_SPINDLE_INCREASE m;
            m.spindle = (int)arg1;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_SPINDLE_DECREASE:
        {
            EMC_SPINDLE_DECREASE m;
            m.spindle = (int)arg1;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_SPINDLE_CONSTANT:
        {
            EMC_SPINDLE_CONSTANT m;
            m.spindle = (int)arg1;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_SPINDLE_OFF:
        {
            EMC_SPINDLE_OFF m;
            m.spindle = (int)arg1;
            emcSendCommand(s, m);
        }
            break;
        default:
            PyErr_Format(PyExc_ValueError,"Spindle direction should be SPINDLE_FORWARD, SPINDLE_REVERSE, SPINDLE_OFF, SPINDLE_INCREASE, SPINDLE_DECREASE, or SPINDLE_CONSTANT");
            return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *mdi(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_EXECUTE m;
    char *cmd;
    int len;
    if(!PyArg_ParseTuple(o, "s#", &cmd, &len)) return NULL;
    if(unsigned(len) > sizeof(m.command) - 1) {
        PyErr_Format(PyExc_ValueError, "MDI commands limited to %zu characters", sizeof(m.command) - 1);
        return NULL;
    }
    strcpy(m.command, cmd);
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *state(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_SET_STATE m;
    if(!PyArg_ParseTuple(o, "i", &m.state)) return NULL;
    switch(m.state){
        case EMC_TASK_STATE_ESTOP:
        case EMC_TASK_STATE_ESTOP_RESET:
        case EMC_TASK_STATE_ON:
        case EMC_TASK_STATE_OFF:
            break;
        default:
            PyErr_Format(PyExc_ValueError,"Machine state should be STATE_ESTOP, STATE_ESTOP_RESET, STATE_ON, or STATE_OFF");
            return NULL;
    }
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *tool_offset(pyCommandChannel *s, PyObject *o) {
    EMC_TOOL_SET_OFFSET m;
    if(!PyArg_ParseTuple(o, "idddddi", &m.toolno, &m.offset.tran.z, &m.offset.tran.x, &m.diameter,
                         &m.frontangle, &m.backangle, &m.orientation))
        return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *mist(pyCommandChannel *s, PyObject *o) {
    int dir;
    if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
    switch(dir) {
        case LOCAL_MIST_ON:
        {
            EMC_COOLANT_MIST_ON m;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_MIST_OFF:
        {
            EMC_COOLANT_MIST_OFF m;
            emcSendCommand(s, m);
        }
            break;
        default:
            PyErr_Format(PyExc_ValueError,"Mist should be MIST_ON or MIST_OFF");
            return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *flood(pyCommandChannel *s, PyObject *o) {
    int dir;
    if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
    switch(dir) {
        case LOCAL_FLOOD_ON:
        {
            EMC_COOLANT_FLOOD_ON m;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_FLOOD_OFF:
        {
            EMC_COOLANT_FLOOD_OFF m;
            emcSendCommand(s, m);
        }
            break;
        default:
            PyErr_Format(PyExc_ValueError,"FLOOD should be FLOOD_ON or FLOOD_OFF");
            return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *brake(pyCommandChannel *s, PyObject *o) {
    int dir;
    int spindle = 0;
    if(!PyArg_ParseTuple(o, "i|i", &dir, &spindle)) return NULL;
    switch(dir) {
        case LOCAL_BRAKE_ENGAGE:
        {
            EMC_SPINDLE_BRAKE_ENGAGE m;
            m.spindle = spindle;
            emcSendCommand(s, m);
        }
            break;
        case LOCAL_BRAKE_RELEASE:
        {
            EMC_SPINDLE_BRAKE_RELEASE m;
            m.spindle = spindle;
            emcSendCommand(s, m);
        }
            break;
        default:
            PyErr_Format(PyExc_ValueError,"BRAKE should be BRAKE_ENGAGE or BRAKE_RELEASE");
            return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *load_tool_table(pyCommandChannel *s, PyObject *o) {
    EMC_TOOL_LOAD_TOOL_TABLE m;
    m.file[0] = '\0'; // don't override the ini file
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *emcabort(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_ABORT m;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *override_limits(pyCommandChannel *s, PyObject *o) {
    EMC_JOINT_OVERRIDE_LIMITS m;
    m.joint = 0; // same number for all
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *home(pyCommandChannel *s, PyObject *o) {
    EMC_JOINT_HOME m;
    if(!PyArg_ParseTuple(o, "i", &m.joint)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *unhome(pyCommandChannel *s, PyObject *o) {
    EMC_JOINT_UNHOME m;
    if(!PyArg_ParseTuple(o, "i", &m.joint)) return NULL;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

// jog(JOG_STOP,       jjogmode, ja_value)
// jog(JOG_CONTINUOUS, jjogmode, ja_value, speed)
// jog(JOG_INCREMENT,  jjogmode, ja_value, speed, increment)
static PyObject *jog(pyCommandChannel *s, PyObject *o) {
    int fn;
    int ja_value,jjogmode;
    double vel, inc;

    if(!PyArg_ParseTuple(o, "iii|dd", &fn, &jjogmode, &ja_value, &vel, &inc)) {
        return NULL;
    }

    if(fn == LOCAL_JOG_STOP) {
        if(PyTuple_Size(o) != 3) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_STOP, ...) takes 3 arguments (%lu given)",
                (unsigned long)PyTuple_Size(o));
            return NULL;
        }
        EMC_JOG_STOP abort;
        abort.joint_or_axis = ja_value;
        abort.jjogmode = jjogmode;
        emcSendCommand(s, abort);
    } else if(fn == LOCAL_JOG_CONTINUOUS) {
        if(PyTuple_Size(o) != 4) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_CONTINUOUS, ...) takes 4 arguments (%lu given)",
                (unsigned long)PyTuple_Size(o));
            return NULL;
        }
        EMC_JOG_CONT cont;
        cont.joint_or_axis = ja_value;
        cont.vel = vel;
        cont.jjogmode = jjogmode;
        emcSendCommand(s, cont);
    } else if(fn == LOCAL_JOG_INCREMENT) {
        if(PyTuple_Size(o) != 5) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_INCREMENT, ...) takes 5 arguments (%lu given)",
                (unsigned long)PyTuple_Size(o));
            return NULL;
        }

        EMC_JOG_INCR incr;
        incr.joint_or_axis = ja_value;
        incr.vel = vel;
        incr.incr = inc;
        incr.jjogmode = jjogmode;
        emcSendCommand(s, incr);
    } else {
        PyErr_Format( PyExc_TypeError, "jog() first argument must be JOG_xxx");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *reset_interpreter(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_INIT m;
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *program_open(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_CLOSE m0;
    emcSendCommand(s, m0);

    EMC_TASK_PLAN_OPEN m;
    char *file;
    int len;

    if(!PyArg_ParseTuple(o, "s#", &file, &len)) return NULL;
    if(unsigned(len) > sizeof(m.file) - 1) {
        PyErr_Format(PyExc_ValueError, "File name limited to %zu characters", sizeof(m.file) - 1);
        return NULL;
    }
    strcpy(m.file, file);
    emcSendCommand(s, m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *emcauto(pyCommandChannel *s, PyObject *o) {
    int fn;
    EMC_TASK_PLAN_RUN run;
    EMC_TASK_PLAN_PAUSE pause;
    EMC_TASK_PLAN_RESUME resume;
    EMC_TASK_PLAN_STEP step;

    if(PyArg_ParseTuple(o, "ii", &fn, &run.line) && fn == LOCAL_AUTO_RUN) {
        emcSendCommand(s, run);
    } else {
        PyErr_Clear();
        if(!PyArg_ParseTuple(o, "i", &fn)) return NULL;
        switch(fn) {
        case LOCAL_AUTO_PAUSE:
            emcSendCommand(s, pause);
            break;
        case LOCAL_AUTO_RESUME:
            emcSendCommand(s, resume);
            break;
        case LOCAL_AUTO_STEP:
            emcSendCommand(s, step);
            break;
        default:
            PyErr_Format(error, "Unexpected argument '%d' to command.auto", fn);
            return NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *debug(pyCommandChannel *s, PyObject *o) {
    EMC_SET_DEBUG d;

    if(!PyArg_ParseTuple(o, "i", &d.debug)) return NULL;
    emcSendCommand(s, d);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *teleop(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_TELEOP_ENABLE en;

    if(!PyArg_ParseTuple(o, "i", &en.enable)) return NULL;

    emcSendCommand(s, en);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_traj_mode(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_MODE mo;

    if(!PyArg_ParseTuple(o, "i", &mo.mode)) return NULL;

    emcSendCommand(s, mo);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_min_limit(pyCommandChannel *s, PyObject *o) {
    EMC_JOINT_SET_MIN_POSITION_LIMIT m;
    if(!PyArg_ParseTuple(o, "id", &m.joint, &m.limit))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_max_limit(pyCommandChannel *s, PyObject *o) {
    EMC_JOINT_SET_MAX_POSITION_LIMIT m;
    if(!PyArg_ParseTuple(o, "id", &m.joint, &m.limit))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_feed_override(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_FO_ENABLE m;
    if(!PyArg_ParseTuple(o, "b", &m.mode))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_spindle_override(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_SO_ENABLE m;
    m.spindle = 0;
    if(!PyArg_ParseTuple(o, "b|i", &m.mode, &m.spindle))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_feed_hold(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_FH_ENABLE m;
    if(!PyArg_ParseTuple(o, "b", &m.mode))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_adaptive_feed(pyCommandChannel *s, PyObject *o) {
    EMC_MOTION_ADAPTIVE m;
    if(!PyArg_ParseTuple(o, "b", &m.status))
        return NULL;

    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_digital_output(pyCommandChannel *s, PyObject *o) {
    EMC_MOTION_SET_DOUT m;
    if(!PyArg_ParseTuple(o, "bb", &m.index, &m.start))
        return NULL;

    m.now = 1;
    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *set_analog_output(pyCommandChannel *s, PyObject *o) {
    EMC_MOTION_SET_AOUT m;
    if(!PyArg_ParseTuple(o, "bd", &m.index, &m.start))
        return NULL;

    m.now = 1;
    emcSendCommand(s, m);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *wait_complete(pyCommandChannel *s, PyObject *o) {
    double timeout = EMC_COMMAND_TIMEOUT;
    if (!PyArg_ParseTuple(o, "|d:emc.command.wait_complete", &timeout))
        return NULL;
    return PyInt_FromLong(emcWaitCommandComplete(s, timeout));
}

static PyMemberDef Command_members[] = {
    {(char*)"serial", T_INT, offsetof(pyCommandChannel, serial), READONLY},
    {NULL}
};

static PyMethodDef Command_methods[] = {
    {"debug", (PyCFunction)debug, METH_VARARGS},
    {"teleop_enable", (PyCFunction)teleop, METH_VARARGS},
    {"traj_mode", (PyCFunction)set_traj_mode, METH_VARARGS},
    {"wait_complete", (PyCFunction)wait_complete, METH_VARARGS},
    {"state", (PyCFunction)state, METH_VARARGS},
    {"mdi", (PyCFunction)mdi, METH_VARARGS},
    {"mode", (PyCFunction)mode, METH_VARARGS},
    {"feedrate", (PyCFunction)feedrate, METH_VARARGS},
    {"rapidrate", (PyCFunction)rapidrate, METH_VARARGS},
    {"maxvel", (PyCFunction)maxvel, METH_VARARGS},
    {"spindleoverride", (PyCFunction)spindleoverride, METH_VARARGS},
    {"spindle", (PyCFunction)spindle, METH_VARARGS},
    {"tool_offset", (PyCFunction)tool_offset, METH_VARARGS},
    {"mist", (PyCFunction)mist, METH_VARARGS},
    {"flood", (PyCFunction)flood, METH_VARARGS},
    {"brake", (PyCFunction)brake, METH_VARARGS},
    {"load_tool_table", (PyCFunction)load_tool_table, METH_NOARGS},
    {"abort", (PyCFunction)emcabort, METH_NOARGS},
    {"task_plan_synch", (PyCFunction)task_plan_synch, METH_NOARGS},
    {"override_limits", (PyCFunction)override_limits, METH_NOARGS},
    {"home", (PyCFunction)home, METH_VARARGS},
    {"unhome", (PyCFunction)unhome, METH_VARARGS},
    {"jog", (PyCFunction)jog, METH_VARARGS,
        "jog(JOG_CONTINUOUS, joint_flag, index, speed)\n"
        "jog(JOG_INCREMENT, joint_flag, index, speed, increment)\n"
        "jog(JOG_STOP, joint_flag, index)\n"
        "\n"
        "Start or stop a continuous or incremental jog of a joint or an axis.\n"
        "\n"
        "    joint_flag: True to jog a joint, False to jog an axis\n"
        "    index: the index of the joint or axis to jog\n"
        "    speed: jog speed\n"
        "    increment: distance to jog\n"
    },
    {"reset_interpreter", (PyCFunction)reset_interpreter, METH_NOARGS},
    {"program_open", (PyCFunction)program_open, METH_VARARGS},
    {"auto", (PyCFunction)emcauto, METH_VARARGS},
    {"set_optional_stop", (PyCFunction)optional_stop, METH_VARARGS},
    {"set_block_delete", (PyCFunction)block_delete, METH_VARARGS},
    {"set_min_limit", (PyCFunction)set_min_limit, METH_VARARGS},
    {"set_max_limit", (PyCFunction)set_max_limit, METH_VARARGS},
    {"set_feed_override", (PyCFunction)set_feed_override, METH_VARARGS},
    {"set_spindle_override", (PyCFunction)set_spindle_override, METH_VARARGS},
    {"set_feed_hold", (PyCFunction)set_feed_hold, METH_VARARGS},
    {"set_adaptive_feed", (PyCFunction)set_adaptive_feed, METH_VARARGS},
    {"set_digital_output", (PyCFunction)set_digital_output, METH_VARARGS},
    {"set_analog_output", (PyCFunction)set_analog_output, METH_VARARGS},
    {NULL}
};

static PyTypeObject Command_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "linuxcnc.command",          /*tp_name*/
    sizeof(pyCommandChannel),/*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Command_dealloc,        /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Command_methods,        /*tp_methods*/
    Command_members,        /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    (initproc)Command_init, /*tp_init*/
    0,                      /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static int Error_init(pyErrorChannel *self, PyObject *a, PyObject *k) {
    char *file = get_nmlfile();
    if(file == NULL) return -1;

    NML *c = new NML(emcFormat, "emcError", "xemc", file);
    if(!c) {
        PyErr_Format( error, "new NML failed");
        return -1;
    }

    self->c = c;
    return 0;
}

static PyObject* Error_poll(pyErrorChannel *s) {
    if(!s->c->valid()) {
        PyErr_Format( error, "Error buffer invalid" );
        return NULL;
    }
    NMLTYPE type = s->c->read();
    if(type == 0) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    PyObject *r = PyTuple_New(2);
    PyTuple_SET_ITEM(r, 0, PyInt_FromLong(type));
#define PASTE(a,b) a ## b
#define _TYPECASE(tag, type, f) \
    case tag: { \
        char error_string[LINELEN]; \
        strncpy(error_string, ((type*)s->c->get_address())->f, LINELEN-1); \
        error_string[LINELEN-1] = 0; \
        PyTuple_SET_ITEM(r, 1, PyString_FromString(error_string)); \
        break; \
    }
#define TYPECASE(x, f) _TYPECASE(PASTE(x, _TYPE), x, f)
    switch(type) {
        TYPECASE(EMC_OPERATOR_ERROR, error)
        TYPECASE(EMC_OPERATOR_TEXT, text)
        TYPECASE(EMC_OPERATOR_DISPLAY, display)
        TYPECASE(NML_ERROR, error)
        TYPECASE(NML_TEXT, text)
        TYPECASE(NML_DISPLAY, display)
    default:
        {
            char error_string[256];
            sprintf(error_string, "unrecognized error %" PRId32, type);
            PyTuple_SET_ITEM(r, 1, PyString_FromString(error_string));
            break;
        }
    }
    return r;
}

static void Error_dealloc(PyObject *self) {
    delete ((pyErrorChannel*)self)->c;
    PyObject_Del(self);
}

static PyMethodDef Error_methods[] = {
    {"poll", (PyCFunction)Error_poll, METH_NOARGS, "Poll for errors"},
    {NULL}
};

static PyTypeObject Error_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "linuxcnc.error_channel",    /*tp_name*/
    sizeof(pyErrorChannel), /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Error_dealloc,        /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Error_methods,          /*tp_methods*/
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    (initproc)Error_init,   /*tp_init*/
    0,                      /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

#include <GL/gl.h>

static void rotate_z(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double tx, ty;
    tx = pt[0] * c - pt[1] * s;
    ty = pt[0] * s + pt[1] * c;

    pt[0] = tx; pt[1] = ty;
}

static void rotate_y(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double tx, tz;
    tx = pt[0] * c - pt[2] * s;
    tz = pt[0] * s + pt[2] * c;

    pt[0] = tx; pt[2] = tz;
}

static void rotate_x(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double tx, tz;
    tx = pt[1] * c - pt[2] * s;
    tz = pt[1] * s + pt[2] * c;

    pt[1] = tx; pt[2] = tz;
}

static void translate(double pt[3], double ox, double oy, double oz) {
    pt[0] += ox;
    pt[1] += oy;
    pt[2] += oz;
}

static void vertex9(const double pt[9], double p[3], const char *geometry) {
    double sign = 1;

    p[0] = 0;
    p[1] = 0;
    p[2] = 0;

    for(; *geometry; geometry++) {
        switch(*geometry) {
            case '-': sign = -1; break;
            case 'X': translate(p, pt[0] * sign, 0, 0); sign=1; break;
            case 'Y': translate(p, 0, pt[1] * sign, 0); sign=1; break;
            case 'Z': translate(p, 0, 0, pt[2] * sign); sign=1; break;
            case 'U': translate(p, pt[6] * sign, 0, 0); sign=1; break;
            case 'V': translate(p, 0, pt[7] * sign, 0); sign=1; break;
            case 'W': translate(p, 0, 0, pt[8] * sign); sign=1; break;
            case 'A': rotate_x(p, pt[3] * sign); sign=1; break;
            case 'B': rotate_y(p, pt[4] * sign); sign=1; break;
            case 'C': rotate_z(p, pt[5] * sign); sign=1; break;
        }
    }
}

static void glvertex9(const double pt[9], const char *geometry) {
    double p[3];
    vertex9(pt, p, geometry);
    glVertex3dv(p);
}

#define max(a,b) ((a) < (b) ? (b) : (a))
#define max3(a,b,c) (max((a),max((b),(c))))

static void line9(const double p1[9], const double p2[9], const char *geometry) {
    if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
        double dc = max3(
            fabs(p2[3] - p1[3]),
            fabs(p2[4] - p1[4]),
            fabs(p2[5] - p1[5]));
        int st = (int)ceil(max(10, dc/10));
        int i;

        for(i=1; i<=st; i++) {
            double t = i * 1.0 / st;
            double v = 1.0 - t;
            double pt[9];
            for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
            glvertex9(pt, geometry);
        }
    } else {
        glvertex9(p2, geometry);
    }
}

static void line9b(const double p1[9], const double p2[9], const char *geometry) {
    glvertex9(p1, geometry);
    if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
        double dc = max3(
            fabs(p2[3] - p1[3]),
            fabs(p2[4] - p1[4]),
            fabs(p2[5] - p1[5]));
        int st = (int)ceil(max(10, dc/10));
        int i;

        for(i=1; i<=st; i++) {
            double t = i * 1.0 / st;
            double v = 1.0 - t;
            double pt[9];
            for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
            glvertex9(pt, geometry);
            if(i != st)
                glvertex9(pt, geometry);
        }
    } else {
        glvertex9(p2, geometry);
    }
}

static PyObject *pyline9(PyObject *s, PyObject *o) {
    double pt1[9], pt2[9];
    const char *geometry;

    if(!PyArg_ParseTuple(o, "s(ddddddddd)(ddddddddd):line9",
            &geometry,
            &pt1[0], &pt1[1], &pt1[2],
            &pt1[3], &pt1[4], &pt1[5],
            &pt1[6], &pt1[7], &pt1[8],
            &pt2[0], &pt2[1], &pt2[2],
            &pt2[3], &pt2[4], &pt2[5],
            &pt2[6], &pt2[7], &pt2[8]))
        return NULL;

    line9b(pt1, pt2, geometry);

    Py_RETURN_NONE;
}

static PyObject *pyvertex9(PyObject *s, PyObject *o) {
    double pt1[9], pt[3];
    char *geometry;
    if(!PyArg_ParseTuple(o, "s(ddddddddd):vertex9",
            &geometry,
            &pt1[0], &pt1[1], &pt1[2],
            &pt1[3], &pt1[4], &pt1[5],
            &pt1[6], &pt1[7], &pt1[8]))
        return NULL;

    vertex9(pt, pt1, geometry);
    return Py_BuildValue("(ddd)", &pt[0], &pt[1], &pt[2]);
}

static PyObject *pydraw_lines(PyObject *s, PyObject *o) {
    PyListObject *li;
    int for_selection = 0;
    int i;
    int first = 1;
    int nl = -1, n;
    double p1[9], p2[9], pl[9];
    char *geometry;

    if(!PyArg_ParseTuple(o, "sO!|i:draw_lines",
			    &geometry, &PyList_Type, &li, &for_selection))
        return NULL;

    for(i=0; i<PyList_GET_SIZE(li); i++) {
        PyObject *it = PyList_GET_ITEM(li, i);
        PyObject *dummy1, *dummy2, *dummy3;
        if(!PyArg_ParseTuple(it, "i(ddddddddd)(ddddddddd)|OOO", &n,
                    p1+0, p1+1, p1+2,
                    p1+3, p1+4, p1+5,
                    p1+6, p1+7, p1+8,
                    p2+0, p2+1, p2+2,
                    p2+3, p2+4, p2+5,
                    p2+6, p2+7, p2+8,
                    &dummy1, &dummy2, &dummy3)) {
            if(!first) glEnd();
            return NULL;
        }
        if(first || memcmp(p1, pl, sizeof(p1))
                || (for_selection && n != nl)) {
            if(!first) glEnd();
            if(for_selection && n != nl) {
                glLoadName(n);
                nl = n;
            }
            glBegin(GL_LINE_STRIP);
            glvertex9(p1, geometry);
            first = 0;
        }
        line9(p1, p2, geometry);
        memcpy(pl, p2, sizeof(p1));
    }

    if(!first) glEnd();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pydraw_dwells(PyObject *s, PyObject *o) {
    PyListObject *li;
    int for_selection = 0, is_lathe = 0, i, n;
    double alpha;
    char *geometry;
    double delta = 0.015625;

    if(!PyArg_ParseTuple(o, "sO!dii:draw_dwells", &geometry, &PyList_Type, &li, &alpha, &for_selection, &is_lathe))
        return NULL;

    if (for_selection == 0)
        glBegin(GL_LINES);

    for(i=0; i<PyList_GET_SIZE(li); i++) {
        PyObject *it = PyList_GET_ITEM(li, i);
        double red, green, blue, x, y, z;
        int axis;
        if(!PyArg_ParseTuple(it, "i(ddd)dddi", &n, &red, &green, &blue, &x, &y, &z, &axis)) {
            return NULL;
        }
        if (for_selection != 1)
            glColor4d(red, green, blue, alpha);
        if (for_selection == 1) {
            glLoadName(n);
            glBegin(GL_LINES);
        }
        if (is_lathe == 1)
            axis = 1;

        if (axis == 0) {
            glVertex3f(x-delta,y-delta,z);
            glVertex3f(x+delta,y+delta,z);
            glVertex3f(x-delta,y+delta,z);
            glVertex3f(x+delta,y-delta,z);

            glVertex3f(x+delta,y+delta,z);
            glVertex3f(x-delta,y-delta,z);
            glVertex3f(x+delta,y-delta,z);
            glVertex3f(x-delta,y+delta,z);
        } else if (axis == 1) {
            glVertex3f(x-delta,y,z-delta);
            glVertex3f(x+delta,y,z+delta);
            glVertex3f(x-delta,y,z+delta);
            glVertex3f(x+delta,y,z-delta);

            glVertex3f(x+delta,y,z+delta);
            glVertex3f(x-delta,y,z-delta);
            glVertex3f(x+delta,y,z-delta);
            glVertex3f(x-delta,y,z+delta);
        } else {
            glVertex3f(x,y-delta,z-delta);
            glVertex3f(x,y+delta,z+delta);
            glVertex3f(x,y+delta,z-delta);
            glVertex3f(x,y-delta,z+delta);

            glVertex3f(x,y+delta,z+delta);
            glVertex3f(x,y-delta,z-delta);
            glVertex3f(x,y-delta,z+delta);
            glVertex3f(x,y+delta,z-delta);
        }
        if (for_selection == 1)
            glEnd();
    }

    if (for_selection == 0)
        glEnd();

    Py_INCREF(Py_None);
    return Py_None;
}

struct color {
    unsigned char r, g, b, a;
    bool operator==(const color &o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const color &o) const {
        return r != o.r || g != o.g || b != o.b || a != o.a;
    }
} color;

struct logger_point {
    float x, y, z;
    struct color c;
    float rx, ry, rz; // or uvw
    struct color c2;
};

#define NUMCOLORS (6)
#define MAX_POINTS (10000)
typedef struct {
    PyObject_HEAD
    int npts, mpts, lpts;
    struct logger_point *p;
    struct color colors[NUMCOLORS];
    bool exit, clear, changed;
    char *geometry;
    int is_xyuv;
    double foam_z, foam_w;
    pyStatChannel *st;
} pyPositionLogger;

static const double epsilon = 1e-4; // 1-cos(1 deg) ~= 1e-4
static const double tiny = 1e-10;

static inline bool colinear(float xa, float ya, float za, float xb, float yb, float zb, float xc, float yc, float zc) {
    double dx1 = xa-xb, dx2 = xb-xc;
    double dy1 = ya-yb, dy2 = yb-yc;
    double dz1 = za-zb, dz2 = zb-zc;
    double dp = sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1);
    double dq = sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
    if( fabs(dp) < tiny || fabs(dq) < tiny ) return true;
    double dot = (dx1*dx2 + dy1*dy2 + dz1*dz2) / dp / dq;
    if( fabs(1-dot) < epsilon) return true;
    return false;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void LOCK() { pthread_mutex_lock(&mutex); }
static void UNLOCK() { pthread_mutex_unlock(&mutex); }

static int Logger_init(pyPositionLogger *self, PyObject *a, PyObject *k) {
    char *geometry;
    struct color *c = self->colors;
    self->p = (logger_point*)malloc(0);
    self->npts = self->mpts = 0;
    self->exit = self->clear = 0;
    self->changed = 1;
    self->st = 0;
    self->is_xyuv = 0;
    self->foam_z = 0;
    self->foam_w = 1.5;  // temporarily hard-code
    if(!PyArg_ParseTuple(a, "O!(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)s|i",
            &Stat_Type, &self->st,
            &c[0].r,&c[0].g, &c[0].b, &c[0].a,
            &c[1].r,&c[1].g, &c[1].b, &c[1].a,
            &c[2].r,&c[2].g, &c[2].b, &c[2].a,
            &c[3].r,&c[3].g, &c[3].b, &c[3].a,
            &c[4].r,&c[4].g, &c[4].b, &c[4].a,
            &c[5].r,&c[5].g, &c[5].b, &c[5].a,
            &geometry, &self->is_xyuv
            ))
        return -1;
    Py_INCREF(self->st);
    self->geometry = strdup(geometry);
    return 0;
}

static void Logger_dealloc(pyPositionLogger *s) {
    free(s->p);
    Py_XDECREF(s->st);
    free(s->geometry);
    PyObject_Del(s);
}

static PyObject *Logger_set_depth(pyPositionLogger *s, PyObject *o) {
    double z, w;
    if(!PyArg_ParseTuple(o, "dd:logger.set_depth", &z, &w)) return NULL;
    s->foam_z = z;
    s->foam_w = w;
    Py_INCREF(Py_None);
    return Py_None;
}

static double dist2(double x1, double y1, double x2, double y2) {
    double dx = x2-x1;
    double dy = y2-y1;
    return dx*dx + dy*dy;
}

static PyObject *Logger_start(pyPositionLogger *s, PyObject *o) {
    double interval;
    struct timespec ts;

    if(!PyArg_ParseTuple(o, "d:logger.start", &interval)) return NULL;
    ts.tv_sec = (int)interval;
    ts.tv_nsec = (long int)(1e9 * (interval - ts.tv_sec));

    Py_INCREF(s->st);

    s->exit = 0;
    s->clear = 0;
    s->npts = 0;

    Py_BEGIN_ALLOW_THREADS
    while(!s->exit) {
        if(s->clear) {
            s->npts = 0;
            s->lpts = 0;
            s->clear = 0;
        }
        if(s->st->c->valid() && s->st->c->peek() == EMC_STAT_TYPE) {
            EMC_STAT *status = static_cast<EMC_STAT*>(s->st->c->get_address());
            int colornum = 2;
            colornum = status->motion.traj.motion_type;
            if(colornum < 0 || colornum > NUMCOLORS) colornum = 0;
            struct color c = s->colors[colornum];
            struct logger_point *op = &s->p[s->npts-1];
            struct logger_point *oop = &s->p[s->npts-2];
            bool add_point = s->npts < 2 || c != op->c;
            double x, y, z, rx, ry, rz;
            if(s->is_xyuv) {
                x = status->motion.traj.position.tran.x - status->task.toolOffset.tran.x,
                y = status->motion.traj.position.tran.y - status->task.toolOffset.tran.y,
                z = s->foam_z;
                rx = status->motion.traj.position.u - status->task.toolOffset.u,
                ry = status->motion.traj.position.v - status->task.toolOffset.v,
                rz = s->foam_w;
                /* TODO .01, the distance at which a preview line is dropped,
                 * should either be dependent on units or configurable, because
                 * 0.1 is inappropriate for mm systems
                 */
                add_point = add_point || (dist2(x, y, oop->x, oop->y) > .01)
                    || (dist2(rx, ry, oop->rx, oop->ry) > .01);
                add_point = add_point || !colinear( x, y, z,
                                op->x, op->y, op->z,
                                oop->x, oop->y, oop->z);
                add_point = add_point || !colinear( rx, ry, rz,
                                op->rx, op->ry, op->rz,
                                oop->rx, oop->ry, oop->rz);
            } else {
                double pt[9] = {
                    status->motion.traj.position.tran.x - status->task.toolOffset.tran.x,
                    status->motion.traj.position.tran.y - status->task.toolOffset.tran.y,
                    status->motion.traj.position.tran.z - status->task.toolOffset.tran.z,
                    status->motion.traj.position.a - status->task.toolOffset.a,
                    status->motion.traj.position.b - status->task.toolOffset.b,
                    status->motion.traj.position.c - status->task.toolOffset.c,
                    status->motion.traj.position.u - status->task.toolOffset.u,
                    status->motion.traj.position.v - status->task.toolOffset.v,
                    status->motion.traj.position.w - status->task.toolOffset.w};

                double p[3];
                vertex9(pt, p, s->geometry);
                x = p[0]; y = p[1]; z = p[2];
                rx = pt[3]; ry = -pt[4]; rz = pt[5];

                add_point = add_point || !colinear( x, y, z,
                                op->x, op->y, op->z,
                                oop->x, oop->y, oop->z);
            }
            if(add_point) {
                // 1 or 2 points may be added, make room whenever
                // fewer than 2 are left
                bool changed_color = s->npts && c != op->c;
                if(s->npts+2 > s->mpts) {
                    LOCK();
                    if(s->mpts >= MAX_POINTS) {
                        int adjust = MAX_POINTS / 10;
                        if(adjust < 2) adjust = 2;
                        s->npts -= adjust;
                        memmove(s->p, s->p + adjust,
                                sizeof(struct logger_point) * s->npts);
                    } else {
                        s->mpts = 2 * s->mpts + 2;
                        s->changed = 1;
                        s->p = (struct logger_point*) realloc(s->p,
                                    sizeof(struct logger_point) * s->mpts);
                    }
                    UNLOCK();
                    op = &s->p[s->npts-1];
                    oop = &s->p[s->npts-2];
                }
                if(changed_color) {
                    {
                    struct logger_point &np = s->p[s->npts];
                    np.x = op->x; np.y = op->y; np.z = op->z;
                    np.rx = rx; np.ry = ry; np.rz = rz;
                    np.c = np.c2 = c;
                    }
                    {
                    struct logger_point &np = s->p[s->npts+1];
                    np.x = x; np.y = y; np.z = z;
                    np.rx = rx; np.ry = ry; np.rz = rz;
                    np.c = np.c2 = c;
                    }
                    s->npts += 2;
                } else {
                    struct logger_point &np = s->p[s->npts];
                    np.x = x; np.y = y; np.z = z;
                    np.rx = rx; np.ry = ry; np.rz = rz;
                    np.c = np.c2 = c;
                    s->npts++;
                }
            } else {
                struct logger_point &np = s->p[s->npts-1];
                np.x = x; np.y = y; np.z = z;
                np.rx = rx; np.ry = ry; np.rz = rz;
            }
        }
        nanosleep(&ts, NULL);
    }
    Py_END_ALLOW_THREADS
    Py_DECREF(s->st);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* Logger_clear(pyPositionLogger *s, PyObject *o) {
    s->clear = true;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* Logger_stop(pyPositionLogger *s, PyObject *o) {
    s->exit = true;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* Logger_call(pyPositionLogger *s, PyObject *o) {
    if(!s->clear) {
        LOCK();
        if(s->is_xyuv) {
            if(s->changed) {
                glVertexPointer(3, GL_FLOAT,
                        sizeof(struct logger_point)/2, &s->p->x);
                glColorPointer(4, GL_UNSIGNED_BYTE,
                        sizeof(struct logger_point)/2, &s->p->c);
                glEnableClientState(GL_COLOR_ARRAY);
                glEnableClientState(GL_VERTEX_ARRAY);
                s->changed = 0;
            }
            s->lpts = s->npts;
            glDrawArrays(GL_LINES, 0, 2*s->npts);
        } else {
            if(s->changed) {
                glVertexPointer(3, GL_FLOAT,
                        sizeof(struct logger_point), &s->p->x);
                glColorPointer(4, GL_UNSIGNED_BYTE,
                        sizeof(struct logger_point), &s->p->c);
                glEnableClientState(GL_COLOR_ARRAY);
                glEnableClientState(GL_VERTEX_ARRAY);
                s->changed = 0;
            }
            s->lpts = s->npts;
            glDrawArrays(GL_LINE_STRIP, 0, s->npts);
        }
        UNLOCK();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Logger_last(pyPositionLogger *s, PyObject *o) {
    int flag=1;
    if(!PyArg_ParseTuple(o, "|i:emc.positionlogger.last", &flag)) return NULL;
    PyObject *result = NULL;
    LOCK();
    int idx = flag ? s->lpts : s->npts;
    if(!idx) {
        Py_INCREF(Py_None);
        result = Py_None;
    } else {
        result = PyTuple_New(6);
        struct logger_point &p = s->p[idx-1];
        PyTuple_SET_ITEM(result, 0, PyFloat_FromDouble(p.x));
        PyTuple_SET_ITEM(result, 1, PyFloat_FromDouble(p.y));
        PyTuple_SET_ITEM(result, 2, PyFloat_FromDouble(p.z));
        PyTuple_SET_ITEM(result, 3, PyFloat_FromDouble(p.rx));
        PyTuple_SET_ITEM(result, 4, PyFloat_FromDouble(p.ry));
        PyTuple_SET_ITEM(result, 5, PyFloat_FromDouble(p.rz));
    }
    UNLOCK();
    return result;
}

static PyMemberDef Logger_members[] = {
    {(char*)"npts", T_INT, offsetof(pyPositionLogger, npts), READONLY},
    {0, 0, 0, 0},
};

static PyMethodDef Logger_methods[] = {
    {"start", (PyCFunction)Logger_start, METH_VARARGS,
        "Start the position logger and run every ARG seconds"},
    {"clear", (PyCFunction)Logger_clear, METH_NOARGS,
        "Clear the position logger"},
    {"stop", (PyCFunction)Logger_stop, METH_NOARGS,
        "Stop the position logger"},
    {"call", (PyCFunction)Logger_call, METH_NOARGS,
        "Plot the backplot now"},
    {"set_depth", (PyCFunction)Logger_set_depth, METH_VARARGS,
        "set the Z and W depths for foam cutter"},
    {"last", (PyCFunction)Logger_last, METH_VARARGS,
        "Return the most recent point on the plot or None"},
    {NULL, NULL, 0, NULL},
};

static PyTypeObject PositionLoggerType = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "linuxcnc.positionlogger",   /*tp_name*/
    sizeof(pyPositionLogger), /*tp_basicsize*/
    0,                      /*tp_itemsize*/
    /* methods */
    (destructor)Logger_dealloc, /*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    Logger_methods,         /*tp_methods*/
    Logger_members,         /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    (initproc)Logger_init,  /*tp_init*/
    0,                      /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static PyMethodDef emc_methods[] = {
#define METH(name, doc) { #name, (PyCFunction) py##name, METH_VARARGS, doc }
METH(draw_lines, "Draw a bunch of lines in the 'rs274.glcanon' format"),
METH(draw_dwells, "Draw a bunch of dwell positions in the 'rs274.glcanon' format"),
METH(line9, "Draw a single line in the 'rs274.glcanon' format; assumes glBegin(GL_LINES)"),
METH(vertex9, "Get the 3d location for a 9d point"),
    {NULL}
#undef METH
};

/* ENUM defines an integer constant with the same name as the C constant.
 * ENUMX defines an integer constant with the first i characters of the C
 * constant name removed (so that ENUMX(4,RCS_TASK_MODE_MDI) creates a constant
 * named TASK_MODE_MDI) */

#define ENUM(e) PyModule_AddIntConstant(m, const_cast<char*>(#e), e)
#define ENUMX(x,e) PyModule_AddIntConstant(m, x + const_cast<char*>(#e), e)

PyMODINIT_FUNC
initlinuxcnc(void) {
    verbose_nml_error_messages = 0;
    clear_rcs_print_flag(~0);

    m = Py_InitModule3("linuxcnc", emc_methods, "Interface to LinuxCNC");

    PyType_Ready(&Stat_Type);
    PyType_Ready(&Command_Type);
    PyType_Ready(&Error_Type);
    PyType_Ready(&Ini_Type);
    error = PyErr_NewException((char*)"linuxcnc.error", PyExc_RuntimeError, NULL);

    PyModule_AddObject(m, "stat", (PyObject*)&Stat_Type);
    PyModule_AddObject(m, "command", (PyObject*)&Command_Type);
    PyModule_AddObject(m, "error_channel", (PyObject*)&Error_Type);
    PyModule_AddObject(m, "ini", (PyObject*)&Ini_Type);
    PyModule_AddObject(m, "error", error);

    PyType_Ready(&PositionLoggerType);
    PyModule_AddObject(m, "positionlogger", (PyObject*)&PositionLoggerType);
    pthread_mutex_init(&mutex, NULL);

    PyModule_AddStringConstant(m, "PREFIX", EMC2_HOME);
    PyModule_AddStringConstant(m, "SHARE", EMC2_HOME "/share");
    PyModule_AddStringConstant(m, "nmlfile", EMC2_DEFAULT_NMLFILE);

    PyModule_AddIntConstant(m, "OPERATOR_ERROR", EMC_OPERATOR_ERROR_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_TEXT", EMC_OPERATOR_TEXT_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_DISPLAY", EMC_OPERATOR_DISPLAY_TYPE);
    PyModule_AddIntConstant(m, "NML_ERROR", NML_ERROR_TYPE);
    PyModule_AddIntConstant(m, "NML_TEXT", NML_TEXT_TYPE);
    PyModule_AddIntConstant(m, "NML_DISPLAY", NML_DISPLAY_TYPE);

    PyStructSequence_InitType(&ToolResultType, &tool_result_desc);
    PyModule_AddObject(m, "tool", (PyObject*)&ToolResultType);
    PyModule_AddObject(m, "version", PyString_FromString(PACKAGE_VERSION));

    ENUMX(4, EMC_LINEAR);
    ENUMX(4, EMC_ANGULAR);

    ENUMX(9, EMC_TASK_INTERP_IDLE);
    ENUMX(9, EMC_TASK_INTERP_READING);
    ENUMX(9, EMC_TASK_INTERP_PAUSED);
    ENUMX(9, EMC_TASK_INTERP_WAITING);

    ENUMX(9, EMC_TASK_MODE_MDI);
    ENUMX(9, EMC_TASK_MODE_MANUAL);
    ENUMX(9, EMC_TASK_MODE_AUTO);

    ENUMX(9, EMC_TASK_STATE_OFF);
    ENUMX(9, EMC_TASK_STATE_ON);
    ENUMX(9, EMC_TASK_STATE_ESTOP);
    ENUMX(9, EMC_TASK_STATE_ESTOP_RESET);

    ENUMX(6, LOCAL_SPINDLE_FORWARD);
    ENUMX(6, LOCAL_SPINDLE_REVERSE);
    ENUMX(6, LOCAL_SPINDLE_OFF);
    ENUMX(6, LOCAL_SPINDLE_INCREASE);
    ENUMX(6, LOCAL_SPINDLE_DECREASE);
    ENUMX(6, LOCAL_SPINDLE_CONSTANT);

    ENUMX(6, LOCAL_MIST_ON);
    ENUMX(6, LOCAL_MIST_OFF);

    ENUMX(6, LOCAL_FLOOD_ON);
    ENUMX(6, LOCAL_FLOOD_OFF);

    ENUMX(6, LOCAL_BRAKE_ENGAGE);
    ENUMX(6, LOCAL_BRAKE_RELEASE);

    ENUMX(6, LOCAL_JOG_STOP);
    ENUMX(6, LOCAL_JOG_CONTINUOUS);
    ENUMX(6, LOCAL_JOG_INCREMENT);

    ENUMX(6, LOCAL_AUTO_RUN);
    ENUMX(6, LOCAL_AUTO_PAUSE);
    ENUMX(6, LOCAL_AUTO_RESUME);
    ENUMX(6, LOCAL_AUTO_STEP);

    ENUMX(4, EMC_TRAJ_MODE_FREE);
    ENUMX(4, EMC_TRAJ_MODE_COORD);
    ENUMX(4, EMC_TRAJ_MODE_TELEOP);

    ENUMX(4, EMC_MOTION_TYPE_TRAVERSE);
    ENUMX(4, EMC_MOTION_TYPE_FEED);
    ENUMX(4, EMC_MOTION_TYPE_ARC);
    ENUMX(4, EMC_MOTION_TYPE_TOOLCHANGE);
    ENUMX(4, EMC_MOTION_TYPE_PROBING);
    ENUMX(4, EMC_MOTION_TYPE_INDEXROTARY);

    ENUM(KINEMATICS_IDENTITY);
    ENUM(KINEMATICS_FORWARD_ONLY);
    ENUM(KINEMATICS_INVERSE_ONLY);
    ENUM(KINEMATICS_BOTH);

    ENUMX(4, EMC_DEBUG_CONFIG);
    ENUMX(4, EMC_DEBUG_VERSIONS);
    ENUMX(4, EMC_DEBUG_TASK_ISSUE);
    ENUMX(4, EMC_DEBUG_NML);
    ENUMX(4, EMC_DEBUG_MOTION_TIME);
    ENUMX(4, EMC_DEBUG_INTERP);
    ENUMX(4, EMC_DEBUG_RCS);
    ENUMX(4, EMC_DEBUG_INTERP_LIST);

    ENUMX(9, EMC_TASK_EXEC_ERROR);
    ENUMX(9, EMC_TASK_EXEC_DONE);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_IO);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_DELAY);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_SYSTEM_CMD);
    ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_SPINDLE_ORIENTED);

    ENUMX(7, EMCMOT_MAX_JOINTS);
    ENUMX(7, EMCMOT_MAX_AXIS);


    ENUM(RCS_DONE);
    ENUM(RCS_EXEC);
    ENUM(RCS_ERROR);
}


// # vim:sw=4:sts=4:et:ts=8:
