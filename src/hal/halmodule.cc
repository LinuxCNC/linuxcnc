//    This is a component of AXIS, a front-end for emc
//    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
//    Chris Radek <chris@timeguy.com>
//    Copyright 2026  B.Stultiens
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

#include <Python.h>
#include <structmember.h>
#include <string>
#include <map>
#include <vector>

#include <rtapi.h>
#include <rtapi_mutex.h>
#include <hal.h>
#include "utils/setps_util.h"

#define EXCEPTION_IF_NOT_LIVE(retval) do { \
        if(self->hal_id <= 0) { \
            PyErr_SetString(PyExc_RuntimeError, "Invalid operation on closed HAL component"); \
            return retval; \
        } \
    } while(0)

// Use stdint interfaces when we have them
#if PY_VERSION_HEX >= 0x030e00f0  // 3.14
#define PyLong_FromRtapiS32 PyLong_FromInt32
#define PyLong_FromRtapiS64 PyLong_FromInt64
#define PyLong_FromRtapiU32 PyLong_FromUInt32
#define PyLong_FromRtapiU64 PyLong_FromUInt64
#else
#define PyLong_FromRtapiS32 PyLong_FromLong
#define PyLong_FromRtapiS64 PyLong_FromLongLong
#define PyLong_FromRtapiU32 PyLong_FromUnsignedLong
#define PyLong_FromRtapiU64 PyLong_FromUnsignedLongLong
#endif

//
// Thread-safe locale switcher for LC_NUMERIC to "C" that can be employed
// within a scoped block and automatically reverts to the previous locale when
// the class instance is destructed.
//
struct scoped_lc_numeric_c {
    scoped_lc_numeric_c() {
        // Make sure we always use the C locale for conversion (thread local)
        // cppcheck-suppress useInitializationList
        oldlc = uselocale(static_cast<locale_t>(NULL));
        newlc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(NULL));
        if(static_cast<locale_t>(NULL) == newlc) {
            // FIXME: This is not nice, to print directly to stderr...
            fprintf(stderr, "halmodule: internal error: Cannot set locale to \"C\" for numeric conversions");
            return;
        }
        uselocale(newlc);
    }

    ~scoped_lc_numeric_c() {
        if(static_cast<locale_t>(NULL) != newlc) {
            uselocale(oldlc);
            freelocale(newlc);
        }
    }
private:
    locale_t oldlc;
    locale_t newlc;
};

PyObject *to_python(bool b) {
    return PyBool_FromLong(b);
}

PyObject *to_python(rtapi_u32 u) {
    return PyLong_FromRtapiU32(u);
}

PyObject *to_python(rtapi_s32 i) {
    return PyLong_FromRtapiS32(i);
}

PyObject *to_python(rtapi_uint u) {
    return PyLong_FromRtapiU64(u);
}

PyObject *to_python(rtapi_sint i) {
    return PyLong_FromRtapiS64(i);
}

PyObject *to_python(rtapi_real d) {
    return PyFloat_FromDouble(d);
}

bool from_python(PyObject *o, bool *b)
{
    // If it is a bool (True, False), we're fine
    if(PyBool_Check(o)) {
        *b = PyObject_IsTrue(o);
        return true;
    }
    // Maybe a textual description of a bool
    if(PyUnicode_Check(o)) {
        // Note that in python: bool("False") == True
        // That means we cannot use the standard conversion if it is expected
        // to work more natural and aligned with the ini-file format.
        const char *cptr = PyUnicode_AsUTF8AndSize(o, NULL);
        if (!cptr) {
            PyErr_Format(PyExc_RuntimeError, "Invalid UTF-8 detected");
            return false;
        }
        static const struct {
            const char *name;
            bool value;
        } boolnames[] = {
            // Both "1" and "0" could be caught with PyNumber_Long, but we're
            // already here and it is easier this way. These are in the order
            // in which you are most likely to see the value written.
            { "1",     true  },
            { "0",     false },
            { "true",  true  },
            { "false", false },
            { "on",    true  },
            { "off",   false },
            { "yes",   true  },
            { "no",    false },
            { NULL,    false } // Termination
        };
        for(int i = 0; boolnames[i].name; i++) {
            if (!strcasecmp(cptr, boolnames[i].name)) {
                *b = boolnames[i].value;
                return true;
            }
        }
    }

    if(PyFloat_Check(o)) {
        // Floating point is false *only* when it is 0.0
        double v = PyFloat_AsDouble(o);
        *b = 0.0 == v;
        return true;
    }

    // Try the usual int(obj) conversion
    PyObject *tmp = NULL;
    long long l;
    tmp = PyLong_Check(o) ? o : PyNumber_Long(o);
    if(!tmp) goto fail;

    l = PyLong_AsLongLong(tmp);
    if(-1 == l && PyErr_Occurred())
        goto fail;

    *b = l != 0;
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return true;
fail:
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return false;
}

bool from_python(PyObject *o, rtapi_real *d) {
    if(PyFloat_Check(o)) {
        *d = PyFloat_AsDouble(o);
        return true;
    } else if(PyLong_Check(o)) {
        *d = PyLong_AsDouble(o);
        return !PyErr_Occurred();
    }

    // Ensure that float conversions are using decimal '.'
    scoped_lc_numeric_c force_lc_numeric_c;

    PyObject *tmp = PyNumber_Float(o);
    if(!tmp) {
        PyErr_Format(PyExc_TypeError, "Number expected, not %s", Py_TYPE(o)->tp_name);
        return false;
    }

    *d = PyFloat_AsDouble(tmp);
    Py_XDECREF(tmp);
    return true;
}

bool from_python(PyObject *o, rtapi_u32 *u) {
    PyObject *tmp = NULL;
    long long l;
    tmp = PyLong_Check(o) ? o : PyNumber_Long(o);
    if(!tmp) goto fail;

    l = PyLong_AsLongLong(tmp);
    if(-1 == l && PyErr_Occurred()) goto fail;
    if(l < 0 || l > RTAPI_UINT32_MAX) {
        PyErr_Format(PyExc_OverflowError, "Value %lld out of range", l);
        goto fail;
    }

    *u = l;
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return true;
fail:
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return false;
}

bool from_python(PyObject *o, rtapi_s32 *i) {
    PyObject *tmp = NULL;
    long long l;
    tmp = PyLong_Check(o) ? o : PyNumber_Long(o);
    if(!tmp) goto fail;

    l = PyLong_AsLongLong(tmp);
    if(-1 == l && PyErr_Occurred()) goto fail;
    if(l < RTAPI_INT32_MIN || l > RTAPI_INT32_MAX) {
        PyErr_Format(PyExc_OverflowError, "Value %lld out of range", l);
        goto fail;
    }

    *i = l;
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return true;
fail:
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return false;
}

bool from_python(PyObject *o, rtapi_uint *u) {
    PyObject *tmp = NULL;
    unsigned long long l;
    tmp = PyLong_Check(o) ? o : PyNumber_Long(o);
    if(!tmp) goto fail;

    l = PyLong_AsUnsignedLongLong(tmp);
    if((unsigned long long)-1 == l && PyErr_Occurred())
        goto fail;

    *u = l;
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return true;
fail:
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return false;
}

bool from_python(PyObject *o, rtapi_sint *i) {
    PyObject *tmp = NULL;
    long long l;
    tmp = PyLong_Check(o) ? o : PyNumber_Long(o);
    if(!tmp) goto fail;

    l = PyLong_AsLongLong(tmp);
    if(-1 == l && PyErr_Occurred())
        goto fail;

    *i = l;
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return true;
fail:
    if(tmp && tmp != o) Py_XDECREF(tmp);
    return false;
}

struct halitem {
    bool is_pin;
    hal_type_t type;
    hal_pdir_t dir;
    hal_refs_u *u;
};

struct pyhalitem {
    PyObject_HEAD
    halitem  pin;
    char * name;
};

static PyObject * pyhal_pin_new(halitem * pin, const char *name);

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
    PyErr_SetString(pyhal_error_type, strerror(-code));
    return NULL;
}

static PyObject *pyhal_error(int code) {
    PyErr_SetString(pyhal_error_type, strerror(-code));
    return NULL;
}

static int pyhal_init(PyObject *_self, PyObject *args, PyObject *kw) {
    (void)kw;
    const char *name;
    const char *prefix = NULL;
    halobject *self = reinterpret_cast<halobject *>(_self);

    if(!PyArg_ParseTuple(args, "s|s:hal.component", &name, &prefix)) return -1;

    self->items = new itemmap();

    self->hal_id = hal_init(name);
    if(self->hal_id <= 0) {
        pyhal_error(self->hal_id);
        return -1;
    }

    self->name = strdup(name);
    self->prefix = strdup(prefix ? prefix : name);
    if(!self->name) {
        PyErr_SetString(PyExc_MemoryError, "strdup(name) failed");
        return -1;
    }
    if(!self->prefix) {
        PyErr_SetString(PyExc_MemoryError, "strdup(prefix) failed");
        return -1;
    }

    return 0;
}

static void pyhal_exit_impl(halobject *self) {
    if(self->hal_id > 0)
        hal_exit(self->hal_id);
    self->hal_id = 0;

    free(self->name);
    self->name = NULL;

    free(self->prefix);
    self->prefix = NULL;

    delete self->items;
    self->items = NULL;
}

static void pyhal_delete(PyObject *_self) {
    halobject *self = reinterpret_cast<halobject *>(_self);
    pyhal_exit_impl(self);
    Py_TYPE(self)->tp_free(self);
}

static int pyhal_write_common(halitem *pin, PyObject *value) {
    if(!pin) return -1;

    switch(pin->type) {
    case HAL_BOOL: {
        bool tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_bool(pin->u->b, tmp);
        break;
    }
    case HAL_REAL: {
        rtapi_real tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_real(pin->u->r, tmp);
        break;
    }
    case HAL_U32: {
        rtapi_u32 tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_ui32(pin->u->u, tmp);
        break;
    }
    case HAL_S32: {
        rtapi_s32 tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_si32(pin->u->s, tmp);
        break;
    }
    case HAL_UINT: {
        rtapi_uint tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_uint(pin->u->u, tmp);
        break;
    }
    case HAL_SINT: {
        rtapi_sint tmp;
        if(!from_python(value, &tmp)) return -1;
        hal_set_sint(pin->u->s, tmp);
        break;
    }
    default:
        PyErr_Format(pyhal_error_type, "Invalid pin type %d", pin->type);
    }
    return 0;
}

static PyObject *pyhal_read_common(halitem *item) {
    if(!item) return NULL;
    switch(item->type) {
    case HAL_BOOL: return to_python(hal_get_bool(item->u->b));
    case HAL_U32:  return to_python(hal_get_ui32(item->u->u));
    case HAL_S32:  return to_python(hal_get_si32(item->u->s));
    case HAL_UINT: return to_python(hal_get_uint(item->u->u));
    case HAL_SINT: return to_python(hal_get_sint(item->u->s));
    case HAL_REAL: return to_python(hal_get_real(item->u->r));
    case HAL_PORT:
        if(item->is_pin)
            return to_python(hal_port_buffer_size(reinterpret_cast<hal_port_t *>(item->u->u)));
        else
            return to_python((unsigned)0); // HAL_PORT cannot be a parameter
    default:
        break;
    }
    PyErr_Format(pyhal_error_type, "Invalid item type %d", item->type);
    return NULL;
}

static halitem *find_item(halobject *self, const char *name) {
    if(!name) return NULL;

    itemmap::iterator i = self->items->find(name);

    if(i == self->items->end()) {
        PyErr_Format(PyExc_AttributeError, "Pin or param '%s' does not exist", name);
        return NULL;
    }

    return &(i->second);
}

static bool is_valid_hal_type(hal_type_t t, bool allowport)
{
    switch(t) {
    case HAL_BOOL:
    case HAL_S32:
    case HAL_U32:
    case HAL_SINT:
    case HAL_UINT:
    case HAL_REAL:
        return true;
    case HAL_PORT:
        return allowport;
    default:
        return false;
    }
}

static PyObject * pyhal_create_param(halobject *self, const char *name, hal_type_t type, hal_param_dir_t dir) {
    int res;
    halitem param;
    param.is_pin = 0;

    if(!is_valid_hal_type(type, false)) {
        PyErr_Format(pyhal_error_type, "Invalid param type %d", type);
        return NULL;
    }

    param.type = type;
    param.dir = dir;
    param.u = (hal_refs_u *)hal_malloc(sizeof(*param.u));
    if(!param.u) {
        PyErr_SetString(PyExc_MemoryError, "hal_malloc failed");
        return NULL;
    }

    switch(type) {
    case HAL_BOOL: res = hal_param_new_bool(self->hal_id, dir, &param.u->b, 0, "%s.%s", self->prefix, name); break;
    case HAL_S32:  res = hal_param_new_si32(self->hal_id, dir, &param.u->s, 0, "%s.%s", self->prefix, name); break;
    case HAL_U32:  res = hal_param_new_ui32(self->hal_id, dir, &param.u->u, 0, "%s.%s", self->prefix, name); break;
    case HAL_SINT: res = hal_param_new_sint(self->hal_id, dir, &param.u->s, 0, "%s.%s", self->prefix, name); break;
    case HAL_UINT: res = hal_param_new_uint(self->hal_id, dir, &param.u->u, 0, "%s.%s", self->prefix, name); break;
    case HAL_REAL: res = hal_param_new_real(self->hal_id, dir, &param.u->r, 0.0, "%s.%s", self->prefix, name); break;
    default: res = -EINVAL; break;
    }
    if(res) return pyhal_error(res);

    (*self->items)[name] = param;

    return pyhal_pin_new(&param, name);
}


static PyObject * pyhal_create_pin(halobject *self, const char *name, hal_type_t type, hal_pin_dir_t dir) {
    char pin_name[HAL_NAME_LEN+1];
    int res;
    halitem pin;
    pin.is_pin = 1;

    if(!is_valid_hal_type(type, true)) {
        PyErr_Format(pyhal_error_type, "Invalid pin type %d", type);
        return NULL;
    }

    pin.type = type;
    pin.dir = dir;
    pin.u = (hal_refs_u *)hal_malloc(sizeof(*pin.u));
    if(!pin.u) {
        PyErr_SetString(PyExc_MemoryError, "hal_malloc failed");
        return NULL;
    }

    res = snprintf(pin_name, sizeof(pin_name), "%s.%s", self->prefix, name);
    if(res > HAL_NAME_LEN || res < 0) {
        PyErr_Format(pyhal_error_type,
            "Invalid pin name length \"%s.%s\": max = %d characters",
            self->prefix, name, HAL_NAME_LEN);
        return NULL;
    }
    switch(type) {
    case HAL_BOOL: res = hal_pin_new_bool(self->hal_id, dir, &pin.u->b, 0, "%s.%s", self->prefix, name); break;
    case HAL_S32:  res = hal_pin_new_si32(self->hal_id, dir, &pin.u->s, 0, "%s.%s", self->prefix, name); break;
    case HAL_U32:  res = hal_pin_new_ui32(self->hal_id, dir, &pin.u->u, 0, "%s.%s", self->prefix, name); break;
    case HAL_SINT: res = hal_pin_new_sint(self->hal_id, dir, &pin.u->s, 0, "%s.%s", self->prefix, name); break;
    case HAL_UINT: res = hal_pin_new_uint(self->hal_id, dir, &pin.u->u, 0, "%s.%s", self->prefix, name); break;
    case HAL_REAL: res = hal_pin_new_real(self->hal_id, dir, &pin.u->r, 0.0, "%s.%s", self->prefix, name); break;
    case HAL_PORT: res = hal_pin_new_port(self->hal_id, dir, &pin.u->s, "%s.%s", self->prefix, name); break;
    default: res = -EINVAL; break;
    }
    if(res) return pyhal_error(res);

    (*self->items)[name] = pin;

    return pyhal_pin_new(&pin, name);
}

static PyObject *pyhal_new_param(PyObject *_self, PyObject *o) {
    const char *name;
    int type, dir;
    halobject *self = reinterpret_cast<halobject *>(_self);

    if(!PyArg_ParseTuple(o, "sii", &name, &type, &dir))
        return NULL;
    EXCEPTION_IF_NOT_LIVE(NULL);

    if (find_item(self, name)) {
        PyErr_Format(PyExc_ValueError, "Duplicate parameter name '%s'", name);
        return NULL;
    } else { PyErr_Clear(); }
    return pyhal_create_param(self, name, (hal_type_t)type, (hal_param_dir_t)dir);
}


static PyObject *pyhal_new_pin(PyObject *_self, PyObject *o) {
    const char *name;
    int type, dir;
    halobject *self = reinterpret_cast<halobject *>(_self);

    if(!PyArg_ParseTuple(o, "sii", &name, &type, &dir))
        return NULL;
    EXCEPTION_IF_NOT_LIVE(NULL);

    if (find_item(self, name)) {
        PyErr_Format(PyExc_ValueError, "Duplicate pin name '%s'", name);
        return NULL;
    } else { PyErr_Clear(); }
    return pyhal_create_pin(self, name, (hal_type_t)type, (hal_pin_dir_t)dir);
}

enum what_type_e {
    WHAT_ANY,
    WHAT_PIN,
    WHAT_PARAM,
};
static PyObject *get_pin_or_param(halobject *self, PyObject *args, what_type_e what)
{
    const char *name;
    if(!PyArg_ParseTuple(args, "s", &name))
        return NULL;
    EXCEPTION_IF_NOT_LIVE(NULL);

    halitem *pin = find_item(self, name);
    if(!pin)
        return NULL;
    if(what == WHAT_ANY || (pin->is_pin && what == WHAT_PIN) || (!pin->is_pin && what == WHAT_PARAM))
        return pyhal_pin_new(pin, name);
    PyErr_Format(PyExc_AttributeError, "%s '%s' does not exist", what == WHAT_PIN ? "Pin" : "Param", name);
    return NULL;
}

static PyObject *pyhal_get_pin(PyObject *self, PyObject *args)
{
    return get_pin_or_param(reinterpret_cast<halobject *>(self), args, WHAT_PIN);
}

static PyObject *pyhal_get_param(PyObject *self, PyObject *args)
{
    return get_pin_or_param(reinterpret_cast<halobject *>(self), args, WHAT_PARAM);
}

static PyObject *pyhal_get_item(PyObject *self, PyObject *args)
{
    return get_pin_or_param(reinterpret_cast<halobject *>(self), args, WHAT_ANY);
}

static PyObject *pyhal_get_pins(PyObject *_self, PyObject * /*o*/) {
  halobject *self = reinterpret_cast<halobject *>(_self);

  EXCEPTION_IF_NOT_LIVE(NULL);

  PyObject *d = PyDict_New();
  for(itemmap::iterator i = self->items->begin(); i != self->items->end(); ++i) {
    halitem * pin = &(i->second);
    PyObject *key = PyUnicode_FromString(i->first.c_str());
    if(!key) {
        Py_DECREF(d);
        return NULL;
    }
    PyObject *val = pyhal_read_common(pin);
    if(!val) {
        Py_DECREF(key);
        Py_DECREF(d);
        return NULL;
    }
    PyDict_SetItem(d, key, val);
    Py_DECREF(val);
    Py_DECREF(key);
  }
  return d;
}


static PyObject *pyhal_ready(PyObject *_self, PyObject * /*o*/) {
    // hal_ready did not exist in EMC 2.0.x, make it a no-op
    halobject *self = reinterpret_cast<halobject *>(_self);
    EXCEPTION_IF_NOT_LIVE(NULL);
    int res = hal_ready(self->hal_id);
    if(res) return pyhal_error(res);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyhal_unready(PyObject *_self, PyObject * /*o*/) {
    // hal_ready did not exist in EMC 2.0.x, make it a no-op
    halobject *self = reinterpret_cast<halobject *>(_self);
    EXCEPTION_IF_NOT_LIVE(NULL);
    int res = hal_unready(self->hal_id);
    if(res) return pyhal_error(res);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyhal_exit(PyObject *_self, PyObject * /*o*/) {
    halobject *self = reinterpret_cast<halobject *>(_self);
    pyhal_exit_impl(self);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyhal_repr(PyObject *_self) {
    halobject *self = reinterpret_cast<halobject *>(_self);
    return PyUnicode_FromFormat("<hal component %s(%d) with %d pins and params>",
            self->name, self->hal_id, (int)self->items->size());
}

static PyObject *pyhal_getattro(PyObject *_self, PyObject *attro)  {
    PyObject *result;
    halobject *self = reinterpret_cast<halobject *>(_self);
    EXCEPTION_IF_NOT_LIVE(NULL);

    result = PyObject_GenericGetAttr(reinterpret_cast<PyObject*>(self), attro);
    if(result) return result;

    PyErr_Clear();
    return pyhal_read_common(find_item(self, PyUnicode_AsUTF8(attro)));
}

static int pyhal_setattro(PyObject *_self, PyObject *attro, PyObject *v) {
    halobject *self = reinterpret_cast<halobject *>(_self);
    // FIXME: The documentation states that when v==NULL it means to delete the
    // attribute and it must be supported.
    EXCEPTION_IF_NOT_LIVE(-1);
    return pyhal_write_common(find_item(self, PyUnicode_AsUTF8(attro)), v);
}

static Py_ssize_t pyhal_len(PyObject *_self) {
    halobject* self = reinterpret_cast<halobject*>(_self);
    EXCEPTION_IF_NOT_LIVE(-1);
    return self->items->size();
}

static PyObject *pyhal_get_prefix(PyObject *_self, PyObject *args) {
    halobject* self = reinterpret_cast<halobject*>(_self);
    if(!PyArg_ParseTuple(args, "")) return NULL;
    EXCEPTION_IF_NOT_LIVE(NULL);

    if(!self->prefix) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    return PyUnicode_FromString(self->prefix);
}


static PyObject *pyhal_set_prefix(PyObject *_self, PyObject *args) {
    const char *newprefix;
    halobject* self = reinterpret_cast<halobject*>(_self);
    if(!PyArg_ParseTuple(args, "s", &newprefix)) return NULL;
    EXCEPTION_IF_NOT_LIVE(NULL);

    if(self->prefix)
        free(self->prefix);
    self->prefix = strdup(newprefix);

    if(!self->prefix) {
        PyErr_SetString(PyExc_MemoryError, "strdup(prefix) failed");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef hal_methods[] = {
    {"setprefix", pyhal_set_prefix, METH_VARARGS,
        "Set the prefix for newly created pins and parameters"},
    {"getprefix", pyhal_get_prefix, METH_VARARGS,
        "Get the prefix for newly created pins and parameters"},
    {"newparam", pyhal_new_param, METH_VARARGS,
        "Create a new parameter"},
    {"newpin", pyhal_new_pin, METH_VARARGS,
        "Create a new pin"},
    {"getitem", pyhal_get_item, METH_VARARGS,
        "Get existing pin or param object"},
    {"getpin", pyhal_get_pin, METH_VARARGS,
        "Get existing pin object"},
    {"getparam", pyhal_get_param, METH_VARARGS,
        "Get existing param object"},
    {"getpins", pyhal_get_pins, METH_VARARGS,
            "Get all pins and values of component"},
    {"exit", pyhal_exit, METH_NOARGS,
        "Call hal_exit"},
    {"ready", pyhal_ready, METH_NOARGS,
        "Call hal_ready"},
    {"unready", pyhal_unready, METH_NOARGS,
        "Call hal_unready"},
    {},
};

static PyMappingMethods halobject_map = {
    pyhal_len,
    pyhal_getattro,
    pyhal_setattro
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static
PyTypeObject halobject_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "hal.component",           /*tp_name*/
    sizeof(halobject),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pyhal_delete,              /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    pyhal_repr,                /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    &halobject_map,            /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    pyhal_getattro,            /*tp_getattro*/
    pyhal_setattro,            /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,        /*tp_flags*/
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
    pyhal_init,                /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
    0,                         /*tp_bases*/
    0,                         /*tp_mro*/
    0,                         /*tp_cache*/
    0,                         /*tp_subclasses*/
    0,                         /*tp_weaklink*/
    0,                         /*tp_del*/
    0,                         /*tp_version_tag*/
    0,                         /*tp_finalize*/
#if PY_VERSION_HEX >= 0x030800f0	// 3.8
    0,                         /*tp_vectorcall*/
#if PY_VERSION_HEX >= 0x030c00f0	// 3.12
    0,                         /*tp_watched*/
#if PY_VERSION_HEX >= 0x030d00f0	// 3.13
    0,                         /*tp_versions_used*/
#endif
#endif
#endif
};
#pragma GCC diagnostic pop

static const char * pin_type2name(hal_type_t type) {
    switch (type) {
    case HAL_BOOL: return "BIT";
    case HAL_S32:  return "S32";
    case HAL_U32:  return "U32";
    case HAL_SINT: return "S64";
    case HAL_UINT: return "U64";
    case HAL_REAL: return "FLOAT";
    case HAL_PORT: return "PORT";
    default: return "unknown";
    }
}

static const char * pin_dir2name(hal_pdir_t type) {
    switch (type) {
    case HAL_IN:  return "IN";
    case HAL_IO:  return "IO";
    case HAL_OUT: return "OUT";
    case HAL_RO:  return "RO";
    case HAL_RW:  return "RW";
    default: return "unknown";
    }
}

static PyObject *pyhalpin_repr(PyObject *_self) {
    pyhalitem *pyself = reinterpret_cast<pyhalitem *>(_self);
    halitem *self = &pyself->pin;

    const char * name = "(null)";
    if (pyself->name) name = pyself->name;

    const char *pp = self->is_pin ? "pin" : "param";
    return PyUnicode_FromFormat("<hal %s \"%s\" %s-%s>", pp, name,
            pin_type2name(self->type), pin_dir2name(self->dir));
}

static int pyhalpin_init(PyObject * /*_self*/, PyObject *, PyObject *) {
    PyErr_Format(PyExc_RuntimeError, "Cannot be constructed directly");
    return -1;
}

static void pyhalpin_delete(PyObject *_self) {
    pyhalitem *self = reinterpret_cast<pyhalitem *>(_self);

    if(self->name) free(self->name);

    PyObject_Del(self);
}

static PyObject * pyhal_pin_set(PyObject * _self, PyObject * value) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    if (pyhal_write_common(&self->pin, value) == -1)
        return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * pyhal_pin_get(PyObject * _self, PyObject *) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    return pyhal_read_common(&self->pin);
}

static PyObject * pyhal_pin_get_type(PyObject * _self, PyObject *) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    return PyLong_FromLong(self->pin.type);
}

static PyObject * pyhal_pin_get_dir(PyObject * _self, PyObject *) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    return PyLong_FromLong(self->pin.dir);
}

static PyObject * pyhal_pin_is_pin(PyObject * _self, PyObject *) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    return PyBool_FromLong(self->pin.is_pin);
}

static PyObject * pyhal_pin_get_name(PyObject * _self, PyObject *) {
    pyhalitem * self = reinterpret_cast<pyhalitem *>(_self);
    if (!self->name) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyUnicode_FromString(self->name);
}

// - - - -
// Port methods
static bool check_port(const pyhalitem *item, const char *pfx)
{
    if(!item->pin.is_pin) {
        PyErr_Format(PyExc_RuntimeError, "%s: %s: Not a pin", pfx, item->name);
        return false;
    }
    if(item->pin.type != HAL_PORT) {
        PyErr_Format(PyExc_RuntimeError, "%s: %s: Pin type not HAL_PORT but '%d'", pfx, (int)item->pin.type);
        return false;
    }
    return true;
}

static PyObject *pyhal_port_write(PyObject *self, PyObject *o)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "write"))
        return NULL;
    if(item->pin.dir != HAL_OUT) {
        PyErr_Format(PyExc_RuntimeError, "write: %s: Pin not output", item->name);
        return NULL;
    }

    Py_ssize_t len;
    const char *cptr;
    if(PyUnicode_Check(o)) {
        cptr = PyUnicode_AsUTF8AndSize(o, &len);
    } else if(PyBytes_Check(o)) {
        cptr = PyBytes_AsString(o);
        len = PyBytes_Size(o);
    } else {
        PyErr_Format(PyExc_RuntimeError, "write: %s: Argument not a string or bytes object", item->name);
        return NULL;
    }

    if(!cptr) {
        PyErr_Format(PyExc_RuntimeError, "write: %s: Object conversion to bytes failed", item->name);
        return NULL;
    }
    if(len < 1 || len > HAL_PORT_SIZE_MAX) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if(len > (Py_ssize_t)hal_port_writable((hal_port_t *)item->pin.u->u)) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    return PyBool_FromLong(hal_port_write((hal_port_t *)item->pin.u->u, cptr, (unsigned)len));
}

static PyObject *pyhal_port_read_peek(pyhalitem *item, PyObject *o, bool isread)
{
    const char *pfx = isread ? "read" : "peek";
    if(!check_port(item, pfx))
        return NULL;
    if(item->pin.dir != HAL_IN) {
        PyErr_Format(PyExc_RuntimeError, "%s: %s: Pin not input", pfx, item->name);
        return NULL;
    }
    if(!PyLong_Check(o)) {
        PyErr_Format(PyExc_RuntimeError, "%s: %s: Argument not an integer", pfx, item->name);
        return NULL;
    }
    unsigned long l = PyLong_AsUnsignedLong(o);
    if((unsigned long)-1 == l && PyErr_Occurred()) {
        return NULL;
    }
    if(l < 1 || l > HAL_PORT_SIZE_MAX) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    if(l > (unsigned long)hal_port_readable((hal_port_t *)item->pin.u->u)) {
        Py_INCREF(Py_False);
        return Py_False;
    }

    PyObject *bts = PyBytes_FromStringAndSize(NULL, (Py_ssize_t)l);
    if(!bts) {
        PyErr_Format(PyExc_RuntimeError, "%s: %s: Failed to create bytes object", pfx, item->name);
        return NULL;
    }
    bool b;
    if(isread)
        b = hal_port_read((hal_port_t *)item->pin.u->u, PyBytes_AsString(bts), (unsigned)l);
    else
        b = hal_port_peek((hal_port_t *)item->pin.u->u, PyBytes_AsString(bts), (unsigned)l);
    if(b) {
        Py_DECREF(bts);
        Py_INCREF(Py_False);
        return Py_False;
    }
    return bts;
}

static PyObject *pyhal_port_read(PyObject *self, PyObject *o)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    return pyhal_port_read_peek(item, o, 1);
}

static PyObject *pyhal_port_peek(PyObject *self, PyObject *o)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    return pyhal_port_read_peek(item, o, 0);
}

static PyObject *pyhal_port_peek_commit(PyObject *self, PyObject *o)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "peek_commit"))
        return NULL;
    if(!PyLong_Check(o)) {
        PyErr_Format(PyExc_RuntimeError, "peek_commit: %s: Argument not an integer", item->name);
        return NULL;
    }
    unsigned long l = PyLong_AsUnsignedLong(o);
    if((unsigned long)-1 == l && PyErr_Occurred()) {
        return NULL;
    }
    if(l < 1 || l > HAL_PORT_SIZE_MAX) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    return PyBool_FromLong(hal_port_peek_commit((hal_port_t *)item->pin.u->u, (unsigned)l));
}

static PyObject *pyhal_port_writable(PyObject *self, PyObject *)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "writable"))
        return NULL;
    if(item->pin.dir != HAL_OUT) {
        PyErr_Format(PyExc_RuntimeError, "writable: %s: Pin not output", item->name);
        return NULL;
    }
    return PyLong_FromUnsignedLong(hal_port_writable((hal_port_t *)item->pin.u->u));
}

static PyObject *pyhal_port_readable(PyObject *self, PyObject *)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "readable"))
        return NULL;
    if(item->pin.dir != HAL_IN) {
        PyErr_Format(PyExc_RuntimeError, "readable: %s: Pin not input", item->name);
        return NULL;
    }
    return PyLong_FromUnsignedLong(hal_port_readable((hal_port_t *)item->pin.u->u));
}

static PyObject *pyhal_port_clear(PyObject *self, PyObject *)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "clear"))
        return NULL;
    hal_port_clear((hal_port_t *)item->pin.u->u);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyhal_port_size(PyObject *self, PyObject *)
{
    pyhalitem *item = reinterpret_cast<pyhalitem *>(self);
    if(!check_port(item, "size"))
        return NULL;
    return PyLong_FromUnsignedLong(hal_port_buffer_size((hal_port_t *)item->pin.u->u));
}

static PyMethodDef halpin_methods[] = {
    {"set", pyhal_pin_set, METH_O, "Set item value"},
    {"get", pyhal_pin_get, METH_NOARGS, "Get item value"},
    {"get_type", pyhal_pin_get_type, METH_NOARGS, "Get item type"},
    {"get_dir", pyhal_pin_get_dir, METH_NOARGS, "Get item direction"},
    {"get_name", pyhal_pin_get_name, METH_NOARGS, "Get item name"},
    {"is_pin", pyhal_pin_is_pin, METH_NOARGS, "If item is pin or param"},
    // Port methods
    {"write",    pyhal_port_write,    METH_O, "Write data to the port queue"},
    {"read",     pyhal_port_read,     METH_O, "Read data from the port queue"},
    {"writable", pyhal_port_writable, METH_NOARGS, "Get the count that can be written to the port"},
    {"readable", pyhal_port_readable, METH_NOARGS, "Get the count that can be read from the port"},
    {"peek",     pyhal_port_peek,     METH_O, "Read pending data without removal from the queue"},
    {"peek_commit", pyhal_port_peek_commit, METH_O, "Advance the read pointer by given amount"},
    {"clear",    pyhal_port_clear,    METH_NOARGS, "Clear the port queue"},
    {"size",     pyhal_port_size,     METH_NOARGS, "Get the queue size of the port"},
    {},
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static
PyTypeObject halpin_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "hal.item",                /*tp_name*/
    sizeof(pyhalitem),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pyhalpin_delete,           /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    pyhalpin_repr,             /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "HAL Pin",                 /*tp_doc*/
    0,                         /*tp_traverse*/
    0,                         /*tp_clear*/
    0,                         /*tp_richcompare*/
    0,                         /*tp_weaklistoffset*/
    0,                         /*tp_iter*/
    0,                         /*tp_iternext*/
    halpin_methods,            /*tp_methods*/
    0,                         /*tp_members*/
    0,                         /*tp_getset*/
    0,                         /*tp_base*/
    0,                         /*tp_dict*/
    0,                         /*tp_descr_get*/
    0,                         /*tp_descr_set*/
    0,                         /*tp_dictoffset*/
    pyhalpin_init,             /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
    0,                         /*tp_bases*/
    0,                         /*tp_mro*/
    0,                         /*tp_cache*/
    0,                         /*tp_subclasses*/
    0,                         /*tp_weaklink*/
    0,                         /*tp_del*/
    0,                         /*tp_version_tag*/
    0,                         /*tp_finalize*/
#if PY_VERSION_HEX >= 0x030800f0	// 3.8
    0,                         /*tp_vectorcall*/
#if PY_VERSION_HEX >= 0x030c00f0	// 3.12
    0,                         /*tp_watched*/
#if PY_VERSION_HEX >= 0x030d00f0	// 3.13
    0,                         /*tp_versions_used*/
#endif
#endif
#endif
};
#pragma GCC diagnostic pop

static PyObject * pyhal_pin_new(halitem * pin, const char * name) {
    pyhalitem * pypin = PyObject_New(pyhalitem, &halpin_type);
    if (!pypin)
        return NULL;
    pypin->pin = *pin;
    if (name)
        pypin->name = strdup(name);
    else
        pypin->name = NULL;

    return reinterpret_cast<PyObject *>(pypin);
}

PyObject *pin_has_writer(PyObject * /*self*/, PyObject *args) {
    hal_query_t q = {};
    if(!PyArg_ParseTuple(args, "s", &q.name)) return NULL;

    q.qtype = HAL_QTYPE_PIN; // Only query a pin
    int rv = hal_get_p(&q, NULL, NULL);
    if(0 == rv) {
        // Success reading. See if it has a driver.
        if(!q.pp.signal) {
            // No signal, it may be its own writer?
            return PyBool_FromLong(q.pp.dir == HAL_OUT);
        }
        hal_query_t qs = {};
        qs.name = q.pp.signal;
        if(0 != (rv = hal_getref_s(&qs))) {
            PyErr_Format(PyExc_NameError, "Signal '%s' of pin '%s' gave unexpected error=%d", q.pp.signal, q.name, rv);
            return NULL;
        }
        return PyBool_FromLong(qs.sig.writers > 0);
    } else if(-ENOENT == rv) {
        // Pin not found
        PyErr_Format(PyExc_NameError, "Pin `%s' does not exist", q.name);
        return NULL;
    }
    PyErr_Format(PyExc_RuntimeError, "pin_has_writer: %s: returned error: %s", q.name, hal_strerror(rv));
    return NULL;
}


PyObject *component_exists(PyObject * /*self*/, PyObject *args) {
    const char *name;
    if(!PyArg_ParseTuple(args, "s", &name)) return NULL;

    int rv = hal_comp_by_name(name, NULL);
    if(0 == rv) {
        Py_INCREF(Py_True);
        return Py_True;
    } else if(-ENOENT == rv) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    PyErr_Format(PyExc_RuntimeError, "component_exists: hal_comp_by_name '%s' returned %d", name, rv);
    return NULL;
}

PyObject *component_is_ready(PyObject * /*self*/, PyObject *args) {
    const char *name;
    if(!PyArg_ParseTuple(args, "s", &name)) return NULL;

    hal_query_t q = {};
    int rv = hal_comp_by_name(name, &q);
    return PyBool_FromLong(0 == rv && q.comp.ready);
}

PyObject *new_sig(PyObject * /*self*/, PyObject *args) {
    const char *name;
    int type,retval;
    if(!PyArg_ParseTuple(args, "si", &name,&type)) return NULL;

    //printf("INFO HALMODULE -- make signal -> %s type %d\n",name,(hal_type_t) type);
    switch (type) {
    case HAL_BOOL:
    case HAL_S32:
    case HAL_U32:
    case HAL_SINT:
    case HAL_UINT:
    case HAL_REAL:
    case HAL_PORT:
        retval = hal_signal_new(name, (hal_type_t)type);
        break;
    default: {
        PyErr_Format(PyExc_RuntimeError, "not a valid HAL signal type");
        return NULL;
        }
    }
    return PyBool_FromLong(retval != 0);
}

PyObject *connect(PyObject * /*self*/, PyObject *args) {
    const char *signame,*pinname;
    if(!PyArg_ParseTuple(args, "ss", &pinname,&signame)) return NULL;

    //printf("INFO HALMODULE -- link sig %s to pin %s\n",signame,pinname);
    return PyBool_FromLong(hal_link(pinname, signame) != 0);
}

PyObject *disconnect(PyObject * /*self*/, PyObject *args) {
    const char *pinname;
    if(!PyArg_ParseTuple(args, "s", &pinname)) return NULL;

    //printf("INFO HALMODULE -- unlink pin %s\n",pinname);
    return PyBool_FromLong(hal_unlink(pinname) != 0);
}

static int set_common_cb(hal_query_t *q, void *arg)
{
    PyObject *obj = static_cast<PyObject *>(arg);
    hal_type_t type = HAL_QTYPE_SIGNAL == q->qtype ? q->sig.type : q->pp.type;
    hal_query_value_u *qvp = HAL_QTYPE_SIGNAL == q->qtype ? &q->sig.value : &q->pp.value;

    switch(type) {
    case HAL_BOOL: {
        bool tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->b = tmp;
        break;
    }
    case HAL_REAL: {
        rtapi_real tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->r = tmp;
        break;
    }
    case HAL_S32: {
        rtapi_s32 tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->s = tmp;
        break;
    }
    case HAL_U32: {
        rtapi_u32 tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->u = tmp;
        break;
    }
    case HAL_SINT: {
        rtapi_sint tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->s = tmp;
        break;
    }
    case HAL_UINT: {
        rtapi_uint tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->u = tmp;
        break;
    }
    case HAL_PORT: {
        unsigned tmp;
        if(!from_python(obj, &tmp)) return -EINVAL;
        qvp->u = tmp;
        break;
    }
    default:
        // Shouldn't get here, but just in case...
        return -EBADF;
    }
    return 0;
}

PyObject *set_p(PyObject * /*self*/, PyObject *args) {
    hal_query_t q = {};
    PyObject *obj;

    if(!PyArg_ParseTuple(args, "sO", &q.name, &obj)) return NULL;

    int rv = hal_set_p(&q, set_common_cb, obj);
    if(rv < 0) {
        PyErr_Format(PyExc_RuntimeError, "set_p: %s: %s", q.name, hal_strerror(rv));
        return NULL;
    }
    return PyBool_FromLong(1);
}

PyObject *set_s(PyObject * /*self*/, PyObject *args) {
    hal_query_t q = {};
    PyObject *obj;

    if(!PyArg_ParseTuple(args, "sO", &q.name, &obj)) return NULL;

    int rv = hal_set_s(&q, set_common_cb, obj);
    if(rv < 0) {
        PyErr_Format(PyExc_RuntimeError, "set_s: %s: %s", q.name, hal_strerror(rv));
        return NULL;
    }
    return PyBool_FromLong(1);
}

/*######################################*/
/* Get a Pin, Param or signal value     */
/* Search order: pin, param, signal     */

static PyObject *halref_to_object(hal_type_t type, const hal_query_value_u *v)
{
    switch(type) {
    case HAL_BOOL: return to_python(v->b);
    case HAL_U32:  return to_python((rtapi_u32)v->u);
    case HAL_S32:  return to_python((rtapi_s32)v->s);
    case HAL_UINT: return to_python(v->u);
    case HAL_SINT: return to_python(v->s);
    case HAL_REAL: return to_python(v->r);
    case HAL_PORT: return to_python(v->s);
    default:
        PyErr_Format(PyExc_RuntimeError, "halref_to_object: unknown hal type '%d'", (int)type);
        return NULL;
    }
}

//
// get_p() tries to get the value of a named pin or parameter.
//
PyObject *get_p(PyObject * /*self*/, PyObject *args)
{
    hal_query_t q = {};

    if(!PyArg_ParseTuple(args, "s", &q.name)) return NULL;

    // A connected pin will return the signal's value
    int rv = hal_get_p(&q, NULL, NULL);
    if(0 == rv)
        return halref_to_object(q.pp.type, &q.pp.value);
    // Get here: most likely no pin/param with that name
    PyErr_Format(PyExc_RuntimeError, "get_p: %s: %s", q.name, hal_strerror(rv));
    return NULL;
}

//
// get_s() tries to get the value of a named signal.
//
PyObject *get_s(PyObject * /*self*/, PyObject *args)
{
    hal_query_t q = {};

    if(!PyArg_ParseTuple(args, "s", &q.name)) return NULL;

    int rv = hal_get_s(&q, NULL, NULL);
    if(0 == rv)
        return halref_to_object(q.sig.type, &q.sig.value);
    // Get here: most likely no signal with that name
    PyErr_Format(PyExc_RuntimeError, "get_s: %s: %s", q.name, hal_strerror(rv));
    return NULL;
}

//
// get_value() first tries pins/params and then signals if no pin/param with
// that name.
//
PyObject *get_value(PyObject * /*self*/, PyObject *args)
{
    hal_query_t q = {};

    if(!PyArg_ParseTuple(args, "s", &q.name)) return NULL;

    // Try a pin/param
    // A connected pin will return the signal's value
    int rv = hal_get_p(&q, NULL, NULL);
    if(0 == rv)
        return halref_to_object(q.pp.type, &q.pp.value);
    // No pin/param with that name, try a signal
    if(-ENOENT == rv) {
        rv = hal_get_s(&q, NULL, NULL);
        if(0 == rv)
            return halref_to_object(q.sig.type, &q.sig.value);
    }
    PyErr_Format(PyExc_RuntimeError, "get_value: %s: %s", q.name, hal_strerror(rv));
    return NULL;
}

/*######################################*/
/* Get a dict of pin info for all pins in system */
static int pinparaminfo_cb(hal_query_t *q, void *arg)
{
    PyObject *lst = static_cast<PyObject *>(arg);
    PyObject *obj;
    static const char str_n[] = "NAME";
    static const char str_v[] = "VALUE";
    static const char str_t[] = "TYPE";
    static const char str_d[] = "DIRECTION";

    switch(q->pp.type) {
    case HAL_BOOL:
        obj = Py_BuildValue("{s:s,s:N,s:N,s:N}",
                str_n, q->name, str_v, PyBool_FromLong(hal_get_bool(q->pp.ref.b)),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_BOOL));
        break;
    case HAL_U32:
        obj = Py_BuildValue("{s:s,s:k,s:N,s:N}",
                str_n, q->name, str_v, hal_get_ui32(q->pp.ref.u),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_U32));
        break;
    case HAL_S32:
        obj = Py_BuildValue("{s:s,s:l,s:N,s:N}",
                str_n, q->name, str_v, hal_get_si32(q->pp.ref.s),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_S32));
        break;
    case HAL_UINT:
        obj = Py_BuildValue("{s:s,s:K,s:N,s:N}",
                str_n, q->name, str_v, hal_get_uint(q->pp.ref.u),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_UINT));
        break;
    case HAL_SINT:
        obj = Py_BuildValue("{s:s,s:L,s:N,s:N}",
                str_n, q->name, str_v, hal_get_sint(q->pp.ref.s),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_SINT));
        break;
    case HAL_REAL:
        obj = Py_BuildValue("{s:s,s:d,s:N,s:N}",
                str_n, q->name, str_v, hal_get_real(q->pp.ref.r),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_REAL));
        break;
    case HAL_PORT:
        obj = Py_BuildValue("{s:s,s:l,s:N,s:N}",
                str_n, q->name, str_v, hal_get_sint(q->pp.ref.s),
                str_d, PyLong_FromLong(q->pp.dir), str_t, PyLong_FromLong(HAL_PORT));
        break;
    default:
         obj = Py_BuildValue("{s:s,s:s,s:N,s:s}",
                str_n, q->name, str_v, NULL,
                str_d, PyLong_FromLong(q->pp.dir), str_t, NULL);
         break;
    }
    PyList_Append(lst, obj);
    Py_DECREF(obj);
    return 0;
}

PyObject *get_info_pins(PyObject * /*self*/, PyObject * /*args*/)
{
    PyObject* python_list = PyList_New(0);
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN; // Only handle pins
    int rv = hal_list_p(&q, pinparaminfo_cb, python_list);
    if(0 != rv) {
        Py_DECREF(python_list);
        PyErr_Format(PyExc_RuntimeError, "hal_list_p: returned '%s' (%d)", hal_strerror(rv), rv);
        return NULL;
    }
    return python_list;
}

/*######################################*/
/* Get a dict of signal info for all signals in system */

static int siginfo_writer_cb(hal_query_t *q, void *arg)
{
    if(HAL_OUT == q->pp.dir) {
        // Found the writer, record and quit the loop
        *((const char **)arg) = q->name;
        return 1;
    }
    return 0;
}

static int siginfo_cb(hal_query_t *q, void *arg)
{
    PyObject *lst = static_cast<PyObject *>(arg);
    PyObject *obj;
    static const char str_n[] = "NAME";
    static const char str_v[] = "VALUE";
    static const char str_t[] = "TYPE";
    static const char str_d[] = "DRIVER";

    const char *writer = NULL;
    if(q->sig.writers > 0) {
        // Retrieve the writer pin name
        hal_query_t qd = {};
        qd.name = q->name;
        hal_list_p_s(&qd, siginfo_writer_cb, (void *)&writer);
    }

    switch(q->sig.type) {
    case HAL_BOOL:
        obj = Py_BuildValue("{s:s,s:N,s:s,s:N}",
                str_n, q->name, str_v, PyBool_FromLong(hal_get_bool(q->sig.ref.b)),
                str_d, writer, str_t, PyLong_FromLong(HAL_BOOL));
        break;
    case HAL_U32:
        obj = Py_BuildValue("{s:s,s:k,s:s,s:N}",
                str_n, q->name, str_v, hal_get_ui32(q->sig.ref.u),
                str_d, writer, str_t, PyLong_FromLong(HAL_U32));
        break;
    case HAL_S32:
        obj = Py_BuildValue("{s:s,s:l,s:s,s:N}",
                str_n, q->name, str_v, hal_get_si32(q->sig.ref.s),
                str_d, writer, str_t, PyLong_FromLong(HAL_S32));
        break;
    case HAL_UINT:
        obj = Py_BuildValue("{s:s,s:K,s:s,s:N}",
                str_n, q->name, str_v, hal_get_uint(q->sig.ref.u),
                str_d, writer, str_t, PyLong_FromLong(HAL_UINT));
        break;
    case HAL_SINT:
        obj = Py_BuildValue("{s:s,s:L,s:s,s:N}",
                str_n, q->name, str_v, hal_get_sint(q->sig.ref.s),
                str_d, writer, str_t, PyLong_FromLong(HAL_SINT));
        break;
    case HAL_REAL:
        obj = Py_BuildValue("{s:s,s:d,s:s,s:N}",
                str_n, q->name, str_v, hal_get_real(q->sig.ref.r),
                str_d, writer, str_t, PyLong_FromLong(HAL_REAL));
        break;
    case HAL_PORT:
        obj = Py_BuildValue("{s:s,s:l,s:s,s:N}",
                str_n, q->name, str_v, hal_get_sint(q->sig.ref.s),
                str_d, writer, str_t, PyLong_FromLong(HAL_PORT));
        break;
    default:
         obj = Py_BuildValue("{s:s,s:s,s:s,s:s}",
                str_n, q->name, str_v, NULL,
                str_d, writer, str_t, NULL);
         break;
    }
    PyList_Append(lst, obj);
    Py_DECREF(obj);
    return 0;
}

PyObject *get_info_signals(PyObject * /*self*/, PyObject * /*args*/)
{
    PyObject* python_list = PyList_New(0);
    hal_query_t q = {};
    int rv = hal_list_s(&q, siginfo_cb, python_list);
    if(0 != rv) {
        Py_DECREF(python_list);
        PyErr_Format(PyExc_RuntimeError, "hal_list_s: returned '%s' (%d)", hal_strerror(rv), rv);
        return NULL;
    }
    return python_list;
}

/*######################################*/
/* Get a dict of parameter info for all parameters in system */
PyObject *get_info_params(PyObject * /*self*/, PyObject * /*args*/)
{
    PyObject* python_list = PyList_New(0);
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM; // Only handle parameters
    int rv = hal_list_p(&q, pinparaminfo_cb, python_list);
    if(0 != rv) {
        Py_DECREF(python_list);
        PyErr_Format(PyExc_RuntimeError, "hal_list_p: returned '%s' (%d)", hal_strerror(rv), rv);
        return NULL;
    }
    return python_list;
}

static PyObject *pyhal_get_realtime_type(PyObject * /*self*/, PyObject * /*o*/) {
    int res = hal_get_realtime_type();
    return PyLong_FromLong(res);
}

static PyObject *pyhal_is_initialized(PyObject * /*self*/, PyObject * /*o*/) {
    return PyBool_FromLong(hal_is_init());
}

struct shmobject {
    PyObject_HEAD
    halobject *comp;
    int key;
    int shm_id;
    unsigned long size;
    void *buf;
};

static int pyshm_init(PyObject *_self, PyObject *args, PyObject * /*kw*/) {
    shmobject *self = reinterpret_cast<shmobject *>(_self);
    self->comp = NULL;
    self->shm_id = -1;

    if(!PyArg_ParseTuple(args, "O!ik", &halobject_type, &self->comp, &self->key, &self->size))
        return -1;

    self->shm_id = rtapi_shmem_new(self->key, self->comp->hal_id, self->size);
    if(self->shm_id < 0) {
        self->comp = NULL;
        self->size = 0;
        pyrtapi_error(self->shm_id);
        return -1;
    }

    rtapi_shmem_getptr(self->shm_id, &self->buf);
    Py_INCREF(self->comp);

    return 0;
}

static void pyshm_delete(PyObject *_self) {
    shmobject *self = reinterpret_cast<shmobject *>(_self);
    if(self->comp && self->shm_id > 0)
        rtapi_shmem_delete(self->shm_id, self->comp->hal_id);
    Py_XDECREF(self->comp);
}

static int shm_buffer_getbuffer(PyObject *obj, Py_buffer *view, int /*flags*/) {
  if (view == NULL) {
    PyErr_SetString(PyExc_ValueError, "NULL view in getbuffer");
    return -1;
  }
  shmobject* self = reinterpret_cast<shmobject *>(obj);
  view->obj = reinterpret_cast<PyObject*>(self);
  view->buf = (void*)self->buf;
  view->len = self->size;
  view->readonly = 0;
  Py_INCREF(self);  // need to increase the reference count
  return 0;
}

static PyObject *pyshm_repr(PyObject *_self) {
    shmobject *self = reinterpret_cast<shmobject *>(_self);
    return PyUnicode_FromFormat("<shared memory buffer key=%08x id=%d size=%ld>",
            self->key, self->shm_id, (unsigned long)self->size);
}

static PyObject *shm_setsize(PyObject *_self, PyObject *args) {
    shmobject *self = reinterpret_cast<shmobject *>(_self);
    if(!PyArg_ParseTuple(args, "k", &self->size)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *shm_getbuffer(PyObject *_self, PyObject * /*dummy*/) {

    shmobject *self = reinterpret_cast<shmobject *>(_self);
    return (PyObject*)PyMemoryView_FromObject(reinterpret_cast<PyObject*>(self));
}

static PyObject *set_msg_level(PyObject * /*_self*/, PyObject *args) {
    int level, res;
    if(!PyArg_ParseTuple(args, "i", &level)) return NULL;
    res = rtapi_set_msg_level(level);
    if(res) return pyhal_error(res);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *get_msg_level(PyObject * /*_self*/, PyObject * /*args*/) {
    return PyLong_FromLong(rtapi_get_msg_level());
}

static PyBufferProcs shmbuffer_procs = {
    (getbufferproc)shm_buffer_getbuffer,         /* bf_getbuffer */
    (releasebufferproc)NULL, //(releasebufferproc)shm_buffer_releasebuffer, /* bf_releasebuffer */
};

static PyMethodDef shm_methods[] = {
    {"getbuffer", shm_getbuffer, METH_NOARGS,
        "Get a writable buffer object for the shared memory segment"},
    {"setsize", shm_setsize, METH_VARARGS,
        "Set the size of the shared memory segment"},
    {},
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static
PyTypeObject shm_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "hal.shm",                 /*tp_name*/
    sizeof(shmobject),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
   (destructor)pyshm_delete,              /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)pyshm_repr,                /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    &shmbuffer_procs,          /*tp_as_buffer*/
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
    (initproc)pyshm_init,                /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
    0,                         /*tp_bases*/
    0,                         /*tp_mro*/
    0,                         /*tp_cache*/
    0,                         /*tp_subclasses*/
    0,                         /*tp_weaklink*/
    0,                         /*tp_del*/
    0,                         /*tp_version_tag*/
    0,                         /*tp_finalize*/
#if PY_VERSION_HEX >= 0x030800f0	// 3.8
    0,                         /*tp_vectorcall*/
#if PY_VERSION_HEX >= 0x030c00f0	// 3.12
    0,                         /*tp_watched*/
#if PY_VERSION_HEX >= 0x030d00f0	// 3.13
    0,                         /*tp_versions_used*/
#endif
#endif
#endif
};
#pragma GCC diagnostic pop

struct streamobj {
    PyObject_HEAD
    hal_stream_t stream;
    PyObject *pyelt;
    halobject *comp;
    int key;
    bool creator;
    unsigned sampleno;
};

static int pystream_init(PyObject *_self, PyObject *args, PyObject * /*kw*/) {
    int depth=0;
    const char *typestring=NULL;

    streamobj *self = reinterpret_cast<streamobj *>(_self);
    self->sampleno = 0;

    // creating a new stream
    int r;
    if(PyTuple_GET_SIZE(args) == 4)
        r = PyArg_ParseTuple(args, "O!iis:hal.stream",
                &halobject_type, &self->comp, &self->key, &depth, &typestring);
    else
        r = PyArg_ParseTuple(args, "O!i|s:hal.stream",
                &halobject_type, &self->comp, &self->key, &typestring);

    if(!r) return -1;

    Py_XINCREF(self->comp);

    if(depth) {
        self->creator = 1;
        r = hal_stream_create(&self->stream, self->comp->hal_id,
                                self->key, depth, typestring);
    } else {
        self->creator = 0;
        r = hal_stream_attach(&self->stream, self->comp->hal_id, self->key, typestring);
    }
    if(r < 0) { errno = -r; PyErr_SetFromErrno(PyExc_IOError); return -1; }

    int n = hal_stream_element_count(&self->stream);
    PyObject *t = PyBytes_FromStringAndSize(NULL, n);
    if(!t) {
        if(self->creator)
            hal_stream_destroy(&self->stream);
        else
            hal_stream_detach(&self->stream);
        return -1;
    }

    char *tbuf = PyBytes_AsString(t);

    for(int i=0; i<n; i++) {
        switch(hal_stream_element_type(&self->stream, i)) {
        case HAL_BOOL: tbuf[i] = 'b'; break;
        case HAL_REAL: tbuf[i] = 'f'; break;
        case HAL_S32:  tbuf[i] = 's'; break;
        case HAL_U32:  tbuf[i] = 'u'; break;
        case HAL_SINT: tbuf[i] = 'l'; break;
        case HAL_UINT: tbuf[i] = 'k'; break;
        default: tbuf[i] = '?'; break;
        }
    }
    self->pyelt = t;

    return 0;
}

PyObject *stream_read(PyObject *_self, PyObject * /*unused*/) {
    streamobj *self = reinterpret_cast<streamobj *>(_self);
    int n = PyBytes_Size(self->pyelt);
    if(n <= 0) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    std::vector<hal_stream_data_u> buf(n);
    if(hal_stream_read(&self->stream, buf.data(), &self->sampleno) < 0) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject *r = PyTuple_New(n);
    if(!r) return NULL;

    for(int i=0; i<n; i++) {
        PyObject *o;
        switch(PyBytes_AS_STRING(self->pyelt)[i]) {
        case 'B':
        case 'b': o = to_python(buf[i].b); break;
        case 'R':
        case 'r':
        case 'F':
        case 'f': o = to_python(buf[i].f); break;
        case 'S':
        case 's': o = to_python(buf[i].s); break;
        case 'U':
        case 'u': o = to_python(buf[i].u); break;
        case 'L':
        case 'l': o = to_python(buf[i].l); break;
        case 'K':
        case 'k': o = to_python(buf[i].k); break;
        default: Py_INCREF(Py_None); o = Py_None; break;
        }
        if(!o) {
            Py_DECREF(r);
            return NULL;
        }
        PyTuple_SET_ITEM(r, i, o);
    }
    return r;
}

PyObject *stream_write(PyObject *_self, PyObject *args) {
    streamobj *self = reinterpret_cast<streamobj *>(_self);
    PyObject *data;
    if(!PyArg_ParseTuple(args, "O!:hal.stream.write", &PyTuple_Type, &data))
        return NULL;

    int n = PyBytes_Size(self->pyelt);
    if(n < PyTuple_GET_SIZE(data)) {
        PyErr_SetString(PyExc_ValueError, "Too few elements to unpack");
        return NULL;
    }
    if(n > PyTuple_GET_SIZE(data)) {
        PyErr_SetString(PyExc_ValueError, "Too many elements to unpack");
        return NULL;
    }

    std::vector<hal_stream_data_u> buf(n);
    for(int i=0; i<n; i++) {
        PyObject *o = PyTuple_GET_ITEM(data, i);
        switch(PyBytes_AS_STRING(self->pyelt)[i]) {
        case 'B':
        case 'b': if(!from_python(o, &buf[i].b)) return NULL; break;
        case 'R':
        case 'r':
        case 'F':
        case 'f': if(!from_python(o, &buf[i].f)) return NULL; break;
        case 'S':
        case 's': if(!from_python(o, &buf[i].s)) return NULL; break;
        case 'U':
        case 'u': if(!from_python(o, &buf[i].u)) return NULL; break;
        case 'L':
        case 'l': if(!from_python(o, &buf[i].l)) return NULL; break;
        case 'K':
        case 'k': if(!from_python(o, &buf[i].k)) return NULL; break;
        default: memset(&buf[i], 0, sizeof(buf[i])); break;
        }
    }
    int r = hal_stream_write(&self->stream, buf.data());
    if(r < 0) {
        errno = -r; PyErr_SetFromErrno(PyExc_IOError); return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef stream_methods[] = {
    {"read", stream_read, METH_NOARGS, NULL},
    {"write", stream_write, METH_VARARGS, NULL},
    {}
};

#define VFC(f) reinterpret_cast<void*>(f)

template<class T>
PyObject *stream_getter(PyObject *_self, void *vfp) {
    streamobj *self = reinterpret_cast<streamobj*>(_self);
    typedef T (*F)(hal_stream_t*);
    F fn = reinterpret_cast<F>(vfp);
    T result = fn(&self->stream);
    return to_python(result);
}

PyObject *stream_element_types(PyObject *_self, void * /*unused*/) {
    streamobj *self = reinterpret_cast<streamobj*>(_self);
    if(!self->pyelt) {
        PyErr_Format(PyExc_RuntimeError, "Stream type-string object was not set");
        return NULL;
    }
    Py_INCREF(self->pyelt);
    return self->pyelt;
}

static PyMemberDef stream_members[] = {
    {"sampleno", T_UINT, offsetof(streamobj, sampleno), READONLY,
        "The number of the last successfully read sample"},
    {}
};

static PyGetSetDef stream_getset[] = {
    {"readable", stream_getter<bool>, NULL, NULL, VFC(hal_stream_readable)},
    {"writable", stream_getter<bool>, NULL, NULL, VFC(hal_stream_writable)},
    {"depth", stream_getter<int>, NULL, NULL, VFC(hal_stream_depth)},
    {"element_types", stream_element_types, NULL, NULL, NULL},
    {"maxdepth", stream_getter<int>, NULL, NULL, VFC(hal_stream_maxdepth)},
    {"num_underruns", stream_getter<int>, NULL, NULL, VFC(hal_stream_num_underruns)},
    {"num_overruns", stream_getter<int>, NULL, NULL, VFC(hal_stream_num_overruns)},
    {}
};

static void pystream_delete(PyObject *_self) {
    streamobj *self = reinterpret_cast<streamobj*>(_self);
    if(self->creator)
        hal_stream_destroy(&self->stream);
    else
        hal_stream_detach(&self->stream);
    Py_XDECREF(self->pyelt);
    Py_XDECREF(self->comp);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *pystream_repr(PyObject *_self) {
    streamobj *self = reinterpret_cast<streamobj*>(_self);
    return PyUnicode_FromFormat("<stream 0x%x%s>", self->key,
        self->creator ? " creator" : "");
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
static
PyTypeObject stream_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "hal.stream",              /*tp_name*/
    sizeof(streamobj),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    pystream_delete,           /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    pystream_repr,             /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "HAL Stream",              /*tp_doc*/
    0,                         /*tp_traverse*/
    0,                         /*tp_clear*/
    0,                         /*tp_richcompare*/
    0,                         /*tp_weaklistoffset*/
    0,                         /*tp_iter*/
    0,                         /*tp_iternext*/
    stream_methods,            /*tp_methods*/
    stream_members,            /*tp_members*/
    stream_getset,             /*tp_getset*/
    0,                         /*tp_base*/
    0,                         /*tp_dict*/
    0,                         /*tp_descr_get*/
    0,                         /*tp_descr_set*/
    0,                         /*tp_dictoffset*/
    pystream_init,             /*tp_init*/
    0,                         /*tp_alloc*/
    PyType_GenericNew,         /*tp_new*/
    0,                         /*tp_free*/
    0,                         /*tp_is_gc*/
    0,                         /*tp_bases*/
    0,                         /*tp_mro*/
    0,                         /*tp_cache*/
    0,                         /*tp_subclasses*/
    0,                         /*tp_weaklink*/
    0,                         /*tp_del*/
    0,                         /*tp_version_tag*/
    0,                         /*tp_finalize*/
#if PY_VERSION_HEX >= 0x030800f0	// 3.8
    0,                         /*tp_vectorcall*/
#if PY_VERSION_HEX >= 0x030c00f0	// 3.12
    0,                         /*tp_watched*/
#if PY_VERSION_HEX >= 0x030d00f0	// 3.13
    0,                         /*tp_versions_used*/
#endif
#endif
#endif
};
#pragma GCC diagnostic pop


static PyMethodDef module_methods[] = {
    {"pin_has_writer", pin_has_writer, METH_VARARGS,
	".pin_has_writer('pin_name'): Return a FALSE value if a pin has no writers and TRUE if it does"},
    {"component_exists", component_exists, METH_VARARGS,
	".component_exists('component_name'): Return a True value if the named component exists"},
    {"component_is_ready", component_is_ready, METH_VARARGS,
	".component_is_ready('component_name'): Return a True value if the named component is ready"},
    {"set_msg_level", set_msg_level, METH_VARARGS,
	".set_msg_level(level): Set the RTAPI message level"},
    {"get_msg_level", get_msg_level, METH_NOARGS,
	".get_msg_level(): Get the RTAPI message level"},
    {"new_sig", new_sig, METH_VARARGS,
	".new_sig('signal_name', type): Create a new signal with the specified name.  'type' is one of HAL_BIT, HAL_FLOAT, HAL_S32, or HAL_U32."},
    {"connect", connect, METH_VARARGS,
	".connect('pin_name', 'signal_name'): Connect the named pin to the named signal."},
    {"disconnect", disconnect, METH_VARARGS,
	".disconnect('pin_name'): Disconnect the named pin from any signal."},

    {"set_p", set_p, METH_VARARGS,
	".set_p('name', 'value'): Set the pin or param value"},
    {"set_s", set_s, METH_VARARGS,
	".set_s('name', 'value'): Set the signal value"},
    {"get_p", get_p, METH_VARARGS,
	".get_p('name', 'value'): Get the pin or param value. Will return the signal value if it is a pin and connected"},
    {"get_s", get_s, METH_VARARGS,
	".get_s('name', 'value'): Get the signal value"},


    {"get_value", get_value, METH_VARARGS,
	".get_value('name'): Gets the pin, param or signal value"},
    {"get_info_pins", get_info_pins, METH_VARARGS,
	".get_info_pins(): Get a list of dicts for all the pins; {NAME:, VALUE:, DIRECTION:}"},
    {"get_info_signals", get_info_signals, METH_VARARGS,
	".get_info_signals(): Get a list of dicts for all the signals; {NAME:, VALUE:}"},
    {"get_info_params", get_info_params, METH_VARARGS,
	".get_info_params(): Get a list of dicts for all the parameters; {NAME:, VALUE:}"},
    {"get_realtime_type", pyhal_get_realtime_type, METH_NOARGS,
        ".get_realtime_type(): Return the type of the running realtime"},
    {"is_initialized", pyhal_is_initialized, METH_NOARGS,
        ".is_initialized(): Return true if hal is initialized, false otherwise"},
    {},
};

const char *module_doc = "Interface to LinuxCNC's hal\n"
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

static struct PyModuleDef hal_moduledef = {
    PyModuleDef_HEAD_INIT,  /* m_base */
    "_hal",                 /* m_name */
    module_doc,             /* m_doc */
    0,                      /* m_size */
    module_methods,         /* m_methods */
    NULL,                   /* m_slots */
    NULL,                   /* m_traverse */
    NULL,                   /* m_clear */
    NULL,                   /* m_free */
};

static void addToDict(PyObject *dict, const char *key, int val)
{
    PyObject *k = PyUnicode_FromString(key);
    PyObject *v = PyLong_FromLong(val);
    PyDict_SetItem(dict, k, v);
    Py_DECREF(v);
    Py_DECREF(k);
}

static PyObject *dictEnumType(void)
{
    PyObject *dict = PyDict_New();
    if(!dict) {
        return NULL;
    }
    addToDict(dict, "BOOL",  HAL_BOOL);
    addToDict(dict, "SINT",  HAL_SINT);
    addToDict(dict, "UINT",  HAL_UINT);
    addToDict(dict, "REAL",  HAL_REAL);
    addToDict(dict, "PORT",  HAL_PORT);
    // These are here for compatibility
    addToDict(dict, "BIT",   HAL_BIT);
    addToDict(dict, "S32",   HAL_S32);
    addToDict(dict, "U32",   HAL_U32);
    addToDict(dict, "FLOAT", HAL_FLOAT);
    addToDict(dict, "S64",   HAL_S64);
    addToDict(dict, "U64",   HAL_U64);
    return dict;
}

static PyObject *dictEnumDir(void)
{
    PyObject *dict = PyDict_New();
    if(!dict) {
        return NULL;
    }
    addToDict(dict, "IN",  HAL_IN);
    addToDict(dict, "OUT", HAL_OUT);
    addToDict(dict, "IO",  HAL_IO);
    addToDict(dict, "RO",  HAL_RO);
    addToDict(dict, "WO",  HAL_WO);
    addToDict(dict, "RW",  HAL_RW);
    return dict;
}

//
// This effectively does:
//   import enum
//   myenum = enum.IntEnum('MyEnumName', dict(key1=val1, key2=val2, ...))
//   del enum
//   return myenum
//
static PyObject *addModuleEnum(PyObject *mod, PyObject *dict, const char *enumname)
{
    // Import enum
    PyObject *enum_module = PyImport_ImportModule("enum");
    if(!enum_module) {
        PyErr_SetString(PyExc_ImportError, "Failed to import 'enum'");
        return NULL;
    }

    // Get enum.Enum, the class we need
    PyObject *enum_class = PyObject_GetAttrString(enum_module, "IntEnum");
    if(!enum_class) {
        PyErr_SetString(PyExc_ImportError, "Failed to retrieve 'enum.IntEnum'");
        Py_DECREF(enum_module);
        return NULL;
    }
    Py_INCREF(enum_class);  // Take ownership, tp_base references it

    PyObject *name = PyUnicode_FromString(enumname);
    PyObject *args = PyTuple_Pack(2, name, dict);
    Py_DECREF(dict);
    Py_DECREF(name);

    PyObject *kwargs = PyDict_New();
    PyObject *modarg  = PyUnicode_FromString("module");
    PyObject *modname = PyModule_GetNameObject(mod);
    PyDict_SetItem(kwargs, modarg, modname);
    Py_DECREF(modname);
    Py_DECREF(modarg);

    // This calls: result = enum.Enum(<enumname>, dict(key=val,...))
    PyObject *enum_type = PyObject_Call(enum_class, args, kwargs);
    Py_DECREF(kwargs);
    Py_DECREF(args);
    Py_DECREF(enum_class);
    Py_DECREF(enum_module);

    if(!enum_type) {
        PyErr_Format(PyExc_ImportError, "Failed to create 'hal.%s'", enumname);
        return NULL;
    }
    return enum_type;
}

PyMODINIT_FUNC PyInit__hal(void);
PyMODINIT_FUNC PyInit__hal(void)
{
    PyObject *m = PyModule_Create(&hal_moduledef);
    if(!m)
        return NULL;

    int rv;
    if(0 != (rv = hal_lib_init())) {
        PyErr_Format(PyExc_ImportError, "Initializing hal_lib returned error=%d", rv);
	Py_DECREF(m);
        return NULL;
    }

    Py_AtExit(hal_lib_exit);

    pyhal_error_type = PyErr_NewException("hal.error", NULL, NULL);
    PyModule_AddObject(m, "error", pyhal_error_type);

    PyType_Ready(&halobject_type);
    PyType_Ready(&shm_type);
    PyType_Ready(&halpin_type);
    PyType_Ready(&stream_type);
    PyModule_AddObject(m, "component", (PyObject*)&halobject_type);
    PyModule_AddObject(m, "shm", (PyObject*)&shm_type);
    PyModule_AddObject(m, "item", (PyObject*)&halpin_type);
    PyModule_AddObject(m, "stream", (PyObject*)&stream_type);

    // hal.Type.BOOL, REAL,...
    PyObject *et = addModuleEnum(m, dictEnumType(), "Type");
    if(et) {
        PyModule_AddObject(m, "Type", et);
    } else {
	Py_DECREF(m);
        return NULL;
    }
    // hal.Dir.IN, OUT,...
    PyObject *ed = addModuleEnum(m, dictEnumDir(), "Dir");
    if(ed) {
        PyModule_AddObject(m, "Dir", ed);
    } else {
	Py_DECREF(m);
        return NULL;
    }

    PyModule_AddIntConstant(m, "MSG_NONE", RTAPI_MSG_NONE);
    PyModule_AddIntConstant(m, "MSG_ERR", RTAPI_MSG_ERR);
    PyModule_AddIntConstant(m, "MSG_WARN", RTAPI_MSG_WARN);
    PyModule_AddIntConstant(m, "MSG_INFO", RTAPI_MSG_INFO);
    PyModule_AddIntConstant(m, "MSG_DBG", RTAPI_MSG_DBG);
    PyModule_AddIntConstant(m, "MSG_ALL", RTAPI_MSG_ALL);

    PyModule_AddIntConstant(m, "HAL_BOOL", HAL_BOOL);
    PyModule_AddIntConstant(m, "HAL_REAL", HAL_REAL);
    PyModule_AddIntConstant(m, "HAL_SINT", HAL_SINT);
    PyModule_AddIntConstant(m, "HAL_UINT", HAL_UINT);
    PyModule_AddIntConstant(m, "HAL_BIT", HAL_BIT);
    PyModule_AddIntConstant(m, "HAL_FLOAT", HAL_FLOAT);
    PyModule_AddIntConstant(m, "HAL_S32", HAL_S32);
    PyModule_AddIntConstant(m, "HAL_U32", HAL_U32);
    PyModule_AddIntConstant(m, "HAL_S64", HAL_S64);
    PyModule_AddIntConstant(m, "HAL_U64", HAL_U64);
    PyModule_AddIntConstant(m, "HAL_PORT", HAL_PORT);

    PyModule_AddIntConstant(m, "HAL_RO", HAL_RO);
    PyModule_AddIntConstant(m, "HAL_RW", HAL_RW);
    PyModule_AddIntConstant(m, "HAL_IN", HAL_IN);
    PyModule_AddIntConstant(m, "HAL_OUT", HAL_OUT);
    PyModule_AddIntConstant(m, "HAL_IO", HAL_IO);

    PyModule_AddIntConstant(m, "REALTIME_TYPE_UNINITIALIZED", REALTIME_TYPE_UNINITIALIZED);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_NONE", REALTIME_TYPE_NONE);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_UNKNOWN", REALTIME_TYPE_UNKNOWN);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_PREEMPT_DYNAMIC", REALTIME_TYPE_PREEMPT_DYNAMIC);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_PREEMPT_RT", REALTIME_TYPE_PREEMPT_RT);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_RTAI", REALTIME_TYPE_RTAI);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_LXRT", REALTIME_TYPE_LXRT);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_XENOMAI", REALTIME_TYPE_XENOMAI);
    PyModule_AddIntConstant(m, "REALTIME_TYPE_XENOMAI_EVL", REALTIME_TYPE_XENOMAI_EVL);

    PyModule_AddIntConstant(m, "is_kernelspace", rtapi_is_kernelspace());
    PyModule_AddIntConstant(m, "is_userspace", !rtapi_is_kernelspace());

    PyModule_AddIntConstant(m, "streamer_base", 0x48535430);
    PyModule_AddIntConstant(m, "sampler_base", 0x48534130);

#ifdef RTAPI_KERNEL_VERSION
    PyModule_AddStringConstant(m, "kernel_version", RTAPI_KERNEL_VERSION);
#else
    PyModule_AddStringConstant(m, "kernel_version", "Not Available");
#endif

    PyRun_SimpleString(
            "(lambda s=__import__('signal'):"
                 "s.signal(s.SIGTERM, s.default_int_handler))()");
    return m;
}
