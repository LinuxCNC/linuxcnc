/*
    halpybind.cc - Python bindings for HAL via pybind11

    Thin binding layer over the C++ HAL interface (hal.hh). All HAL
    access goes through the public C API and the query API; this module
    contains no HAL internals.

    Exposes:
      component  - userspace component with pins/params
      Pin        - runtime-typed pin/param reference
      module fns - by-name get/set (when the query API is available),
                   signals, component queries
*/
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "hal.hh"

#ifdef HALXX_WITH_QUERY_API
#include "utils/setps_util.h"
#endif

namespace py = pybind11;
namespace halxx = linuxcnc::hal;

#ifdef HALXX_WITH_QUERY_API
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
#endif

PYBIND11_MODULE(halpp, m) {
    m.doc() = "Interface to linuxcnc hal";

#ifdef HALXX_WITH_QUERY_API
    // Initialize the user-land HAL library at import so the by-name
    // query functions work without a component. Teardown reports
    // components the user forgot to exit.
    if(int rv = hal_lib_init()) {
        PyErr_Format(PyExc_ImportError, "halpp: hal_lib_init failed: %s", hal_strerror(rv));
        throw py::error_already_set();
    }
    Py_AtExit(hal_lib_exit);

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
#endif

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
        .def("get_name", &halxx::anypin::name);

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
        .def("setitem", &halxx::component::setitem<rtapi_s32>)
        .def("setitem", &halxx::component::setitem<rtapi_u32>)
        .def("setitem", &halxx::component::setitem<rtapi_sint>)
        .def("setitem", &halxx::component::setitem<rtapi_uint>)
        .def("__setitem__", &halxx::component::setitem<rtapi_real>)
        .def("__setitem__", &halxx::component::setitem<rtapi_bool>)
        .def("__setitem__", &halxx::component::setitem<rtapi_s32>)
        .def("__setitem__", &halxx::component::setitem<rtapi_u32>)
        .def("__setitem__", &halxx::component::setitem<rtapi_sint>)
        .def("__setitem__", &halxx::component::setitem<rtapi_uint>)
        .def("__contains__", &halxx::component::contains)
        .def("ready", &halxx::component::ready)
        .def("exit", &halxx::component::exit);

    py::enum_<hal_type_t>(m, "hal_type_t")
        .value("HAL_BIT", HAL_BIT)
        .value("HAL_BOOL", HAL_BOOL)
        .value("HAL_FLOAT", HAL_FLOAT)
        .value("HAL_REAL", HAL_REAL)
        .value("HAL_S32", HAL_S32)
        .value("HAL_U32", HAL_U32)
        .value("HAL_S64", HAL_S64)
        .value("HAL_U64", HAL_U64)
        .value("HAL_PORT", HAL_PORT)
        .export_values();

    py::enum_<halxx::dir>(m, "hal_dir")
        .value("HAL_IN", halxx::dir::IN)
        .value("HAL_OUT", halxx::dir::OUT)
        .value("HAL_IO", halxx::dir::IO)
        .value("HAL_RO", halxx::dir::RO)
        .value("HAL_RW", halxx::dir::RW)
        .export_values();
}
