/*
    halenum.hh - native IntEnum classes for HAL type/direction tagging

    Single source of truth for the Python-visible enums: the member
    values are the hal.h constants themselves, so the classes cannot
    drift from the C headers. Built through the plain Python C API (the
    enum module's functional interface) so that any extension module can
    create or instantiate them, with or without pybind11.

    _hal builds the two classes at module init and registers them as
    _hal.Type and _hal.Dir; every other consumer fetches those shared
    classes with halenum_shared_class() instead of building its own.

    TODO: when the oldest supported pybind11 is 3.0 or newer, class
    construction can move to py::native_enum and instantiation to its
    casters. The Python-visible classes stay enum.IntEnum either way,
    so user code will not notice the switch.

    Note: this header is deliberately not exported to include/. The
    header-sanity test preprocesses every exported header without
    Python include paths, which <Python.h> does not survive.
*/
#ifndef HALENUM_HH
#define HALENUM_HH

#include <Python.h>

#include <hal.h>

struct halenum_member {
    const char *name;
    long value;
};

// Member order matters: interactive tools pick the first spelling when
// several members share a value, so the table lists the preferred
// spelling of each value first (bool, real, sint, uint, port, s32, u32)
// and the alternatives (s64, u64, and the HAL_* spellings) after. The
// enum module preserves dict order, so the first occurrence of each
// value is the canonical member.
static const halenum_member halenum_type_members[] = {
    {"BOOL", HAL_BOOL},
    {"REAL", HAL_REAL},
    {"SINT", HAL_SINT},
    {"UINT", HAL_UINT},
    {"PORT", HAL_PORT},
    {"S32",  HAL_S32},
    {"U32",  HAL_U32},
    {"S64",  HAL_S64},
    {"U64",  HAL_U64},
    {"HAL_BOOL",  HAL_BOOL},
    {"HAL_REAL",  HAL_REAL},
    {"HAL_SINT",  HAL_SINT},
    {"HAL_UINT",  HAL_UINT},
    {"HAL_PORT",  HAL_PORT},
    {"HAL_S32",   HAL_S32},
    {"HAL_U32",   HAL_U32},
    {"HAL_S64",   HAL_S64},
    {"HAL_U64",   HAL_U64},
    {"HAL_BIT",   HAL_BIT},
    {"HAL_FLOAT", HAL_FLOAT},
};

static const halenum_member halenum_dir_members[] = {
    {"IN",  HAL_IN},
    {"OUT", HAL_OUT},
    {"IO",  HAL_IO},
    {"RO",  HAL_RO},
    {"WO",  HAL_WO},
    {"RW",  HAL_RW},
    {"HAL_IN",  HAL_IN},
    {"HAL_OUT", HAL_OUT},
    {"HAL_IO",  HAL_IO},
    {"HAL_RO",  HAL_RO},
    {"HAL_WO",  HAL_WO},
    {"HAL_RW",  HAL_RW},
};

// Build an enum.IntEnum subclass from a member table. The class claims
// __module__ "hal", its public home, so repr() and pickle look right.
// Returns a new reference, or NULL with an exception set.
static inline PyObject *halenum_build(const char *clsname,
                                      const halenum_member *members, size_t n)
{
    PyObject *enummod = PyImport_ImportModule("enum");
    if(!enummod)
        return NULL;
    PyObject *intenum = PyObject_GetAttrString(enummod, "IntEnum");
    Py_DECREF(enummod);
    if(!intenum)
        return NULL;

    PyObject *names = PyDict_New();
    if(!names) {
        Py_DECREF(intenum);
        return NULL;
    }
    for(size_t i = 0; i < n; i++) {
        PyObject *v = PyLong_FromLong(members[i].value);
        if(!v || PyDict_SetItemString(names, members[i].name, v)) {
            Py_XDECREF(v);
            Py_DECREF(names);
            Py_DECREF(intenum);
            return NULL;
        }
        Py_DECREF(v);
    }

    PyObject *args = Py_BuildValue("(sO)", clsname, names);
    PyObject *kwargs = Py_BuildValue("{ss}", "module", "hal");
    PyObject *cls = (args && kwargs)
        ? PyObject_Call(intenum, args, kwargs) : NULL;
    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    Py_DECREF(names);
    Py_DECREF(intenum);
    return cls;
}

static inline PyObject *halenum_make_type(void)
{
    return halenum_build("Type", halenum_type_members,
                         sizeof(halenum_type_members)/sizeof(halenum_type_members[0]));
}

static inline PyObject *halenum_make_dir(void)
{
    return halenum_build("Dir", halenum_dir_members,
                         sizeof(halenum_dir_members)/sizeof(halenum_dir_members[0]));
}

// Fetch one of the shared classes registered by _hal ("Type" or "Dir"),
// importing _hal if necessary. New reference.
static inline PyObject *halenum_shared_class(const char *attr)
{
    PyObject *m = PyImport_ImportModule("_hal");
    if(!m)
        return NULL;
    PyObject *cls = PyObject_GetAttrString(m, attr);
    Py_DECREF(m);
    return cls;
}

// Instantiate a member: halenum_instance(cls, HAL_FLOAT) is Type.REAL.
// New reference, or NULL with ValueError set for an unknown value.
static inline PyObject *halenum_instance(PyObject *cls, long value)
{
    return PyObject_CallFunction(cls, "l", value);
}

#endif
