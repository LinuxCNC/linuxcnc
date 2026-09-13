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

#ifndef GCODEMODULE_HH
#define GCODEMODULE_HH

// The G-code module's own boundary: the protocol a parse runs on, the state of
// the parse in flight, and the guards every canon-side touch of Python goes
// through. Everything here is the module's, not the renderer's - gcode.so
// exports these symbols and librs274 resolves the canon functions out of them.

#include <pybind11/pybind11.h>

#include <array>
#include <memory>

#include <emcpos.h>

// What a Point9's nine slots are, and the order the canon functions take them
// in. Plain and unscoped so it indexes without a cast - `p[P9_Y]` - and
// prefixed so the letters do not collide with anything. `P9_COUNT` is
// what sizes Point9, so the two cannot drift apart.
enum P9Axis {
    P9_X, P9_Y, P9_Z,
    P9_A, P9_B, P9_C,
    P9_U, P9_V, P9_W,
    P9_COUNT
};

// A point in the interpreter's nine axes. A value, so the chain point and the
// offsets are copied by assignment rather than by a memcpy whose length is
// spelled at every call site - `sizeof p` was right only while `p` was an
// array in scope. Trivially copyable and 72 bytes, so the generated code is
// what memcpy gave.
using Point9 = std::array<double, P9_COUNT>;

// Whole-point arithmetic, so the transform and the arc segmenter read as the
// vector operations they are rather than as nine-iteration loops. Componentwise
// throughout, in the order the loops they replace used, so the results are the
// same bits and not merely the same value.
//
// Note what a scalar means on each side: `p * 2.0` scales every component,
// while `p + 2.0` *adds two to all nine* - two inches of X and two degrees of
// A. That is rarely what a caller means, and nothing here uses it; it is
// defined only because the set is easier to remember whole than in part.
inline Point9 &operator+=(Point9 &a, const Point9 &b) {
    for(int i = 0; i < P9_COUNT; i++) a[i] += b[i];
    return a;
}
inline Point9 &operator-=(Point9 &a, const Point9 &b) {
    for(int i = 0; i < P9_COUNT; i++) a[i] -= b[i];
    return a;
}
inline Point9 &operator+=(Point9 &a, double s) {
    for(int i = 0; i < P9_COUNT; i++) a[i] += s;
    return a;
}
inline Point9 &operator-=(Point9 &a, double s) {
    for(int i = 0; i < P9_COUNT; i++) a[i] -= s;
    return a;
}
inline Point9 &operator*=(Point9 &a, double s) {
    for(int i = 0; i < P9_COUNT; i++) a[i] *= s;
    return a;
}
inline Point9 &operator/=(Point9 &a, double s) {
    for(int i = 0; i < P9_COUNT; i++) a[i] /= s;
    return a;
}

inline Point9 operator+(Point9 a, const Point9 &b) { return a += b; }
inline Point9 operator-(Point9 a, const Point9 &b) { return a -= b; }
inline Point9 operator+(Point9 a, double s) { return a += s; }
inline Point9 operator-(Point9 a, double s) { return a -= s; }
inline Point9 operator*(Point9 a, double s) { return a *= s; }
inline Point9 operator/(Point9 a, double s) { return a /= s; }
inline Point9 operator*(double s, Point9 a) { return a *= s; }

// ---------------------------------------------------------------------------
// The canon protocol
// ---------------------------------------------------------------------------
//
// One virtual method per canon event whose handling depends on which protocol
// the parse runs. The free canon functions librs274 resolves out of gcode.so
// do only what both protocols share - unit conversion, the _pos_/tool-offset
// bookkeeping - and dispatch the rest through `parse_state.canon`:
//
//   * CallbackCanon (gcodemodule.cc) forwards each event to the Python canon,
//     one call per event, byte-for-byte the sequence this module has always
//     produced. The default, and what canons that are not previews
//     (rs274.interpret's PrintCanon, the interpreter tests, out-of-tree users
//     of gcode.parse) are built on.
//   * GCodeRenderer (gcode_renderer.hh) renders moves into a finished program
//     during the parse and hands it over once, at the end.
//
// Methods with a body here are events only one implementation listens to; the
// other inherits the no-op.
class Canon {
public:
    virtual ~Canon() = default;

    // The moves.
    virtual void arc_feed(int line_number,
                          double first_end, double second_end,
                          double first_axis, double second_axis, int rotation,
                          double axis_end_point, double a, double b, double c,
                          double u, double v, double w) = 0;
    // The endpoint as one 9-DOF point, already in inches. arc_feed keeps its
    // loose arguments: its first six are arc geometry in the active plane,
    // not axes, so there is no point to pack.
    virtual void straight_feed(int line_number, const Point9 &p) = 0;
    virtual void straight_traverse(int line_number, const Point9 &p) = 0;
    virtual void straight_probe(int line_number, const Point9 &p) = 0;
    virtual void rigid_tap(int line_number,
                           double x, double y, double z) = 0;

    // The events between them.
    virtual void dwell(double seconds) = 0;
    virtual void user_defined_function(int num, double arg1, double arg2) = 0;
    virtual void change_tool(int tool) = 0;
    // The nine offsets, already in inches; the raw EmcPose stays on
    // `parse_state.tool_offset` for the GET_EXTERNAL_TOOL_LENGTH_* getters.
    virtual void tool_offset(const Point9 &offsets) = 0;

    // The transform and the modes.
    virtual void set_g5x_offset(int index, const Point9 &offsets) = 0;
    virtual void set_g92_offset(const Point9 &offsets) = 0;
    virtual void set_xy_rotation(double degrees) = 0;
    virtual void set_plane(int plane) = 0;
    virtual void set_feed_rate(double rate) = 0;
    virtual void set_traverse_rate(double rate) = 0;
    // Only the renderer listens: SET_SPINDLE_SPEED has never forwarded a
    // callback, so the callback protocol has nothing to do here.
    virtual void set_spindle_speed(double /*rpm*/) {}
    // A comment, *after* the canon has had it - COMMENT forwards first for
    // both protocols and dispatches here only when the forward succeeded, so
    // `(AXIS,stop)` still stops the parse before the word after it could open
    // a hidden span. Only the renderer listens, for its own `(AXIS,hide)`
    // depth.
    virtual void comment(const char * /*text*/) {}

    // The two per-parse unit constants. The callback protocol asks the canon
    // every time, as it always has - a canon may legitimately watch the
    // traffic; the renderer asks once and caches, because arc_segments() asks
    // once per arc *rendered* for a number that cannot move.
    virtual double external_length_units() = 0;
    virtual double external_angle_units() = 0;

    // Parse-loop hooks: the renderer's progress report and its end-of-parse
    // handover. Nothing to do on the callback protocol, where every event
    // already reached the canon as it happened.
    virtual void progress() {}
    virtual void finish() {}
};

// ---------------------------------------------------------------------------
// The state of the parse in flight
// ---------------------------------------------------------------------------
//
// One per process, like the parse itself: gcode.parse is not reentrant -
// parse_file refuses a second entry, because it would delete the interpreter
// the first parse is executing and redirect its canon calls - and everything
// here is written by parse_file and by the canon functions librs274 resolves
// out of gcode.so. Defined in gcodemodule.cc.
class InterpBase;
struct ParseState {
    // The Python canon of the parse in flight. Borrowed for its length, as it
    // always has been: nothing holds a reference after the parse returns.
    // gcode.arc_to_segments reads it too, which is why it is kept after the
    // parse rather than cleared.
    PyObject *callback = nullptr;
    // The interpreter of the last parse; gcode.strerror needs it afterwards.
    InterpBase *pinterp = nullptr;
    // The protocol of the parse: CallbackCanon unless the canon opted into
    // the renderer. Replaced at the start of every parse and kept after it,
    // like the rest of this state.
    std::unique_ptr<Canon> canon;
    // Failures crossing the Python boundary: bumped with the Python error
    // left set, checked by the parse loop. See canon_guard below.
    int interp_error = 0;
    // The source line the parse has reached; parse_file returns it and error
    // reporting reads it. A rendered move advances it without a next_line
    // having been delivered, hence the separate delivery guard below: a
    // still-forwarded callback later on the same line must not be mistaken
    // for a repeat. The two move in lockstep on the callback protocol, where
    // nothing but delivery touches either.
    int last_sequence_number = -1;
    int last_delivered_sequence_number = -1;
    int selected_tool = 0;
    bool metric = false;
    // The reentry latch parse_file refuses on.
    bool in_parse = false;
    // Where the program is, machine units, raw - what the probe/position
    // getters answer with.
    Point9 pos = {};
    EmcPose tool_offset = {};

    // The line number for a canon event that is not given one.
    int current_line() const;

    // Leak the protocol at process exit rather than destroy it: this is a
    // static, so its destructor runs during Python finalization, and a
    // renderer holds py::objects whose decref would touch the interpreter
    // after the GIL is gone. Mid-run replacement in parse_file still
    // destroys the old one normally, with the GIL held.
    ~ParseState() { (void)canon.release(); }
};

extern ParseState parse_state;

// Run one canon-side step that touches Python. A failure becomes
// `interp_error++` with the Python error left set, which is the protocol
// above; nothing may leave as a C++ exception, because these run from inside
// Interp::execute() and would unwind past state it owns. Every use of
// pybind11 on the canon side goes through this or through forward().
template <typename F>
static inline void canon_guard(F &&body) {
    if(parse_state.interp_error) return;
    try {
        body();
    } catch(pybind11::error_already_set &e) {
        e.restore();                    // the error stays set, as it was
        parse_state.interp_error ++;
    } catch(pybind11::builtin_exception &e) {
        e.set_error();                  // py::type_error and friends, by kind
        parse_state.interp_error ++;
    } catch(const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        parse_state.interp_error ++;
    }
}

// The common case: call a canon method and discard what it returned.
template <typename... A>
static inline void forward(const char *method, A &&...args) {
    canon_guard([&]{
        pybind11::handle(parse_state.callback).attr(method)(std::forward<A>(args)...);
    });
}

// The exact-type unit reads off the Python canon, shared by both protocols'
// external_*_units. Defined in gcodemodule.cc.
double read_external_length_units();
double read_external_angle_units();

#endif  // GCODEMODULE_HH
