/*
    halpybind.cc - Python bindings for HAL via pybind11

    Thin binding layer over the C++ HAL interface (hal.hh). All HAL
    access goes through the public C API and the query API; this module
    contains no HAL internals.

    Exposes:
      component  - userspace component with pins/params
      Pin        - runtime-typed pin/param reference
      stream     - sample FIFO shared with sampler/streamer
      module fns - by-name get/set, signals, component queries
*/
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdio>
#include <system_error>

#include "hal.hh"
#include "setps_util.h"

namespace py = pybind11;
namespace halxx = linuxcnc::hal;

// The IntEnum classes _hal.Type and _hal.Dir, fetched at import. The
// references are leaked on purpose, as in halquery.cc, so no py::object
// destructor runs during interpreter teardown.
static py::object halenumtype;
static py::object halenumdir;

namespace pybind11 { namespace detail {

// Casts between the native enum values and the shared IntEnum classes
// registered by _hal, built from the hal.h constants. Arguments
// accept the enums and plain ints alike; results come back as enum
// members, so tags print with their names.
template <> struct type_caster<hal_type_t> {
    PYBIND11_TYPE_CASTER(hal_type_t, const_name("hal.Type"));

    bool load(handle src, bool) {
        PyObject *idx = PyNumber_Index(src.ptr());
        if(!idx) {
            PyErr_Clear();
            return false;
        }
        long v = PyLong_AsLong(idx);
        Py_DECREF(idx);
        if(v == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        value = static_cast<hal_type_t>(v);
        return true;
    }

    static handle cast(hal_type_t v, return_value_policy, handle) {
        return halenumtype(static_cast<long>(v)).release();
    }
};

template <> struct type_caster<linuxcnc::hal::dir> {
    PYBIND11_TYPE_CASTER(linuxcnc::hal::dir, const_name("hal.Dir"));

    bool load(handle src, bool) {
        PyObject *idx = PyNumber_Index(src.ptr());
        if(!idx) {
            PyErr_Clear();
            return false;
        }
        long v = PyLong_AsLong(idx);
        Py_DECREF(idx);
        if(v == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        value = static_cast<linuxcnc::hal::dir>(v);
        return true;
    }

    static handle cast(linuxcnc::hal::dir v, return_value_policy, handle) {
        return halenumdir(static_cast<long>(v)).release();
    }
};

}} // namespace pybind11::detail

// Text-to-value conversion is delegated to setps_common_cb so that
// string parsing is consistent with halcmd setp/sets for all types.
static void set_value_str(const std::string &name, const std::string &value)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_set_p(&q, setps_common_cb, (void *)value.c_str());
    if(rv)
        throw std::invalid_argument("halpp: set_value(" + name + ") failed: " + hal_strerror(rv));
}
static void set_signal_str(const std::string &name, const std::string &value)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_set_s(&q, setps_common_cb, (void *)value.c_str());
    if(rv)
        throw std::invalid_argument("halpp: set_signal(" + name + ") failed: " + hal_strerror(rv));
}

PYBIND11_MODULE(halpp, m) {
    m.doc() = "Interface to linuxcnc hal";

    // Failures reported by the library as a negative errno become
    // OSError, as they do in the _hal module. Everything else keeps
    // pybind11's default mapping (invalid_argument -> ValueError,
    // out_of_range -> IndexError, ...).
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if(p)
                std::rethrow_exception(p);
        } catch(const std::system_error &e) {
            PyErr_SetObject(PyExc_OSError,
                Py_BuildValue("(is)", e.code().value(), e.what()));
        }
    });

    // Importing _hal initializes the user-land HAL library, so the
    // by-name query functions work without a component, and provides
    // the IntEnum type and direction tags.
    py::module_ halmod = py::module_::import("_hal");
    halenumtype = halmod.attr("Type");
    halenumdir = halmod.attr("Dir");
    halenumtype.inc_ref();
    halenumdir.inc_ref();
    m.attr("Type") = halenumtype;
    m.attr("Dir") = halenumdir;

    // By-name queries and setters (query API)
    m.def("component_exists", &halxx::component_exists);
    m.def("component_is_ready", &halxx::component_is_ready);
    m.def("pin_has_writer", &halxx::pin_has_writer);
    m.def("get_value", &halxx::get_value);
    m.def("set_value", &halxx::set_value);
    m.def("set_value", &set_value_str);
    m.def("set_p", &halxx::set_value);       // compatibility name
    m.def("set_p", &set_value_str);
    m.def("set_signal", &halxx::set_signal);
    m.def("set_signal", &set_signal_str);

    // Signals
    m.def("signal_new", &halxx::signal_new);
    m.def("signal_delete", &halxx::signal_delete);
    m.def("link", &halxx::link);
    m.def("unlink", &halxx::unlink);
    m.def("new_sig", &halxx::signal_new);    // compatibility names
    m.def("sigNew", &halxx::signal_new);
    m.def("sigLink", &halxx::link);
    m.def("connect", &halxx::link);
    m.def("disconnect", &halxx::unlink);

    m.attr("is_kernelspace") = py::int_(rtapi_is_kernelspace());
    m.attr("is_userspace") = py::int_(!rtapi_is_kernelspace());

    py::class_<halxx::anypin>(m, "Pin")
        .def("get", &halxx::anypin::get)
        .def("set", &halxx::anypin::set)
        .def_property("value", &halxx::anypin::get, &halxx::anypin::set)
        .def_property_readonly("name", &halxx::anypin::name)
        .def_property_readonly("type", &halxx::anypin::type)
        .def("get_name", &halxx::anypin::name)
        // Port pins: byte-stream access, all or nothing, as in the _hal
        // module. read()/peek() return None when fewer than n bytes are
        // readable; the other calls raise ValueError on a non-port pin.
        .def("read", [](const halxx::anypin &p, unsigned n) -> py::object {
            auto data = p.as_port().read(n);
            if(!data)
                return py::none();
            return py::bytes(data->data(), data->size());
        }, py::arg("n"))
        .def("peek", [](const halxx::anypin &p, unsigned n) -> py::object {
            auto data = p.as_port().peek(n);
            if(!data)
                return py::none();
            return py::bytes(data->data(), data->size());
        }, py::arg("n"))
        .def("peek_commit", [](const halxx::anypin &p, unsigned n) {
            return p.as_port().peek_commit(n);
        }, py::arg("n"))
        // str is written as its UTF-8 encoding.
        .def("write", [](const halxx::anypin &p, const std::string &data) {
            return p.as_port().write(data.data(), (unsigned)data.size());
        }, py::arg("data"))
        .def("readable", [](const halxx::anypin &p) { return p.as_port().readable(); })
        .def("writable", [](const halxx::anypin &p) { return p.as_port().writable(); })
        .def("size", [](const halxx::anypin &p) { return p.as_port().size(); })
        .def("clear", [](const halxx::anypin &p) { p.as_port().clear(); });

    py::class_<halxx::component>(m, "component")
        .def(py::init<std::string>())
        .def("id", &halxx::component::id)
        .def("newpin", static_cast<halxx::anypin (halxx::component::*)(const std::string &, hal_type_t, halxx::dir)>(&halxx::component::newpin))
        .def("newparam", static_cast<halxx::anypin (halxx::component::*)(const std::string &, hal_type_t, halxx::dir)>(&halxx::component::newparam))
        .def("setprefix", &halxx::component::setprefix)
        .def("getprefix", &halxx::component::getprefix)
        .def("getitem", &halxx::component::getitem)
        .def("__getitem__", &halxx::component::getitem)
        .def("setitem", &halxx::component::setitem<rtapi_real>)
        .def("setitem", &halxx::component::setitem<rtapi_bool>)
        .def("setitem", &halxx::component::setitem<rtapi_sint>)
        .def("setitem", &halxx::component::setitem<rtapi_uint>)
        .def("__setitem__", &halxx::component::setitem<rtapi_real>)
        .def("__setitem__", &halxx::component::setitem<rtapi_bool>)
        .def("__setitem__", &halxx::component::setitem<rtapi_sint>)
        .def("__setitem__", &halxx::component::setitem<rtapi_uint>)
        .def("__contains__", &halxx::component::contains)
        .def("ready", &halxx::component::ready)
        .def("exit", &halxx::component::exit);

    // Streams. The key is an integer; sampler and streamer derive theirs
    // from these bases, so a Python reader/writer can pair with them.
    m.attr("streamer_base") = py::int_(0x48535430);
    m.attr("sampler_base") = py::int_(0x48534130);

    py::class_<halxx::stream>(m, "stream")
        .def(py::init<halxx::component &, int, unsigned, const std::string &>(),
             py::arg("comp"), py::arg("key"), py::arg("depth"), py::arg("typestring"),
             py::keep_alive<1, 2>())
        .def(py::init<halxx::component &, int, const std::string &>(),
             py::arg("comp"), py::arg("key"), py::arg("typestring") = std::string(),
             py::keep_alive<1, 2>())
        // A tuple, like the _hal stream, so samples can be compared and
        // unpacked the same way.
        .def("read", [](halxx::stream &s) -> py::object {
            auto sample = s.read();
            if(!sample)
                return py::none();
            return py::tuple(py::cast(*sample));
        })
        .def("write", [](halxx::stream &s, const std::vector<halxx::value_t> &data) {
            s.write(data);
        }, py::arg("data"))
        .def("close", &halxx::stream::close)
        .def("element_type", &halxx::stream::element_type, py::arg("idx"))
        .def_property_readonly("element_count", &halxx::stream::element_count)
        // Bytes of typestring characters, as in the _hal stream.
        .def_property_readonly("element_types", [](const halxx::stream &s) {
            return py::bytes(s.typestring());
        })
        .def_property_readonly("key", &halxx::stream::key)
        .def_property_readonly("is_creator", &halxx::stream::is_creator)
        .def_property_readonly("is_open", &halxx::stream::is_open)
        .def_property_readonly("readable", &halxx::stream::readable)
        .def_property_readonly("writable", &halxx::stream::writable)
        .def_property_readonly("depth", &halxx::stream::depth)
        .def_property_readonly("maxdepth", &halxx::stream::maxdepth)
        .def_property_readonly("num_underruns", &halxx::stream::num_underruns)
        .def_property_readonly("num_overruns", &halxx::stream::num_overruns)
        .def_property_readonly("sampleno", &halxx::stream::sampleno)
        .def("__repr__", [](const halxx::stream &s) {
            char buf[64];
            snprintf(buf, sizeof(buf), "<stream 0x%x%s>", (unsigned)s.key(),
                     s.is_creator() ? " creator" : "");
            return std::string(buf);
        });

    // 'halcmd unload' terminates a userspace component with SIGTERM.
    // Raise KeyboardInterrupt for it, as the _hal module does, so the
    // component runs its cleanup and flushes its output instead of
    // dying where it stands.
    py::module_ signal = py::module_::import("signal");
    signal.attr("signal")(signal.attr("SIGTERM"), signal.attr("default_int_handler"));
}
