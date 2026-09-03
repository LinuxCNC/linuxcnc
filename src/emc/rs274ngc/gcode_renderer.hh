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

#ifndef GCODE_RENDERER_HH
#define GCODE_RENDERER_HH

#include <pybind11/pybind11.h>

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

// Point9, the Canon protocol and the state of the parse in flight: the
// renderer is one implementation of that protocol, not its owner.
#include "gcodemodule.hh"

// ---------------------------------------------------------------------------
// The program the renderer builds: the C++ side of rs274.glcanon_bake's
// ProgramGeometry, filled during the parse and handed to Python at the end of
// it. Layouts match that module's PLANE_DTYPE and ATTR_DTYPE exactly, so the
// arrays reach numpy as views rather than copies.
// ---------------------------------------------------------------------------

// A program is drawn once per plane: one plane for a mill, two when a lathe
// draws its mirrored back side beside the front.
constexpr int MAX_PLANES = 2;
// The two bounds of a box around drawn points, and - as with P3_COUNT and
// P9_COUNT - the count that sizes one.
enum { BOX_MIN, BOX_MAX, BOX_BOUNDS };
// The machine-frame extent boxes a program keeps, in the order
// `calc_extents` has always given them: as commanded, with the tool offset
// added back, and each of those with the g5x XY rotation taken out.
enum { EXT_RAW, EXT_NOTOOL, EXT_ZERO_RXY, EXT_NOTOOL_ZERO_RXY };
constexpr int EXTENT_KINDS = EXT_NOTOOL_ZERO_RXY + 1;
// Per vertex, beside its xyz: the line number, then kind|tool.
constexpr int ATTRS_PER_VERTEX = 2;

// What a Point3's three slots are, as P9_X..P9_W are a Point9's, and what
// sizes it. A drawn point is xyz and nothing else: the rotary and UVW axes
// have already been folded in by the GEOMETRY transform.
enum P3Axis { P3_X, P3_Y, P3_Z, P3_COUNT };

// Point9's drawn counterpart: one xyz, the same point once per drawn plane,
// and a (min, max) box over such points.
using Point3 = std::array<double, P3_COUNT>;
using PlanePoints = std::array<Point3, MAX_PLANES>;
using Box3 = std::array<Point3, BOX_BOUNDS>;

// One step of a GEOMETRY string, compiled once per parse. Translate adds an
// axis of the 9-DOF point to an output component; rotate turns a component
// pair by a rotary letter's value (the C vertex9's rotate_x/y/z).
struct GeomOp {
    bool rotate;
    P9Axis col;                         // 9-DOF column the step reads
    P3Axis a, b;                        // output components (b: rotate only)
    double sign;
    // The pivot a rotation turns about, baked at compile time: the work
    // origin (g5x + g92) under GEOMETRY's `!`, and zero without it, so the
    // hot loop subtracts unconditionally rather than branching per point.
    double offa, offb;

    // Fold this step into `out`, one point through one plane. Inline and
    // non-virtual on purpose: this runs once per op per vertex per plane.
    //
    // The rotation is a transcription of rotate_x/rotate_y/rotate_z in
    // emc/usr_intf/axis/extensions/emcmodule.cc, quirk included: those
    // subtract the pivot before the sin/cos and never add it back, so a
    // `!` GEOMETRY leaves the drawn point in pivot-relative coordinates
    // rather than turning it about the pivot in place (which would be
    // translate(-o), rotate, translate(+o)). Whether that is what anyone
    // wanted, it is what the preview has always drawn; reproduce it, do not
    // fix it here. A fix belongs in both transforms at once, with the
    // reference in tests/gcode-renderer/line9_reference.py moved with them.
    void apply(const Point9 &pts9, Point3 &out) const {
        if(!rotate) {
            out[a] += pts9[col] * sign;
            return;
        }
        double theta = pts9[col] * sign * (M_PI / 180.0);
        double c = cos(theta), s = sin(theta);
        double pa = out[a] - offa, pb = out[b] - offb;
        out[a] = pa * c - pb * s;
        out[b] = pa * s + pb * c;
    }

    // Compile one GEOMETRY string into the steps apply() walks. Pure: the
    // caller reads the axis mask and the rotation pivot off the canon and
    // decides where the result lives. A rotary letter is dropped when its
    // AXIS_MASK bit is off; unknown letters ('!', ';') are skipped with the
    // pending '-' sign preserved, as the Python compiler always has.
    static std::vector<GeomOp> compile(const std::string &geom, long axis_mask,
                                       double rox, double roy, double roz);
};

struct DwellRecord {
    int lineno;
    int plane;                          // 0/1/2, as GLCanon._record_dwell
    bool m1xx;                          // which colour Python attaches
    Point3 raw;                         // machine coords, for canon.dwells
    PlanePoints pts;                    // transformed, per drawn plane
};

struct ToolChangeRecord {
    int lineno;
    int tool;                           // as commanded, not the ordinal's entry
    PlanePoints pts;
};

struct PreviewData {
    ~PreviewData();
    // False when the arrays could not grow: the caller must stop writing, as
    // the old buffers are still their old size.
    bool reserve(size_t extra);
    void shrink();

    int nplanes = 1;
    std::vector<GeomOp> ops[MAX_PLANES];    // one compiled transform per plane
    float *pos[MAX_PLANES] = {};        // P3_COUNT floats per vertex, per plane
    uint32_t *attrs = nullptr;          // line, kind|tool per vertex
    size_t n = 0, cap = 0;

    std::array<Box3, EXTENT_KINDS> extents = {};
    Box3 drawn = {};
    // Summed a move at a time, so a running total drifts with move count -
    // about 4e-12 relative over a million moves, nanometres on a metre of tool
    // path. The baked expectations allow for it; no reader of a path length
    // can see it.
    double rapid_length = 0.0;
    std::map<double, double> cut_length; // commanded rate -> cutting length
    size_t moves = 0;

    std::vector<int> tool_numbers;      // entry 0 is the None before any change
    uint32_t tool = 0;
    double dwell_time = 0.0;
    std::vector<DwellRecord> dwells;
    std::vector<ToolChangeRecord> toolchanges;

    Point9 cur9 = {};                   // where the trajectory is
    bool has_cur = false;
};

// The finished program as `gcode.PreviewGeometry`; takes ownership of `data`.
pybind11::object preview_geometry_new(PreviewData *data);
// Register PreviewGeometry and its array views on the module.
void preview_geometry_register(pybind11::module_ &m);

// Defines `gcode.RendererCanon`, the empty base a canon opts in by subclassing.
void renderer_canon_register(pybind11::module_ &m);

// One arc as up to `max_segments`-ish 9-DOF points, transformed the way a move
// is. Shared by gcode.arc_to_segments and the renderer.
int arc_segments(const Point9 &lo, int plane,
                 double rotation_cos, double rotation_sin,
                 const Point9 &g5xoffset, const Point9 &g92offset,
                 double x1, double y1, double cx, double cy, int rot,
                 double z1, double a, double b, double c,
                 double u, double v, double w,
                 int max_segments, std::vector<Point9> &out);


// ---------------------------------------------------------------------------
// The G-code renderer
// ---------------------------------------------------------------------------
//
// An *opt-in* alternative to the per-event callback protocol, for a canon that
// wants a finished preview rather than a million Python calls. A canon opts in
// by subclassing `gcode.RendererCanon` and defining a callable
// `adopt_geometry`, checked once before any interpretation - a mode that
// flipped mid-parse would leave the program half in each protocol. A subclass
// without the method is a TypeError, not a quiet fall back.
//
// In renderer mode the canon methods listed on `Canon` above do not call
// Python at all. Instead the renderer runs the whole preview pipeline - the
// g92 -> XY rotation -> g5x transform, the chain point, arc segmentation, the
// two segments a rigid tap draws, the leading traverses `first_move` drops,
// suppression, rotary subdivision, the GEOMETRY-string transform per drawn
// plane, the extents, the path lengths, and the dwell and tool-change
// records - into a `PreviewData`, and hands the finished program to
// `adopt_geometry` once, at the end of the parse, as a `gcode.PreviewGeometry`.
//
// What still crosses back from Python, and why:
//
//   * `comment` is still forwarded, because the rest of the `(AXIS,...)`
//     vocabulary - `stop`, `notify`, the foam Z levels - is the canon's. The
//     suppression depth those comments used to carry back is read here
//     instead, out of the same text, after the canon has had it.
//   * `message` is still forwarded: an `(MSG,...)` is addressed to the
//     operator, not to the preview.
//   * `change_tool` is still forwarded - not for the record, which is written
//     here, but because the interpreter reads the canon's tool table for a G43
//     after it, and a GUI's override is what moves the simulated spindle slot.
//     The canon keeps no list of its own: `GLCanon.adopt_geometry` rebuilds
//     `tool_list` out of the records at the end of the parse.
//   * `next_line`, before each of the above, because `GLCanon.comment` reads
//     `self.state.gcodes` for the foam Z levels and gremlin and qtvcp read it
//     again after the parse for the program's units. It is delivered on the
//     lines that still forward, which is a handful per parse rather than one
//     per line.
//
// Everything else is dropped. The g5x/g92 offsets, the XY rotation, the plane
// and the feed rate used to be forwarded as pure observations; nothing in the
// tree reads the canon's copy of any of them on a rendered parse (the DROs
// read the *status channel*, and the lengths and the transform are the
// record's), and the feed rate alone was thousands of calls a file, because
// the interpreter reports an F word whether or not it changed anything. A
// canon that wants them can read the finished program instead.
// `tool_offset` is likewise not forwarded: it moved only geometry state the
// renderer now owns.
//
// Ordering falls out: a move is rendered where it happens, under exactly the
// offsets, rotation, plane and suppression in force at that point, because
// nothing is held back, and every one of those is captured here as the call
// that changes it goes past. Nothing flows Python to C mid-parse.
//
// A parse therefore starts from a zero transform rather than from whatever the
// canon was holding: the interpreter re-issues the offsets and the rotation
// out of the parameter file during `init()`, which happens after the renderer
// is made, so it receives them as canon calls like any others.
//
// Progress is reported through the canon's optional `renderer_progress`: a
// rendered move delivers no `next_line`, so that is what a GUI's progress bar
// counts instead. It fires before each still-forwarded callback and on
// parse_file's 100ms tick, which is what actually paces it - a rendered parse
// forwards so little that the tick is usually the only source. A report that
// had nothing to report since the last one is dropped, so an idle stretch
// costs nothing.
//
// Lifetime: the program's arrays are owned by the `gcode.PreviewGeometry` the
// handover creates and are never handed out before the parse ends, so no reader
// can hold a view over a buffer that is still growing.
//
// Ownership: `parse_state.canon` owns the renderer, one per parse, made in
// parse_file before anything is interpreted and replaced by the next parse's
// protocol. It can never be armed for a canon other than the one being parsed
// into: parse_file refuses reentry outright, which is what used to take a
// per-entry owner check.

// Hidden visibility, as pybind11 asks of anything holding a py::object: its
// own types are hidden, and a default-visibility class with one as a member
// is an ODR hazard across shared objects (and a -Wattributes warning here).
class __attribute__((visibility("hidden"))) GCodeRenderer final : public Canon {
public:
    // Read the opt-in off `canon` and, if it is set, make a renderer ready
    // for it. Returns the renderer when the canon opted in; nullptr with no
    // Python error set when it did not (callback protocol); nullptr with an
    // error set when the opt-in is unusable.
    static std::unique_ptr<GCodeRenderer> make(PyObject *canon);

    ~GCodeRenderer();
    GCodeRenderer(const GCodeRenderer &) = delete;
    GCodeRenderer &operator=(const GCodeRenderer &) = delete;

    // -- the canon protocol, rendered --------------------------------------

    void arc_feed(int line_number, double first_end, double second_end,
                  double first_axis, double second_axis, int rotation,
                  double axis_end_point, double a, double b, double c,
                  double u, double v, double w) override;
    void straight_feed(int line_number, const Point9 &p) override {
        append(Feed, line_number, p);
    }
    void straight_traverse(int line_number, const Point9 &p) override {
        append(Traverse, line_number, p);
    }
    void straight_probe(int line_number, const Point9 &p) override {
        append(Probe, line_number, p);
    }
    // a..w zero, exactly the arguments the `rigid_tap` callback does not
    // have; the renderer joins x,y,z to the chain point's a..w.
    void rigid_tap(int line_number, double x, double y, double z) override {
        append(RigidTap, line_number, {x, y, z});
    }
    // Rendered like any other event, and not a progress report of its own: a
    // G81/G82 cycle emits one dwell per hole.
    void dwell(double seconds) override {
        append(Dwell, parse_state.current_line(), {seconds});
    }
    void user_defined_function(int num, double arg1, double arg2) override {
        append(M1xx, parse_state.current_line(), {(double)num, arg1, arg2});
    }
    void change_tool(int tool) override {
        append(ChangeTool, parse_state.current_line(), {(double)tool});
    }
    void tool_offset(const Point9 &o) override {
        append(ToolOffset, parse_state.current_line(), o);
    }

    // The transform, from the three canon calls that carry it. Each arrives
    // in inches, already converted, at the moment its callback fires - so
    // there is nothing to read back off the canon and nothing that can go
    // stale between the call and the move it applies to. The renderer
    // transforms with its own copy and never reads the canon's.
    void set_g5x_offset(int /*index*/, const Point9 &offsets) override {
        if(parse_state.interp_error) return;
        g5x_ = offsets;
    }
    void set_g92_offset(const Point9 &offsets) override {
        if(parse_state.interp_error) return;
        g92_ = offsets;
    }
    void set_xy_rotation(double degrees) override;
    // The plane reaches the record and the arc segmenter from here; nothing
    // on a rendered parse reads the canon's own copy.
    void set_plane(int plane) override { plane_ = plane; }
    // Record the rate the following moves are made at. One store: the rate
    // reaches the program in every move's own length table, so there is
    // nothing left for the canon to be told - and telling it was not cheap:
    // the interpreter reports an F word whether or not it changed anything
    // (interp_execute.cc branches on `block->f_flag` alone), so CAM output
    // with adaptive feed lands here once per move.
    void set_feed_rate(double rate) override { rate_ = rate; }
    // A traverse carries no rate into the record - rapid_length is a length,
    // not a time - so a rendered parse has nothing to say about this.
    void set_traverse_rate(double /*rate*/) override {}
    // One store. Nothing reads it yet: the program record has no per-move
    // spindle speed, and the G95 (units per revolution) feed mode is what
    // will need one.
    void set_spindle_speed(double rpm) override { speed_ = rpm; }
    // A comment, after the canon has had it: the `(AXIS,hide)`/`(AXIS,show)`
    // depth is the renderer's own.
    void comment(const char *text) override;

    double external_length_units() override;
    double external_angle_units() override;

    // Report progress for everything rendered since the last call.
    void progress() override { report_progress(); }
    // End of parse: hand the program over and give the canon back the state
    // the renderer took over, so a reader of canon.lo/first_move/xo..wo sees
    // what it always saw.
    void finish() override { hand_over(); }

private:
    explicit GCodeRenderer(pybind11::handle canon) : canon_(canon) {}

    // What a canon method reports into the pipeline. Kinds 0-3 are moves;
    // 4-7 are the events between them, carrying their payload in the axis
    // arguments:
    //
    //     dwell (4)        seconds in x
    //     m1xx  (5)        function index, P, Q in x, y, z
    //     change_tool (6)  tool number in x
    //     tool_offset (7)  the nine offsets in x..w
    enum Kind : int {
        Traverse = 0,
        Feed = 1,
        Probe = 2,
        RigidTap = 3,
        Dwell = 4,
        M1xx = 5,
        ChangeTool = 6,
        ToolOffset = 7,
    };

    // Vertex kinds, matching rs274.glcanon_bake. The three drawn ones are also
    // the categories a move carries; the rest are records the shaders discard.
    static constexpr unsigned char CAT_TRAVERSE = 0;
    static constexpr unsigned char CAT_FEED = 1;
    static constexpr unsigned char CAT_ARC = 2;
    static constexpr unsigned char KIND_NOOP = 3;
    static constexpr unsigned char KIND_DWELL = 4;
    static constexpr unsigned char KIND_TOOLCHANGE = 5;

    // Every rendered move and event funnels through here. No next_line is
    // delivered for a rendered move, but the error line the parse reports
    // must still advance with it.
    // The trailing axes an event does not carry are zero: a Point9 written
    // short zero-fills, which is what every caller below wants.
    void append(Kind kind, int line_number, const Point9 &p) {
        if(parse_state.interp_error) return;
        parse_state.last_sequence_number = line_number;
        move(kind, line_number, p, rate_);
    }

    // -- one parse's pipeline and its program ------------------------------
    void move(Kind kind, int line_number, const Point9 &in, double rate);
    void render_arc(int line_number, double first_end, double second_end,
                    double first_axis, double second_axis, int rotation,
                    double axis_end_point, double a, double b, double c,
                    double u, double v, double w, double rate);
    void report_progress();
    void hand_over();
    void sync_out(bool with_line);
    void sync_in();
    // One move into the geometry: extents, length, then its vertices.
    void fill(int line_number, const Point9 &p1, const Point9 &p2,
              double feedrate, unsigned char cat);
    // One record vertex at `at`, writing its per-plane position to `points`.
    void mark(int line_number, const Point9 &at, unsigned char kind,
              PlanePoints *points);
    void write_vertex(const Point9 &pts9, int line_number, unsigned char kind,
                      PlanePoints *points);
    void accumulate_extents(const Point9 &p1, const Point9 &p2);
    bool read_planes();
    void unrotate_xy(const Point9 &p, Point3 &out) const;
    // g92 -> XY rotation -> g5x, the operations and the order
    // `rs274.interpret.Translated.rotate_and_translate` applies - which is
    // where this came from, though that method no longer runs on a rendered
    // parse. Not bit-identical to it by construction: the compiler is free to
    // contract the rotation's multiply-add, so the tests allow a few ULPs.
    void transform(const Point9 &in, Point9 &out) const;
    void event(Kind kind, int line_number, const Point9 &axes);

    pybind11::handle canon_;            // borrowed, as parse_state.callback is
    pybind11::object progress_;         // canon.renderer_progress, or empty
    PreviewData *data_ = nullptr;       // the program being built, owned
    bool handed_over_ = false;

    // The transform, zero until the interpreter's startup re-issues it.
    Point9 g92_ = {};
    Point9 g5x_ = {};
    double rotation_xy_ = 0.0;
    double rotation_cos_ = 1.0;
    double rotation_sin_ = 0.0;
    double unrot_cos_ = 1.0;            // the same rotation, negated, for the
    double unrot_sin_ = 0.0;            // rotation-removed extents

    Point9 lo_ = {};                    // chain point
    Point9 tool_ = {};                  // xo..wo
    bool first_move_ = true;
    // The `(AXIS,hide)` depth, counted here from the comments themselves.
    // A parse starts at zero: a canon that set `suppress` before the parse
    // was never a supported idiom, and there is no attribute to read now.
    long suppress_ = 0;
    int plane_ = 1;                     // CANON_PLANE, for arc segmentation
    int arcdivision_ = 64;              // the canon's, read once at make time
    std::vector<Point9> segs_;          // reused by render_arc()

    double rate_ = 60.0;                // the commanded feed, inches
    double speed_ = 0.0;                // spindle 0's rpm

    // The unit constants, read off the canon on first ask.
    double length_units_ = 0.0;
    bool length_units_known_ = false;
    double angle_units_ = 0.0;
    bool angle_units_known_ = false;

    // Progress is reported once per delivery that consumed anything, moves or
    // not: a hidden stretch still costs parse time.
    int last_line_ = -1;
    bool consumed_ = false;
};

#endif  // GCODE_RENDERER_HH
