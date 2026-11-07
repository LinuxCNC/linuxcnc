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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

/*

  Notes:

  NURBS
  -----
  The code in this file for nurbs calculations is from University of Palermo.
  The publications can be found at: http://wiki.linuxcnc.org/cgi-bin/wiki.pl?NURBS
  AMST08_art837759.pdf and ECME14.pdf

  1:
  M. Leto, R. Licari, E. Lo Valvo1 , M. Piacentini:
  CAD/CAM INTEGRATION FOR NURBS PATH INTERPOLATION ON PC BASED REAL-TIME NUMERICAL CONTROL
  Proceedings of AMST 2008 Conference, 2008, pp. 223-233

  2:
  ERNESTO LO VALVO, STEFANO DRAGO:
  An Efficient NURBS Path Generator for a Open Source CNC
  Recent Advances in Mechanical Engineering (pp.173-180). WSEAS Press

  The code from University of Palermo is modified to work on planes xy, yz and zx by Joachim Franek
  */


// pybind11/pybind11.h - and so Python.h - reaches this file through
// gcodemodule.hh, which is included first for that reason: CPython wants its
// header ahead of the standard ones.
#include "gcodemodule.hh"
#include "gcode_renderer.hh"

#include <structmember.h>

#include <chrono>
#include <errno.h>
#include <float.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <tuple>
#include <unistd.h>

#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include "nml_intf/interp_return.hh"
#include "nml_intf/canon.hh"
#include "tooldata/tooldata.hh"
#include <axis_kinds.hh>

int _task = 0; // control preview behaviour when remapping

char _parameter_file_name[LINELEN];

extern "C" PyObject* PyInit_interpreter(void);
extern "C" PyObject* PyInit_emccanon(void);
extern "C" struct _inittab builtin_modules[];
struct _inittab builtin_modules[] = {
    { "interpreter", PyInit_interpreter },
    { "emccanon", PyInit_emccanon },
    { NULL, NULL }
};


namespace py = pybind11;

// What `next_line` hands the canon: the interpreter's active settings and its
// active G and M codes for one source line. Plain data behind a hand-written
// type - the read-only members below are its whole Python surface, and
// `sequence_number` overlays gcodes[0].
//
// It is hand-written rather than a `py::class_` because it is the one object
// this module builds per source line. A pybind11 instance costs a malloc for
// the holder and an insert into pybind11's `registered_instances` hash on the
// way in, an erase on the way out, and turns every member read into a call
// into C++; none of that buys anything for plain read-only data.
//
// A canon can also build one itself, and both protocols reach the
// constructor - but only from inside a call the parse makes into the canon,
// and so only on the lines that make one. A canon on the per-move callbacks
// is called on every line and can ask on any of them. A renderer canon is
// called only for the events a rendered parse still forwards, moves not
// among them, so the lines it can ask on are the lines that forwarded
// something. Neither can ask outside a parse; the constructor raises there.
struct LineCode {
    PyObject_HEAD
    double settings[ACTIVE_SETTINGS];
    int gcodes[ACTIVE_G_CODES];
    int mcodes[ACTIVE_M_CODES];
};

// Only the head is spelled here - PyType_Ready needs it before the type is a
// type at all. Every other slot is zero and `linecode_register` fills in the
// handful this type uses, which keeps the table off the version-conditional
// tail of PyTypeObject.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static PyTypeObject LineCodeType = { PyVarObject_HEAD_INIT(nullptr, 0) };
#pragma GCC diagnostic pop

static py::tuple int_array(const int *arr, int sz) {
    py::tuple res(sz);
    for(int i = 0; i < sz; i++) res[i] = arr[i];
    return res;
}

// The interpreter's active settings and codes as of now, tagged with the
// source line they belong to. The one place a LineCode is ever filled in:
// both a delivery and a canon building its own go through here, so the two
// cannot drift. Returns a new reference, or null with the error set.
static LineCode *snapshot_line(int sequence_number) {
    LineCode *line = PyObject_New(LineCode, &LineCodeType);
    if(!line) return nullptr;
    parse_state.pinterp->active_settings(line->settings);
    parse_state.pinterp->active_g_codes(line->gcodes);
    parse_state.pinterp->active_m_codes(line->mcodes);
    line->gcodes[0] = sequence_number;
    return line;
}

// The interpreter as of now, for a canon wanting the modal state at a point
// no delivery covers. It has to be filled in here: a delivery builds its own
// object and every member is read-only, so a linecode that starts empty stays
// empty for its whole life. Outside a parse there is nothing to read and this
// raises rather than answer zeros. The guard is `in_parse`, not `pinterp`:
// `pinterp` stays set after a parse ends and would answer with a finished
// parse's modal state.
static PyObject *linecode_new(PyTypeObject *, PyObject *args, PyObject *kw) {
    if(PyTuple_GET_SIZE(args) || (kw && PyDict_Size(kw))) {
        PyErr_SetString(PyExc_TypeError, "linecode() takes no arguments");
        return nullptr;
    }
    if(!parse_state.in_parse || !parse_state.pinterp) {
        PyErr_SetString(PyExc_ValueError, "linecode: no parse in progress");
        return nullptr;
    }
    return reinterpret_cast<PyObject *>(snapshot_line(parse_state.current_line()));
}

// The two array members. Everything else is a struct offset read straight out
// of the instance by PyMemberDef below; these two have to build a tuple, so
// they are the only members that run code - and the only ones that can raise,
// which a getter reached from C has to turn back into a set error.
static PyObject *linecode_array(const int *arr, int sz) {
    try {
        return int_array(arr, sz).release().ptr();
    } catch(py::error_already_set &e) {
        e.restore();
        return nullptr;
    } catch(const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return nullptr;
    }
}

static PyObject *linecode_gcodes(PyObject *self, void *) {
    return linecode_array((reinterpret_cast<LineCode *>(self))->gcodes, ACTIVE_G_CODES);
}

static PyObject *linecode_mcodes(PyObject *self, void *) {
    return linecode_array((reinterpret_cast<LineCode *>(self))->mcodes, ACTIVE_M_CODES);
}

static PyGetSetDef LineCodeGetSet[] = {
    {(char*)"gcodes", linecode_gcodes, nullptr, nullptr, nullptr},
    {(char*)"mcodes", linecode_mcodes, nullptr, nullptr, nullptr},
    {},
};

// Read through a struct offset, which is what makes this type worth
// hand-writing: a canon reads at least one of these per delivered line, and a
// descriptor over an offset costs a fraction of a call into C++.
#define LC(name, type, slot) \
    {(char*)name, type, offsetof(LineCode, slot), READONLY, nullptr}
static PyMemberDef LineCodeMembers[] = {
    LC("sequence_number", T_INT, gcodes[0]),

    LC("feed_rate", T_DOUBLE, settings[1]),
    LC("speed", T_DOUBLE, settings[2]),
    LC("motion_mode", T_INT, gcodes[1]),
    LC("block", T_INT, gcodes[2]),
    LC("plane", T_INT, gcodes[3]),
    LC("cutter_side", T_INT, gcodes[4]),
    LC("units", T_INT, gcodes[5]),
    LC("distance_mode", T_INT, gcodes[6]),
    LC("feed_mode", T_INT, gcodes[7]),
    LC("origin", T_INT, gcodes[8]),
    LC("tool_length_offset", T_INT, gcodes[9]),
    LC("retract_mode", T_INT, gcodes[10]),
    LC("path_mode", T_INT, gcodes[11]),

    LC("stopping", T_INT, mcodes[1]),
    LC("spindle", T_INT, mcodes[2]),
    LC("toolchange", T_INT, mcodes[3]),
    LC("mist", T_INT, mcodes[4]),
    LC("flood", T_INT, mcodes[5]),
    LC("overrides", T_INT, mcodes[6]),
    {}
};
#undef LC

static void linecode_register(py::module_ &m) {
    LineCodeType.tp_name = "gcode.linecode";
    LineCodeType.tp_basicsize = sizeof(LineCode);
    LineCodeType.tp_flags = Py_TPFLAGS_DEFAULT;
    LineCodeType.tp_members = LineCodeMembers;
    LineCodeType.tp_getset = LineCodeGetSet;
    LineCodeType.tp_new = linecode_new;
    if(PyType_Ready(&LineCodeType) < 0) throw py::error_already_set();
    m.add_object("linecode", py::handle((PyObject*)&LineCodeType));
}

ParseState parse_state;

void remember_canon_error() {
    if(parse_state.error_type || !PyErr_Occurred()) return;
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    Py_XINCREF(type); Py_XINCREF(value); Py_XINCREF(traceback);
    parse_state.error_type = type;
    parse_state.error_value = value;
    parse_state.error_traceback = traceback;
    PyErr_Restore(type, value, traceback);
}

static void forget_canon_error() {
    Py_CLEAR(parse_state.error_type);
    Py_CLEAR(parse_state.error_value);
    Py_CLEAR(parse_state.error_traceback);
}

// The line the interpreter is executing, as the program numbers it. While a
// remap runs in the context of the block that triggered it, the interpreter's
// sequence_number is 0 (enter_remap() parks the real one); what the remap
// does belongs to the remapped line, which source_line_number() gives.
static int interp_line(int sequence_number) {
    Interp *interp = dynamic_cast<Interp *>(parse_state.pinterp);
    if(!interp || sequence_number != interp->sequence_number())
        return sequence_number;
    return interp->source_line_number();
}

int ParseState::current_line() const {
    return pinterp ? interp_line(pinterp->sequence_number()) : last_sequence_number;
}

static void maybe_new_line(int sequence_number);
static void maybe_new_line();

// Which file the interpreter is reading and how deep in calls and remaps it
// is: the main program, or a subroutine's or a remap's .ngc, made absolute.
// False for an interpreter that is not ours, which does not say.
static bool current_source(const std::string *&file, int &call_level) {
    Interp *interp = dynamic_cast<Interp *>(parse_state.pinterp);
    if(!interp) return false;
    // realpath() once per file rather than once per line
    static std::string raw, absolute;
    const char *name = interp->_setup.filename;
    // the line before the first (next_line 0) comes ahead of the
    // interpreter naming the file it opened: that is the main program
    if(!*name && interp->_setup.call_level == 0) name = parse_state.main_file.c_str();
    if(raw != name) {
        raw = name;
        char buf[PATH_MAX];
        absolute = *name && realpath(name, buf) ? buf : name;
    }
    file = &absolute;
    call_level = interp->_setup.call_level;
    return true;
}

// Deliver next_line to the canon, at most once per source line - and then
// source_line(filename, line, call_level) to a canon that defines it. For
// such a canon a line is its number *in its file at its call level*, so the
// first line of a subroutine or remap is delivered even when it has the
// number the caller's line had; a canon without source_line keeps the
// number-only test, and the sequence it always had.
static void deliver_new_line(int sequence_number) {
    if(!parse_state.pinterp) return;
    if(parse_state.interp_error) return;
    const std::string *file = nullptr;
    int call_level = 0;
    bool located = parse_state.has_source_line && current_source(file, call_level);
    if(sequence_number == parse_state.last_delivered_sequence_number
            && (!located || (call_level == parse_state.last_delivered_call_level
                             && *file == parse_state.last_delivered_file)))
        return;
    parse_state.last_sequence_number = sequence_number;
    parse_state.last_delivered_sequence_number = sequence_number;
    if(located) {
        parse_state.last_delivered_file = *file;
        parse_state.last_delivered_call_level = call_level;
    }
    // Building it is inside the guard too: it is the one step here besides the
    // call that can fail, and it fails the way the call does - with the error
    // set - so the guard is what turns either into interp_error.
    canon_guard([&]{
        py::object line = py::reinterpret_steal<py::object>(
                reinterpret_cast<PyObject *>(snapshot_line(sequence_number)));
        if(!line) throw py::error_already_set();
        py::handle(parse_state.callback).attr("next_line")(line);
    });
    if(located) {
        canon_guard([&]{
            py::handle(parse_state.callback).attr("source_line")(
                    *file, sequence_number, call_level);
        });
    }
}

static void maybe_new_line(int sequence_number) {
    // Every canon function that forwards a Python callback calls
    // maybe_new_line() first, so this is where the renderer's progress report
    // lands: once per source line that produced anything, which is what a GUI
    // counts now that a rendered move delivers no next_line. In renderer mode
    // very little still forwards, so the periodic report in the parse loop is
    // what actually bounds how stale a progress bar gets.
    parse_state.canon->progress();
    deliver_new_line(interp_line(sequence_number));
}

static void maybe_new_line() {
    if(!parse_state.pinterp) return;
    maybe_new_line(parse_state.current_line());
}

// An exact type check, not py::cast: these two reject an int where a float is
// wanted, and the message is the one callers have always seen.
static double exact_float(const char *func, py::handle p) {
    if(!PyFloat_Check(p.ptr()))
        throw py::type_error(std::string(func) + ": Expected float, got "
                             + Py_TYPE(p.ptr())->tp_name);
    return PyFloat_AS_DOUBLE(p.ptr());
}

double read_external_length_units() {
    double dresult = 0.03937007874016;
    canon_guard([&]{
        dresult = exact_float("get_external_length_units",
                py::handle(parse_state.callback).attr("get_external_length_units")());
    });
    return dresult;
}

double read_external_angle_units() {
    double dresult = 1.0;
    canon_guard([&]{
        // Note the mismatch, kept: the method is `angular`, the message says
        // `angle`.
        dresult = exact_float("get_external_angle_units",
                py::handle(parse_state.callback).attr("get_external_angular_units")());
    });
    return dresult;
}

// A canon event whose payload is a Point9 spread into nine doubles - the
// callback's signature predates Point9 and takes x..w as separate arguments.
static void forward9(const char *method, const Point9 &p) {
    std::apply([&](auto... c) { forward(method, c...); }, p);
}
static void forward9(const char *method, int index, const Point9 &p) {
    std::apply([&](auto... c) { forward(method, index, c...); }, p);
}

// Whether the canon defines `name`: on its class (and so its bases) or set on
// the instance itself. Deliberately not hasattr(): a canon with a catch-all
// __getattr__ - several tests and out-of-tree canons stub every callback that
// way - would claim every optional event and start getting calls, and None
// answers to M66, it never asked for.
static bool canon_defines(py::handle cb, const char *name) {
    if(PyObject_HasAttrString(reinterpret_cast<PyObject *>(Py_TYPE(cb.ptr())), name))
        return true;
    // The instance's own dict, if it has a real one; anything else (no dict,
    // or a catch-all answering for __dict__ too) defines nothing here.
    PyObject *dict = PyObject_GetAttrString(cb.ptr(), "__dict__");
    if(!dict) { PyErr_Clear(); return false; }
    bool found = PyDict_Check(dict) && PyDict_GetItemString(dict, name);
    Py_DECREF(dict);
    return found;
}

// The time estimate of the callback protocol, for a canon that defines
// machine_limits() and set_move_times(): a TimeEstimator fed exactly as
// GCodeRenderer feeds its own - the same frame (the transform without the
// tool offset, which the estimator rebases across instead), the same feed
// and spindle state, the untimed first traverse, (AXIS,hide) spans left
// untimed, arcs cut by arc_segments() at the canon's arcdivision - so the
// last time it reports is the renderer's total for the same program. One
// entry per timed event the canon was handed, in order; an event that took
// no time (or was not timed) repeats the time before it.
class CallbackTimer {
public:
    CallbackTimer(const MachineLimits &limits, int arcdivision)
        : est_(limits), tool_change_seconds_(limits.tool_change_seconds),
          arcdivision_(arcdivision) {
        est_.record_events(&times_);
    }

    void set_feed_rate(double rate) { rate_ = rate; }
    // as GCodeRenderer::set_spindle_speed: the CSS factor out of the units
    // in force at the S word
    void set_spindle_speed(double speed) {
        speed_ = speed;
        css_factor_ = (parse_state.metric ? 1000.0 / 25.4 : 12.0)
                / (2 * M_PI) * speed;
    }
    void set_feed_mode(int per_rev) { per_rev_ = per_rev != 0; }
    void set_spindle_mode(double css_max) { css_max_ = css_max; }
    void set_spindle_sync(double pitch) { sync_pitch_ = pitch; est_.stop(); }
    void comment(const char *text) {
        const char *rest;
        if(!strncmp(text, "AXIS,", 5)) rest = text + 5;
        else if(!strncmp(text, "PREVIEW,", 8)) rest = text + 8;
        else return;
        if(strcspn(rest, ",") != 4) return;
        if(!strncmp(rest, "hide", 4)) suppress_ ++;
        else if(!strncmp(rest, "show", 4)) suppress_ --;
    }
    void tool_offset(const Point9 &o) {
        lo_ = lo_ - o + tool_;
        tool_ = o;
    }
    void user_defined_function() { if(suppress_ <= 0) est_.stop(); }

    enum Kind { Traverse, Feed, Probe };
    void move(Kind kind, int line, const Point9 &in) {
        int ev = new_event();
        if(suppress_ > 0) return;
        est_.mark(ev);
        Point9 p;
        transform(in, p);
        FeedState fs = feed_state();
        bool timed = true;
        if(first_) {
            // the caller's `G53 G0 <where the machine is>` initcode
            first_ = false;
            if(kind == Traverse) timed = false;
        }
        if(!timed) ;
        else if(kind == Traverse) est_.traverse(line, lo_, p, fs);
        else if(kind == Probe) est_.probe(line, lo_, p, fs, lo_x_, in[P9_X]);
        else est_.feed(line, lo_, p, fs, lo_x_, in[P9_X], 0.0);
        lo_ = p;
        lo_x_ = in[P9_X];
        est_.mark(-1);
    }
    // In to the point and back out: two entries.
    void rigid_tap(int line, double x, double y, double z, double scale) {
        int in = new_event(), out = new_event();
        if(suppress_ > 0) return;
        Point9 p;
        transform({x, y, z}, p);
        Point9 end = lo_;
        end[P9_X] = p[P9_X]; end[P9_Y] = p[P9_Y]; end[P9_Z] = p[P9_Z];
        est_.mark(in, out);
        est_.rigid_tap(line, lo_, end, feed_state(), scale);
        est_.mark(-1);
    }
    void arc(int line, double first_end, double second_end, double first_axis,
             double second_axis, int rotation, double axis_end_point,
             double a, double b, double c, double u, double v, double w) {
        int ev = new_event();
        if(suppress_ > 0) return;
        double radius = 0.0;
        double rad = parse_state.rotation_xy * (M_PI / 180.0);
        arc_segments(lo_, parse_state.plane, cos(rad), sin(rad),
                     parse_state.g5x, parse_state.g92, parse_state.frame,
                     first_end, second_end, first_axis, second_axis, rotation,
                     axis_end_point, a, b, c, u, v, w,
                     arcdivision_, segs_, &radius);
        first_ = false;
        est_.mark(ev);
        FeedState fs = feed_state();
        for(const Point9 &q : segs_) {
            double prog_x = untransform_x(q);
            est_.feed(line, lo_, q, fs, lo_x_, prog_x, radius);
            lo_ = q;
            lo_x_ = prog_x;
        }
        est_.mark(-1);
    }
    void dwell(int line, double seconds) {
        int ev = new_event();
        if(suppress_ > 0) return;
        est_.mark(ev);
        est_.fixed(line, seconds);
        est_.mark(-1);
    }
    void change_tool(int line) {
        est_.mark(new_event());
        est_.fixed(line, tool_change_seconds_);
        est_.mark(-1);
    }

    // The cumulative times, every entry filled, and what was not modelled.
    std::vector<double> finish(std::vector<std::string> &flags) {
        est_.finish();
        double last = 0.0;
        for(double &t : times_) {
            if(std::isnan(t)) t = last;
            else last = t;
        }
        flags = est_.flags();
        return times_;
    }

private:
    int new_event() {
        times_.push_back(NAN);
        return (int)times_.size() - 1;
    }
    FeedState feed_state() const {
        FeedState fs;
        fs.rate = rate_;
        fs.per_rev = per_rev_;
        fs.sync_pitch = sync_pitch_;
        fs.rpm = speed_;
        fs.css_max = css_max_;
        fs.css_factor = css_factor_;
        fs.metric = parse_state.metric;
        return fs;
    }
    // GCodeRenderer::transform, off the frame the module keeps
    static void transform(const Point9 &in, Point9 &out) {
        out = in;
        parse_state.frame.apply(out);
        out += parse_state.g92;
        if(parse_state.rotation_xy != 0.0) {
            double rad = parse_state.rotation_xy * (M_PI / 180.0);
            double c = cos(rad), sn = sin(rad);
            double rotx = out[P9_X] * c - out[P9_Y] * sn;
            out[P9_Y] = out[P9_X] * sn + out[P9_Y] * c;
            out[P9_X] = rotx;
        }
        out += parse_state.g5x;
    }
    // ... and GCodeRenderer::untransform's X, the CSS radius of a segment
    static double untransform_x(const Point9 &in) {
        Point9 out = in - parse_state.g5x;
        if(parse_state.rotation_xy != 0.0) {
            double rad = parse_state.rotation_xy * (M_PI / 180.0);
            double c = cos(rad), sn = -sin(rad);
            out[P9_X] = out[P9_X] * c - out[P9_Y] * sn;
        }
        return out[P9_X] - parse_state.g92[P9_X];
    }

    TimeEstimator est_;
    std::vector<double> times_;
    double tool_change_seconds_;
    int arcdivision_;
    std::vector<Point9> segs_;
    double rate_ = 0.0, speed_ = 0.0, css_max_ = 0.0, css_factor_ = 0.0;
    double sync_pitch_ = 0.0;
    bool per_rev_ = false;
    int suppress_ = 0;
    bool first_ = true;
    Point9 lo_ = {}, tool_ = {};
    double lo_x_ = 0.0;
};

// The per-event callback protocol: each canon event becomes one Python call,
// preceded by at most one next_line per source line - byte-for-byte the
// sequence this module has always produced.
//
// The events added for program simulators (stops, spindle, feed modes, M66)
// are optional: each is looked up once, when the protocol is made at the
// start of a parse (see canon_defines), and a canon without the method gets
// neither the call nor the next_line it would have brought - the sequence it
// always had.
class CallbackCanon final : public Canon {
public:
    CallbackCanon() {
        py::handle cb(parse_state.callback);
        has_program_stop_ = canon_defines(cb, "program_stop");
        has_optional_program_stop_ = canon_defines(cb, "optional_program_stop");
        has_program_end_ = canon_defines(cb, "program_end");
        has_spindle_on_ = canon_defines(cb, "spindle_on");
        has_spindle_off_ = canon_defines(cb, "spindle_off");
        has_set_spindle_speed_ = canon_defines(cb, "set_spindle_speed");
        has_set_spindle_mode_ = canon_defines(cb, "set_spindle_mode");
        has_set_feed_mode_ = canon_defines(cb, "set_feed_mode");
        has_set_spindle_sync_ = canon_defines(cb, "set_spindle_sync");
        has_get_digital_input_ = canon_defines(cb, "get_digital_input");
        has_get_analog_input_ = canon_defines(cb, "get_analog_input");
        has_wait_ = canon_defines(cb, "wait");
        has_set_position_ = canon_defines(cb, "set_position");
        if(canon_defines(cb, "machine_limits") && canon_defines(cb, "set_move_times"))
            make_timer();
    }

    void arc_feed(int line_number, double first_end, double second_end,
                  double first_axis, double second_axis, int rotation,
                  double axis_end_point, double a, double b, double c,
                  double u, double v, double w) override {
        maybe_new_line(line_number);
        forward("arc_feed", first_end, second_end, first_axis, second_axis,
                rotation, axis_end_point, a, b, c, u, v, w);
        if(timed()) timer_->arc(line_number, first_end, second_end, first_axis,
                                second_axis, rotation, axis_end_point,
                                a, b, c, u, v, w);
    }
    void straight_feed(int line_number, const Point9 &p) override {
        maybe_new_line(line_number);
        forward9("straight_feed", p);
        if(timed()) timer_->move(CallbackTimer::Feed, line_number, p);
    }
    void straight_traverse(int line_number, const Point9 &p) override {
        maybe_new_line(line_number);
        forward9("straight_traverse", p);
        if(timed()) timer_->move(CallbackTimer::Traverse, line_number, p);
    }
    void straight_probe(int line_number, const Point9 &p) override {
        maybe_new_line(line_number);
        forward9("straight_probe", p);
        if(timed()) timer_->move(CallbackTimer::Probe, line_number, p);
    }
    // The retract multiplier is dropped: the callback takes three arguments.
    void rigid_tap(int line_number, double x, double y, double z,
                   double retract_scale) override {
        maybe_new_line(line_number);
        forward("rigid_tap", x, y, z);
        if(timed()) timer_->rigid_tap(line_number, x, y, z, retract_scale);
    }
    void dwell(double seconds) override {
        maybe_new_line();
        forward("dwell", seconds);
        if(timed()) timer_->dwell(parse_state.current_line(), seconds);
    }
    void user_defined_function(int num, double arg1, double arg2) override {
        maybe_new_line();
        forward("user_defined_function", num, arg1, arg2);
        if(timed()) timer_->user_defined_function();
    }
    void change_tool(int tool) override {
        maybe_new_line();
        forward("change_tool", tool);
        if(timed()) timer_->change_tool(parse_state.current_line());
    }
    void tool_offset(const Point9 &o) override {
        maybe_new_line();
        forward9("tool_offset", o);
        if(timer_) timer_->tool_offset(o);
    }
    void set_g5x_offset(int index, const Point9 &o) override {
        maybe_new_line();
        forward9("set_g5x_offset", index, o);
    }
    void set_g92_offset(const Point9 &o) override {
        maybe_new_line();
        forward9("set_g92_offset", o);
    }
    void set_xy_rotation(double degrees) override {
        maybe_new_line();
        forward("set_xy_rotation", degrees);
    }
    void set_g68_frame(const WorkFrame &f) override {
        maybe_new_line();
        const std::array<double, 9> &r = f.rotation;
        forward("set_g68_frame", f.origin[P9_X], f.origin[P9_Y], f.origin[P9_Z],
                r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8],
                (int)f.active);
    }
    void set_plane(int plane) override {
        maybe_new_line();
        forward("set_plane", plane);
    }
    void set_feed_rate(double rate) override {
        maybe_new_line();
        forward("set_feed_rate", rate);
        if(timer_) timer_->set_feed_rate(rate);
    }
    // the timer's (AXIS,hide) depth, after the canon has had the comment
    void comment(const char *text) override {
        if(timer_) timer_->comment(text);
    }
    void set_traverse_rate(double rate) override {
        maybe_new_line();
        forward("set_traverse_rate", rate);
    }
    void set_position(const Point9 &p) override {
        if(!has_set_position_) return;
        maybe_new_line();
        forward9("set_position", p);
    }
    void program_stop() override {
        if(!has_program_stop_) return;
        maybe_new_line();
        forward("program_stop");
    }
    void optional_program_stop() override {
        if(!has_optional_program_stop_) return;
        maybe_new_line();
        forward("optional_program_stop");
    }
    void program_end() override {
        if(!has_program_end_) return;
        maybe_new_line();
        forward("program_end");
    }
    void spindle_on(int direction) override {
        if(!has_spindle_on_) return;
        maybe_new_line();
        forward("spindle_on", direction);
    }
    void spindle_off() override {
        if(!has_spindle_off_) return;
        maybe_new_line();
        forward("spindle_off");
    }
    void set_spindle_speed(double speed) override {
        if(timer_) timer_->set_spindle_speed(speed);
        if(!has_set_spindle_speed_) return;
        maybe_new_line();
        forward("set_spindle_speed", speed);
    }
    void set_spindle_mode(double css_max) override {
        if(timer_) timer_->set_spindle_mode(css_max);
        if(!has_set_spindle_mode_) return;
        maybe_new_line();
        forward("set_spindle_mode", css_max);
    }
    void set_feed_mode(int per_rev) override {
        if(timer_) timer_->set_feed_mode(per_rev);
        if(!has_set_feed_mode_) return;
        maybe_new_line();
        forward("set_feed_mode", per_rev);
    }
    void set_spindle_sync(double units_per_rev) override {
        if(timer_) timer_->set_spindle_sync(units_per_rev);
        if(!has_set_spindle_sync_) return;
        maybe_new_line();
        forward("set_spindle_sync", units_per_rev);
    }
    // The answers go through int() and float(), as a Python caller would
    // convert them; a failed call or conversion ends the parse like any
    // other callback, and the interpreter gets the default meanwhile.
    int digital_input(int index, int def) override {
        if(!has_get_digital_input_) return def;
        maybe_new_line();
        int result = def;
        canon_guard([&]{
            result = py::int_(py::handle(parse_state.callback)
                    .attr("get_digital_input")(index, def)).cast<int>();
        });
        return result;
    }
    double analog_input(int index, double def) override {
        if(!has_get_analog_input_) return def;
        maybe_new_line();
        double result = def;
        canon_guard([&]{
            result = py::float_(py::handle(parse_state.callback)
                    .attr("get_analog_input")(index, def)).cast<double>();
        });
        return result;
    }
    int wait_input(int index, int input_type, int wait_type,
                   double timeout) override {
        if(!has_wait_) return 0;
        maybe_new_line();
        int result = 0;
        canon_guard([&]{
            result = py::int_(py::handle(parse_state.callback)
                    .attr("wait")(index, input_type, wait_type, timeout)).cast<int>();
        });
        return result;
    }
    // Asked every time, uncached: a canon on this protocol was going to be
    // called per event anyway, and one may legitimately watch the traffic.
    double external_length_units() override {
        return read_external_length_units();
    }
    double external_angle_units() override {
        return read_external_angle_units();
    }

    // The estimate, handed over once the parse is done.
    void finish() override {
        if(!timer_ || parse_state.interp_error) return;
        std::vector<std::string> flags;
        std::vector<double> times = timer_->finish(flags);
        timer_.reset();
        canon_guard([&]{
            py::list seq(times.size());
            for(size_t i = 0; i < times.size(); i++) seq[i] = times[i];
            py::handle(parse_state.callback).attr("set_move_times")(seq);
            if(!flags.empty()) {
                py::list f;
                for(const std::string &name : flags) f.append(name);
                py::handle(parse_state.callback).attr("set_time_flags")(f);
            }
        });
    }

private:
    // A timed event the canon was handed: one that reached it.
    bool timed() const { return timer_ && !parse_state.interp_error; }

    // machine_limits(), read as the renderer reads it; None or limits that
    // time nothing leave the parse untimed.
    void make_timer() {
        canon_guard([&]{
            py::handle cb(parse_state.callback);
            py::object ml = cb.attr("machine_limits")();
            if(ml.is_none()) return;
            double scale = 25.4 * read_external_length_units();
            if(parse_state.interp_error) return;
            if(!(scale > 0.0))
                throw py::value_error("machine_limits: the canon's length "
                                      "units are zero");
            MachineLimits limits = machine_limits_from(ml, scale);
            if(!limits.usable()) return;
            int arcdivision = 64;
            py::object div = py::getattr(cb, "arcdivision", py::none());
            if(!div.is_none()) {
                long n = PyLong_AsLong(div.ptr());
                if(n > 0) arcdivision = (int)n;
                PyErr_Clear();
            }
            timer_.reset(new CallbackTimer(limits, arcdivision));
        });
    }

    std::unique_ptr<CallbackTimer> timer_;
    bool has_program_stop_ = false;
    bool has_optional_program_stop_ = false;
    bool has_program_end_ = false;
    bool has_spindle_on_ = false;
    bool has_spindle_off_ = false;
    bool has_set_spindle_speed_ = false;
    bool has_set_spindle_mode_ = false;
    bool has_set_feed_mode_ = false;
    bool has_set_spindle_sync_ = false;
    bool has_get_digital_input_ = false;
    bool has_get_analog_input_ = false;
    bool has_wait_ = false;
    bool has_set_position_ = false;
};

// The interpreter works in the program's own units; every canon method below
// hands the canon inches. Only lengths convert - the angular components of a
// 9-DOF point are degrees in either unit system, and so are left alone.
static double ensure_inch(double length) {
    return parse_state.metric ? length / 25.4 : length;
}

// axis 0 X to 8 W
static double ensure_inch(int axis, double value) {
    return (parse_state.angular >> axis) & 1 ? value : ensure_inch(value);
}

static Point9 ensure_inch(const Point9 &p) {
    Point9 q;
    for(int i = 0; i < 9; i++) q[i] = ensure_inch(i, p[i]);
    return q;
}

// Back from inches to the program's units, for what the interpreter reads.
static Point9 from_inch(const Point9 &p) {
    Point9 q;
    for(int i = 0; i < 9; i++)
        q[i] = (parse_state.angular >> i) & 1 || !parse_state.metric
             ? p[i] : p[i] * 25.4;
    return q;
}

// Machine units - those of stat.position and stat.tool_offset - to inches
// and degrees, by the canon's external units as the interpreter converts a
// tool offset (USER_TO_PROGRAM_LEN).
static Point9 machine_units_to_inch(const Point9 &m) {
    double length_units = GET_EXTERNAL_LENGTH_UNITS();
    double angle_units = GET_EXTERNAL_ANGLE_UNITS();
    Point9 q;
    for(int i = 0; i < 9; i++)
        q[i] = (parse_state.angular >> i) & 1 ? m[i] / angle_units
                                              : m[i] / length_units / 25.4;
    return q;
}

// The program frame, in the order task's canon applies it
// (rotate_and_offset_pos / unoffset_and_unrotate_pos in emccanon.cc):
// machine = g5x + Rz(rotation_xy) * (g92 + frame(program)) + tool.
static void rotate_xy(Point9 &p, double degrees) {
    if(degrees == 0) return;
    double t = degrees * M_PI / 180, c = cos(t), sn = sin(t);
    double x = p[P9_X], y = p[P9_Y];
    p[P9_X] = x * c - y * sn;
    p[P9_Y] = x * sn + y * c;
}

static Point9 to_machine(Point9 p) {
    parse_state.frame.apply(p);
    p += parse_state.g92;
    rotate_xy(p, parse_state.rotation_xy);
    p += parse_state.g5x;
    p += parse_state.tool;
    return p;
}

static Point9 to_program(Point9 m) {
    m -= parse_state.tool;
    m -= parse_state.g5x;
    rotate_xy(m, -parse_state.rotation_xy);
    m -= parse_state.g92;
    parse_state.frame.remove(m);
    return m;
}

// A move was delivered and ends at `p` (inches, the straight_traverse
// frame): the point the next move starts from, until an offset moves it.
static void moved_to(const Point9 &p) {
    parse_state.have_point = true;
    parse_state.point = p;
    parse_state.machine = to_machine(p);
    parse_state.position_dirty = false;
}

// If an offset changed since the last move, the machine stayed where that
// move left it and the program frame moved: tell the canon where that puts
// the controlled point, unless it is the point it already has. Called
// before every move and after every block, so it lands on the line that
// changed the offset and ahead of any move after it.
static void flush_position() {
    if(!parse_state.position_dirty) return;
    parse_state.position_dirty = false;
    if(!parse_state.have_point || parse_state.interp_error) return;
    Point9 now = to_program(parse_state.machine);
    bool same = true;
    for(int i = 0; i < 9; i++)
        if(fabs(now[i] - parse_state.point[i])
                > 1e-9 * (1 + fabs(parse_state.point[i]))) same = false;
    if(same) return;
    parse_state.point = now;
    parse_state.canon->set_position(now);
}

static void offsets_changed() { parse_state.position_dirty = true; }

// One point of a NURBS curve, fed through the canon. The curve's two
// components land in the active plane and every other axis holds the position
// the interpreter last reported; a plane that carries no NURBS feeds nothing,
// as the three separate `if`s this replaces did. STRAIGHT_FEED is a canon
// entry point and keeps its loose arguments, so this is where the nine are
// assembled.
static void nurbs_feed(int line_number, CANON_PLANE plane,
                       double first, double second) {
    const Point9 &p = parse_state.pos;
    switch(plane) {
    case CANON_PLANE::XY:
        STRAIGHT_FEED(line_number, first, second, p[P9_Z], p[P9_A], p[P9_B], p[P9_C],
                      p[P9_U], p[P9_V], p[P9_W]);
        break;
    case CANON_PLANE::YZ:
        STRAIGHT_FEED(line_number, p[P9_X], first, second, p[P9_A], p[P9_B], p[P9_C],
                      p[P9_U], p[P9_V], p[P9_W]);
        break;
    case CANON_PLANE::XZ:
        STRAIGHT_FEED(line_number, second, p[P9_Y], first, p[P9_A], p[P9_B], p[P9_C],
                      p[P9_U], p[P9_V], p[P9_W]);
        break;
    default:
        break;
    }
}

//das ist für die Vorschau
/* G_5_2/G_5_3*/
void NURBS_G5_FEED(int line_number, const std::vector<NURBS_CONTROL_POINT>& nurbs_control_points, unsigned int nurbs_order, CANON_PLANE plane)
    {
    double u = 0.0;
    unsigned int n = nurbs_control_points.size() - 1;
    double umax = n - nurbs_order + 2;
    unsigned int div = nurbs_control_points.size()*15;
    std::vector<unsigned int> knot_vector = nurbs_G5_knot_vector_creator(n, nurbs_order);
    NURBS_PLANE_POINT P1;
    while (u+umax/div < umax) {
        NURBS_PLANE_POINT P1 = nurbs_G5_point(u+umax/div,nurbs_order,nurbs_control_points,knot_vector);
        //printf("P1 X: %8.4f Y: %8.4f pos_x: %8.4f pos_y: %8.4f pos_z: %8.4f (F: %s L: %d)\n",P1.NURBS_X,P1.NURBS_Y,parse_state.pos_x,parse_state.pos_y,parse_state.pos_z,__FILE__,__LINE__);

        //STRAIGHT_FEED(line_number, P1.X,P1.Y, parse_state.pos_z, parse_state.pos_a, parse_state.pos_b, parse_state.pos_c, parse_state.pos_u, parse_state.pos_v, parse_state.pos_w);
        nurbs_feed(line_number, plane, P1.NURBS_X, P1.NURBS_Y);
        u = u + umax/div;
        }
    P1.NURBS_X = nurbs_control_points[n].NURBS_X;
    P1.NURBS_Y = nurbs_control_points[n].NURBS_Y;
    //printf("Pn X: %8.4f Y: %8.4f pos_x: %8.4f pos_y: %8.4f pos_z: %8.4f (F: %s L: %d)\n",P1.X,P1.Y,parse_state.pos_x,parse_state.pos_y,parse_state.pos_z,__FILE__,__LINE__);
    //STRAIGHT_FEED(line_number, P1.X,P1.Y, parse_state.pos_z, parse_state.pos_a, parse_state.pos_b, parse_state.pos_c, parse_state.pos_u, parse_state.pos_v, parse_state.pos_w);
    nurbs_feed(line_number, plane, P1.NURBS_X, P1.NURBS_Y);
    knot_vector.clear();
}

/* G_6_2  L_option is unused */
//-----------------------------------------------------------------------------------------------------------------------------------------
void NURBS_G6_FEED(int line_number, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, unsigned int k, double /*feedrate*/, int /*L_option*/, CANON_PLANE plane) { // (L_option: NICU, NICL, NICC see publication from Lo Valvo and Drago)
    double u = 0.0;
    unsigned int n = nurbs_control_points.size() - 1-k;
    double umax = nurbs_control_points[n+k].NURBS_K;
    unsigned int div = (nurbs_control_points.size()-k)*15;
    std::vector<double> knot_vector = nurbs_g6_knot_vector_creator(n, k, nurbs_control_points);

    //printf("gcodemodule NURBS_G6_FEED cps: %ld k: %d L: %d fr: %f (F: %s L: %d)\n",nurbs_control_points.size(), k, L_option, feedrate, __FILE__, __LINE__);
    NURBS_PLANE_POINT P1x, P1;
    std::vector< std::vector<double> > A6;
    A6 = nurbs_G6_Nmix_creator(u+umax/div, k, n+1, knot_vector);
    P1 = nurbs_G6_pointx(knot_vector[0],k,nurbs_control_points,knot_vector,A6);
    //printf("%.3d P1  X: %8.4f Y: %8.4f pos_x: %8.4f pos_y: %8.4f pos_z: %8.4f (F: %s L: %d)\n",line_number,P1.NURBS_X,P1.NURBS_Y,parse_state.pos_x,parse_state.pos_y,parse_state.pos_z,__FILE__,__LINE__);
    //STRAIGHT_FEED(line_number, P1.NURBS_X,P1.NURBS_Y, parse_state.pos_z, parse_state.pos_a, parse_state.pos_b, parse_state.pos_c, parse_state.pos_u, parse_state.pos_v, parse_state.pos_w);
    nurbs_feed(line_number, plane, P1.NURBS_X, P1.NURBS_Y);
    u=0.1;
    while (u+umax/div < umax) {
        P1x = nurbs_G6_point_x(u+umax/div,k,nurbs_control_points,knot_vector);
        //printf("%.3d P1x X: %8.4f Y: %8.4f pos_x: %8.4f pos_y: %8.4f pos_z: %8.4f (F: %s L: %d)\n",line_number,P1x.NURBS_X,P1x.NURBS_Y,parse_state.pos_x,parse_state.pos_y,parse_state.pos_z,__FILE__,__LINE__);
        //STRAIGHT_FEED(line_number, P1x.NURBS_X,P1x.NURBS_Y, parse_state.pos_z, parse_state.pos_a, parse_state.pos_b, parse_state.pos_c, parse_state.pos_u, parse_state.pos_v, parse_state.pos_w);
		nurbs_feed(line_number, plane, P1x.NURBS_X, P1x.NURBS_Y);
		u = u + umax/div;
    }
    A6 = nurbs_G6_Nmix_creator (umax,  k, n+1, knot_vector);
    P1 = nurbs_G6_pointx(umax,k,nurbs_control_points,knot_vector,A6);
    //printf("%.3d P1  X: %8.4f Y: %8.4f pos_x: %8.4f pos_y: %8.4f pos_z: %8.4f (F: %s L: %d)\n",line_number,P1.NURBS_X,P1.NURBS_Y,parse_state.pos_x,parse_state.pos_y,parse_state.pos_z,__FILE__,__LINE__);
    //STRAIGHT_FEED(line_number, P1.NURBS_X,P1.NURBS_Y, parse_state.pos_z, parse_state.pos_a, parse_state.pos_b, parse_state.pos_c, parse_state.pos_u, parse_state.pos_v, parse_state.pos_w);
    nurbs_feed(line_number, plane, P1.NURBS_X, P1.NURBS_Y);
    knot_vector.clear();
	}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ARC_FEED(int line_number,
              double first_end, double second_end, double first_axis,
              double second_axis, int rotation, double axis_end_point,
              double a_position, double b_position, double c_position,
              double u_position, double v_position, double w_position) {
    // XXX: set _pos_*
    flush_position();
    parse_state.canon->arc_feed(line_number,
                                ensure_inch(first_end),
                                ensure_inch(second_end),
                                ensure_inch(first_axis),
                                ensure_inch(second_axis),
                                rotation,
                                ensure_inch(axis_end_point),
                                ensure_inch(P9_A, a_position),
                                ensure_inch(P9_B, b_position),
                                ensure_inch(P9_C, c_position),
                                ensure_inch(P9_U, u_position),
                                ensure_inch(P9_V, v_position),
                                ensure_inch(P9_W, w_position));
    // The end in the nine axes, by the plane's pairing of first and second.
    Point9 end = {first_end, second_end, axis_end_point,
                  a_position, b_position, c_position,
                  u_position, v_position, w_position};
    if(parse_state.plane == static_cast<int>(CANON_PLANE::XZ)) {
        end[P9_X] = second_end; end[P9_Y] = axis_end_point; end[P9_Z] = first_end;
    } else if(parse_state.plane == static_cast<int>(CANON_PLANE::YZ)) {
        end[P9_X] = axis_end_point; end[P9_Y] = first_end; end[P9_Z] = second_end;
    }
    moved_to(ensure_inch(end));
}

void STRAIGHT_FEED(int line_number,
                   double x, double y, double z,
                   double a, double b, double c,
                   double u, double v, double w) {
    parse_state.pos = {x, y, z, a, b, c, u, v, w};
    flush_position();
    Point9 p = ensure_inch({x, y, z, a, b, c, u, v, w});
    parse_state.canon->straight_feed(line_number, p);
    moved_to(p);
}

void STRAIGHT_TRAVERSE(int line_number,
                       double x, double y, double z,
                       double a, double b, double c,
                       double u, double v, double w) {
    parse_state.pos = {x, y, z, a, b, c, u, v, w};
    flush_position();
    Point9 p = ensure_inch({x, y, z, a, b, c, u, v, w});
    parse_state.canon->straight_traverse(line_number, p);
    moved_to(p);
}

void SET_G5X_OFFSET(int g5x_index,
                    double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {
    parse_state.g5x = ensure_inch({x, y, z, a, b, c, u, v, w});
    offsets_changed();
    parse_state.canon->set_g5x_offset(g5x_index, parse_state.g5x);
}

void SET_G92_OFFSET(double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w) {
    parse_state.g92 = ensure_inch({x, y, z, a, b, c, u, v, w});
    offsets_changed();
    parse_state.canon->set_g92_offset(parse_state.g92);
}

void SET_XY_ROTATION(double t) {
    parse_state.rotation_xy = t;
    offsets_changed();
    parse_state.canon->set_xy_rotation(t);
};

void SET_G68_FRAME(double x, double y, double z,
                   const double rotation[9], int active) {
    WorkFrame frame;
    frame.active = active != 0;
    frame.origin = ensure_inch({x, y, z});
    std::copy(rotation, rotation + 9, frame.rotation.begin());
    parse_state.frame = frame;
    offsets_changed();
    parse_state.canon->set_g68_frame(frame);
};

void USE_LENGTH_UNITS(CANON_UNITS u) { parse_state.metric = u == CANON_UNITS_MM; }

void SELECT_PLANE(CANON_PLANE pl) {
    parse_state.plane = static_cast<int>(pl);
    parse_state.canon->set_plane(static_cast<int>(pl));
}

void HOME_CYCLE(void) {}
void HOME_CYCLE_JOINT(int) {}

void SET_TRAVERSE_RATE(double rate) {
    parse_state.canon->set_traverse_rate(rate);
}

// Any spindle: G95 arrives as the block's active spindle, so filtering on 0
// would drop a per-revolution feed on a machine whose turning spindle is not
// the first. Only spindle 0's speed is tracked either way.
void SET_FEED_MODE(int /*spindle*/, int mode) {
    parse_state.canon->set_feed_mode(mode);
}

// Ask the canon `method()` for None or nine numbers. True with `out` filled
// for nine; false for None, or after a failure (left on the interp_error
// protocol, so the parse ends with it).
static bool optional_nine(const char *method, Point9 &out) {
    bool answered = false;
    canon_guard([&]{
        py::object r = py::handle(parse_state.callback).attr(method)();
        if(r.is_none()) return;
        if(!PySequence_Check(r.ptr()) || PySequence_Size(r.ptr()) != 9)
            throw py::type_error(std::string(method)
                                 + ": expected None or nine numbers");
        py::sequence seq = py::reinterpret_borrow<py::sequence>(r);
        for(size_t i = 0; i < 9; i++) out[i] = py::cast<double>(seq[i]);
        answered = true;
    });
    return answered && !parse_state.interp_error;
}

// The spindle slot of the private tool table at a tool change, as iocontrol
// does it (emcToolLoad, then load_tool once the change is done).
static void private_table_change_tool(int toolno) {
    int idx = tooldata_find_index_for_tool(toolno);
    if(idx < 0) return;
    bool random = tool_mmap_is_random_toolchanger();
    // it does not make sense to load a tool from the spindle pocket
    if(random && idx == 0) return;
    if(!random && idx > 0) {
        // nor to load the tool already in the spindle
        CANON_TOOL_TABLE wanted, spindle;
        if(tooldata_get(&wanted, idx) == IDX_OK
                && tooldata_get(&spindle, 0) == IDX_OK
                && spindle.toolno == wanted.toolno)
            return;
    }
    tooldata_load_spindle(idx, random);
}

// The machine as it stands at cycle start, from a simulator canon - what
// task's synch would hand a real interpreter. In two halves around the
// interpreter's init(): the tool in the spindle and the tool offset first,
// since init() loads the tool table; the position after, since turning a
// machine position into program coordinates needs the work offsets init()
// sends.
static void machine_state_before_init() {
    // The tool in the spindle, as an M61 Q<n> leaves the private table
    // (emcToolSetNumber): its entry loaded into the spindle slot.
    if(parse_state.has_get_tool_in_spindle && parse_state.private_tool_table) {
        canon_guard([&]{
            py::object r = py::handle(parse_state.callback)
                    .attr("get_tool_in_spindle")();
            if(r.is_none()) return;
            int toolno = py::cast<int>(r);
            if(toolno == 0) {
                CANON_TOOL_TABLE empty = tooldata_entry_init();
                empty.toolno = 0;
                empty.pocketno = 0;
                tooldata_put(empty, 0);
                return;
            }
            int idx = tooldata_find_index_for_tool(toolno);
            if(idx < 0)
                throw py::value_error("get_tool_in_spindle: tool "
                        + std::to_string(toolno) + " is not in the tool table");
            if(idx > 0)
                tooldata_load_spindle(idx, tool_mmap_is_random_toolchanger());
        });
    }
    // The tool offset in effect (stat.tool_offset), machine units; None is
    // none, as after G49.
    if(parse_state.has_get_tool_offset) {
        Point9 offset = {};
        if(optional_nine("get_tool_offset", offset))
            parse_state.tool = machine_units_to_inch(offset);
        else if(!parse_state.interp_error)
            parse_state.tool = {};
    }
}

static void machine_state_after_init() {
    Interp *interp = dynamic_cast<Interp *>(parse_state.pinterp);
    // init() zeroes its tool offset; give it the one in effect, in its
    // units (inches until the program says otherwise)
    if(parse_state.has_get_tool_offset && interp) {
        Point9 t = from_inch(parse_state.tool);
        EmcPose &o = interp->_setup.tool_offset;
        o.tran.x = t[P9_X]; o.tran.y = t[P9_Y]; o.tran.z = t[P9_Z];
        o.a = t[P9_A]; o.b = t[P9_B]; o.c = t[P9_C];
        o.u = t[P9_U]; o.v = t[P9_V]; o.w = t[P9_W];
    }
    // The machine position: the interpreter starts there, and the canon is
    // told where that is in the straight_traverse frame.
    if(parse_state.has_get_initial_position) {
        Point9 machine;
        if(!optional_nine("get_initial_position", machine)) return;
        machine = machine_units_to_inch(machine);
        Point9 p = to_program(machine);
        parse_state.pos = from_inch(p);
        if(interp) interp->refresh_actual_position(&interp->_setup);
        parse_state.have_point = true;
        parse_state.point = p;
        parse_state.machine = machine;
        parse_state.position_dirty = false;
        parse_state.canon->set_position(p);
    }
}

void CHANGE_TOOL() {
    parse_state.canon->change_tool(parse_state.selected_tool);
    if(parse_state.private_tool_table)
        private_table_change_tool(parse_state.selected_tool);
}

void CHANGE_TOOL_NUMBER(int /*pocket*/) {
    maybe_new_line();
    if(parse_state.interp_error) return;
}

void RELOAD_TOOLDATA(void) {
    return;
}

/* XXX: This needs to be re-thought.  Sometimes feed rate is not in linear
 * units--e.g., it could be inverse time feed mode.  in that case, it's wrong
 * to convert from mm to inch here.  but the gcode time estimate gets inverse
 * time feed wrong anyway..
 */
void SET_FEED_RATE(double rate) {
    parse_state.canon->set_feed_rate(ensure_inch(rate));
}

void DWELL(double time) {
    parse_state.canon->dwell(time);
}

void MESSAGE(char *comment) {
    maybe_new_line();
    forward("message", comment);
}

void LOG(char * /*s*/) {}
void LOGOPEN(char * /*f*/) {}
void LOGAPPEND(char * /*f*/) {}
void LOGCLOSE() {}

void COMMENT(const char *comment) {
    maybe_new_line();
    forward("comment", comment);
    if(parse_state.interp_error) return;
    parse_state.canon->comment(comment);
}

void SET_TOOL_TABLE_ENTRY(int /*pocket*/, int /*toolno*/, const EmcPose& /*offset*/, double /*diameter*/,
                          double /*frontangle*/, double /*backangle*/, int /*orientation*/) {
}

void USE_TOOL_LENGTH_OFFSET(const EmcPose& offset) {
    parse_state.tool = ensure_inch({
            offset.tran.x, offset.tran.y, offset.tran.z,
            offset.a, offset.b, offset.c,
            offset.u, offset.v, offset.w});
    offsets_changed();
    parse_state.canon->tool_offset(parse_state.tool);
}

void SET_FEED_REFERENCE(double /*reference*/) { }
void SET_CUTTER_RADIUS_COMPENSATION(double /*radius*/) {}
void START_CUTTER_RADIUS_COMPENSATION(int /*direction*/) {}
void STOP_CUTTER_RADIUS_COMPENSATION(int /*direction*/) {}
void START_SPEED_FEED_SYNCH() {}
// G33/G33.1/G76 pitch: inches advanced per spindle revolution.
void START_SPEED_FEED_SYNCH(int /*spindle*/, double sync, bool /*vel*/, double /*angle*/) {
    parse_state.canon->set_spindle_sync(ensure_inch(sync));
}
void STOP_SPEED_FEED_SYNCH() {
    parse_state.canon->set_spindle_sync(0.0);
}
// Spindle 0 only, as SET_SPINDLE_SPEED below.
void START_SPINDLE_COUNTERCLOCKWISE(int spindle, int /*wait_for_at_speed*/) {
    if(spindle == 0) parse_state.canon->spindle_on(-1);
}
void START_SPINDLE_CLOCKWISE(int spindle, int /*wait_for_at_speed*/) {
    if(spindle == 0) parse_state.canon->spindle_on(1);
}
void SET_SPINDLE_MODE(int spindle, double css_max) {
    if(spindle == 0) parse_state.canon->set_spindle_mode(css_max);
}
void STOP_SPINDLE_TURNING(int spindle, int /*wait_for_at_speed*/) {
    if(spindle == 0) parse_state.canon->spindle_off();
}
void SET_SPINDLE_SPEED(int spindle, double rpm) {
    if(spindle == 0) parse_state.canon->set_spindle_speed(rpm);
}
void ORIENT_SPINDLE(int /*spindle*/, double /*d*/, int /*i*/) {}
void WAIT_SPINDLE_ORIENT_COMPLETE(int /*s*/, double /*timeout*/) {}
void PROGRAM_STOP() { parse_state.canon->program_stop(); }
void PROGRAM_END() { parse_state.canon->program_end(); }
void FINISH() {}
void ON_RESET() {}
void PALLET_SHUTTLE() {}
void SELECT_TOOL(int tool) {parse_state.selected_tool = tool;}
void UPDATE_TAG(const StateTag& /*tag*/) {}
void SELECT_KINS_TYPE(int /*switchkins_type*/) {}
void OPTIONAL_PROGRAM_STOP() { parse_state.canon->optional_program_stop(); }
int  GET_EXTERNAL_TC_FAULT() {return 0;}
int  GET_EXTERNAL_TC_REASON() {return 0;}


extern bool GET_BLOCK_DELETE(void) {
    int bd = 0;
    canon_guard([&]{
        // PyObject_IsTrue, not a bool cast: any object the canon returns
        // answers this, as it always has.
        bd = PyObject_IsTrue(py::handle(parse_state.callback).attr("get_block_delete")().ptr());
    });
    return bd;
}

void CANON_ERROR(const char * /*fmt*/, ...) {};
void CLAMP_AXIS(CANON_AXIS /*axis*/) {}
bool GET_OPTIONAL_PROGRAM_STOP() { return false;}
void SET_OPTIONAL_PROGRAM_STOP(bool /*state*/) {}
void SPINDLE_RETRACT_TRAVERSE() {}
void SPINDLE_RETRACT() {}
void STOP_CUTTER_RADIUS_COMPENSATION() {}
void USE_NO_SPINDLE_FORCE() {}
void SET_BLOCK_DELETE(bool /*enabled*/) {}

void DISABLE_FEED_OVERRIDE() {}
void DISABLE_FEED_HOLD() {}
void ENABLE_FEED_HOLD() {}
void DISABLE_SPEED_OVERRIDE(int /*spindle*/) {}
void ENABLE_FEED_OVERRIDE() {}
void ENABLE_SPEED_OVERRIDE(int /*spindle*/) {}
void LOCK_SPEED_OVERRIDE(int /*spindle*/) {}
void MIST_OFF() {}
void FLOOD_OFF() {}
void MIST_ON() {}
void FLOOD_ON() {}
void CLEAR_AUX_OUTPUT_BIT(int /*bit*/) {}
void SET_AUX_OUTPUT_BIT(int /*bit*/) {}
void SET_AUX_OUTPUT_VALUE(int /*index*/, double /*value*/) {}
void CLEAR_MOTION_OUTPUT_BIT(int /*bit*/) {}
void SET_MOTION_OUTPUT_BIT(int /*bit*/) {}
void SET_MOTION_OUTPUT_VALUE(int /*index*/, double /*value*/) {}
void TURN_PROBE_ON() {}
void TURN_PROBE_OFF() {}
int UNLOCK_ROTARY(int /*line_no*/, int /*joint_num*/) {return 0;}
int LOCK_ROTARY(int /*line_no*/, int /*joint_num*/) {return 0;}
void INTERP_ABORT(int /*reason*/, const char * /*message*/) {}

void STRAIGHT_PROBE(int line_number,
                    double x, double y, double z,
                    double a, double b, double c,
                    double u, double v, double w, unsigned char /*probe_type*/) {
    parse_state.pos = {x, y, z, a, b, c, u, v, w};
    flush_position();
    Point9 p = ensure_inch({x, y, z, a, b, c, u, v, w});
    parse_state.canon->straight_probe(line_number, p);
    moved_to(p);
}
void RIGID_TAP(int line_number,
               double x, double y, double z, double scale) {
    // returns to where it started, so the point stays
    flush_position();
    parse_state.canon->rigid_tap(line_number, ensure_inch(x),
                                 ensure_inch(y),
                                 ensure_inch(z), scale);
}
double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() { return 0.1; }
double GET_EXTERNAL_MOTION_CONTROL_NAIVECAM_TOLERANCE() { return 0.1; }
double GET_EXTERNAL_PROBE_POSITION_X() { return parse_state.pos[P9_X]; }
double GET_EXTERNAL_PROBE_POSITION_Y() { return parse_state.pos[P9_Y]; }
double GET_EXTERNAL_PROBE_POSITION_Z() { return parse_state.pos[P9_Z]; }
double GET_EXTERNAL_PROBE_POSITION_A() { return parse_state.pos[P9_A]; }
double GET_EXTERNAL_PROBE_POSITION_B() { return parse_state.pos[P9_B]; }
double GET_EXTERNAL_PROBE_POSITION_C() { return parse_state.pos[P9_C]; }
double GET_EXTERNAL_PROBE_POSITION_U() { return parse_state.pos[P9_U]; }
double GET_EXTERNAL_PROBE_POSITION_V() { return parse_state.pos[P9_V]; }
double GET_EXTERNAL_PROBE_POSITION_W() { return parse_state.pos[P9_W]; }
double GET_EXTERNAL_PROBE_VALUE() { return 0.0; }
int GET_EXTERNAL_PROBE_TRIPPED_VALUE() { return 0; }
double GET_EXTERNAL_POSITION_X() { return parse_state.pos[P9_X]; }
double GET_EXTERNAL_POSITION_Y() { return parse_state.pos[P9_Y]; }
double GET_EXTERNAL_POSITION_Z() { return parse_state.pos[P9_Z]; }
double GET_EXTERNAL_POSITION_A() { return parse_state.pos[P9_A]; }
double GET_EXTERNAL_POSITION_B() { return parse_state.pos[P9_B]; }
double GET_EXTERNAL_POSITION_C() { return parse_state.pos[P9_C]; }
double GET_EXTERNAL_POSITION_U() { return parse_state.pos[P9_U]; }
double GET_EXTERNAL_POSITION_V() { return parse_state.pos[P9_V]; }
double GET_EXTERNAL_POSITION_W() { return parse_state.pos[P9_W]; }
void INIT_CANON() {}

void SET_PARAMETER_FILE_NAME(const char *name)
{
  strncpy(_parameter_file_name, name, PARAMETER_FILE_NAME_LENGTH);
}

void GET_EXTERNAL_PARAMETER_FILE_NAME(char *name, int max_size) {
    name[0] = 0;
    // Not on the interp_error protocol: a canon without the attribute has
    // always just left the name empty, with the error still on the indicator.
    try {
        std::string file = py::cast<std::string>(
                py::handle(parse_state.callback).attr("parameter_file"));
        memset(name, 0, max_size);
        strncpy(name, file.c_str(), max_size - 1);
    } catch(py::error_already_set &e) {
        e.restore();
    }
}
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE() { return CANON_UNITS_INCHES; }
CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket) {
    CANON_TOOL_TABLE tdata = {-1,-1,{{0,0,0},0,0,0,0,0,0},0,0,0,0,{}};
    // A canon's own get_tool wins; without one, the private table answers.
    if(!parse_state.has_get_tool && parse_state.private_tool_table) {
        CANON_TOOL_TABLE got;
        if(tooldata_get(&got, pocket) == IDX_OK) tdata = got;
        return tdata;
    }
    canon_guard([&]{
        py::object result = py::handle(parse_state.callback).attr("get_tool")(pocket);
        if(!PyTuple_Check(result.ptr()) || PyTuple_GET_SIZE(result.ptr()) != 14)
            throw py::type_error("get_tool: expected a tuple of 14 items");
        py::tuple t = py::reinterpret_borrow<py::tuple>(result);
        // Filled only once every field parsed, so a bad entry leaves the
        // caller the same all-defaults table PyArg_ParseTuple used to.
        CANON_TOOL_TABLE got = tdata;
        got.toolno         = py::cast<int>(t[0]);
        got.offset.tran.x  = py::cast<double>(t[1]);
        got.offset.tran.y  = py::cast<double>(t[2]);
        got.offset.tran.z  = py::cast<double>(t[3]);
        got.offset.a       = py::cast<double>(t[4]);
        got.offset.b       = py::cast<double>(t[5]);
        got.offset.c       = py::cast<double>(t[6]);
        got.offset.u       = py::cast<double>(t[7]);
        got.offset.v       = py::cast<double>(t[8]);
        got.offset.w       = py::cast<double>(t[9]);
        got.diameter       = py::cast<double>(t[10]);
        got.frontangle     = py::cast<double>(t[11]);
        got.backangle      = py::cast<double>(t[12]);
        got.orientation    = py::cast<int>(t[13]);
        tdata = got;
    });
    return tdata;
}

// M66: the canon's answer when it has one, the defaults otherwise.
int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) {
    if(!parse_state.canon) return def;
    return parse_state.canon->digital_input(index, def);
}
double GET_EXTERNAL_ANALOG_INPUT(int index, double def) {
    if(!parse_state.canon) return def;
    return parse_state.canon->analog_input(index, def);
}
int WAIT(int index, int input_type, int wait_type, double timeout) {
    if(!parse_state.canon) return 0;
    return parse_state.canon->wait_input(index, input_type, wait_type, timeout);
}

static void user_defined_function(int num, double arg1, double arg2) {
    if(parse_state.interp_error) return;
    parse_state.canon->user_defined_function(num, arg1, arg2);
}

void SET_FEED_REFERENCE(CANON_FEED_REFERENCE /*ref*/) {}
int GET_EXTERNAL_QUEUE_EMPTY() { return true; }
CANON_DIRECTION GET_EXTERNAL_SPINDLE(int) { return CANON_STOPPED; }
int GET_EXTERNAL_TOOL_SLOT() { return 0; }
int GET_EXTERNAL_SELECTED_TOOL_SLOT() { return 0; }
double GET_EXTERNAL_FEED_RATE() { return 1; }
double GET_EXTERNAL_AXIS_MAX_VELOCITY(int /*axis*/) { return 0; }
double GET_EXTERNAL_SPINDLE_MAX_VELOCITY(int /*spindle*/) { return 0; }
double GET_EXTERNAL_TRAVERSE_RATE() { return 0; }
int GET_EXTERNAL_FLOOD() { return 0; }
int GET_EXTERNAL_MIST() { return 0; }
CANON_PLANE GET_EXTERNAL_PLANE() { return CANON_PLANE::XY; }
double GET_EXTERNAL_SPEED(int /*spindle*/) { return 0; }
void DISABLE_ADAPTIVE_FEED() {}
void ENABLE_ADAPTIVE_FEED() {}

int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() {return 1;}
int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE(int /*spindle*/) {return 1;}
int GET_EXTERNAL_ADAPTIVE_FEED_ENABLE() {return 0;}
int GET_EXTERNAL_FEED_HOLD_ENABLE() {return 1;}

int GET_EXTERNAL_OFFSET_APPLIED() {return 0;}

EmcPose GET_EXTERNAL_OFFSETS() { return {}; } // Aggregate value-initialisation zeroes tran.x..w.

int GET_EXTERNAL_AXIS_MASK() {
    int mask = 7;                                       /* XYZABC */
    canon_guard([&]{
        py::object result = py::handle(parse_state.callback).attr("get_axis_mask")();
        if(!PyLong_Check(result.ptr())) { parse_state.interp_error ++; return; }
        mask = (int)PyLong_AsLong(result.ptr());
    });
    return mask;
}

// The tool offset in effect, in the program's units now - as task's canon
// answers (TO_PROG_LEN of its tool offset). Not the raw numbers of the last
// USE_TOOL_LENGTH_OFFSET: those were in the units of whatever program set
// them, and the next parse's init() reads them as its own, in inches.
static double tool_length_offset(int axis) {
    return from_inch(parse_state.tool)[axis];
}
double GET_EXTERNAL_TOOL_LENGTH_XOFFSET() { return tool_length_offset(P9_X); }
double GET_EXTERNAL_TOOL_LENGTH_YOFFSET() { return tool_length_offset(P9_Y); }
double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET() { return tool_length_offset(P9_Z); }
double GET_EXTERNAL_TOOL_LENGTH_AOFFSET() { return tool_length_offset(P9_A); }
double GET_EXTERNAL_TOOL_LENGTH_BOFFSET() { return tool_length_offset(P9_B); }
double GET_EXTERNAL_TOOL_LENGTH_COFFSET() { return tool_length_offset(P9_C); }
double GET_EXTERNAL_TOOL_LENGTH_UOFFSET() { return tool_length_offset(P9_U); }
double GET_EXTERNAL_TOOL_LENGTH_VOFFSET() { return tool_length_offset(P9_V); }
double GET_EXTERNAL_TOOL_LENGTH_WOFFSET() { return tool_length_offset(P9_W); }

// The unit constants, answered by the parse's protocol: the callback protocol
// asks the canon every time, the renderer caches. Before any parse there is
// no protocol yet; answer the defaults a failed read has always answered.
double GET_EXTERNAL_ANGLE_UNITS() {
    if(!parse_state.canon) return 1.0;
    return parse_state.canon->external_angle_units();
}

double GET_EXTERNAL_LENGTH_UNITS() {
    if(!parse_state.canon) return 0.03937007874016; // 1/25.4
    return parse_state.canon->external_length_units();
}

// True to stop the parse. A failed call aborts it too, and - unlike the
// forwarders - without touching interp_error: parse_file returns on this
// answer alone, carrying whatever exception is set.
static bool check_abort() {
    try {
        py::object result = py::handle(parse_state.callback).attr("check_abort")();
        if(PyObject_IsTrue(result.ptr())) {
            PyErr_Format(PyExc_KeyboardInterrupt, "Load aborted");
            return true;
        }
    } catch(py::error_already_set &e) {
        e.restore();
        return true;
    }
    return false;
}

USER_DEFINED_FUNCTION_TYPE USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM];

CANON_MOTION_MODE motion_mode;
/* G64_R_PLANNER: preview module ignores the planner-mode args (no motion) */
void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double /*tolerance*/, int /*planner_type*/, double /*scurve_peak_scale*/) { motion_mode = mode; }
void SET_MOTION_CONTROL_MODE(double /*tolerance*/) { }
void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode) { motion_mode = mode; }
CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE() { return motion_mode; }
int GET_EXTERNAL_KINS_TYPE() { return 0; }
int GET_EXTERNAL_KINS_TYPE_FLAGS(int ktype) { (void)ktype; return -1; }
int GET_EXTERNAL_G68_FRAME(double * /*origin*/, double * /*rotation*/) { return -1; }
void SET_NAIVECAM_TOLERANCE(double /*tolerance*/) { }

#define RESULT_OK (result == INTERP_OK || result == INTERP_EXECUTE_FINISH)

// The parse. `initcodes` is the list of the new signature or null for the
// legacy one; the two overloads registered on the module below pick between
// them the way the pair of PyArg_ParseTuple attempts used to.
//
// Every failure exit throws, and the ones that must not skip the epilogue -
// anything after the interpreter is open - reach it through `out_error`.
static py::object parse_file(const char *f, py::handle canon,
                             py::handle initcodes,
                             const char *unitcode, const char *initcode,
                             const char *interpname) {
    int error_line_offset = 0;
    // How often the parse stops to report progress and let the GUI breathe.
    // A GUI's progress bar and its abort button both hang off this one tick -
    // the report moves the bar, and check_abort() is what pumps the event
    // loop the button is waiting in. It used to be one second, from when a
    // preview was built one Python call per move and a big file took minutes;
    // a rendered million-move file now parses in under a second, so the bar
    // moved exactly once, at the end. 100ms reads as continuous, and what it
    // gates is two Python calls - nothing beside a parse.
    // steady_clock, not gettimeofday: a wall clock can step backwards under
    // NTP and stall the tick for as long as the step.
    //
    // Read every 1024 lines rather than every line. The clock is ~15ns and a
    // line is ~800ns, so per-line it was about 2% of the parse for a question
    // whose answer changes once in 125000 lines. 1024 lines is under a
    // millisecond of ordinary parsing - two orders finer than the tick - and
    // on a file slow enough for that to matter (heavy O-word subs, NURBS) it
    // degrades to about the tick itself rather than past it. Which also
    // bounds how long a cancel waits, since check_abort() is on this tick.
    using clock = std::chrono::steady_clock;
    const auto tick = std::chrono::milliseconds(100);
    constexpr unsigned long clock_every = 1024;         // a power of two: one
    unsigned long lines_read = 0;                       // `and` per line
    clock::time_point last;
    int result = INTERP_OK;

    // gcode.parse has never been reentrant - a second entry deletes the
    // interpreter the first parse is still executing and redirects its canon
    // calls into the other canon. It can happen: check_abort() pumps AXIS's
    // Tk event loop, which can get back here mid-parse. Refuse at the door;
    // the per-entry ownership check this replaces could only catch the
    // renderer half of the damage.
    if(parse_state.in_parse)
        throw std::runtime_error("gcode.parse is not reentrant: "
                                 "a parse is already in flight");
    struct InParse {
        InParse() { parse_state.in_parse = true; }
        ~InParse() { parse_state.in_parse = false; }
    } in_parse_latch;

    // Borrowed for the length of the parse, as it always has been: nothing
    // holds a reference to the canon after this returns.
    parse_state.callback = canon.ptr();
    parse_state.main_file = f;

    // Protocol selection, once, before anything is interpreted: a mode that
    // could flip mid-parse would leave the canon's program half in each
    // protocol, and a per-call attribute check would cost more than the
    // renderer saves.
    // Before the renderer is made: it reads its starting state off the canon,
    // and a failed read has to be visible rather than cleared by the reset
    // below.
    parse_state.interp_error = 0;
    forget_canon_error();
    // The optional methods read off any canon, whichever protocol it is on.
    // Before the interpreter exists: its init() asks for the tool table.
    parse_state.has_get_tool = canon_defines(canon, "get_tool");
    parse_state.has_source_line = canon_defines(canon, "source_line");
    parse_state.has_get_initial_position =
            canon_defines(canon, "get_initial_position");
    parse_state.has_get_tool_in_spindle =
            canon_defines(canon, "get_tool_in_spindle");
    parse_state.has_get_tool_offset = canon_defines(canon, "get_tool_offset");
    {
        auto renderer = GCodeRenderer::make(parse_state.callback);
        if(!renderer && PyErr_Occurred()) throw py::error_already_set();
        if(renderer) parse_state.canon = std::move(renderer);
        else parse_state.canon = std::make_unique<CallbackCanon>();
    }
    parse_state.last_delivered_sequence_number = -1;
    parse_state.last_delivered_file.clear();
    parse_state.last_delivered_call_level = -1;

    if(parse_state.pinterp) {
        delete parse_state.pinterp;
        parse_state.pinterp = NULL;
    }
    if(interpname && *interpname)
        parse_state.pinterp = interp_from_shlib(interpname);
    if(!parse_state.pinterp)
        parse_state.pinterp = new Interp;

    for(int i=0; i<USER_DEFINED_FUNCTION_NUM; i++)
        USER_DEFINED_FUNCTION[i] = user_defined_function;

    last = clock::now();

    parse_state.metric = false;
    parse_state.last_sequence_number = -1;
    // the kinds the interpreter reads from the same INI; one it refuses
    // leaves the defaults here, and the interpreter says why
    {
        AxisKinds kinds = axisKindsDefault();
        const char *ini_name = getenv("INI_FILE_NAME");
        if(ini_name) {
            linuxcnc::IniFile ini(ini_name);
            std::string err;
            if(ini && axisKindsRead(ini, &kinds, &err)) kinds = axisKindsDefault();
        }
        parse_state.angular = kinds.angular;
    }

    // Every parse starts with no point yet and the frame the interpreter's
    // init() starts from: the offsets it sends again, and the tool offset it
    // reads back through GET_EXTERNAL_TOOL_LENGTH_* - the last parse's, as a
    // machine keeps its tool offset from one program to the next - without a
    // USE_TOOL_LENGTH_OFFSET to say so. `tool` is left as it is for that.
    parse_state.g5x = {};
    parse_state.g92 = {};
    parse_state.rotation_xy = 0;
    parse_state.frame = WorkFrame();
    parse_state.plane = static_cast<int>(CANON_PLANE::XY);
    parse_state.have_point = false;
    parse_state.position_dirty = false;

    parse_state.pos = {};

    machine_state_before_init();
    parse_state.pinterp->init();
    machine_state_after_init();
    parse_state.pinterp->open(f);

    maybe_new_line();

    if(initcodes) {
        Py_ssize_t ncodes = PyList_Size(initcodes.ptr());
        for(Py_ssize_t i=0; i<ncodes && RESULT_OK; i++)
        {
            PyObject *item = PyList_GetItem(initcodes.ptr(), i);
            if(!item) throw py::error_already_set();
            const char *code = PyUnicode_AsUTF8(item);
            if(!code) throw py::error_already_set();
            result = parse_state.pinterp->read(code);
            if(!RESULT_OK) goto out_error;
            flush_position();
            result = parse_state.pinterp->execute();
            flush_position();
        }
    }
    if(unitcode && RESULT_OK) {
        result = parse_state.pinterp->read(unitcode);
        if(!RESULT_OK) goto out_error;
        result = parse_state.pinterp->execute();
    }

    if(initcode && RESULT_OK) {
        result = parse_state.pinterp->read(initcode);
        if(!RESULT_OK) goto out_error;
        result = parse_state.pinterp->execute();
    }

    while(!parse_state.interp_error && RESULT_OK) {
        error_line_offset = 1;
        result = parse_state.pinterp->read();
        if((++lines_read & (clock_every - 1)) == 0) {
            clock::time_point now = clock::now();
            if(now - last >= tick) {
                // Bounds how stale a canon's progress report can get through
                // a long run of moves on one line, and keeps check_abort() -
                // which pumps AXIS's event loop - from running with a pending
                // exception raised by the report.
                parse_state.canon->progress();
                if(parse_state.interp_error) break;
                if(check_abort()) {
                    parse_state.canon->finish();
                    throw py::error_already_set();
                }
                // The reading taken before the two calls, so a slow repaint
                // does not push the next tick out by its own duration.
                last = now;
            }
        }
        if(!RESULT_OK) break;
        // read() resyncs after a tool change, probe or home: where that
        // puts the point belongs to the line just read, ahead of its moves
        flush_position();
        error_line_offset = 0;
        result = parse_state.pinterp->execute();
        flush_position();
    }
out_error:
    if(parse_state.pinterp)
    {
        auto interp = dynamic_cast<Interp*>(parse_state.pinterp);
        if(interp) interp->_setup.use_lazy_close = false;
        parse_state.pinterp->close();
    }
    if(parse_state.interp_error) {
        // Hand over what was rendered before the failure: a partial preview is
        // what a partial program has always produced.
        parse_state.canon->finish();
        // The first error a canon call raised, even if something on the way
        // here cleared it or raised another.
        if(parse_state.error_type) {
            PyErr_Restore(parse_state.error_type, parse_state.error_value,
                          parse_state.error_traceback);
            parse_state.error_type = parse_state.error_value =
                    parse_state.error_traceback = nullptr;
        }
        // A method the canon does not have: the AttributeError naming it is
        // the answer, not a RuntimeError about interp_error.
        if(PyErr_ExceptionMatches(PyExc_AttributeError))
            throw py::error_already_set();
        if(!PyErr_Occurred()) {
            PyErr_Format(PyExc_RuntimeError,
                    "interp_error > 0 but no Python exception set");
        } else {
            // seems a PyErr_Ocurred(), but no exception was set ?
            // so return error info that can be caught and handled
            PyErr_Format(PyExc_RuntimeError,"parse_file interp_error");
            fprintf(stderr,"!!!%s: parse_file() f=%s\n"
                    "!!!interp_error=%d result=%d last_sequence_number=%d\n",
                    __FILE__,f,parse_state.interp_error,result,
                    parse_state.last_sequence_number);
        }
        throw py::error_already_set();
    }
    PyErr_Clear();
    forget_canon_error();
    maybe_new_line();
    parse_state.canon->finish();
    if(PyErr_Occurred()) { parse_state.interp_error = 1; goto out_error; }
    return py::make_tuple(result,
                          parse_state.last_sequence_number + error_line_offset);
}


static int maxerror = -1;

static char savedError[LINELEN+1];
static py::str rs274_strerror(int err) {
    // The text belongs to the interpreter the last parse built; without one
    // there is nothing to ask, and dereferencing it would end the process.
    if(!parse_state.pinterp)
        throw std::runtime_error("gcode.strerror: no interpreter yet - "
                                 "call gcode.parse first");
    parse_state.pinterp->error_text(err, savedError, LINELEN);
    return py::str(savedError);
}

// One (min, max) box over every point of every move handed in, plus the box
// with each point's tool offset added. Legacy API: the renderer accumulates
// its own extents during the parse and never comes through here.
static py::tuple rs274_calc_extents(py::args args) {
    double min_x = DBL_MAX, min_y = DBL_MAX, min_z = DBL_MAX,
           min_xt = DBL_MAX, min_yt = DBL_MAX, min_zt = DBL_MAX,
           max_x = -DBL_MAX, max_y = -DBL_MAX, max_z = -DBL_MAX,
           max_xt = -DBL_MAX, max_yt = -DBL_MAX, max_zt = -DBL_MAX;
    for(py::handle group : args) {
        py::sequence segs = py::reinterpret_borrow<py::sequence>(group);
        size_t n = py::len(segs);
        double xe = 0, ye = 0, ze = 0, xt = 0, yt = 0, zt = 0;
        for(size_t j=0; j<n; j++) {
            py::sequence seg = segs[j].cast<py::sequence>();
            // The two accepted shapes - (line, start, end, tooloffset) and
            // (line, start, end, ?, tooloffset) - differ only in where the
            // offset sits, so both reduce to these three reads.
            py::sequence start = seg[1].cast<py::sequence>();
            py::sequence end = seg[2].cast<py::sequence>();
            py::sequence tool = seg[py::len(seg) - 1].cast<py::sequence>();
            double xs = start[0].cast<double>();
            double ys = start[1].cast<double>();
            double zs = start[2].cast<double>();
            xe = end[0].cast<double>();
            ye = end[1].cast<double>();
            ze = end[2].cast<double>();
            xt = tool[0].cast<double>();
            yt = tool[1].cast<double>();
            zt = tool[2].cast<double>();
            max_x = std::max(max_x, xs);
            max_y = std::max(max_y, ys);
            max_z = std::max(max_z, zs);
            min_x = std::min(min_x, xs);
            min_y = std::min(min_y, ys);
            min_z = std::min(min_z, zs);
            max_xt = std::max(max_xt, xs+xt);
            max_yt = std::max(max_yt, ys+yt);
            max_zt = std::max(max_zt, zs+zt);
            min_xt = std::min(min_xt, xs+xt);
            min_yt = std::min(min_yt, ys+yt);
            min_zt = std::min(min_zt, zs+zt);
        }
        // The last segment's endpoint: every other one is some segment's
        // start and was taken above.
        if(n > 0) {
            max_x = std::max(max_x, xe);
            max_y = std::max(max_y, ye);
            max_z = std::max(max_z, ze);
            min_x = std::min(min_x, xe);
            min_y = std::min(min_y, ye);
            min_z = std::min(min_z, ze);
            max_xt = std::max(max_xt, xe+xt);
            max_yt = std::max(max_yt, ye+yt);
            max_zt = std::max(max_zt, ze+zt);
            min_xt = std::min(min_xt, xe+xt);
            min_yt = std::min(min_yt, ye+yt);
            min_zt = std::min(min_zt, ze+zt);
        }
    }
    auto box = [](double a, double b, double c) {
        py::list l(3);
        l[0] = a; l[1] = b; l[2] = c;
        return l;
    };
    return py::make_tuple(box(min_x, min_y, min_z), box(max_x, max_y, max_z),
                          box(min_xt, min_yt, min_zt),
                          box(max_xt, max_yt, max_zt));
}

// The two exact-type attribute reads the arc entry point has always made. An
// int where a float is wanted is an error, and the message is the old one.
static int attr_int(py::handle o, const char *name) {
    py::object v = o.attr(name);
    if(!PyLong_Check(v.ptr()))
        throw py::type_error(std::string(name) + ": Expected int, got "
                             + Py_TYPE(v.ptr())->tp_name);
    return (int)PyLong_AsLong(v.ptr());
}

static double attr_double(py::handle o, const char *name) {
    py::object v = o.attr(name);
    if(!PyFloat_Check(v.ptr()))
        throw py::type_error(std::string(name) + ": Expected float, got "
                             + Py_TYPE(v.ptr())->tp_name);
    return PyFloat_AS_DOUBLE(v.ptr());
}

static py::list rs274_arc_to_segments(py::handle canon,
        double x1, double y1, double cx, double cy, int rot,
        double z1, double a, double b, double c, double u, double v, double w,
        int max_segments) {
    Point9 o, g5xoffset, g92offset;

    py::sequence lo = canon.attr("lo").cast<py::sequence>();
    if(py::len(lo) != 9)
        throw py::value_error("arc_to_segments: canon.lo is not nine numbers");
    for(size_t i=0; i<9; i++) o[i] = lo[i].cast<double>();
    int plane = attr_int(canon, "plane");
    double rotation_cos = attr_double(canon, "rotation_cos");
    double rotation_sin = attr_double(canon, "rotation_sin");
    static const char *const G5X[9] = {"g5x_offset_x", "g5x_offset_y",
        "g5x_offset_z", "g5x_offset_a", "g5x_offset_b", "g5x_offset_c",
        "g5x_offset_u", "g5x_offset_v", "g5x_offset_w"};
    static const char *const G92[9] = {"g92_offset_x", "g92_offset_y",
        "g92_offset_z", "g92_offset_a", "g92_offset_b", "g92_offset_c",
        "g92_offset_u", "g92_offset_v", "g92_offset_w"};
    for(int i=0; i<9; i++) {
        g5xoffset[i] = attr_double(canon, G5X[i]);
        g92offset[i] = attr_double(canon, G92[i]);
    }
    // The tilted work plane, from a canon that keeps one (rs274.interpret
    // .Translated does); one without the attributes has no plane.
    WorkFrame frame;
    if(py::hasattr(canon, "g68_active") && attr_int(canon, "g68_active")) {
        frame.active = true;
        py::sequence origin = canon.attr("g68_offset").cast<py::sequence>();
        py::sequence rotation = canon.attr("g68_rotation").cast<py::sequence>();
        if(py::len(origin) != 3 || py::len(rotation) != 9)
            throw py::value_error("arc_to_segments: canon.g68_offset is three "
                                  "numbers and canon.g68_rotation nine");
        for(size_t i=0; i<3; i++) frame.origin[i] = origin[i].cast<double>();
        for(size_t i=0; i<9; i++) frame.rotation[i] = rotation[i].cast<double>();
    }

    std::vector<Point9> pts;
    int steps = arc_segments(o, plane, rotation_cos, rotation_sin,
                             g5xoffset, g92offset, frame,
                             x1, y1, cx, cy, rot, z1, a, b, c, u, v, w,
                             max_segments, pts);
    py::list segs(steps);
    for(int i=0; i<steps; i++) {
        const Point9 &p = pts[(size_t)i];
        segs[i] = py::make_tuple(p[0], p[1], p[2], p[3], p[4], p[5],
                                 p[6], p[7], p[8]);
    }
    return segs;
}

// A private tool table for this process, read from a .tbl file - what
// `rs274 -t` does (sai/driver.cc): its own mmap file, created with mkstemp()
// so the name cannot be pre-placed, and unlinked at exit. Process-wide, as
// the tooldata mmap is: it replaces whatever this process had mapped,
// including a running LinuxCNC's table that linuxcnc.stat() attached to -
// that one is only unmapped, never truncated or unlinked, since
// tool_mmap_close() removes the file only for the process that created it.
static void load_tool_table(const std::string &path, bool random_toolchanger) {
    // Checked before anything is dropped, so a bad path leaves the table
    // this process had.
    FILE *fp = fopen(path.c_str(), "r");
    if(!fp) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, path.c_str());
        throw py::error_already_set();
    }
    fclose(fp);

    // Ours from an earlier call is unlinked here; one attached to a running
    // LinuxCNC is only unmapped. Before the new name is set: close unlinks
    // by the current name.
    tool_mmap_close();

    char fname[LINELEN];
    const char *tmpdir = getenv("TMPDIR");
    if(!tmpdir || !*tmpdir) tmpdir = "/tmp";
    snprintf(fname, sizeof(fname), "%s/gcode.tool.mmap.XXXXXX", tmpdir);
    int fd = mkstemp(fname);
    if(fd < 0)
        throw std::runtime_error(std::string("gcode.load_tool_table: "
                "cannot create the tool mmap file: ") + strerror(errno));
    close(fd);   // tool_mmap_creator_try() opens it by name, O_NOFOLLOW

    tool_mmap_set_fname(fname);
    char why[LINELEN] = {};
    if(tool_mmap_creator_try(nullptr, random_toolchanger, why, sizeof(why))) {
        unlink(fname);
        throw std::runtime_error(std::string("gcode.load_tool_table: ") + why);
    }
    static bool atexit_registered = false;
    if(!atexit_registered) {
        atexit(tool_mmap_close);   // unlinks the file this process created
        atexit_registered = true;
    }

    tooldata_init(random_toolchanger);
    parse_state.private_tool_table = true;
    if(tooldata_load(path.c_str()))
        throw std::runtime_error("gcode.load_tool_table: cannot load the "
                                 "tool table '" + path + "'");
}

PYBIND11_MODULE(gcode, m) {
    m.doc() = "Interface to EMC rs274ngc interpreter";

    linecode_register(m);
    preview_geometry_register(m);
    time_estimate_register(m);
    renderer_canon_register(m);

    // Registration order is the dispatch order: the list form is tried first,
    // exactly as the pair of PyArg_ParseTuple attempts did.
    m.def("parse", [](const char *f, py::handle canon, py::list initcodes,
                      const char *interpname) {
                return parse_file(f, canon, initcodes, nullptr, nullptr,
                                  interpname);
            },
            py::arg("filename"), py::arg("canon"), py::arg("initcodes"),
            py::arg("interpname") = py::none(),
            "Parse a G-Code file");
    m.def("parse", [](const char *f, py::handle canon, const char *unitcode,
                      const char *initcode, const char *interpname) {
                return parse_file(f, canon, py::handle(), unitcode, initcode,
                                  interpname);
            },
            py::arg("filename"), py::arg("canon"),
            py::arg("unitcode") = py::none(), py::arg("initcode") = py::none(),
            py::arg("interpname") = py::none(),
            "Parse a G-Code file");

    m.def("load_tool_table", &load_tool_table,
            py::arg("path"), py::arg("random_toolchanger") = false,
            "Give this process a private tool table read from a .tbl file,\n"
            "for parsing T words with no LinuxCNC running - what rs274 -t\n"
            "does. Calling it again replaces the table. Process-wide: it\n"
            "replaces the table linuxcnc.stat() attached to in this process,\n"
            "though the running LinuxCNC's own file is never touched.\n"
            "Raises OSError or RuntimeError with the reason on failure.");
    m.def("strerror", &rs274_strerror, py::arg("error"),
            "Convert a numeric error to a string");
    m.def("calc_extents", &rs274_calc_extents,
            "Calculate information about extents of gcode");
    m.def("arc_to_segments", &rs274_arc_to_segments,
            py::arg("canon"), py::arg("x1"), py::arg("y1"),
            py::arg("cx"), py::arg("cy"), py::arg("rot"), py::arg("z1"),
            py::arg("a"), py::arg("b"), py::arg("c"),
            py::arg("u"), py::arg("v"), py::arg("w"),
            py::arg("max_segments") = 128,
            "Convert an arc to straight segments");

    // What a table's samples are spaced by, so a test need not repeat it.
    m.attr("TIME_SAMPLE_RESOLUTION") = TIME_SAMPLE_RESOLUTION;
    m.attr("MAX_ERROR") = maxerror;
    m.attr("MIN_ERROR") = static_cast<int>(INTERP_MIN_ERROR);
}
// vim:ts=8:sts=4:sw=4:et:
