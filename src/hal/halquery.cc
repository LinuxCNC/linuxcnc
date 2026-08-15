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
#include <pybind11/pybind11.h>

#include "halqrec.hh"

namespace py = pybind11;

// These are IntEnum imported from _hal
static py::object halenumtype;  // hal_type_t
static py::object halenumdir;   // hal_pdir_t
static py::object halenumcomp;  // hal_comp_type_t

static py::object value_object(hal_type_t type, hal_refs_u u, bool ispin)
{
    switch(type) {
    case HAL_BOOL: return py::bool_(hal_get_bool(u.b));
    case HAL_S32:  return py::int_(hal_get_si32(u.s));
    case HAL_SINT: return py::int_(hal_get_sint(u.s));
    case HAL_U32:  return py::int_(hal_get_ui32(u.u));
    case HAL_UINT: return py::int_(hal_get_uint(u.u));
    case HAL_REAL: return py::float_(hal_get_real(u.r));
    case HAL_PORT:
        // FIXME
        // Needs to change when we break the API and change hal_port_t
        if(ispin)
            return py::int_(hal_port_buffer_size(reinterpret_cast<hal_port_t *>(u.u)));
        else
            return py::int_(0); // HAL_PORT cannot be a parameter
    default: return py::none();
    }
}

// NOTE:
// Using py::dict("bla"_a = value) results in a cppcheck portability warning.
// We simply circumvent the problem by constructing the dict piece by piece.
//
static py::object build_dict_comp(const hal_query_t *q)
{
    py::dict dict;
    dict["haltype"] = "component";
    dict["name"]    = q->name;
    dict["type"]    = halenumcomp((long)q->comp.type);
    dict["id"]      = q->comp.comp_id;
    dict["pid"]     = q->comp.pid;
    dict["ready"]   = py::bool_(q->comp.ready);
    dict["insmod"]  = q->comp.insmod;
    return dict;
}

static py::object build_dict_pin_param(const hal_query_t *q, bool ispin)
{
    py::dict dict;
    dict["haltype"] = ispin ? "pin" : "parameter";
    dict["name"]    = q->name;
    dict["type"]    = halenumtype((long)q->pp.type);
    dict["dir"]     = halenumdir((long)q->pp.dir);
    dict["value"]   = value_object(q->pp.type, q->pp.ref, ispin);
    dict["alias"]   = q->pp.alias;
    if(ispin)
        dict["signal"]  = q->pp.signal;
    dict["comp"]    = q->pp.comp;
    dict["comp_id"] = q->pp.comp_id;
    return dict;
}

static py::object build_dict_signal(const hal_query_t *q, const py::object &drv)
{
    py::dict dict;
    dict["haltype"] = "signal";
    dict["name"]    = q->name;
    dict["type"]    = halenumtype((long)q->sig.type);
    dict["value"]   = value_object(q->sig.type, q->sig.ref, false);
    dict["writers"] = q->sig.writers;
    dict["readers"] = q->sig.readers;
    dict["bidirs"]  = q->sig.bidirs;
    dict["driver"]  = drv;
    return dict;
}

static py::object build_dict_funct(const hal_query_t *q)
{
    py::dict dict;
    dict["haltype"]   = "function";
    dict["name"]      = q->name;
    dict["comp"]      = q->funct.comp;
    dict["comp_id"]   = q->funct.comp_id;
    dict["users"]     = q->funct.users;
    dict["reentrant"] = py::bool_(q->funct.reentrant);
    return dict;
}

static py::object build_dict_threadfunct(const hal_query_t *q)
{
    py::dict dict;
    dict["haltype"] = "threadfunction";
    dict["name"]    = q->thread.funct;
    dict["index"]   = q->thread.functidx;
    dict["is_init"] = py::bool_(q->thread.is_init);
    return dict;
}

static py::object build_dict_thread(const hal_query_t *q, py::list &functs)
{
    py::dict dict;
    dict["haltype"]   = "thread";
    dict["name"]      = q->name;
    dict["comp"]      = q->thread.comp;
    dict["comp_id"]   = q->thread.comp_id;
    dict["priority"]  = q->thread.priority;
    dict["period"]    = q->thread.period;
    dict["functions"] = functs;
    return dict;
}

//
// Fetch the name of the signal driver if there is one
//
static py::object fetch_signal_driver(const hal_query_t *sig)
{
    // Can only have a driver when there is a writer
    if(sig->sig.writers <= 0)
        return py::none();

    hal_query_t q = {};
    q.name = sig->name;
    HalQRec qrec;
    int rv = hal_list_p_s(&q, HalQRec::get_qrec_cb, &qrec);
    if(0 == rv) {
        for(size_t i = 0; i < qrec.size(); i++) {
            if(HAL_OUT == qrec.rec(i)->pp.dir) {
                // This is the driver pin
                return py::str(qrec.rec(i)->name);
            }
        }
        // Getting here would be bad...
        // However, there is apparently no driver (anymore)
        return py::none();
    }
    throw std::runtime_error(fmt::format("fetch_signal_driver({}): hal_list_p_s: error={} ({})", sig->name, rv, hal_strerror(rv)));
}

//
// *** Get component on name/ID ***
//
static py::object get_comp_i(int comp_id)
{
    hal_query_t q = {};
    int rv = hal_comp_by_id(comp_id, &q);
    switch(rv) {
    case -ENOENT: return py::none();
    case 0: return build_dict_comp(&q);
    default:
        throw std::runtime_error(fmt::format("get_comp_i({}): hal_comp_by_id: error={} ({})", comp_id, rv, hal_strerror(rv)));
    }
}

static py::object get_comp_s(const std::string &name)
{
    hal_query_t q = {};
    int rv = hal_comp_by_name(name.c_str(), &q);
    switch(rv) {
    case -ENOENT: return py::none();
    case 0: return build_dict_comp(&q);
    default:
        throw std::runtime_error(fmt::format("get_comp_s({}): hal_comp_by_name: error={} ({})", name, rv, hal_strerror(rv)));
    }
}

//
// *** Get named pin/param ***
//
static py::object get_pin_param(const std::string &name, bool ispin)
{
    hal_query_t q = {};
    q.qtype = ispin ? HAL_QTYPE_PIN : HAL_QTYPE_PARAM;
    q.name = name.c_str();
    int rv = hal_getref_p(&q);
    switch(rv) {
    case -ENOENT: return py::none();
    case 0: return build_dict_pin_param(&q, ispin);
    default:
        throw std::runtime_error(fmt::format("get_pin_param({},{}): hal_get_p: error={} ({})", name, ispin, rv, hal_strerror(rv)));
    }
}

static py::object get_pin(const std::string &name)
{
    return get_pin_param(name, true);
}

static py::object get_param(const std::string &name)
{
    return get_pin_param(name, false);
}

//
// *** Get named signal ***
//
static py::object get_signal(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_getref_s(&q);
    switch(rv) {
    case -ENOENT: return py::none();
    case 0: return build_dict_signal(&q, fetch_signal_driver(&q));
    default:
        throw std::runtime_error(fmt::format("get_signal({}): hal_get_s: error={} ({})", name, rv, hal_strerror(rv)));
    }
}

//
// *** Get named function ***
//
static int get_funct_cb(hal_query_t *q, void *arg)
{
    if(!strcmp(q->name, (const char *)arg))
        return 1;  // Found it, break the loop without error
    return 0;
}

static py::object get_funct(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_list_funct(&q, get_funct_cb, (void *)name.c_str());
    switch(rv) {
    case 0: return py::none();
    case 1: return build_dict_funct(&q);
    default:
        throw std::runtime_error(fmt::format("get_funct({}): hal_list_funct: error={} ({})", name, rv, hal_strerror(rv)));
    }
}

//
// *** Get named thread ***
//
typedef struct {
    const char *name;
    HalQRec *qrec;
} threadlist_t;

static int get_thread_cb(hal_query_t *q, void *arg)
{
    threadlist_t *tlp = reinterpret_cast<threadlist_t *>(arg);
    if(!strcmp(tlp->name, q->name)) {
        // The first match has q->qtype set to HAL_QTYPE_THREAD. The following
        // matches with have it set to HAL_QTYPE_THREAD_FUNCT.
        return tlp->qrec->append(q);
    } else if(tlp->qrec->size() > 0) {
        // This is a new thread. We scooped up the matched thread's data, so
        // it is fine to quit without error.
        return 1;
    }
    return 0;
}

static py::object get_thread(const std::string &name)
{
    HalQRec qrec;
    threadlist_t tl = { .name = name.c_str(), .qrec = &qrec };

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    int rv = hal_list_thread(&q, get_thread_cb, &tl);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_thread({}): hal_list_thread: error={} ({})", name, rv, hal_strerror(rv)));
    }

    if(qrec.size() < 1) {
        // Thread name not found
        return py::none();
    }
    py::list flst;
    for(size_t i = 1; i < qrec.size(); i++) {
        flst.append(build_dict_threadfunct(qrec.rec(i)));
    }

    return build_dict_thread(qrec.rec(0), flst);
}

//
// *** Get all pins connected to a named signal  ***
//
static py::object get_signalpins(const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();

    HalQRec qrec;

    int rv = hal_list_p_s(&q, HalQRec::get_qrec_cb, &qrec);
    if(-ENOENT == rv) {
        return py::none();
    } else if(rv < 0) {
        throw std::runtime_error(fmt::format("get_signalpins({}): hal_list_p_s: error={} ({})", name, rv, hal_strerror(rv)));
    }

    py::dict dict;
    for(size_t i = 0; i < qrec.size(); i++) {
        dict[qrec.rec(i)->name] = build_dict_pin_param(qrec.rec(i), true);
    }
    return dict;
}

//
// *** Get all pins ***
//
static py::object build_pps(const HalQRec &qrec, bool ispin)
{
    py::dict dict;

    for(size_t i = 0; i < qrec.size(); i++) {
        dict[qrec.rec(i)->name] = build_dict_pin_param(qrec.rec(i), ispin);
    }
    return dict;
}

static py::dict get_pins()
{
    HalQRec qrec(1024);  // There are generally a lot of pins

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    int rv = hal_list_p(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_pins(): hal_list_p: error={} ({})", rv, hal_strerror(rv)));
    }

    return build_pps(qrec, true);
}

//
// *** Get all params ***
//
static py::dict get_params()
{
    HalQRec qrec(512);  // There are generally a lot of params

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;
    int rv = hal_list_p(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_params(): hal_list_p: error={} ({})", rv, hal_strerror(rv)));
    }

    return build_pps(qrec, false);
}

//
// *** Get all signals ***
//
static py::dict get_signals()
{
    HalQRec qrec(256);  // The are generally a lot of signals

    hal_query_t q = {};
    int rv = hal_list_s(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_signals(): hal_list_s: error={} ({})", rv, hal_strerror(rv)));
    }

    py::dict dict;
    for(size_t i = 0; i < qrec.size(); i++) {
        dict[qrec.rec(i)->name] = build_dict_signal(qrec.rec(i), fetch_signal_driver(qrec.rec(i)));
    }
    return dict;
}

//
// *** Get all components ***
//
static py::dict get_comps()
{
    HalQRec qrec;

    hal_query_t q = {};
    int rv = hal_list_comp(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_comps(): hal_list_comp: error={} ({})", rv, hal_strerror(rv)));
    }

    py::dict dict;
    for(size_t i = 0; i < qrec.size(); i++) {
        dict[qrec.rec(i)->name] = build_dict_comp(qrec.rec(i));
    }
    return dict;
}

//
// *** Get all functions ***
//
static py::dict get_functs()
{
    HalQRec qrec;

    hal_query_t q = {};
    int rv = hal_list_funct(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_functs(): hal_list_funct: error={} ({})", rv, hal_strerror(rv)));
    }

    py::dict dict;
    for(size_t i = 0; i < qrec.size(); i++) {
        dict[qrec.rec(i)->name] = build_dict_funct(qrec.rec(i));
    }
    return dict;
}

//
// *** Get all threads ***
//
static py::dict get_threads()
{
    HalQRec qrec;

    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    int rv = hal_list_thread(&q, HalQRec::get_qrec_cb, &qrec);
    if(rv < 0) {
        throw std::runtime_error(fmt::format("get_threads(): hal_list_thread: error={} ({})", rv, hal_strerror(rv)));
    }

    py::dict dict;
    if(qrec.size() < 1)
        return dict;    // No threads, return empty list

    py::list flst;
    size_t tag = 0;
    for(size_t i = 1; i < qrec.size(); i++) {
        if(HAL_QTYPE_THREAD == qrec.rec(i)->qtype) {
            tag = i;
        } else {
            // This must be HAL_QTYPE_THREAD_FUNCT
            flst.append(build_dict_threadfunct(qrec.rec(i)));
        }
        if(i == qrec.size() - 1 || (i < qrec.size() - 1 && HAL_QTYPE_THREAD == qrec.rec(i+1)->qtype)) {
            // There is no next enrty or next entry is new thread
            dict[qrec.rec(tag)->name] = build_dict_thread(qrec.rec(tag), flst);
            if(i < qrec.size() - 1) {
                flst = py::list();   // New thread follows, new function list
            }
        }
    }
    return dict;
}

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
    " import hal\n"
    " # print info on one component\n"
    " print(\"Component xyz:\", hal.query.comp(\"xyz\"))\n"
    "\n"
    " # print info on all signals\n"
    " print(\"Signals:\")\n"
    " print(hal.query.signals())\n"
    ;

//
// NOTE:
// The halquery is a sub-module to hal under hal.query. That means importing it
// requires hal/_hal to be imported. That is a good thing because then we don't
// have to think about hal_lib_init() and hal_lib_exit() anymore. The _hal base
// will do that for us and it is imported before this sub-module is.
//
int halquery_add_submodule(PyObject *parent)
{
    try {
        py::module_ p = py::reinterpret_borrow<py::module_>(parent);
        py::module_ m = p.def_submodule("query", halquery_module_doc);

        m.def("pin",    &get_pin,    "Get information about the named pin or None if not found.");
        m.def("param",  &get_param,  "Get information about the named param or None if not found.");
        m.def("signal", &get_signal, "Get information about the named signal or None if not found.");
        m.def("comp",   &get_comp_i, "Get information about the component by integer ID or None if not found.");
        m.def("comp",   &get_comp_s, "Get information about the component by name or None if not found.");
        m.def("funct",  &get_funct,  "Get information about the named function or None if not found.");
        m.def("thread", &get_thread, "Get information about the named thread or None if not found.");

        m.def("signalpins", &get_signalpins, "Get information about all pins connected to named signal or None if not found");

        m.def("pins",    &get_pins,    "Get information about all pins as a dictionary.");
        m.def("params",  &get_params,  "Get information about all params as a dictionary.");
        m.def("signals", &get_signals, "Get information about all signals as a dictionary.");
        m.def("comps",   &get_comps,   "Get information about all components as a dictionary.");
        m.def("functs",  &get_functs,  "Get information about all functions as a dictionary.");
        m.def("threads", &get_threads, "Get information about all threads as a dictionary.");

        halenumtype = p.attr("Type");
        halenumdir  = p.attr("Dir");
        halenumcomp = p.attr("CompType");
        // These inc_ref()'s will leak the enum references on purpose. This
        // prevents any freeing of the underlying PyObject when the program
        // ends. The py::object destructor can otherwise cause interference.
        halenumtype.inc_ref();
        halenumdir.inc_ref();
        halenumcomp.inc_ref();
        return 0;
    } catch(const std::exception &e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return -1;
    }
}

// vim: ts=4 sw=4 et
