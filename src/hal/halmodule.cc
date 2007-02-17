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
#include <string>
#include <map>
using namespace std;

#include "rtapi.h"
#include "hal.h"
#include "hal_priv.h"

#include "emc.hh"

typedef unsigned long Py_ssize_t;

union paramunion {
    hal_bit_t b;
    hal_u32_t u32;
    hal_s32_t s32;
    hal_float_t f;
};

union pinunion {
    void *v;
    hal_bit_t *b;
    hal_u32_t *u32;
    hal_s32_t *s32;
    hal_float_t *f;
};

union halunion {
    union pinunion pin;
    union paramunion param;
};

union haldirunion {
    hal_pin_dir_t pindir;
    hal_param_dir_t paramdir;
};

struct halitem {
    bool is_pin;
    hal_type_t type;
    union haldirunion dir;
    union halunion *u; 
};

typedef std::map<std::string, struct halitem> itemmap;

typedef struct halobject {
        PyObject_HEAD
    int hal_id;
    char *name;
    char *prefix;
    itemmap *items;
} halobject;

PyObject *pyhal_error_type = NULL;

static PyObject *pyrtapi_error(int code) {
    switch(code) {
	case RTAPI_SUCCESS:
            PyErr_SetString(pyhal_error_type, "Call Successful"); break;
        case RTAPI_UNSUP:
            PyErr_SetString(pyhal_error_type, "Function not supported"); break;
        case RTAPI_BADID:
            PyErr_SetString(pyhal_error_type, "Bad Identifier"); break;
        case RTAPI_INVAL:
            PyErr_SetString(pyhal_error_type, "Invalid argument"); break;
        case RTAPI_NOMEM:
            PyErr_SetString(pyhal_error_type, "Not enough memory"); break;
        case RTAPI_LIMIT:
            PyErr_SetString(pyhal_error_type, "Resource limit reached"); break;
        case RTAPI_PERM:
            PyErr_SetString(pyhal_error_type, "Permission denied"); break;
        case RTAPI_BUSY:
            PyErr_SetString(pyhal_error_type, "Resource is busy or locked"); break;
        case RTAPI_FAIL:
            PyErr_SetString(pyhal_error_type, "Operation failed"); break;
        default:
            PyErr_Format(pyhal_error_type, "Unknown RTAPI error code %d", code);
    }
    return NULL;
}

static PyObject *pyhal_error(int code) {
    switch(code) {
        case HAL_SUCCESS:
            PyErr_SetString(pyhal_error_type, "Call Successful"); break;
        case HAL_UNSUP:
            PyErr_SetString(pyhal_error_type, "Function not supported"); break;
        case HAL_BADVAR:
            PyErr_SetString(pyhal_error_type, "Duplicate or not-found variable name"); break;
        case HAL_INVAL:
            PyErr_SetString(pyhal_error_type, "Invalid argument"); break;
        case HAL_NOMEM:
            PyErr_SetString(pyhal_error_type, "Not enough memory"); break;
        case HAL_LIMIT:
            PyErr_SetString(pyhal_error_type, "Resource limit reached"); break;
        case HAL_PERM:
            PyErr_SetString(pyhal_error_type, "Permission Denied"); break;
        case HAL_BUSY:
            PyErr_SetString(pyhal_error_type, "Resources is busy or locked");
            break;
        case HAL_NOTFND:
            PyErr_SetString(pyhal_error_type, "Object not found"); break;
        case HAL_FAIL:
            PyErr_SetString(pyhal_error_type, "Operation failed"); break;
        default:
            PyErr_Format(pyhal_error_type, "Unknown HAL error code %d", code);
            break;
    }
    return NULL;
}

static int pyhal_init(halobject *self, PyObject *args, PyObject *kw) {
    char *name;
    char *prefix = 0;

    if(!PyArg_ParseTuple(args, "s|s:hal.component", &name, &prefix)) return -1;

    self->items = new itemmap();

    self->hal_id = hal_init(name);
    if(self->hal_id <= 0) {
        pyhal_error(self->hal_id);
        return -1;
    }
    self->name = strdup(name);
    self->prefix = strdup(prefix ? prefix : name);
    if(!name) {
        PyErr_SetString(PyExc_MemoryError, "strdup(name) failed");
        return -1;
    }

    return 0;
}

static void pyhal_delete(halobject *self) {
    if(self->hal_id > 0) 
        hal_exit(self->hal_id);

    if(self->name)
        free(self->name);

    if(self->prefix)
        free(self->prefix);

    if(self->items)
        delete self->items;

    PyObject_Del(self);
}

#define MAX_PIN_SIZE sizeof(hal_float_t)

static int pyhal_write_common(halitem *pin, PyObject *value) {
    int is_int = PyInt_Check(value);
    int intval = is_int ? PyInt_AsLong(value) : -1;
    unsigned int uintval;
    if(!pin) return -1;

    if(pin->is_pin) {
        switch(pin->type) {
            case HAL_BIT:
                *pin->u->pin.b = PyObject_IsTrue(value);
                break;
            case HAL_FLOAT:
                if(PyFloat_Check(value)) 
                    *pin->u->pin.f = PyFloat_AsDouble(value);
                else if(is_int)
                    *pin->u->pin.f = intval;
                else {
                    PyErr_Format(PyExc_TypeError,
                            "Integer or float expected, not %s",
                            value->ob_type->tp_name);
                    return -1;
                }
                break;
            case HAL_U32:
                if(is_int) {
                    if(intval < 0) goto rangeerr;
                    *pin->u->pin.u32 = intval;
                    break;
                }
                if(!PyLong_Check(value)) {
                    PyErr_Format(PyExc_TypeError,
                            "Integer or long expected, not %s",
                            value->ob_type->tp_name);
                    return -1;
                }
                uintval = PyLong_AsUnsignedLong(value);
                if(PyErr_Occurred()) return -1;
                *pin->u->pin.u32 = uintval;
                break;
            case HAL_S32:
                if(!is_int) goto typeerr;
                *pin->u->pin.s32 = intval;
                break;
            default:
                PyErr_Format(pyhal_error_type, "Invalid pin type %d", pin->type);
        }
    } else {
        switch(pin->type) {
            case HAL_BIT:
                pin->u->param.b = PyObject_IsTrue(value);
                break;
            case HAL_FLOAT:
                if(PyFloat_Check(value)) 
                    pin->u->param.f = PyFloat_AsDouble(value);
                else if(is_int)
                    pin->u->param.f = intval;
                else {
                    PyErr_Format(PyExc_TypeError,
                            "Integer or float expected, not %s",
                            value->ob_type->tp_name);
                    return -1;
                }
                break;
            case HAL_U32:
                if(is_int) {
                    if(intval < 0) goto rangeerr;
                    pin->u->param.u32 = intval;
                    break;
                }
                if(!PyLong_Check(value)) {
                    PyErr_Format(PyExc_TypeError,
                            "Integer or long expected, not %s",
                            value->ob_type->tp_name);
                    return -1;
                }
                uintval = PyLong_AsUnsignedLong(value);
                if(PyErr_Occurred()) return -1;
                pin->u->param.u32 = uintval;
                break;
            case HAL_S32:
                if(!is_int) goto typeerr;
                pin->u->param.s32 = intval;
                break;
            default:
                PyErr_Format(pyhal_error_type, "Invalid pin type %d", pin->type);
        }
    }
    return 0;
typeerr:
    PyErr_Format(PyExc_TypeError, "Integer expected, not %s",
            value->ob_type->tp_name);
    return -1;
rangeerr:
    PyErr_Format(PyExc_OverflowError, "Value %d out of range for pin", intval);
    return -1;
}

static PyObject *pyhal_read_common(halitem *item) {
    if(!item) return NULL;
    if(item->is_pin) {
        switch(item->type) {
            case HAL_BIT: return PyBool_FromLong(*(item->u->pin.b));
            case HAL_U32: return PyLong_FromUnsignedLong(*(item->u->pin.u32));
            case HAL_S32: return PyInt_FromLong(*(item->u->pin.s32));
            case HAL_FLOAT: return PyFloat_FromDouble(*(item->u->pin.f));
        }
    } else {
        switch(item->type) {
            case HAL_BIT: return PyBool_FromLong(item->u->param.b);
            case HAL_U32: return PyLong_FromUnsignedLong(item->u->param.u32);
            case HAL_S32: return PyInt_FromLong(item->u->param.s32);
            case HAL_FLOAT: return PyFloat_FromDouble(item->u->param.f);
        }
    }
    PyErr_Format(pyhal_error_type, "Invalid item type %d", item->type);
    return NULL;
}

static halitem *find_item(halobject *self, char *name) {
    if(!name) return NULL;

    itemmap::iterator i = self->items->find(name);

    if(i == self->items->end()) {
        PyErr_Format(PyExc_AttributeError, "Pin '%s' does not exist", name);
        return NULL;
    }
    
    return &(i->second);
}

static PyObject * pyhal_create_param(halobject *self, char *name, hal_type_t type, hal_param_dir_t dir) {
    char param_name[HAL_NAME_LEN];
    int res;
    halitem param;
    param.is_pin = 0;

    if(type < HAL_BIT || type > HAL_U32) {
        PyErr_Format(pyhal_error_type, "Invalid param type %d", type);
        return NULL;
    }
    
    param.type = type;
    param.dir.paramdir = dir;
    param.u = (halunion*)hal_malloc(sizeof(halunion));
    if(!param.u) {
        PyErr_SetString(PyExc_MemoryError, "hal_malloc failed");
        return NULL;
    }

    snprintf(param_name, HAL_NAME_LEN, "%s.%s", self->prefix, name);
    res = hal_param_new(param_name, type, dir, (void*)param.u, self->hal_id);
    if(res) return pyhal_error(res);

    (*self->items)[name] = param;

    Py_RETURN_NONE;
}


static PyObject * pyhal_create_pin(halobject *self, char *name, hal_type_t type, hal_pin_dir_t dir) {
    char pin_name[HAL_NAME_LEN];
    int res;
    halitem pin;
    pin.is_pin = 1;

    if(type < HAL_BIT || type > HAL_U32) {
        PyErr_Format(pyhal_error_type, "Invalid pin type %d", type);
        return NULL;
    }

    pin.type = type;
    pin.dir.pindir = dir;
    pin.u = (halunion*)hal_malloc(sizeof(halunion));
    if(!pin.u) {
        PyErr_SetString(PyExc_MemoryError, "hal_malloc failed");
        return NULL;
    }

    snprintf(pin_name, HAL_NAME_LEN, "%s.%s", self->prefix, name);
    res = hal_pin_new(pin_name, type, dir, (void**)pin.u, self->hal_id);
    if(res) return pyhal_error(res);

    (*self->items)[name] = pin;

    Py_RETURN_NONE;
}

static PyObject *pyhal_new_param(halobject *self, PyObject *o) {
    char *name;
    int type, dir;

    if(!PyArg_ParseTuple(o, "sii", &name, &type, &dir)) 
        return NULL;

    if (find_item(self, name)) {
        PyErr_Format(PyExc_ValueError, "Duplicate item name '%s'", name);
        return NULL;
    } else { PyErr_Clear(); }
    return pyhal_create_param(self, name, (hal_type_t)type, (hal_param_dir_t)dir);
}


static PyObject *pyhal_new_pin(halobject *self, PyObject *o) {
    char *name;
    int type, dir;

    if(!PyArg_ParseTuple(o, "sii", &name, &type, &dir)) 
        return NULL;

    if (find_item(self, name)) {
        PyErr_Format(PyExc_ValueError, "Duplicate item name '%s'", name);
        return NULL;
    } else { PyErr_Clear(); }
    return pyhal_create_pin(self, name, (hal_type_t)type, (hal_pin_dir_t)dir);
}

static PyObject *pyhal_ready(halobject *self, PyObject *o) {
    // hal_ready did not exist in EMC 2.0.x, make it a no-op
    int res = hal_ready(self->hal_id);
    if(res) return pyhal_error(res);
    Py_RETURN_NONE;
}

static PyObject *pyhal_exit(halobject *self, PyObject *o) {
    if(self->hal_id > 0) 
        hal_exit(self->hal_id);
    self->hal_id = 0;
    Py_RETURN_NONE;
}

static PyObject *pyhal_repr(halobject *self) {
    return PyString_FromFormat("<hal component %s(%d) with %d pins and params>",
            self->name, self->hal_id, (int)self->items->size());
}

static PyObject *pyhal_getattro(halobject *self, PyObject *attro)  {
    PyObject *result;

    result = PyObject_GenericGetAttr((PyObject*)self, attro);
    if(result) return result;

    PyErr_Clear();
    return pyhal_read_common(find_item(self, PyString_AsString(attro)));
}

static int pyhal_setattro(halobject *self, PyObject *attro, PyObject *v) {
    return pyhal_write_common(find_item(self, PyString_AsString(attro)), v);
}

static int pyhal_len(halobject *self) {
    return self->items->size();
}

static PyObject *pyhal_get_prefix(halobject *self, PyObject *args) {
    if(!PyArg_ParseTuple(args, "")) return NULL;

    if(!self->prefix)
	Py_RETURN_NONE;

    return PyString_FromString(self->prefix);
}


static PyObject *pyhal_set_prefix(halobject *self, PyObject *args) {
    char *newprefix;
    if(!PyArg_ParseTuple(args, "s", &newprefix)) return NULL;

    if(self->prefix)
        free(self->prefix);
    self->prefix = strdup(newprefix);

    if(!self->prefix) {
        PyErr_SetString(PyExc_MemoryError, "strdup(prefix) failed");
    }

    Py_RETURN_NONE;
}

static PyMethodDef hal_methods[] = {
    {"setprefix", (PyCFunction)pyhal_set_prefix, METH_VARARGS,
        "Set the prefix for newly created pins and parameters"},
    {"getprefix", (PyCFunction)pyhal_get_prefix, METH_VARARGS,
        "Get the prefix for newly created pins and parameters"},
    {"newparam", (PyCFunction)pyhal_new_param, METH_VARARGS,
        "Create a new parameter"},
    {"newpin", (PyCFunction)pyhal_new_pin, METH_VARARGS,
        "Create a new pin"},
    {"exit", (PyCFunction)pyhal_exit, METH_NOARGS,
        "Call hal_exit"},
    {"ready", (PyCFunction)pyhal_ready, METH_NOARGS,
        "Call hal_ready"},
    {NULL},
};

static PyMappingMethods halobject_map = {
    (inquiry)pyhal_len,
    (binaryfunc)pyhal_getattro,
    (objobjargproc)pyhal_setattro
};

static 
PyTypeObject halobject_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "hal.component",           /*tp_name*/
    sizeof(halobject),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) pyhal_delete, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc) pyhal_repr,     /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    &halobject_map,            /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    (getattrofunc)pyhal_getattro,/*tp_getattro*/
    (setattrofunc)pyhal_setattro,/*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "HAL Component",           /*tp_doc*/
    0,                         /*tp_traverse*/
    0,                         /*tp_clear*/
    0,                         /*tp_richcompare*/
    0,                         /*tp_weaklistoffset*/
    0,                         /*tp_iter*/
    0,                         /*tp_iternext*/
    hal_methods,               /*tp_methods*/
    0,                         /*tp_members*/
    0,                         /*tp_getset*/
    0,                         /*tp_base*/
    0,                         /*tp_dict*/
    0,                         /*tp_descr_get*/
    0,                         /*tp_descr_set*/
    0,                         /*tp_dictoffset*/
    (initproc)pyhal_init,      /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
};

PyObject *pin_has_writer(PyObject *self, PyObject *args) {
    char *name;
    if(!PyArg_ParseTuple(args, "s", &name)) return NULL;
    if(!SHMPTR(0)) {
	PyErr_Format(PyExc_RuntimeError,
		"Cannot call before creating component");
	return NULL;
    }

    hal_pin_t *pin = halpr_find_pin_by_name(name);
    if(!pin) {
	PyErr_Format(PyExc_NameError, "Pin `%s' does not exist", name);
	return NULL;
    }

    if(pin->signal) {
	hal_sig_t *signal = (hal_sig_t*)SHMPTR(pin->signal);
	return PyBool_FromLong(signal->writers > 0);
    }
    Py_INCREF(Py_False);
    return Py_False;
}

typedef struct shmobject {
    PyObject_HEAD
    halobject *comp;
    int key;
    int shm_id;
    unsigned long size;
    void *buf;
};

static int pyshm_init(shmobject *self, PyObject *args, PyObject *kw) {
    self->comp = 0;
    self->shm_id = -1;

    if(!PyArg_ParseTuple(args, "O!ik",
		&halobject_type, &self->comp, &self->key, &self->size))
	return -1;

    self->shm_id = rtapi_shmem_new(self->key, self->comp->hal_id, self->size);
    if(self->shm_id < 0) {
	self->comp = 0;
	self->size = 0;
	pyrtapi_error(self->shm_id);
	return -1;
    }

    rtapi_shmem_getptr(self->shm_id, &self->buf);
    Py_INCREF(self->comp);

    return 0;
}

static void pyshm_delete(shmobject *self) {
    if(self->comp && self->shm_id > 0)
	rtapi_shmem_delete(self->shm_id, self->comp->hal_id);
    Py_XDECREF(self->comp);
}

static int shm_buffer(shmobject *self, Py_ssize_t segment, void **ptrptr){
    if(ptrptr) *ptrptr = self->buf;
    return self->size;
}
static int shm_segcount(shmobject *self, Py_ssize_t *lenp) {
    if(lenp) *lenp = self->size;
    return 1;
}

static PyObject *pyshm_repr(shmobject *self) {
    return PyString_FromFormat("<shared memory buffer key=%08x id=%d size=%ld>",
	    self->key, self->shm_id, (unsigned long)self->size);
}

static PyObject *shm_setsize(shmobject *self, PyObject *args) {
    if(!PyArg_ParseTuple(args, "k", &self->size)) return NULL;
    Py_RETURN_NONE;
}

static PyObject *shm_getbuffer(shmobject *self) {
    return (PyObject*)PyBuffer_FromReadWriteObject((PyObject*)self, 0, self->size);
}

static
PyBufferProcs shmbuffer_procs = {
    (getreadbufferproc)shm_buffer,
    (getwritebufferproc)shm_buffer,
    (getsegcountproc)shm_segcount,
    (getcharbufferproc)NULL
};

static PyMethodDef shm_methods[] = {
    {"getbuffer", (PyCFunction) shm_getbuffer, METH_NOARGS, 
	"Get a writable buffer object for the shared memory segment"},
    {"setsize", (PyCFunction) shm_setsize, METH_VARARGS, 
	"Set the size of the shared memory segment"},
    {NULL},
};

static 
PyTypeObject shm_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "hal.shm",                 /*tp_name*/
    sizeof(shmobject),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) pyshm_delete, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc) pyshm_repr,     /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    &shmbuffer_procs,          /*tp_as_buffer*/
    // Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GETCHARBUFFER,        /*tp_flags*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "HAL Shared Memory",       /*tp_doc*/
    0,                         /*tp_traverse*/
    0,                         /*tp_clear*/
    0,                         /*tp_richcompare*/
    0,                         /*tp_weaklistoffset*/
    0,                         /*tp_iter*/
    0,                         /*tp_iternext*/
    shm_methods,               /*tp_methods*/
    0,                         /*tp_members*/
    0,                         /*tp_getset*/
    0,                         /*tp_base*/
    0,                         /*tp_dict*/
    0,                         /*tp_descr_get*/
    0,                         /*tp_descr_set*/
    0,                         /*tp_dictoffset*/
    (initproc)pyshm_init,      /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
};


PyMethodDef module_methods[] = {
    {"pin_has_writer", (PyCFunction)pin_has_writer, METH_VARARGS,
	"Return a FALSE value if a pin has no writers and TRUE if it does"},
    {NULL},
};

char *module_doc = "Interface to emc2's hal\n"
"\n"
"This module allows the creation of userspace HAL components in Python.\n"
"This includes pins and parameters of the various HAL types.\n"
"\n"
"Typical usage:\n"
"\n"
"import hal, time\n"
"h = hal.component(\"component-name\")\n"
"# create pins and parameters with calls to h.newpin and h.newparam\n"
"h.newpin(\"in\", hal.HAL_FLOAT, hal.HAL_IN)\n"
"h.newpin(\"out\", hal.HAL_FLOAT, hal.HAL_OUT)\n"
"h.ready() # mark the component as 'ready'\n"
"\n"
"try:\n"
"    while 1:\n"
"        # act on changed input pins; update values on output pins\n"
"        time.sleep(1)\n"
"        h['out'] = h['in']\n"
"except KeyboardInterrupt: pass"
"\n"
"\n"
"When the component is requested to exit with 'halcmd unload', a\n"
"KeyboardInterrupt exception will be raised."
;

extern "C"
void inithal(void) {
    PyObject *m = Py_InitModule3("hal", module_methods,
            module_doc);

    pyhal_error_type = PyErr_NewException("hal.error", NULL, NULL);
    PyModule_AddObject(m, "error", pyhal_error_type);

    PyType_Ready(&halobject_type);
    PyType_Ready(&shm_type);
    PyModule_AddObject(m, "component", (PyObject*)&halobject_type);
    PyModule_AddObject(m, "shm", (PyObject*)&shm_type);

    PyModule_AddIntConstant(m, "HAL_BIT", HAL_BIT);
    PyModule_AddIntConstant(m, "HAL_FLOAT", HAL_FLOAT);
    PyModule_AddIntConstant(m, "HAL_S32", HAL_S32);
    PyModule_AddIntConstant(m, "HAL_U32", HAL_U32);

    PyModule_AddIntConstant(m, "HAL_RO", HAL_RO);
    PyModule_AddIntConstant(m, "HAL_RW", HAL_RW);
    PyModule_AddIntConstant(m, "HAL_IN", HAL_IN);
    PyModule_AddIntConstant(m, "HAL_OUT", HAL_OUT);
    PyModule_AddIntConstant(m, "HAL_IO", HAL_IO);

    PyRun_SimpleString(
            "(lambda s=__import__('signal'):"
                 "s.signal(s.SIGTERM, s.default_int_handler))()");
}

