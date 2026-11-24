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

//
// The _hal Python module, wrapped by lib/python/hal.py. A pybind11 binding
// of the C++ HAL interface in hal.hh.
//
// Python values are converted to HAL values here, before any HAL call, by
// the lenient rules this module always had (a float into a bool, "yes"
// into a bool, int() truncation into the integer types, ...). The result
// is exact for its HAL type, so hal.hh's strict conversion passes it on
// unchanged, and no Python code runs while the library holds its mutex.
//
#include <pybind11/pybind11.h>

#include <cerrno>
#include <climits>
#include <cstring>
#include <memory>
#include <string>
#include <strings.h>

#include <rtapi.h>
#include <hal.hh>

#include "halqrec.hh"

namespace py = pybind11;
namespace halxx = linuxcnc::hal;

int halquery_add_submodule(PyObject *); // halquery.cc

// hal.error and the class objects the binding checks against. Created
// once at import and never released, as for the enums in halquery.cc, so
// nothing is freed during interpreter teardown.
static PyObject *pyhal_error_type = nullptr;
static PyTypeObject *comp_type = nullptr;
static py::handle rttype_enum;

[[noreturn]] static void raise(PyObject *type, const std::string &msg)
{
    PyErr_SetString(type, msg.c_str());
    throw py::error_already_set();
}

// hal.error with the C library text for an errno, as for all library
// failures of a component.
[[noreturn]] static void raise_hal_errno(int code)
{
    raise(pyhal_error_type, strerror(code));
}

[[noreturn]] static void raise_oserror(int code)
{
    errno = code;
    PyErr_SetFromErrno(PyExc_OSError);
    throw py::error_already_set();
}

//----------------------------------------------------------------------
// Python <=> HAL value conversion
//----------------------------------------------------------------------

// int(o), a new reference
static py::object as_pylong(PyObject *o)
{
    if(PyLong_Check(o))
        return py::reinterpret_borrow<py::object>(o);
    PyObject *t = PyNumber_Long(o);
    if(!t)
        throw py::error_already_set();
    return py::reinterpret_steal<py::object>(t);
}

static rtapi_sint to_sint(PyObject *o)
{
    py::object l = as_pylong(o);
    long long v = PyLong_AsLongLong(l.ptr());
    if(-1 == v && PyErr_Occurred())
        throw py::error_already_set();
    return v;
}

static rtapi_uint to_uint(PyObject *o)
{
    py::object l = as_pylong(o);
    unsigned long long v = PyLong_AsUnsignedLongLong(l.ptr());
    if((unsigned long long)-1 == v && PyErr_Occurred())
        throw py::error_already_set();
    return v;
}

// The buffer size of a port signal.
static rtapi_uint to_u32(PyObject *o)
{
    rtapi_sint v = to_sint(o);
    if(v < 0 || v > RTAPI_UINT32_MAX)
        raise(PyExc_OverflowError, fmt::format("Value {} out of range", v));
    return (rtapi_uint)v;
}

static rtapi_bool to_bool(PyObject *o)
{
    if(PyBool_Check(o))
        return Py_True == o;
    // Text as in an ini file. Note that bool("False") is True in Python.
    if(PyUnicode_Check(o)) {
        const char *s = PyUnicode_AsUTF8AndSize(o, nullptr);
        if(!s) {
            PyErr_Clear();
            raise(PyExc_RuntimeError, "Invalid UTF-8 detected");
        }
        static const struct {
            const char *name;
            bool value;
        } boolnames[] = {
            { "1",     true  },
            { "0",     false },
            { "true",  true  },
            { "false", false },
            { "on",    true  },
            { "off",   false },
            { "yes",   true  },
            { "no",    false },
        };
        for(const auto &bn : boolnames) {
            if(!strcasecmp(s, bn.name))
                return bn.value;
        }
    }
    // A float is false only when it is 0.0
    if(PyFloat_Check(o))
        return 0.0 != PyFloat_AsDouble(o);
    return 0 != to_sint(o);
}

static rtapi_real to_real(PyObject *o)
{
    if(PyFloat_Check(o))
        return PyFloat_AsDouble(o);
    if(PyLong_Check(o)) {
        double d = PyLong_AsDouble(o);
        if(-1.0 == d && PyErr_Occurred())
            throw py::error_already_set();
        return d;
    }
    PyObject *t = PyNumber_Float(o);
    if(!t) {
        PyErr_Clear();
        raise(PyExc_TypeError, fmt::format("Number expected, not {}", Py_TYPE(o)->tp_name));
    }
    double d = PyFloat_AsDouble(t);
    Py_DECREF(t);
    return d;
}

// A Python value as an exact HAL value of type t. A port takes its
// buffer size, which only a port signal can be set to.
static halxx::value_t value_from_python(hal_type_t t, PyObject *o)
{
    switch(t) {
    case HAL_BOOL: return halxx::value_t(std::in_place_type<rtapi_bool>, to_bool(o));
    case HAL_SINT: return halxx::value_t(std::in_place_type<rtapi_sint>, to_sint(o));
    case HAL_UINT: return halxx::value_t(std::in_place_type<rtapi_uint>, to_uint(o));
    case HAL_REAL: return halxx::value_t(std::in_place_type<rtapi_real>, to_real(o));
    case HAL_PORT: return halxx::value_t(std::in_place_type<rtapi_uint>, to_u32(o));
    default:
        raise(pyhal_error_type, fmt::format("Invalid item type {}", (int)t));
    }
}

static py::object value_to_python(const halxx::value_t &v)
{
    return std::visit([](auto x) -> py::object {
        using X = decltype(x);
        if constexpr(std::is_same_v<X, rtapi_bool>)
            return py::bool_(x);
        else if constexpr(std::is_same_v<X, rtapi_real>)
            return py::float_(x);
        else
            return py::int_(x);
    }, v);
}

static unsigned port_size(hal_port_t p)
{
    return 0 != hal_get_port(p) ? hal_port_buffer_size(p) : 0;
}

// The value behind a by-name query reference. A port reads as its buffer
// size when 'portsize' is set (pins and signals), else as 0.
static py::object ref_value(hal_type_t type, const hal_refs_u &ref, bool portsize)
{
    switch(type) {
    case HAL_BOOL: return py::bool_(hal_get_bool(ref.b));
    case HAL_UINT: return py::int_(hal_get_uint(ref.u));
    case HAL_SINT: return py::int_(hal_get_sint(ref.s));
    case HAL_REAL: return py::float_(hal_get_real(ref.r));
    case HAL_PORT: return py::int_(portsize ? port_size(ref.p) : 0u);
    default: return py::none();
    }
}

//----------------------------------------------------------------------
// hal.component and hal.item
//----------------------------------------------------------------------
// The binding's classes have internal linkage, like the pybind11 types
// they hold.
namespace {

struct PyComp {
    PyComp(const std::string &n, const std::string &prefix)
        : name(n), c(std::make_unique<halxx::component>(n)) {
        c->setprefix(prefix);
    }
    bool live() const { return c && c->id() > 0; }

    std::string name;
    std::unique_ptr<halxx::component> c;    // reset by exit()
};

// An item holds its component, so the HAL pin or param it refers to stays
// in place as long as the item exists.
struct PyItem {
    py::object comp;
    std::string name;   // short name, without the prefix
    halxx::anypin pin;
};

} // namespace

// The component of 'self', which must not have exited.
static PyComp &live_comp(py::handle self)
{
    if(!PyObject_TypeCheck(self.ptr(), comp_type))
        raise(PyExc_TypeError, fmt::format("descriptor requires a 'hal.component' object but received a '{}'",
                                           Py_TYPE(self.ptr())->tp_name));
    PyComp *pc = nullptr;
    try {
        pc = py::cast<PyComp *>(self);
    } catch(const py::cast_error &) {
        pc = nullptr;
    }
    if(!pc || !pc->live())
        raise(PyExc_RuntimeError, "Invalid operation on closed HAL component");
    return *pc;
}

static halxx::anypin &find_item(PyComp &pc, const char *name)
{
    if(!pc.c->contains(name))
        raise(PyExc_AttributeError, fmt::format("Pin or param '{}' does not exist", name));
    return pc.c->item(name);
}

// The value of an item: the buffer size for a port pin.
static py::object item_read(const halxx::anypin &p)
{
    if(p.is_port())
        return py::int_(p.is_pin() ? p.as_port().size() : 0u);
    return value_to_python(p.get());
}

static void item_write(halxx::anypin &p, PyObject *v)
{
    hal_type_t t = p.type();
    if(HAL_PORT == t)
        raise(pyhal_error_type, fmt::format("Invalid pin type {}", (int)t));
    p.set(value_from_python(t, v));
}

static const char *item_name(PyObject *name)
{
    const char *s = PyUnicode_AsUTF8(name);
    if(!s)
        throw py::error_already_set();
    return s;
}

// Attribute and item lookup: the normal lookup first (methods, class and
// instance attributes), then the value of the pin or param of that name.
static py::object comp_getattr(py::handle self, py::handle name)
{
    PyComp &pc = live_comp(self);
    PyObject *r = PyObject_GenericGetAttr(self.ptr(), name.ptr());
    if(r)
        return py::reinterpret_steal<py::object>(r);
    PyErr_Clear();
    return item_read(find_item(pc, item_name(name.ptr())));
}

// Attribute and item assignment always write a pin or param.
static void comp_setattr(py::handle self, py::handle name, py::handle v)
{
    PyComp &pc = live_comp(self);
    item_write(find_item(pc, item_name(name.ptr())), v.ptr());
}

static void comp_delattr(py::handle self, py::handle name)
{
    PyComp &pc = live_comp(self);
    const char *s = item_name(name.ptr());
    find_item(pc, s);
    raise(PyExc_TypeError, fmt::format("Pin or param '{}' cannot be deleted", s));
}

static py::object comp_new_item(py::handle self, const std::string &name, int type, int dir, bool ispin)
{
    PyComp &pc = live_comp(self);
    if(pc.c->contains(name))
        raise(PyExc_ValueError, fmt::format("Duplicate {} name '{}'", ispin ? "pin" : "parameter", name));
    switch(type) {
    case HAL_BOOL:
    case HAL_SINT:
    case HAL_UINT:
    case HAL_REAL:
        break;
    case HAL_PORT:
        if(ispin)
            break;
        [[fallthrough]];
    default:
        raise(pyhal_error_type, fmt::format("Invalid {} type {}", ispin ? "pin" : "param", type));
    }
    const std::string &prefix = pc.c->getprefix();
    if(ispin && prefix.size() + 1 + name.size() > HAL_NAME_LEN)
        raise(pyhal_error_type, fmt::format("Invalid pin name length \"{}.{}\": max = {} characters",
                                            prefix, name, HAL_NAME_LEN));
    try {
        halxx::anypin p = ispin ? pc.c->newpin(name, (hal_type_t)type, (halxx::dir)dir)
                                : pc.c->newparam(name, (hal_type_t)type, (halxx::dir)dir);
        return py::cast(PyItem{py::reinterpret_borrow<py::object>(self), name, std::move(p)});
    } catch(const std::system_error &e) {
        raise_hal_errno(e.code().value());
    } catch(const std::bad_alloc &) {
        raise(PyExc_MemoryError, "hal_malloc failed");
    }
}

enum what_type_e {
    WHAT_ANY,
    WHAT_PIN,
    WHAT_PARAM,
};
static py::object comp_get_item(py::handle self, const std::string &name, what_type_e what)
{
    PyComp &pc = live_comp(self);
    halxx::anypin &p = find_item(pc, name.c_str());
    if(what == WHAT_ANY || (p.is_pin() && what == WHAT_PIN) || (!p.is_pin() && what == WHAT_PARAM))
        return py::cast(PyItem{py::reinterpret_borrow<py::object>(self), name, p.dup()});
    raise(PyExc_AttributeError, fmt::format("{} '{}' does not exist", what == WHAT_PIN ? "Pin" : "Param", name));
}

static PyComp &live_item(const PyItem &it)
{
    return live_comp(it.comp);
}

static const char *pin_type2name(hal_type_t type)
{
    switch(type) {
    case HAL_BOOL: return "BOOL";
    case HAL_SINT: return "SINT";
    case HAL_UINT: return "UINT";
    case HAL_REAL: return "REAL";
    case HAL_PORT: return "PORT";
    default: return "unknown";
    }
}

static const char *pin_dir2name(halxx::dir d)
{
    switch((hal_pdir_t)d) {
    case HAL_IN:  return "IN";
    case HAL_IO:  return "IO";
    case HAL_OUT: return "OUT";
    case HAL_RO:  return "RO";
    case HAL_RW:  return "RW";
    default: return "unknown";
    }
}

//----------------------------------------------------------------------
// Port pin methods. Reads and writes are all or nothing; a count out of
// range or more than the port holds returns False.
//----------------------------------------------------------------------
static const halxx::port &check_port(const PyItem &it, const char *pfx)
{
    live_item(it);
    if(!it.pin.is_pin())
        raise(PyExc_RuntimeError, fmt::format("{}: {}: Not a pin", pfx, it.name));
    if(!it.pin.is_port())
        raise(PyExc_RuntimeError, fmt::format("{}: {}: Pin type not HAL_PORT but '{}'", pfx, it.name, (int)it.pin.type()));
    return it.pin.as_port();
}

static void check_dir(const PyItem &it, const char *pfx, halxx::dir d)
{
    if(it.pin.direction() != d)
        raise(PyExc_RuntimeError, fmt::format("{}: {}: Pin not {}", pfx, it.name,
                                              halxx::dir::OUT == d ? "output" : "input"));
}

static unsigned long port_count(const PyItem &it, const char *pfx, py::handle o)
{
    if(!PyLong_Check(o.ptr()))
        raise(PyExc_RuntimeError, fmt::format("{}: {}: Argument not an integer", pfx, it.name));
    unsigned long l = PyLong_AsUnsignedLong(o.ptr());
    if((unsigned long)-1 == l && PyErr_Occurred())
        throw py::error_already_set();
    return l;
}

static py::object port_write(const PyItem &it, py::handle o)
{
    const halxx::port &p = check_port(it, "write");
    check_dir(it, "write", halxx::dir::OUT);

    Py_ssize_t len = 0;
    const char *cptr;
    if(PyUnicode_Check(o.ptr())) {
        cptr = PyUnicode_AsUTF8AndSize(o.ptr(), &len);
    } else if(PyBytes_Check(o.ptr())) {
        cptr = PyBytes_AsString(o.ptr());
        len = PyBytes_Size(o.ptr());
    } else {
        raise(PyExc_RuntimeError, fmt::format("write: {}: Argument not a string or bytes object", it.name));
    }
    if(!cptr) {
        PyErr_Clear();
        raise(PyExc_RuntimeError, fmt::format("write: {}: Object conversion to bytes failed", it.name));
    }
    if(len < 1 || len > HAL_PORT_SIZE_MAX || len > (Py_ssize_t)p.writable())
        return py::bool_(false);
    return py::bool_(p.write(cptr, (unsigned)len));
}

static py::object port_read_peek(const PyItem &it, py::handle o, bool isread)
{
    const char *pfx = isread ? "read" : "peek";
    const halxx::port &p = check_port(it, pfx);
    check_dir(it, pfx, halxx::dir::IN);
    unsigned long l = port_count(it, pfx, o);
    if(l < 1 || l > HAL_PORT_SIZE_MAX || l > p.readable())
        return py::bool_(false);
    std::string buf(l, '\0');
    if(!(isread ? p.read(buf.data(), (unsigned)l) : p.peek(buf.data(), (unsigned)l)))
        return py::bool_(false);
    return py::bytes(buf);
}

//----------------------------------------------------------------------
// hal.stream
//----------------------------------------------------------------------
// The stream member is declared last, so it is closed before the
// reference to its component is dropped.
namespace {
struct PyStream {
    PyStream(py::object c, halxx::component &hc, int key, unsigned depth, const std::string &ts)
        : comp(std::move(c)), s(hc, key, depth, ts) {}
    PyStream(py::object c, halxx::component &hc, int key, const std::string &ts)
        : comp(std::move(c)), s(hc, key, ts) {}

    py::object comp;
    halxx::stream s;
};
} // namespace

// A positional 'int' argument of a constructor that takes *args.
static int int_arg(py::handle o, const char *fn, int argno)
{
    if(PyFloat_Check(o.ptr()))
        raise(PyExc_TypeError, fmt::format("{}() argument {} must be int, not float", fn, argno));
    PyObject *idx = PyNumber_Index(o.ptr());
    if(!idx)
        throw py::error_already_set();
    long v = PyLong_AsLong(idx);
    Py_DECREF(idx);
    if(-1 == v && PyErr_Occurred())
        throw py::error_already_set();
    if(v < INT_MIN || v > INT_MAX)
        raise(PyExc_OverflowError, "signed integer is out of range");
    return (int)v;
}

static std::string str_arg(py::handle o, const char *fn, int argno)
{
    if(!PyUnicode_Check(o.ptr()))
        raise(PyExc_TypeError, fmt::format("{}() argument {} must be str, not {}", fn, argno, Py_TYPE(o.ptr())->tp_name));
    Py_ssize_t len;
    const char *s = PyUnicode_AsUTF8AndSize(o.ptr(), &len);
    if(!s)
        throw py::error_already_set();
    return std::string(s, len);
}

// hal.stream(comp, key, depth, typestring) creates a stream, as does
// any call with four arguments and a non-zero depth; hal.stream(comp,
// key[, typestring]) attaches to one. Keyword arguments are ignored.
static std::unique_ptr<PyStream> stream_new(py::args args, py::kwargs)
{
    size_t n = args.size();
    if(n < 2 || n > 4)
        raise(PyExc_TypeError, fmt::format("hal.stream() takes from 2 to 4 positional arguments but {} were given", n));
    if(!PyObject_TypeCheck(args[0].ptr(), comp_type))
        raise(PyExc_TypeError, fmt::format("hal.stream() argument 1 must be hal.component, not {}",
                                           Py_TYPE(args[0].ptr())->tp_name));
    int key = int_arg(args[1], "hal.stream", 2);
    int depth = 0;
    std::string typestring;
    if(4 == n) {
        depth = int_arg(args[2], "hal.stream", 3);
        typestring = str_arg(args[3], "hal.stream", 4);
    } else if(3 == n) {
        typestring = str_arg(args[2], "hal.stream", 3);
    }
    PyComp &pc = live_comp(args[0]);
    py::object comp = py::reinterpret_borrow<py::object>(args[0]);
    try {
        if(depth)
            return std::make_unique<PyStream>(comp, *pc.c, key, (unsigned)depth, typestring);
        return std::make_unique<PyStream>(comp, *pc.c, key, typestring);
    } catch(const std::system_error &e) {
        raise_oserror(e.code().value());
    }
}

static py::object stream_read(PyStream &ps)
{
    auto sample = ps.s.read();
    if(!sample)
        return py::none();
    py::tuple t(sample->size());
    for(size_t i = 0; i < sample->size(); i++)
        t[i] = value_to_python((*sample)[i]);
    return t;
}

static void stream_write(PyStream &ps, py::handle data)
{
    if(!PyTuple_Check(data.ptr()))
        raise(PyExc_TypeError, fmt::format("hal.stream.write() argument 1 must be tuple, not {}",
                                           Py_TYPE(data.ptr())->tp_name));
    size_t n = (size_t)ps.s.element_count();
    size_t nd = (size_t)PyTuple_GET_SIZE(data.ptr());
    if(nd > n)
        raise(PyExc_ValueError, "Too many elements to unpack");
    if(nd < n)
        raise(PyExc_ValueError, "Too few elements to unpack");
    std::vector<halxx::value_t> buf;
    buf.reserve(n);
    for(size_t i = 0; i < n; i++)
        buf.push_back(value_from_python(ps.s.element_type((int)i), PyTuple_GET_ITEM(data.ptr(), i)));
    try {
        ps.s.write(buf);
    } catch(const std::system_error &e) {
        raise_oserror(e.code().value());
    }
}

//----------------------------------------------------------------------
// Module functions
//----------------------------------------------------------------------
[[noreturn]] static void raise_query(const char *fn, const std::string &name, const std::system_error &e)
{
    raise(PyExc_RuntimeError, fmt::format("{}: {}: {}", fn, name, hal_strerror(-e.code().value())));
}

static py::object pin_has_writer(const std::string &name)
{
    std::optional<hal_query_t> q;
    try {
        q = halxx::pin_param_query(name, HAL_QTYPE_PIN);
    } catch(const std::system_error &e) {
        raise(PyExc_RuntimeError, fmt::format("pin_has_writer: {}: returned error: {}", name,
                                              hal_strerror(-e.code().value())));
    }
    if(!q)
        raise(PyExc_NameError, fmt::format("Pin `{}' does not exist", name));
    if(!q->pp.signal)
        return py::bool_(false);   // No signal, no writer
    std::string sig = q->pp.signal;
    int rv = -ENOENT;
    try {
        if(auto s = halxx::signal_query(sig))
            return py::bool_(s->sig.writers > 0);
    } catch(const std::system_error &e) {
        rv = -e.code().value();
    }
    raise(PyExc_NameError, fmt::format("Signal '{}' of pin '{}' gave unexpected error={}", sig, name, rv));
}

static py::object component_exists(const std::string &name)
{
    try {
        return py::bool_(halxx::component_query(name).has_value());
    } catch(const std::system_error &e) {
        raise(PyExc_RuntimeError, fmt::format("component_exists: hal_comp_by_name '{}' returned {}",
                                              name, -e.code().value()));
    }
}

static py::object component_is_ready(const std::string &name)
{
    try {
        return py::bool_(halxx::component_is_ready(name));
    } catch(const std::system_error &) {
        return py::bool_(false);
    }
}

static void set_msg_level(int level)
{
    int res = rtapi_set_msg_level(level);
    if(res)
        raise_hal_errno(-res);
}

// new_sig(), connect() and disconnect() return True when the library
// call failed, as they always did.
static py::object new_sig(const std::string &name, int type)
{
    switch(type) {
    case HAL_BOOL:
    case HAL_SINT:
    case HAL_UINT:
    case HAL_REAL:
    case HAL_PORT:
        return py::bool_(0 != halxx::signal_new(name, (hal_type_t)type));
    default:
        raise(PyExc_RuntimeError, "not a valid HAL signal type");
    }
}

static py::object connect(const std::string &pinname, const std::string &signame)
{
    return py::bool_(0 != halxx::link(pinname, signame));
}

static py::object disconnect(const std::string &pinname)
{
    return py::bool_(0 != halxx::unlink(pinname));
}

// set_p() and set_s() look the item up for its type, convert the value
// to it and hand it to the library. Any failure is a RuntimeError with
// the library's description.
static py::object set_p(const std::string &name, py::handle obj)
{
    std::optional<hal_query_t> q;
    try {
        q = halxx::pin_param_query(name);
    } catch(const std::system_error &e) {
        raise_query("set_p", name, e);
    }
    if(!q)
        raise(PyExc_RuntimeError, fmt::format("set_p: {}: {}", name, hal_strerror(-ENOENT)));
    halxx::value_t v;
    try {
        v = value_from_python(q->pp.type, obj.ptr());
    } catch(const py::error_already_set &) {
        raise(PyExc_RuntimeError, fmt::format("set_p: {}: {}", name, hal_strerror(-EINVAL)));
    }
    try {
        halxx::set_value(name, v);
    } catch(const std::out_of_range &) {
        raise(PyExc_RuntimeError, fmt::format("set_p: {}: {}", name, hal_strerror(-ERANGE)));
    } catch(const std::system_error &e) {
        raise_query("set_p", name, e);
    }
    return py::bool_(true);
}

static py::object set_s(const std::string &name, py::handle obj)
{
    std::optional<hal_query_t> q;
    try {
        q = halxx::signal_query(name);
    } catch(const std::system_error &e) {
        raise_query("set_s", name, e);
    }
    if(!q)
        raise(PyExc_RuntimeError, fmt::format("set_s: {}: {}", name, hal_strerror(-ENOENT)));
    halxx::value_t v;
    try {
        v = value_from_python(q->sig.type, obj.ptr());
    } catch(const py::error_already_set &) {
        raise(PyExc_RuntimeError, fmt::format("set_s: {}: {}", name, hal_strerror(-EINVAL)));
    }
    try {
        halxx::set_signal(name, v);
    } catch(const std::out_of_range &) {
        raise(PyExc_RuntimeError, fmt::format("set_s: {}: {}", name, hal_strerror(-ERANGE)));
    } catch(const std::system_error &e) {
        raise_query("set_s", name, e);
    }
    return py::bool_(true);
}

// get_p() and get_s() return None when there is no such item; a
// connected pin reads its signal's value. A port pin reads 0, a port
// signal its buffer size.
static py::object get_p(const std::string &name)
{
    try {
        if(auto q = halxx::pin_param_query(name))
            return ref_value(q->pp.type, q->pp.ref, false);
    } catch(const std::system_error &e) {
        raise_query("get_p", name, e);
    }
    return py::none();
}

static py::object get_s(const std::string &name)
{
    try {
        if(auto q = halxx::signal_query(name))
            return ref_value(q->sig.type, q->sig.ref, true);
    } catch(const std::system_error &e) {
        raise_query("get_s", name, e);
    }
    return py::none();
}

// get_value() tries pins and params, then signals, and raises when
// there is neither.
static py::object get_value(const std::string &name)
{
    try {
        if(auto q = halxx::pin_param_query(name))
            return ref_value(q->pp.type, q->pp.ref, false);
        if(auto q = halxx::signal_query(name))
            return ref_value(q->sig.type, q->sig.ref, true);
    } catch(const std::system_error &e) {
        raise_query("get_value", name, e);
    }
    raise(PyExc_RuntimeError, fmt::format("get_value: {}: {}", name, hal_strerror(-ENOENT)));
}

static bool known_type(hal_type_t t)
{
    return HAL_BOOL == t || HAL_SINT == t || HAL_UINT == t || HAL_REAL == t || HAL_PORT == t;
}

// A list of {NAME, VALUE, DIRECTION, TYPE} dicts for all pins or params.
static py::list get_info_pp(bool ispin)
{
    HalQRec qrec(ispin ? 1024 : 512); // We normally have many of them
    hal_query_t q = {};
    q.qtype = ispin ? HAL_QTYPE_PIN : HAL_QTYPE_PARAM;
    int rv = hal_list_p(&q, HalQRec::get_qrec_cb, &qrec);
    if(0 != rv)
        raise(PyExc_RuntimeError, fmt::format("hal_list_p: returned '{}' ({})", hal_strerror(rv), rv));

    py::list lst;
    for(size_t i = 0; i < qrec.size(); i++) {
        const hal_query_t *r = qrec.rec(i);
        bool known = known_type(r->pp.type);
        py::dict d;
        d["NAME"] = r->name;
        d["VALUE"] = ref_value(r->pp.type, r->pp.ref, ispin);
        d["DIRECTION"] = py::int_((int)r->pp.dir);
        d["TYPE"] = known ? py::object(py::int_((int)r->pp.type)) : py::object(py::none());
        lst.append(d);
    }
    return lst;
}

static int siginfo_writer_cb(hal_query_t *q, void *arg) noexcept
{
    if(HAL_OUT == q->pp.dir) {
        // Found the writer, record and quit the loop
        *((const char **)arg) = q->name;
        return 1;
    }
    return 0;
}

// A list of {NAME, VALUE, DRIVER, TYPE} dicts for all signals.
static py::list get_info_signals(py::args)
{
    HalQRec qrec(256); // We normally have many signals
    hal_query_t q = {};
    int rv = hal_list_s(&q, HalQRec::get_qrec_cb, &qrec);
    if(0 != rv)
        raise(PyExc_RuntimeError, fmt::format("hal_list_s: returned '{}' ({})", hal_strerror(rv), rv));

    py::list lst;
    for(size_t i = 0; i < qrec.size(); i++) {
        const hal_query_t *r = qrec.rec(i);
        const char *writer = nullptr;
        if(r->sig.writers > 0) {
            // Retrieve the writer pin name
            hal_query_t qd = {};
            qd.name = r->name;
            hal_list_p_s(&qd, siginfo_writer_cb, (void *)&writer);
        }
        bool known = known_type(r->sig.type);
        py::dict d;
        d["NAME"] = r->name;
        d["VALUE"] = ref_value(r->sig.type, r->sig.ref, true);
        d["DRIVER"] = writer ? py::object(py::str(writer)) : py::object(py::none());
        d["TYPE"] = known ? py::object(py::int_((int)r->sig.type)) : py::object(py::none());
        lst.append(d);
    }
    return lst;
}

//----------------------------------------------------------------------
// Module
//----------------------------------------------------------------------
static const char module_doc[] = "Interface to LinuxCNC's hal\n"
"\n"
"This module allows the creation of userspace HAL components in Python.\n"
"This includes pins and parameters of the various HAL types.\n"
"\n"
"Typical usage:\n"
"\n"
"import hal, time\n"
"h = hal.component(\"component-name\")\n"
"# create pins and parameters with calls to h.newpin and h.newparam\n"
"h.newpin(\"in\", hal.Type.REAL, hal.Dir.IN)\n"
"h.newpin(\"out\", hal.Type.REAL, hal.Dir.OUT)\n"
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

// Member order matters: interactive tools pick the first spelling when
// several members share a value, so the table lists the preferred
// spelling of each value first and the HAL_* spellings after. The enum
// module preserves dict order, so the first occurrence of each value is
// the canonical member.
struct halenum_member_t {
    const char *name;
    long value;
};

static const halenum_member_t halenum_type_members[] = {
    {"BOOL", HAL_BOOL},
    {"REAL", HAL_REAL},
    {"SINT", HAL_SINT},
    {"UINT", HAL_UINT},
    {"PORT", HAL_PORT},
    {"HAL_BOOL",  HAL_BOOL},
    {"HAL_REAL",  HAL_REAL},
    {"HAL_SINT",  HAL_SINT},
    {"HAL_UINT",  HAL_UINT},
    {"HAL_PORT",  HAL_PORT},
    {nullptr, 0}
};

static const halenum_member_t halenum_dir_members[] = {
    {"IN",  HAL_IN},
    {"OUT", HAL_OUT},
    {"IO",  HAL_IO},
    {"RO",  HAL_RO},
    {"RW",  HAL_RW},
    {"HAL_IN",  HAL_IN},
    {"HAL_OUT", HAL_OUT},
    {"HAL_IO",  HAL_IO},
    {"HAL_RO",  HAL_RO},
    {"HAL_RW",  HAL_RW},
    {nullptr, 0}
};

// Not previously registered, no need to have compat names.
static const halenum_member_t halenum_comp_members[] = {
    {"UNKNOWN",  HAL_COMP_TYPE_UNKNOWN},
    {"USER",     HAL_COMP_TYPE_USER},
    {"REALTIME", HAL_COMP_TYPE_REALTIME},
    {"OTHER",    HAL_COMP_TYPE_OTHER},
    {nullptr, 0}
};

// Previously registered, but it is new so we don't do compat names.
static const halenum_member_t halenum_rt_members[] = {
    {"UNINITIALIZED",   REALTIME_TYPE_UNINITIALIZED},
    {"NONE",            REALTIME_TYPE_NONE},
    {"UNKNOWN",         REALTIME_TYPE_UNKNOWN},
    {"PREEMPT_DYNAMIC", REALTIME_TYPE_PREEMPT_DYNAMIC},
    {"PREEMPT_RT",      REALTIME_TYPE_PREEMPT_RT},
    {"RTAI",            REALTIME_TYPE_RTAI},
    {"LXRT",            REALTIME_TYPE_LXRT},
    {"XENOMAI",         REALTIME_TYPE_XENOMAI},
    {"XENOMAI_EVL",     REALTIME_TYPE_XENOMAI_EVL},
    {nullptr, 0}
};

// An enum.IntEnum subclass from a member table. The class claims
// __module__ "hal", its public home, so repr() and pickle look right.
static py::object halenum_build(const char *clsname, const halenum_member_t *members)
{
    py::dict names;
    for(size_t i = 0; members[i].name; i++)
        names[members[i].name] = py::int_(members[i].value);
    return py::module_::import("enum").attr("IntEnum")(clsname, names, py::arg("module") = "hal");
}

PYBIND11_MODULE(_hal, m)
{
    m.doc() = module_doc;

    m.def("pin_has_writer", &pin_has_writer,
        ".pin_has_writer('pin_name'): Return a FALSE value if a pin has no writers and TRUE if it does");
    m.def("component_exists", &component_exists,
        ".component_exists('component_name'): Return a True value if the named component exists");
    m.def("component_is_ready", &component_is_ready,
        ".component_is_ready('component_name'): Return a True value if the named component is ready");
    m.def("set_msg_level", &set_msg_level,
        ".set_msg_level(level): Set the RTAPI message level");
    m.def("get_msg_level", []() { return rtapi_get_msg_level(); },
        ".get_msg_level(): Get the RTAPI message level");
    m.def("new_sig", &new_sig,
        ".new_sig('signal_name', type): Create a new signal with the specified name.  'type' is one of hal.Type.[BOOL,REAL,SINT,UINT,PORT].");
    m.def("connect", &connect,
        ".connect('pin_name', 'signal_name'): Connect the named pin to the named signal.");
    m.def("disconnect", &disconnect,
        ".disconnect('pin_name'): Disconnect the named pin from any signal.");
    m.def("set_p", &set_p,
        ".set_p('name', value): Set the pin or param value");
    m.def("set_s", &set_s,
        ".set_s('name', value): Set the signal value");
    m.def("get_p", &get_p,
        ".get_p('name'): Get the pin or param value. Will return the signal value if it is a pin and connected");
    m.def("get_s", &get_s,
        ".get_s('name'): Get the signal value");
    m.def("get_value", &get_value,
        ".get_value('name'): Gets the pin, param or signal value");
    m.def("get_info_pins", [](py::args) { return get_info_pp(true); },
        ".get_info_pins(): Get a list of dicts for all the pins; {NAME:, VALUE:, DIRECTION:, TYPE:}");
    m.def("get_info_signals", &get_info_signals,
        ".get_info_signals(): Get a list of dicts for all the signals; {NAME:, VALUE:, DRIVER:, TYPE:}");
    m.def("get_info_params", [](py::args) { return get_info_pp(false); },
        ".get_info_params(): Get a list of dicts for all the parameters; {NAME:, VALUE:, DIRECTION:, TYPE:}");
    m.def("get_realtime_type", []() { return rttype_enum((long)hal_get_realtime_type()); },
        ".get_realtime_type(): Return the type of the running realtime");
    m.def("is_initialized", []() { return py::bool_(hal_is_init()); },
        ".is_initialized(): Return true if hal is initialized, false otherwise");

    // A component, a pin or param of one, or a stream needs the HAL shared
    // memory, and so do all the by-name functions; map it now.
    int rv = hal_lib_init();
    if(0 != rv)
        throw std::runtime_error(fmt::format("Initializing hal_lib returned error={}", rv));
    Py_AtExit(hal_lib_exit);

    pyhal_error_type = PyErr_NewException("hal.error", nullptr, nullptr);
    if(!pyhal_error_type)
        throw py::error_already_set();
    m.attr("error") = py::handle(pyhal_error_type);

    py::class_<PyComp> comp(m, "component", "HAL Component");
    comp_type = (PyTypeObject *)comp.ptr();
    comp.attr("__module__") = "hal";
    comp
        // hal.component(name[, prefix]). Keyword arguments are ignored.
        .def(py::init([](py::args args, py::kwargs) {
            size_t n = args.size();
            if(n < 1)
                raise(PyExc_TypeError, "hal.component() takes at least 1 argument (0 given)");
            if(n > 2)
                raise(PyExc_TypeError, fmt::format("hal.component() takes at most 2 arguments ({} given)", n));
            std::string name = str_arg(args[0], "hal.component", 1);
            std::string prefix = n > 1 ? str_arg(args[1], "hal.component", 2) : name;
            try {
                return std::make_unique<PyComp>(name, prefix);
            } catch(const std::system_error &e) {
                raise_hal_errno(e.code().value());
            }
        }))
        .def("setprefix", [](py::handle self, const std::string &prefix) {
            live_comp(self).c->setprefix(prefix);
        }, "Set the prefix for newly created pins and parameters")
        .def("getprefix", [](py::handle self) {
            return live_comp(self).c->getprefix();
        }, "Get the prefix for newly created pins and parameters")
        .def("newparam", [](py::handle self, const std::string &name, int type, int dir) {
            return comp_new_item(self, name, type, dir, false);
        }, "Create a new parameter")
        .def("newpin", [](py::handle self, const std::string &name, int type, int dir) {
            return comp_new_item(self, name, type, dir, true);
        }, "Create a new pin")
        .def("getitem", [](py::handle self, const std::string &name) {
            return comp_get_item(self, name, WHAT_ANY);
        }, "Get existing pin or param object")
        .def("getpin", [](py::handle self, const std::string &name) {
            return comp_get_item(self, name, WHAT_PIN);
        }, "Get existing pin object")
        .def("getparam", [](py::handle self, const std::string &name) {
            return comp_get_item(self, name, WHAT_PARAM);
        }, "Get existing param object")
        .def("getpins", [](py::handle self, py::args) {
            PyComp &pc = live_comp(self);
            py::dict d;
            for(const auto &[name, p] : pc.c->items())
                d[py::str(name)] = item_read(p);
            return d;
        }, "Get all pins and values of component")
        .def("exit", [](py::handle self) {
            // Not a liveness check: exit() of an exited component is fine
            PyComp *pc = nullptr;
            if(PyObject_TypeCheck(self.ptr(), comp_type))
                pc = py::cast<PyComp *>(self);
            if(pc)
                pc->c.reset();
        }, "Call hal_exit")
        .def("ready", [](py::handle self) {
            PyComp &pc = live_comp(self);
            try {
                pc.c->ready();
            } catch(const std::system_error &e) {
                raise_hal_errno(e.code().value());
            }
        }, "Call hal_ready")
        .def("unready", [](py::handle self) {
            PyComp &pc = live_comp(self);
            try {
                pc.c->unready();
            } catch(const std::system_error &e) {
                raise_hal_errno(e.code().value());
            }
        }, "Call hal_unready")
        .def("__repr__", [](py::handle self) {
            PyComp *pc = py::cast<PyComp *>(self);
            if(!pc || !pc->live())
                return fmt::format("<hal component {} closed>", pc ? pc->name : std::string());
            return fmt::format("<hal component {}({}) with {} pins and params>",
                               pc->name, pc->c->id(), pc->c->items().size());
        })
        .def("__getattribute__", &comp_getattr)
        .def("__setattr__", &comp_setattr)
        .def("__delattr__", &comp_delattr)
        .def("__getitem__", &comp_getattr)
        .def("__setitem__", &comp_setattr)
        .def("__delitem__", &comp_delattr)
        .def("__len__", [](py::handle self) { return live_comp(self).c->items().size(); });

    py::class_<PyItem> item(m, "item", "HAL Pin", py::is_final());
    item.attr("__module__") = "hal";
    item
        .def(py::init([](py::args) -> std::unique_ptr<PyItem> {
            raise(PyExc_RuntimeError, "Cannot be constructed directly");
        }))
        .def("set", [](PyItem &it, py::handle v) {
            live_item(it);
            item_write(it.pin, v.ptr());
        }, "Set item value")
        .def("get", [](const PyItem &it) {
            live_item(it);
            return item_read(it.pin);
        }, "Get item value")
        .def("get_type", [](const PyItem &it) { return (int)it.pin.type(); }, "Get item type")
        .def("get_dir", [](const PyItem &it) { return (int)it.pin.direction(); }, "Get item direction")
        .def("get_name", [](const PyItem &it) { return it.name; }, "Get item name")
        .def("is_pin", [](const PyItem &it) { return it.pin.is_pin(); }, "If item is pin or param")
        .def("write", &port_write, "Write data to the port queue")
        .def("read", [](const PyItem &it, py::handle n) { return port_read_peek(it, n, true); },
            "Read data from the port queue")
        .def("writable", [](const PyItem &it) {
            const halxx::port &p = check_port(it, "writable");
            check_dir(it, "writable", halxx::dir::OUT);
            return p.writable();
        }, "Get the count that can be written to the port")
        .def("readable", [](const PyItem &it) {
            const halxx::port &p = check_port(it, "readable");
            check_dir(it, "readable", halxx::dir::IN);
            return p.readable();
        }, "Get the count that can be read from the port")
        .def("peek", [](const PyItem &it, py::handle n) { return port_read_peek(it, n, false); },
            "Read pending data without removal from the queue")
        .def("peek_commit", [](const PyItem &it, py::handle n) {
            const halxx::port &p = check_port(it, "peek_commit");
            unsigned long l = port_count(it, "peek_commit", n);
            if(l < 1 || l > HAL_PORT_SIZE_MAX)
                return false;
            return p.peek_commit((unsigned)l);
        }, "Advance the read pointer by given amount")
        .def("clear", [](const PyItem &it) { check_port(it, "clear").clear(); }, "Clear the port queue")
        .def("size", [](const PyItem &it) { return check_port(it, "size").size(); },
            "Get the queue size of the port")
        .def("__repr__", [](const PyItem &it) {
            return fmt::format("<hal {} \"{}\" {}-{}>", it.pin.is_pin() ? "pin" : "param", it.name,
                               pin_type2name(it.pin.type()), pin_dir2name(it.pin.direction()));
        });

    py::class_<PyStream> stream(m, "stream", "HAL Stream", py::is_final());
    stream.attr("__module__") = "hal";
    stream
        .def(py::init(&stream_new))
        .def("read", &stream_read)
        .def("write", &stream_write)
        .def_property_readonly("readable", [](const PyStream &ps) { return ps.s.readable(); })
        .def_property_readonly("writable", [](const PyStream &ps) { return ps.s.writable(); })
        .def_property_readonly("depth", [](const PyStream &ps) { return ps.s.depth(); })
        .def_property_readonly("element_types", [](const PyStream &ps) { return py::bytes(ps.s.typestring()); })
        .def_property_readonly("maxdepth", [](const PyStream &ps) { return ps.s.maxdepth(); })
        .def_property_readonly("num_underruns", [](const PyStream &ps) { return ps.s.num_underruns(); })
        .def_property_readonly("num_overruns", [](const PyStream &ps) { return ps.s.num_overruns(); })
        .def_property_readonly("sampleno", [](const PyStream &ps) { return ps.s.sampleno(); },
            "The number of the last successfully read sample")
        .def("__repr__", [](const PyStream &ps) {
            return fmt::format("<stream 0x{:x}{}>", (unsigned)ps.s.key(), ps.s.is_creator() ? " creator" : "");
        });

    m.attr("MSG_NONE") = py::int_((int)RTAPI_MSG_NONE);
    m.attr("MSG_ERR")  = py::int_((int)RTAPI_MSG_ERR);
    m.attr("MSG_WARN") = py::int_((int)RTAPI_MSG_WARN);
    m.attr("MSG_INFO") = py::int_((int)RTAPI_MSG_INFO);
    m.attr("MSG_DBG")  = py::int_((int)RTAPI_MSG_DBG);
    m.attr("MSG_ALL")  = py::int_((int)RTAPI_MSG_ALL);

    m.attr("HAL_BOOL") = py::int_((int)HAL_BOOL);
    m.attr("HAL_REAL") = py::int_((int)HAL_REAL);
    m.attr("HAL_SINT") = py::int_((int)HAL_SINT);
    m.attr("HAL_UINT") = py::int_((int)HAL_UINT);
    m.attr("HAL_PORT") = py::int_((int)HAL_PORT);

    m.attr("HAL_RO")  = py::int_((int)HAL_RO);
    m.attr("HAL_RW")  = py::int_((int)HAL_RW);
    m.attr("HAL_IN")  = py::int_((int)HAL_IN);
    m.attr("HAL_OUT") = py::int_((int)HAL_OUT);
    m.attr("HAL_IO")  = py::int_((int)HAL_IO);

    // IntEnum tagging classes, built from the hal.h constants. Registered
    // here so that every consumer shares the same classes.
    m.attr("Type")     = halenum_build("Type", halenum_type_members);
    m.attr("Dir")      = halenum_build("Dir", halenum_dir_members);
    m.attr("CompType") = halenum_build("CompType", halenum_comp_members);
    m.attr("RTType")   = halenum_build("RTType", halenum_rt_members);
    rttype_enum = py::object(m.attr("RTType")).release();

    m.attr("REALTIME_TYPE_UNINITIALIZED")   = py::int_((int)REALTIME_TYPE_UNINITIALIZED);
    m.attr("REALTIME_TYPE_NONE")            = py::int_((int)REALTIME_TYPE_NONE);
    m.attr("REALTIME_TYPE_UNKNOWN")         = py::int_((int)REALTIME_TYPE_UNKNOWN);
    m.attr("REALTIME_TYPE_PREEMPT_DYNAMIC") = py::int_((int)REALTIME_TYPE_PREEMPT_DYNAMIC);
    m.attr("REALTIME_TYPE_PREEMPT_RT")      = py::int_((int)REALTIME_TYPE_PREEMPT_RT);
    m.attr("REALTIME_TYPE_RTAI")            = py::int_((int)REALTIME_TYPE_RTAI);
    m.attr("REALTIME_TYPE_LXRT")            = py::int_((int)REALTIME_TYPE_LXRT);
    m.attr("REALTIME_TYPE_XENOMAI")         = py::int_((int)REALTIME_TYPE_XENOMAI);
    m.attr("REALTIME_TYPE_XENOMAI_EVL")     = py::int_((int)REALTIME_TYPE_XENOMAI_EVL);

    m.attr("is_kernelspace") = py::int_(rtapi_is_kernelspace());
    m.attr("is_userspace")   = py::int_(!rtapi_is_kernelspace());

    m.attr("streamer_base") = py::int_(0x48535430);
    m.attr("sampler_base")  = py::int_(0x48534130);

#ifdef RTAPI_KERNEL_VERSION
    m.attr("kernel_version") = RTAPI_KERNEL_VERSION;
#else
    m.attr("kernel_version") = "Not Available";
#endif

    // Now that we have everything registered, add the halquery sub-module
    if(halquery_add_submodule(m.ptr()) < 0)
        throw py::error_already_set();

    // A halcmd unload arrives as SIGTERM; deliver it to a component as
    // a KeyboardInterrupt. A program that has already set a handler of
    // its own keeps it.
    PyRun_SimpleString(
            "(lambda s=__import__('signal'):"
                 "s.getsignal(s.SIGTERM) is s.SIG_DFL and "
                 "s.signal(s.SIGTERM, s.default_int_handler))()");
}
