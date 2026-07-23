//
// HAL Python query API
//
// Copyright (c) 2026  B.Stultiens
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
#include <Python.h>
#include <hal.h>

#include "utils/setps_util.h"

static PyObject *value_object(hal_type_t type, hal_refs_u u)
{
    switch(type) {
    case HAL_BOOL: return PyBool_FromLong(hal_get_bool(u.b));
    case HAL_S32:  return PyLong_FromLong(hal_get_si32(u.s));
    case HAL_SINT: return PyLong_FromLongLong(hal_get_sint(u.s));
    case HAL_U32:  return PyLong_FromUnsignedLong(hal_get_ui32(u.u));
    case HAL_UINT: return PyLong_FromUnsignedLongLong(hal_get_uint(u.u));
    case HAL_REAL: return PyFloat_FromDouble(hal_get_real(u.r));
    case HAL_PORT: return PyLong_FromLong(0);  // FIXME
    default: Py_INCREF(Py_None); return Py_None;
    }
}

static PyObject *build_dict_comp(const hal_query_t *q)
{
    return Py_BuildValue("{s:s,s:s,s:i,s:i,s:i,s:p,s:s}",
        "haltype", "component",
        "name",    q->name,
        "type",    q->comp.type,
        "id",      q->comp.comp_id,
        "pid",     q->comp.pid,
        "ready",   (int)q->comp.ready,
        "insmod",  q->comp.insmod);
}

static PyObject *build_dict_pin_param(const hal_query_t *q, int ispin)
{
    return Py_BuildValue("{s:s,s:s,s:i,s:i,s:N,s:s,s:s,s:s,s:i}",
        "haltype", ispin ? "pin" : "param",
        "name",    q->name,
        "type",    (int)q->pp.type,
        "dir",     (int)q->pp.dir,
        "value",   value_object(q->pp.type, q->pp.ref),
        "alias",   q->pp.alias,
        "signal",  q->pp.signal,
        "comp",    q->pp.comp,
        "comp_id", q->pp.comp_id);
}

static PyObject *build_dict_signal(const hal_query_t *q)
{
    return Py_BuildValue("{s:s,s:s,s:i,s:N,s:i,s:i,s:i}",
        "haltype", "signal",
        "name",    q->name,
        "type",    (int)q->sig.type,
        "value",   value_object(q->sig.type, q->sig.ref),
        "writers", q->sig.writers,
        "readers", q->sig.readers,
        "bidirs",  q->sig.bidirs);
}

static PyObject *build_dict_funct(const hal_query_t *q)
{
    return Py_BuildValue("{s:s,s:s,s:s,s:i,s:i,s:p}",
        "haltype",   "function",
        "name",      q->name,
        "comp",      q->funct.comp,
        "comp_id",   q->funct.comp_id,
        "users",     q->funct.users,
        "reentrant", q->funct.reentrant);
}

static PyObject *build_dict_threadfunct(const hal_query_t *q)
{
    return Py_BuildValue("{s:s,s:s,s:i,s:p}",
        "haltype", "threadfunction",
        "name",    q->thread.funct,
        "index",   q->thread.functidx,
        "is_init", (int)q->thread.is_init);
}

static PyObject *build_dict_thread(const hal_query_t *q, PyObject *functs)
{
    return Py_BuildValue("{s:s,s:s,s:s,s:i,s:i,s:l,s:N}",
       "haltype",   "thread",
       "name",      q->name,
       "comp",      q->thread.comp,
       "comp_id",   q->thread.comp_id,
       "priority",  q->thread.priority,
       "period",    q->thread.period,
       "functions", functs);
}

//
// *** Get named component ***
//
static PyObject *get_comp(PyObject *self, PyObject *args)
{
    (void)self;
    PyObject *obj = NULL;
    if(!PyArg_ParseTuple(args, "O", &obj))
        return NULL;

    hal_query_t q = {};
    int rv;
    if(PyUnicode_Check(obj)) {
        rv = hal_comp_by_name(PyUnicode_AsUTF8(obj), &q);
    } else if(PyLong_Check(obj)) {
        rv = hal_comp_by_id((int)PyLong_AsLong(obj), &q);
    } else {
        PyErr_SetString(PyExc_TypeError, "component() expects a string or an integer as argument");
        return NULL;
    }
    switch(rv) {
    case -ENOENT: Py_INCREF(Py_None); return Py_None;
    case 0: return build_dict_comp(&q);
    default:
        PyErr_Format(PyExc_RuntimeError, "hal_comp_by_*: error=%d (%s)", rv, hal_strerror(rv));
        return NULL;
    }
}

//
// *** Get named pin/param ***
//
static PyObject *get_pin_param(PyObject *args, int ispin)
{
    hal_query_t q = {};
    q.qtype = ispin ? HAL_QTYPE_PIN : HAL_QTYPE_PARAM;

    if(!PyArg_ParseTuple(args, "s", &q.name))
        return NULL;

    int rv = hal_getref_p(&q);
    switch(rv) {
    case -ENOENT: Py_INCREF(Py_None); return Py_None;
    case 0: return build_dict_pin_param(&q, ispin);
    default:
        PyErr_Format(PyExc_RuntimeError, "hal_get_p: error=%d (%s)", rv, hal_strerror(rv));
        return NULL;
    }
}

static PyObject *get_pin(PyObject *self, PyObject *args)
{
    (void)self;
    return get_pin_param(args, 1);
}

static PyObject *get_param(PyObject *self, PyObject *args)
{
    (void)self;
    return get_pin_param(args, 0);
}

//
// *** Get named signal ***
//
static PyObject *get_signal(PyObject *self, PyObject *args)
{
    (void)self;
    hal_query_t q = {};

    if(!PyArg_ParseTuple(args, "s", &q.name))
        return NULL;

    int rv = hal_getref_s(&q);
    switch(rv) {
    case -ENOENT: Py_INCREF(Py_None); return Py_None;
    case 0: return build_dict_signal(&q);
    default:
        PyErr_Format(PyExc_RuntimeError, "hal_get_s: error=%d (%s)", rv, hal_strerror(rv));
        return NULL;
    }
}

//
// *** Get named function ***
//
static int get_funct_cb(hal_query_t *q, void *arg)
{
    if(!strcmp(q->name, (const char *)arg))
        return 1;  // Found it,break the loop without error
    return 0;
}

static PyObject *get_funct(PyObject *self, PyObject *args)
{
    (void)self;
    const char *name;
    if(!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    hal_query_t q = {};
    int rv = hal_list_funct(&q, get_funct_cb, (void *)name);
    switch(rv) {
    case 0: Py_INCREF(Py_None); return Py_None;
    case 1: return build_dict_funct(&q);
    default:
        PyErr_Format(PyExc_RuntimeError, "hal_list_funct: error=%d (%s)", rv, hal_strerror(rv));
        return NULL;
    }
}

//
// *** Get named thread ***
//
typedef struct {
    const char *name;
    PyObject *dict;
    PyObject *functs;
    hal_query_t qt;
} threadlist_t;

static int get_thread_cb(hal_query_t *q, void *arg)
{
    threadlist_t *tlp = (threadlist_t *)arg;
    if(!strcmp(tlp->name, q->name)) {
        if(q->qtype == HAL_QTYPE_THREAD) {
            // Thread construct
            tlp->qt = *q;
            tlp->functs = PyList_New(0);
            if(!tlp->functs)
                return -ENOMEM;
        } else if(q->qtype == HAL_QTYPE_THREAD_FUNCT) {
            // Attached function to thread
            PyObject *obj = build_dict_threadfunct(q);
            PyList_Append(tlp->functs, obj);
            Py_DECREF(obj);
        } else {
            return -EINVAL;
        }
    } else if(tlp->dict) {
        // We moved to the next thread name and caught the one we want
        tlp->dict = build_dict_thread(&tlp->qt, tlp->functs);
        tlp->functs = NULL;
        return 1;
    }
    return 0;
}

static PyObject *get_thread(PyObject *self, PyObject *args)
{
    (void)self;
    const char *name;
    if(!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    threadlist_t tl = { .name = name, .dict = NULL, .functs = NULL, .qt = {} };
    int rv = hal_list_thread(&q, get_thread_cb, &tl);
    if(rv >= 0) {
        // Return value can be zero if the last in the list was the one we
        // wanted. Iteration stops by end-of-list and not by a name change.
        if(tl.functs) {
            // List created, but callback terminated without seeing a new
            // thread object. This happens when the found thread is the last in
            // the list. We need to create the thread dict here.
            tl.dict = build_dict_thread(&tl.qt, tl.functs);
        }
        if(!tl.dict) {
            // List not created means name not found
            Py_INCREF(Py_None);
            return Py_None;
        }
        return tl.dict;
    }
    PyErr_Format(PyExc_RuntimeError, "hal_list_thread: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all pins connected to a named signal  ***
//
static int get_signalpins_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_pin_param(q, 1);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_signalpins(PyObject *self, PyObject *args)
{
    (void)self;
    hal_query_t q = {};
    if(!PyArg_ParseTuple(args, "s", &q.name))
        return NULL;

    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_SetString(PyExc_RuntimeError, "get_signalpins: PyDict_New failed");
        return NULL;
    }
    int rv = hal_list_p_s(&q, get_signalpins_cb, (void *)dict);
    if(!rv)
        return dict;
    if(-ENOENT == rv) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    PyErr_Format(PyExc_RuntimeError, "hal_list_p_s: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all pins ***
//
static int get_pins_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_pin_param(q, 1);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_pins(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_SetString(PyExc_RuntimeError, "get_pins: PyDict_New failed");
        return NULL;
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    int rv = hal_list_p(&q, get_pins_cb, (void *)dict);
    if(!rv)
        return dict;
    PyErr_Format(PyExc_RuntimeError, "hal_list_p: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all params ***
//
static int get_params_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_pin_param(q, 0);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_params(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_SetString(PyExc_RuntimeError, "get_params: PyDict_New failed");
        return NULL;
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;
    int rv = hal_list_p(&q, get_params_cb, (void *)dict);
    if(!rv)
        return dict;
    PyErr_Format(PyExc_RuntimeError, "hal_list_p: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all signals ***
//
static int get_signals_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_signal(q);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_signals(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_Format(PyExc_RuntimeError, "get_signals: PyDict_New failed");
        return NULL;
    }
    hal_query_t q = {};
    int rv = hal_list_s(&q, get_signals_cb, (void *)dict);
    if(!rv)
        return dict;
    PyErr_Format(PyExc_RuntimeError, "hal_list_s: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all components ***
//
static int get_comps_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_comp(q);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_comps(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_Format(PyExc_RuntimeError, "get_comps: PyDict_New failed");
        return NULL;
    }
    hal_query_t q = {};
    int rv = hal_list_comp(&q, get_comps_cb, (void *)dict);
    if(!rv)
        return dict;
    PyErr_Format(PyExc_RuntimeError, "hal_list_comp: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all functions ***
//
static int get_functs_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = build_dict_funct(q);
    PyObject *str = PyUnicode_FromString(q->name);
    PyDict_SetItem((PyObject *)arg, str, obj);
    Py_DECREF(str);
    Py_DECREF(obj);
    return 0;
}

static PyObject *get_functs(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    PyObject *dict = PyDict_New();
    if(!dict) {
        PyErr_Format(PyExc_RuntimeError, "get_functs: PyDict_New failed");
        return NULL;
    }
    hal_query_t q = {};
    int rv = hal_list_funct(&q, get_functs_cb, (void *)dict);
    if(!rv)
        return dict;
    PyErr_Format(PyExc_RuntimeError, "hal_list_funct: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

//
// *** Get all threads ***
//
typedef struct {
    hal_query_t qt;
    PyObject *threads;
    PyObject *functs;
} threadscb_t;
static int get_threads_cb(hal_query_t *q, void *arg)
{
    threadscb_t *tcb = (threadscb_t *)arg;

    if(HAL_QTYPE_THREAD == q->qtype) {
        if(tcb->functs) {
            // Seen functions from previous thread
            PyObject *obj = build_dict_thread(&tcb->qt, tcb->functs);
            PyObject *str = PyUnicode_FromString(tcb->qt.name);
            PyDict_SetItem(tcb->threads, str, obj);
            Py_DECREF(str);
            Py_DECREF(obj);
            tcb->functs = NULL;
        }
        tcb->qt = *q; // Save the current thread info
        // Create a function dictionary
        if(!(tcb->functs = PyList_New(0)))
            return -ENOMEM;
    } else if(HAL_QTYPE_THREAD_FUNCT == q->qtype) {
        PyObject *obj = build_dict_threadfunct(q);
        PyList_Append(tcb->functs, obj);
        Py_DECREF(obj);
    } else {
        return -EINVAL;
    }
    return 0;
}

static PyObject *get_threads(PyObject *self, PyObject *args)
{
    (void)self;
    (void)args;
    threadscb_t tc = {};
    tc.threads = PyDict_New();
    if(!tc.threads) {
        PyErr_Format(PyExc_RuntimeError, "get_threads: PyDict_New failed");
        return NULL;
    }

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    int rv = hal_list_thread(&q, get_threads_cb, &tc);
    if(rv >= 0) {
        // Return value can be zero if the last in the list was the one we
        // wanted. Iteration stops by end-of-list and not by a name change.
        if(tc.functs) {
            PyObject *obj = build_dict_thread(&tc.qt, tc.functs);
            PyObject *str = PyUnicode_FromString(tc.qt.name);
            PyDict_SetItem(tc.threads, str, obj);
            Py_DECREF(str);
            Py_DECREF(obj);
        }
        return tc.threads;
    }
    PyErr_Format(PyExc_RuntimeError, "hal_list_thread: error=%d (%s)", rv, hal_strerror(rv));
    return NULL;
}

static PyMethodDef halquery_module_methods[] = {
    {"pin",     get_pin,    METH_VARARGS, "pin(name:string)"},
    {"param",   get_param,  METH_VARARGS, "param(name:string)"},
    {"signal",  get_signal, METH_VARARGS, "signal(name:string)"},
    {"comp",    get_comp,   METH_VARARGS, "comp(name:string|id:int)"},
    {"funct",   get_funct,  METH_VARARGS, "funct(name:string)"},
    {"thread",  get_thread, METH_VARARGS, "thread(name:string)"},

    {"signalpins",  get_signalpins, METH_VARARGS, "signalpins(name:string)"},

    {"pins",    get_pins,    METH_NOARGS, "pins()"},
    {"params",  get_params,  METH_NOARGS, "params()"},
    {"signals", get_signals, METH_NOARGS, "signals()"},
    {"comps",   get_comps,   METH_NOARGS, "comps()"},
    {"functs",  get_functs,  METH_NOARGS, "functs()"},
    {"threads", get_threads, METH_NOARGS, "threads()"},
    {},
};

static const char halquery_module_doc[] =
    "Query interface to LinuxCNC's HAL internals\n"
    "\n"
    "This module allows you to retrieve information about all HAL\n"
    "internal constructs, such as:\n"
    " - pins\n"
    " - parameters\n"
    " - signals\n"
    " - components\n"
    " - functions\n"
    " - threads\n"
    "\n"
    "Typical usage:\n"
    " import halquery\n"
    " # print info on one component\n"
    " print(\"Component xyz:\", halquery.component(\"xyz\"))\n"
    "\n"
    " # print info on all signals\n"
    " print(\"Signals:\")\n"
    " print(halquery.signals())\n"
    ;


static int halquery_module_exec(PyObject *m)
{
    (void)m;
    static int hq_init = 0;
    if(hq_init) {
        PyErr_SetString(PyExc_ImportError, "Cannot initialize halquery module more than once");
        return -1;
    }
    hq_init = 1;

    int rv;
    if(0 != (rv = hal_lib_init())) {
        PyErr_Format(PyExc_ImportError, "Initializing hal_lib returned error=%d", rv);
        return -1;
    }
    return 0;
}

static PyModuleDef_Slot halquery_module_slots[] = {
    {Py_mod_exec, (void *)halquery_module_exec},
    {0, NULL}
};

static struct PyModuleDef halquery_moduledef = {
    .m_base    = PyModuleDef_HEAD_INIT,
    .m_name    = "halquery",
    .m_doc     = halquery_module_doc,
    .m_methods = halquery_module_methods,
    .m_slots   = halquery_module_slots,
};

PyMODINIT_FUNC PyInit_halquery(void)
{
    return PyModuleDef_Init(&halquery_moduledef);
}
// vim: ts=4 sw=4 et
