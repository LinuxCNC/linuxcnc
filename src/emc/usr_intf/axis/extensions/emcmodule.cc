//    This is a component of AXIS, a front-end for emc
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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <Python.h>
#include <structseq.h>
#include <pthread.h>
#include <GL/gl.h>
#include <structmember.h>
#include "rcs.hh"
#include "emc.hh"
#include "kinematics.h"
#include "config.h"
#include "inifile.hh"
#define INIFILE_CHECK_FAILURE(x) (!(x))
#include <cmath>

#ifndef PyMODINIT_FUNC
#if defined(__cplusplus)
#define PyMODINIT_FUNC extern "C" void
#else /* __cplusplus */
#define PyMODINIT_FUNC void
#endif /* __cplusplus */
#endif

#ifndef DEFAULT_NMLFILE
#define DEFAULT_NMLFILE EMC_NMLFILE
#endif

#define NUM_AXES (6)

#define EMC_OPERATOR_ERROR_LEN LINELEN
#define EMC_OPERATOR_TEXT_LEN LINELEN
#define EMC_OPERATOR_DISPLAY_LEN LINELEN

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
#undef offsetof
#define offsetof(T,x) (size_t)(-1+(char*)&(((T*)1)->x))

static bool feq(double a, double b) { return fabs(a-b) < 1e-5; }

struct pyIniFile {
    PyObject_HEAD
    Inifile i;
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

    if (INIFILE_CHECK_FAILURE(self->i.open(inifile))) {
        PyErr_Format( error, "inifile.open() failed");
        return -1;
    }
    return 0;
}

static PyObject *Ini_find(pyIniFile *self, PyObject *args) {
    const char *s1, *s2, *out;
    int num = 1; 
    if(!PyArg_ParseTuple(args, "ss|i:find", &s1, &s2, &num)) return NULL;
    
    out = self->i.find(s2, s1, num);
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
        out = self->i.find(s2, s1, num);
        if(out == NULL) {
            break;
        }
        PyList_Append(result, PyString_FromString(const_cast<char*>(out)));
        num++;
    }
    return result;
}

static void Ini_dealloc(pyIniFile *self) {
    self->i.close();
    PyObject_Del(self);
}

static PyMethodDef Ini_methods[] = {
    {"find", (PyCFunction)Ini_find, METH_VARARGS,
        "Find value in inifile as string.  This uses the ConfigParser-style "
        "(section,option) order, not the emc order."},
    {"findall", (PyCFunction)Ini_findall, METH_VARARGS,
        "Find value in inifile as a list.  This uses the ConfigParser-style "
        "(section,option) order, not the emc order."},
    {NULL}
};

PyTypeObject Ini_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "emc.ini",              /*tp_name*/
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


#define EMC_COMMAND_TIMEOUT 1.0  // how long to wait until timeout
#define EMC_COMMAND_DELAY   0.01 // how long to sleep between checks

static int emcWaitCommandComplete(int serial_number, RCS_STAT_CHANNEL *s) {
    double start = etime();

    while (etime() - start < EMC_COMMAND_TIMEOUT) {
        if(s->peek() == EMC_STAT_TYPE) {
           EMC_STAT *stat = (EMC_STAT*)s->get_address();
//           printf("WaitComplete: %d %d %d\n", serial_number, stat->echo_serial_number, stat->status);
           if (stat->echo_serial_number == serial_number &&
               ( stat->status == RCS_DONE || stat->status == RCS_ERROR )) {
                return s->get_address()->status;
           }
        }
        esleep(EMC_COMMAND_DELAY);
    }
    return -1;
}


static void emcWaitCommandReceived(int serial_number, RCS_STAT_CHANNEL *s) {
    double start = etime();

    while (etime() - start < EMC_COMMAND_TIMEOUT) {
        if(s->peek() == EMC_STAT_TYPE &&
           s->get_address()->echo_serial_number == serial_number) {
                return;
           }
        esleep(EMC_COMMAND_DELAY);
    }
}

static int next_serial(pyCommandChannel *c) {
    if(c->serial != 0) {
        emcWaitCommandReceived(c->serial, c->s);
    }
    return ++c->serial;
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
        PyErr_Format( error, "emcStatusBuffer invalid" );
        return false;
    }
    return true;
}

static bool wait_stat(RCS_STAT_CHANNEL *emcStatusBuffer) {
    if(!emcStatusBuffer->valid()) { 
        PyErr_Format( error, "emcStatusBuffer invalid" );
        return false;
    }
    while(emcStatusBuffer->peek() != EMC_STAT_TYPE) {
        usleep(10000); 
    }
    return true;
}

static PyObject *gettaskfile(pyStatChannel *s, PyObject *o) {
    if(!wait_stat(s->c)) return NULL;
    EMC_STAT *emcStatus = static_cast<EMC_STAT*>(s->c->get_address());
    return PyString_FromString(emcStatus->task.file);
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
    {"gettaskfile", (PyCFunction)gettaskfile, METH_NOARGS,
                                             "Get current task file"},
    {NULL}
};


#define O(x) offsetof(pyStatChannel,status.x)
#define POS(prefix, member) \
    {prefix "x", T_DOUBLE, O(member.tran.x), READONLY}, \
    {prefix "y", T_DOUBLE, O(member.tran.y), READONLY}, \
    {prefix "z", T_DOUBLE, O(member.tran.z), READONLY}, \
    {prefix "a", T_DOUBLE, O(member.a), READONLY}, \
    {prefix "b", T_DOUBLE, O(member.b), READONLY}, \
    {prefix "c", T_DOUBLE, O(member.c), READONLY}
static PyMemberDef Stat_members[] = {
// stat 
    {"echo_serial_number", T_INT, O(echo_serial_number), READONLY},
    {"state", T_INT, O(status), READONLY},
    {"line", T_INT, O(line), READONLY},
    {"source_line", T_INT, O(line), READONLY},
    {"source_file", T_STRING_INPLACE, O(source_file), READONLY},

// task
    {"task_mode", T_INT, O(task.mode), READONLY},
    {"task_state", T_INT, O(task.state), READONLY},
    {"exec_state", T_INT, O(task.execState), READONLY},
    {"interp_state", T_INT, O(task.interpState), READONLY},
    {"read_line", T_INT, O(task.readLine), READONLY},
    {"motion_line", T_INT, O(task.motionLine), READONLY},
    {"current_line", T_INT, O(task.currentLine), READONLY},
    {"file", T_STRING_INPLACE, O(task.file), READONLY},
    {"command", T_STRING_INPLACE, O(task.command), READONLY},
    {"program_units", T_INT, O(task.programUnits), READONLY},
    {"interpreter_errcode", T_INT, O(task.interpreter_errcode), READONLY},

// motion
//   EMC_TRAJ_STAT traj
    {"linear_units", T_DOUBLE, O(motion.traj.linearUnits), READONLY},
    {"angular_units", T_DOUBLE, O(motion.traj.angularUnits), READONLY},
    {"cycle_time", T_DOUBLE, O(motion.traj.cycleTime), READONLY},
    {"axes", T_INT, O(motion.traj.axes), READONLY},
    {"motion_mode", T_INT, O(motion.traj.mode), READONLY},
    {"enabled", T_INT, O(motion.traj.enabled), READONLY},
    {"inpos", T_INT, O(motion.traj.inpos), READONLY},
    {"queue", T_INT, O(motion.traj.queue), READONLY},
    {"id", T_INT, O(motion.traj.id), READONLY},
    {"paused", T_INT, O(motion.traj.paused), READONLY},
    {"feedrate", T_DOUBLE, O(motion.traj.scale), READONLY},
    
    {"velocity", T_DOUBLE, O(motion.traj.velocity), READONLY},
    {"acceleration", T_DOUBLE, O(motion.traj.acceleration), READONLY},
    {"max_velocity", T_DOUBLE, O(motion.traj.maxVelocity), READONLY},
    {"max_acceleration", T_DOUBLE, O(motion.traj.maxAcceleration), READONLY},
    {"probe_index", T_INT, O(motion.traj.probe_index), READONLY},
    {"probe_polarity", T_INT, O(motion.traj.probe_polarity), READONLY},
    {"probe_tripped", T_INT, O(motion.traj.probe_tripped), READONLY},
    {"probing", T_INT, O(motion.traj.probing), READONLY},
    {"probe_val", T_INT, O(motion.traj.probeval), READONLY},
    {"kinematics_type", T_INT, O(motion.traj.kinematics_type), READONLY},
    {"motion_type", T_INT, O(motion.traj.motion_type), READONLY},
    {"distance_to_go", T_DOUBLE, O(motion.traj.distance_to_go), READONLY},

// io
// EMC_TOOL_STAT io.tool
    {"tool_prepped", T_INT, O(io.tool.toolPrepped), READONLY},
    {"tool_in_spindle", T_INT, O(io.tool.toolInSpindle), READONLY},
    

// EMC_SPINDLE_STAT motion.spindle
    {"spindle_speed", T_DOUBLE, O(motion.spindle.speed), READONLY},
    {"spindle_direction", T_INT, O(motion.spindle.direction), READONLY},
    {"spindle_brake", T_INT, O(motion.spindle.brake), READONLY},
    {"spindle_increasing", T_INT, O(motion.spindle.increasing), READONLY},
    {"spindle_enabled", T_INT, O(motion.spindle.enabled), READONLY},

// EMC_COOLANT_STAT io.cooland
    {"mist", T_INT, O(io.coolant.mist), READONLY},
    {"flood", T_INT, O(io.coolant.flood), READONLY},

// EMC_AUX_STAT     io.aux
    {"estop", T_INT, O(io.aux.estop), READONLY},

// EMC_LUBE_STAT    io.lube
    {"lube", T_INT, O(io.lube.on), READONLY},
    {"lube_level", T_INT, O(io.lube.level), READONLY},

    {"debug", T_INT, O(debug), READONLY},
    {NULL}
};

static PyObject *uchar_array(unsigned char *arr, int sz) {
    PyObject *res = PyTuple_New(sz);
    for(int i = 0; i < sz; i++) {
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(arr[i]));
    }
    return res;
}

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
    PyObject *res = PyTuple_New(6);
    PyTuple_SET_ITEM(res, 0, PyFloat_FromDouble(p.tran.x));
    PyTuple_SET_ITEM(res, 1, PyFloat_FromDouble(p.tran.y));
    PyTuple_SET_ITEM(res, 2, PyFloat_FromDouble(p.tran.z));
    PyTuple_SET_ITEM(res, 3, PyFloat_FromDouble(p.a));
    PyTuple_SET_ITEM(res, 4, PyFloat_FromDouble(p.b));
    PyTuple_SET_ITEM(res, 5, PyFloat_FromDouble(p.c));
    return res;
}

static PyObject *Stat_origin(pyStatChannel *s) {
    return pose(s->status.task.origin);
}

static PyObject *Stat_tool_offset(pyStatChannel *s) {
    return pose(s->status.task.toolOffset);
}

static PyObject *Stat_position(pyStatChannel *s) {
    return pose(s->status.motion.traj.position);
}

static PyObject *Stat_actual(pyStatChannel *s) {
    return pose(s->status.motion.traj.actualPosition);
}

static PyObject *Stat_joint_position(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMC_AXIS_MAX);
    for(int i=0; i<EMC_AXIS_MAX; i++) {
        PyTuple_SetItem(res, i,
                PyFloat_FromDouble(s->status.motion.axis[i].output));
    }
    return res;
}

static PyObject *Stat_joint_actual(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMC_AXIS_MAX);
    for(int i=0; i<EMC_AXIS_MAX; i++) {
        PyTuple_SetItem(res, i,
                PyFloat_FromDouble(s->status.motion.axis[i].input));
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
    return uchar_array(s->status.io.aux.din, EMC_AUX_MAX_DIN);
}

static PyObject *Stat_dout(pyStatChannel *s) {
    return uchar_array(s->status.io.aux.dout, EMC_AUX_MAX_DOUT);
}

static PyObject *Stat_limit(pyStatChannel *s) {
    int sz = NUM_AXES;
    PyObject *res = PyTuple_New(sz);
    for(int i = 0; i < sz; i++) {
        int v = 0;
        if(s->status.motion.axis[i].minHardLimit) v |= 1;
        if(s->status.motion.axis[i].maxHardLimit) v |= 2;
        if(s->status.motion.axis[i].minSoftLimit) v |= 4;
        if(s->status.motion.axis[i].maxSoftLimit) v |= 8;
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(v));
    }
    return res;
}

static PyObject *Stat_homed(pyStatChannel *s) {
    int sz = NUM_AXES;
    PyObject *res = PyTuple_New(sz);
    for(int i = 0; i < sz; i++) {
        PyTuple_SET_ITEM(res, i, PyInt_FromLong(s->status.motion.axis[i].homed));
    }
    return res;
}

static PyObject *Stat_ain(pyStatChannel *s) {
    return double_array(s->status.io.aux.ain, EMC_AUX_MAX_AIN);
}

static PyObject *Stat_aout(pyStatChannel *s) {
    return double_array(s->status.io.aux.aout, EMC_AUX_MAX_AOUT);
}

static void dict_add(PyObject *d, char *name, unsigned char v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyInt_FromLong(v));
    Py_XDECREF(o);
}
static void dict_add(PyObject *d, char *name, double v) {
    PyObject *o;
    PyDict_SetItemString(d, name, o = PyFloat_FromDouble(v));
    Py_XDECREF(o);
}
#define F(x) F2(#x, x)
#define F2(y,x) dict_add(res, y, s->status.motion.axis[axisno].x)
static PyObject *Stat_axis_one(pyStatChannel *s, int axisno) {
    PyObject *res = PyDict_New();
    F(axisType);
    F(units);
    F(p);
    F(i);
    F(d);
    F(ff0);
    F(ff1);
    F(ff2);
    F(backlash);
    F(bias);
    F2("max_error", maxError);
    F(deadband);
    F2("cycle_type", cycleTime);
    F2("input_scale", inputScale);
    F2("input_offset", inputOffset);
    F2("output_scale", outputScale);
    F2("output_offset", outputOffset);
    F2("min_position_limit", minPositionLimit);
    F2("max_position_limit", maxPositionLimit);
    F2("min_output_limit", minOutputLimit);
    F2("max_output_limit", maxOutputLimit);
    F2("max_ferror", maxFerror);
    F2("min_ferror", minFerror);
    F2("homing_vel", homingVel);
    F(setup_time);
    F(hold_time);
    F2("home_offset", homeOffset);
    F(setpoint);
    F2("ferror_current", ferrorCurrent);
    F2("ferror_highmark", ferrorHighMark);
    F(output);
    F(input);
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
    F(scale);
    F(alter);
    return res;
}
#undef F
#undef F2

static PyObject *Stat_axis(pyStatChannel *s) {
    PyObject *res = PyTuple_New(EMC_AXIS_MAX);
    for(int i=0; i<EMC_AXIS_MAX; i++) {
        PyTuple_SetItem(res, i, Stat_axis_one(s, i));
    }
    return res;
}

static PyStructSequence_Field tool_fields[] = {
    {"id", },
    {"zoffset", },
    {"xoffset", },
    {"diameter", },
    {"frontangle", },
    {"backangle", },
    {"orientation", },
    {0,},
};

static PyStructSequence_Desc tool_result_desc = {
    "tool_result", /* name */
    "", /* doc */
    tool_fields,
    7
};


static PyTypeObject ToolResultType;

static PyObject *Stat_tool_table(pyStatChannel *s) {
    PyObject *res = PyTuple_New(CANON_TOOL_MAX);
    int j=0;
    for(int i=1; i<=CANON_TOOL_MAX; i++) {
        struct CANON_TOOL_TABLE &t = s->status.io.tool.toolTable[i];
        if(t.id == 0) continue;
        PyObject *tool = PyStructSequence_New(&ToolResultType);
#if EMC_VERSION_CHECK(2,1,0)
        PyStructSequence_SET_ITEM(tool, 0, PyInt_FromLong(t.id));
        PyStructSequence_SET_ITEM(tool, 1, PyFloat_FromDouble(t.zoffset));
        PyStructSequence_SET_ITEM(tool, 2, PyFloat_FromDouble(t.xoffset));
        PyStructSequence_SET_ITEM(tool, 3, PyFloat_FromDouble(t.diameter));
        PyStructSequence_SET_ITEM(tool, 4, PyFloat_FromDouble(t.frontangle));
        PyStructSequence_SET_ITEM(tool, 5, PyFloat_FromDouble(t.backangle));
        PyStructSequence_SET_ITEM(tool, 6, PyInt_FromLong(t.orientation));
#else
        PyStructSequence_SET_ITEM(tool, 0, PyInt_FromLong(t.id));
        PyStructSequence_SET_ITEM(tool, 1, PyFloat_FromDouble(t.length));
        PyStructSequence_SET_ITEM(tool, 2, PyFloat_FromDouble(0.));
        PyStructSequence_SET_ITEM(tool, 3, PyFloat_FromDouble(t.diameter));
        PyStructSequence_SET_ITEM(tool, 4, PyFloat_FromDouble(0.));
        PyStructSequence_SET_ITEM(tool, 5, PyFloat_FromDouble(0.));
        PyStructSequence_SET_ITEM(tool, 6, PyInt_FromLong(0));
#endif
        PyTuple_SetItem(res, j, tool);
        j++;
    }
    _PyTuple_Resize(&res, j);
    return res;
}

// XXX io.tool.toolTable
// XXX EMC_AXIS_STAT motion.axis[]

static PyGetSetDef Stat_getsetlist[] = {
    {"actual_position", (getter)Stat_actual},
    {"ain", (getter)Stat_ain},
    {"aout", (getter)Stat_aout},
    {"axis", (getter)Stat_axis},
    {"din", (getter)Stat_din},
    {"dout", (getter)Stat_dout},
    {"gcodes", (getter)Stat_activegcodes},
    {"homed", (getter)Stat_homed},
    {"limit", (getter)Stat_limit},
    {"mcodes", (getter)Stat_activemcodes},
    {"origin", (getter)Stat_origin},
    {"position", (getter)Stat_position},
    {"joint_position", (getter)Stat_joint_position},
    {"joint_actual_position", (getter)Stat_joint_actual},
    {"probed_position", (getter)Stat_probed},
    {"settings", (getter)Stat_activesettings},
    {"tool_offset", (getter)Stat_tool_offset},
    {"tool_table", (getter)Stat_tool_table},
    {NULL}
};

PyTypeObject Stat_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "emc.stat",             /*tp_name*/
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


static PyObject *optional_stop(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_SET_OPTIONAL_STOP m;

    if(!PyArg_ParseTuple(o, "i", &m.state)) return NULL;
            
    m.serial_number = next_serial(s);
    s->c->write(m);

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
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *feedrate(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_SCALE m;
    if(!PyArg_ParseTuple(o, "d", &m.scale)) return NULL;
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *spindle(pyCommandChannel *s, PyObject *o) {
    int dir;
    if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
    switch(dir) {
        case LOCAL_SPINDLE_FORWARD:
        case LOCAL_SPINDLE_REVERSE:
        {
            EMC_SPINDLE_ON m;
            m.speed = dir;
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_SPINDLE_INCREASE:
        {
            EMC_SPINDLE_INCREASE m;
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_SPINDLE_DECREASE:
        {
            EMC_SPINDLE_DECREASE m;
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_SPINDLE_CONSTANT:
        {
            EMC_SPINDLE_CONSTANT m;
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_SPINDLE_OFF:
        {
            EMC_SPINDLE_OFF m;
            m.serial_number = next_serial(s);
            s->c->write(m);
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
    char *cmd;
    int len;
    if(!PyArg_ParseTuple(o, "s#", &cmd, &len)) return NULL;
    if(len >= 255) {
        PyErr_Format(PyExc_ValueError,"MDI commands limited to 255 characters");
        return NULL;
    }
    EMC_TASK_PLAN_EXECUTE m;
    m.serial_number = next_serial(s);
    strcpy(m.command, cmd);
    s->c->write(m);
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
            PyErr_Format(PyExc_ValueError,"Spindle direction should be STATE_ESTOP, STATE_ESTOP_RESET, STATE_ON, or STATE_OFF");
            return NULL;
    }
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *tool_offset(pyCommandChannel *s, PyObject *o) {
    EMC_TOOL_SET_OFFSET m;
    if(!PyArg_ParseTuple(o, "idd", &m.tool, &m.length, &m.diameter)) return NULL;
    m.serial_number = next_serial(s);
    s->c->write(m);
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
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_MIST_OFF:
        {
            EMC_COOLANT_MIST_OFF m;
            m.serial_number = next_serial(s);
            s->c->write(m);
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
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_FLOOD_OFF:
        {
            EMC_COOLANT_FLOOD_OFF m;
            m.serial_number = next_serial(s);
            s->c->write(m);
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
    if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
    switch(dir) {
        case LOCAL_BRAKE_ENGAGE:
        {
            EMC_SPINDLE_BRAKE_ENGAGE m;
            m.serial_number = next_serial(s);
            s->c->write(m);
        }
            break;
        case LOCAL_BRAKE_RELEASE:
        {
            EMC_SPINDLE_BRAKE_RELEASE m;
            m.serial_number = next_serial(s);
            s->c->write(m);
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
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *emcabort(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_ABORT m;
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *override_limits(pyCommandChannel *s, PyObject *o) {
    EMC_AXIS_OVERRIDE_LIMITS m;
    m.axis = 0; // same number for all
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *home(pyCommandChannel *s, PyObject *o) {
    EMC_AXIS_HOME m;
    if(!PyArg_ParseTuple(o, "i", &m.axis)) return NULL;
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

// jog(JOG_STOP, axis) 
// jog(JOG_CONTINUOUS, axis, speed) 
// jog(JOG_INCREMENT, axis, speed, increment)

static PyObject *jog(pyCommandChannel *s, PyObject *o) {
    int fn;
    int axis;
    double vel, inc;


    if(!PyArg_ParseTuple(o, "ii|dd", &fn, &axis, &vel, &inc)) return NULL;
    if(fn == LOCAL_JOG_STOP) {
        if(PyTuple_Size(o) != 2) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_STOP, ...) takes 2 arguments (%d given)",
                PyTuple_Size(o));
            return NULL;
        }
        EMC_AXIS_ABORT abort;
        abort.axis = axis;
        abort.serial_number = next_serial(s);
        s->c->write(abort);
    } else if(fn == LOCAL_JOG_CONTINUOUS) {
        if(PyTuple_Size(o) != 3) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_CONTINUOUS, ...) takes 3 arguments (%d given)",
                PyTuple_Size(o));
            return NULL;
        }
        EMC_AXIS_JOG cont;
        cont.axis = axis;
        cont.vel = vel;
        cont.serial_number = next_serial(s);
        s->c->write(cont);
    } else if(fn == LOCAL_JOG_INCREMENT) {
        if(PyTuple_Size(o) != 4) {
            PyErr_Format( PyExc_TypeError,
                "jog(JOG_INCREMENT, ...) takes 4 arguments (%d given)",
                PyTuple_Size(o));
            return NULL;
        }

        EMC_AXIS_INCR_JOG incr;
        incr.axis = axis;
        incr.vel = vel;
        incr.incr = inc;
        incr.serial_number = next_serial(s);
        s->c->write(incr);
    } else {
        PyErr_Format( PyExc_TypeError, "jog() first argument must be JOG_xxx");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *reset_interpreter(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_INIT m;
    m.serial_number = next_serial(s);
    s->c->write(m);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *program_open(pyCommandChannel *s, PyObject *o) {
    EMC_TASK_PLAN_OPEN m;
    char *file;
    int len;

    if(!PyArg_ParseTuple(o, "s#", &file, &len)) return NULL;
    m.serial_number = next_serial(s);
    strcpy(m.file, file);
    s->c->write(m);
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
        run.serial_number = next_serial(s);
        s->c->write(run);
    } else {
        PyErr_Clear();
        if(!PyArg_ParseTuple(o, "i", &fn)) return NULL;
        switch(fn) {
        case LOCAL_AUTO_PAUSE:
            pause.serial_number = next_serial(s);
            s->c->write(pause);
            break;
        case LOCAL_AUTO_RESUME:
            resume.serial_number = next_serial(s);
            s->c->write(resume);
            break;
        case LOCAL_AUTO_STEP:
            step.serial_number = next_serial(s);
            s->c->write(step);
            break;
        default:
            PyErr_Format(error, "Unexpected argument '%d' to command.auto", fn);
            return NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *debug(pyCommandChannel *s, PyObject *o) {
    EMC_SET_DEBUG d;

    if(!PyArg_ParseTuple(o, "i", &d.debug)) return NULL;
    d.serial_number = next_serial(s);
    s->c->write(d);
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *teleop(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_TELEOP_ENABLE en;

    if(!PyArg_ParseTuple(o, "i", &en.enable)) return NULL;

    en.serial_number = next_serial(s);
    s->c->write(en);
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *set_traj_mode(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_MODE mo;

    if(!PyArg_ParseTuple(o, "i", &mo.mode)) return NULL;

    mo.serial_number = next_serial(s);
    s->c->write(mo);
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *set_teleop_vector(pyCommandChannel *s, PyObject *o) {
    EMC_TRAJ_SET_TELEOP_VECTOR mo;

    mo.vector.a = mo.vector.b = mo.vector.c = 0.;

    if(!PyArg_ParseTuple(o, "ddd|ddd", &mo.vector.tran.x, &mo.vector.tran.y, &mo.vector.tran.z, &mo.vector.a, &mo.vector.b, &mo.vector.c))
        return NULL;

    mo.serial_number = next_serial(s);
    s->c->write(mo);

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject *wait_complete(pyCommandChannel *s, PyObject *o) {
    return PyInt_FromLong(emcWaitCommandComplete(s->serial, s->s));
}

static PyMemberDef Command_members[] = {
    {"serial", T_INT, offsetof(pyCommandChannel, serial), READONLY},
    {NULL}
};

static PyMethodDef Command_methods[] = {
    {"debug", (PyCFunction)debug, METH_VARARGS},
    {"teleop_enable", (PyCFunction)teleop, METH_VARARGS},
    {"teleop_vector", (PyCFunction)set_teleop_vector, METH_VARARGS},
    {"traj_mode", (PyCFunction)set_traj_mode, METH_VARARGS},
    {"wait_complete", (PyCFunction)wait_complete, METH_NOARGS},
    {"state", (PyCFunction)state, METH_VARARGS},
    {"mdi", (PyCFunction)mdi, METH_VARARGS},
    {"mode", (PyCFunction)mode, METH_VARARGS},
    {"feedrate", (PyCFunction)feedrate, METH_VARARGS},
    {"spindle", (PyCFunction)spindle, METH_VARARGS},
    {"tool_offset", (PyCFunction)tool_offset, METH_VARARGS},
    {"mist", (PyCFunction)mist, METH_VARARGS},
    {"flood", (PyCFunction)flood, METH_VARARGS},
    {"brake", (PyCFunction)brake, METH_VARARGS},
    {"load_tool_table", (PyCFunction)load_tool_table, METH_NOARGS},
    {"abort", (PyCFunction)emcabort, METH_NOARGS},
    {"override_limits", (PyCFunction)override_limits, METH_NOARGS},
    {"home", (PyCFunction)home, METH_VARARGS},
    {"jog", (PyCFunction)jog, METH_VARARGS},
    {"reset_interpreter", (PyCFunction)reset_interpreter, METH_NOARGS},
    {"program_open", (PyCFunction)program_open, METH_VARARGS},
    {"auto", (PyCFunction)emcauto, METH_VARARGS},
    {"set_optional_stop", (PyCFunction)optional_stop, METH_VARARGS},
    {NULL}
};

PyTypeObject Command_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "emc.command",          /*tp_name*/
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
#define _TYPECASE(tag, type, maxlen, f) \
    case tag: { \
        char error_string[maxlen]; \
        strncpy(error_string, ((type*)s->c->get_address())->f, maxlen-1); \
        error_string[maxlen-1] = 0; \
        PyTuple_SET_ITEM(r, 1, PyString_FromString(error_string)); \
        break; \
    }
#define TYPECASE(x, f) _TYPECASE(PASTE(x, _TYPE), x, PASTE(x, _LEN), f)
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
            sprintf(error_string, "unrecognized error %ld", type);
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
    "emc.error_channel",    /*tp_name*/
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
};

#define NUMCOLORS 6

typedef struct {
    PyObject_HEAD
    int npts, mpts, lpts;
    struct logger_point *p;
    struct color colors[NUMCOLORS];
    bool exit, clear, changed;
    double ave_ddt;
    pyStatChannel *st;
} pyPositionLogger;

static const double epsilon = 1e-8;

static inline bool colinear(float xa, float ya, float za, float xb, float yb, float zb, float xc, float yc, float zc) {
    double dx1 = xa-xb, dx2 = xb-xc;
    double dy1 = ya-yb, dy2 = yb-yc;
    double dz1 = za-zb, dz2 = zb-zc;
    double dp = sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1);
    double dq = sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
    if( fabs(dp) < epsilon || fabs(dq) < epsilon ) return true;
    double dot = (dx1*dx2 + dy1*dy2 + dz1*dz2) / dp / dq;
    if( fabs(1-dot) < epsilon) return true;
    return false;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void LOCK() { pthread_mutex_lock(&mutex); }
static void UNLOCK() { pthread_mutex_unlock(&mutex); }

static int Logger_init(pyPositionLogger *self, PyObject *a, PyObject *k) {
    struct color *c = self->colors;
    self->p = (logger_point*)malloc(0);
    self->npts = self->mpts = 0;
    self->exit = self->clear = 0;
    self->changed = 1;
    self->ave_ddt = 0;
    if(!PyArg_ParseTuple(a, "O!(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)",
            &Stat_Type, &self->st,
            &c[0].r,&c[0].g, &c[0].b, &c[0].a,
            &c[1].r,&c[1].g, &c[1].b, &c[1].a,
            &c[2].r,&c[2].g, &c[2].b, &c[2].a,
            &c[3].r,&c[3].g, &c[3].b, &c[3].a,
            &c[4].r,&c[4].g, &c[4].b, &c[4].a,
            &c[5].r,&c[5].g, &c[5].b, &c[5].a
            ))
        return -1;
    return 0;
}

static void Logger_dealloc(pyPositionLogger *s) {
    free(s->p);
    PyObject_Del(s);
}

double gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + 1e-6 * tv.tv_usec;
}

double dt() {
    static double last = gettime();
    double now = gettime();
    double diff = now - last;
    last = now;
    return diff;
}

const double weight_factor = .92;

static double hypot3(double a, double b, double c) {
    return sqrt(a*a + b*b + c*c);
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
            double x, y, z;
            colornum = status->motion.traj.motion_type;
            if(colornum < 0 || colornum > NUMCOLORS) colornum = 0;

            x = status->motion.traj.position.tran.x - status->task.toolOffset.tran.x;
            y = status->motion.traj.position.tran.y - status->task.toolOffset.tran.y;
            z = status->motion.traj.position.tran.z - status->task.toolOffset.tran.z;

            struct color c = s->colors[colornum];
            struct logger_point &op = s->p[s->npts-1];
            struct logger_point &oop = s->p[s->npts-2];
            bool add_point = s->npts < 2 || c != op.c
                || !colinear(x, y, z, op.x, op.y, op.z, oop.x, oop.y, oop.z);

            double delta = dt();
            if(s->npts) {
                if(feq(op.x, x) && feq(op.y, y) && feq(op.z, z)) {
                    s->ave_ddt = 0;
                } else {
                    double dist = hypot3(op.x - x, op.y - y, op.z - z);
                    double ddt = dist / delta;
                    s->ave_ddt = s->ave_ddt*weight_factor
                        + ddt*(1-weight_factor);
                }
            }
            if(add_point) {
                // 1 or 2 points may be added, make room whenever
                // fewer than 2 are left
                bool changed_color = s->npts && c != op.c;
                if(s->npts+2 > s->mpts) {
                    s->mpts = 2 * s->mpts + 2;
                    LOCK();
                    s->changed = 1;
                    s->p = (struct logger_point*)
                        realloc(s->p, sizeof(struct logger_point) * s->mpts);
                    UNLOCK();
                }
                if(changed_color) {
                    {
                    struct logger_point &np = s->p[s->npts];
                    np.x = op.x; np.y = op.y; np.z = op.z;
                    np.c = c;
                    }
                    {
                    struct logger_point &np = s->p[s->npts+1];
                    np.x = x; np.y = y; np.z = z;
                    np.c = c;
                    }
                    s->npts += 2;
                } else {
                    struct logger_point &np = s->p[s->npts];
                    np.x = x; np.y = y; np.z = z;
                    np.c = c;
                    s->npts++;
                }
            } else {
                struct logger_point &np = s->p[s->npts-1];
                np.x = x; np.y = y; np.z = z;
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
        UNLOCK();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *Logger_last(pyPositionLogger *s, PyObject *o) {
    PyObject *result = NULL;
    LOCK();
    if(!s->lpts) {
        Py_INCREF(Py_None);
        result = Py_None;
    } else {
        result = PyTuple_New(3);
        struct logger_point &p = s->p[s->lpts-1];
        PyTuple_SET_ITEM(result, 0, PyFloat_FromDouble(p.x));
        PyTuple_SET_ITEM(result, 1, PyFloat_FromDouble(p.y));
        PyTuple_SET_ITEM(result, 2, PyFloat_FromDouble(p.z));
    }
    UNLOCK();
    return result;
}

static PyMemberDef Logger_members[] = {
    {"average_speed", T_DOUBLE, offsetof(pyPositionLogger, ave_ddt), READONLY},
    {"npts", T_INT, offsetof(pyPositionLogger, npts), READONLY},
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
    {"last", (PyCFunction)Logger_last, METH_NOARGS,
        "Return the most recent point on the plot or None"},
    {NULL, NULL, 0, NULL},
};

static PyTypeObject PositionLoggerType = {
    PyObject_HEAD_INIT(NULL)
    0,                      /*ob_size*/
    "emc.positionlogger",   /*tp_name*/
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
    {NULL}
};

/* ENUM defines an integer constant with the same name as the C constant.
 * ENUMX defines an integer constant with the first i characters of the C
 * constant name removed (so that ENUMX(4,RCS_TASK_MODE_MDI) creates a constant
 * named TASK_MODE_MDI) */

#define ENUM(e) PyModule_AddIntConstant(m, const_cast<char*>(#e), e)
#define ENUMX(x,e) PyModule_AddIntConstant(m, x + const_cast<char*>(#e), e)

PyMODINIT_FUNC
initemc(void) {
    emcInitGlobals();
    verbose_nml_error_messages = 0;
    clear_rcs_print_flag(~0);

    m = Py_InitModule3("emc", emc_methods, "Interface to EMC");

    PyType_Ready(&Stat_Type);
    PyType_Ready(&Command_Type);
    PyType_Ready(&Error_Type);
    PyType_Ready(&Ini_Type);
    PyType_Ready(&PositionLoggerType);
    error = PyErr_NewException("emc.error", PyExc_RuntimeError, NULL);

    PyModule_AddObject(m, "stat", (PyObject*)&Stat_Type);
    PyModule_AddObject(m, "command", (PyObject*)&Command_Type);
    PyModule_AddObject(m, "error_channel", (PyObject*)&Error_Type);
    PyModule_AddObject(m, "ini", (PyObject*)&Ini_Type);
    PyModule_AddObject(m, "error", error);
    PyModule_AddObject(m, "positionlogger", (PyObject*)&PositionLoggerType);

    PyModule_AddStringConstant(m, "nmlfile", DEFAULT_NMLFILE);

    PyModule_AddIntConstant(m, "OPERATOR_ERROR", EMC_OPERATOR_ERROR_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_TEXT", EMC_OPERATOR_TEXT_TYPE);
    PyModule_AddIntConstant(m, "OPERATOR_DISPLAY", EMC_OPERATOR_DISPLAY_TYPE);
    PyModule_AddIntConstant(m, "NML_ERROR", NML_ERROR_TYPE);
    PyModule_AddIntConstant(m, "NML_TEXT", NML_TEXT_TYPE);
    PyModule_AddIntConstant(m, "NML_DISPLAY", NML_DISPLAY_TYPE);

    PyStructSequence_InitType(&ToolResultType, &tool_result_desc);
    PyModule_AddObject(m, "tool", (PyObject*)&ToolResultType);

    ENUMX(4, EMC_AXIS_LINEAR);
    ENUMX(4, EMC_AXIS_ANGULAR);

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

    ENUM(KINEMATICS_IDENTITY);
    ENUM(KINEMATICS_FORWARD_ONLY);
    ENUM(KINEMATICS_INVERSE_ONLY);
    ENUM(KINEMATICS_BOTH);

    pthread_mutex_init(&mutex, NULL);
}


// # vim:sw=4:sts=4:et:ts=8:

