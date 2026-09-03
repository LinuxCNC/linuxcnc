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
//
// The G-code renderer: the whole preview pipeline, run during the parse. Only
// the one entry point a canon function reaches per move stays inline in
// gcode_renderer.hh, where the call sites can see it.

#include "gcode_renderer.hh"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nml_intf/canon.hh"
#include "posemath.h"

namespace py = pybind11;

void GCodeRenderer::set_xy_rotation(double degrees) {
    if(parse_state.interp_error) return;
    rotation_xy_ = degrees;
    // `M_PI / 180.0` folded first, which is what `math.radians` multiplies by:
    // the fill this replaced took its sin and cos from the canon, so keeping
    // the argument bit-identical keeps the baked expectations so too.
    double rad = degrees * (M_PI / 180.0);
    rotation_cos_ = cos(rad);
    rotation_sin_ = sin(rad);
    // The angle back, as it has always been computed here.
    double back = -degrees * M_PI / 180.0;
    unrot_cos_ = cos(back);
    unrot_sin_ = sin(back);
}

void GCodeRenderer::arc_feed(int line_number, double first_end, double second_end,
                             double first_axis, double second_axis, int rotation,
                             double axis_end_point, double a, double b, double c,
                             double u, double v, double w) {
    if(parse_state.interp_error) return;
    parse_state.last_sequence_number = line_number;
    render_arc(line_number, first_end, second_end, first_axis,
               second_axis, rotation, axis_end_point, a, b, c,
               u, v, w, rate_);
}

// The unit constants, cached on first ask: arc_segments() wants the length
// units once per arc for its small-arc test, which here is once per arc
// *rendered* - thousands of Python calls for a number that cannot move.
double GCodeRenderer::external_length_units() {
    if(length_units_known_) return length_units_;
    double d = read_external_length_units();
    if(!parse_state.interp_error) {
        length_units_ = d;
        length_units_known_ = true;
    }
    return d;
}

double GCodeRenderer::external_angle_units() {
    if(angle_units_known_) return angle_units_;
    double d = read_external_angle_units();
    if(!parse_state.interp_error) {
        angle_units_ = d;
        angle_units_known_ = true;
    }
    return d;
}

// ---------------------------------------------------------------------------
// One parse's renderer
// ---------------------------------------------------------------------------

static const char AXES[P9_COUNT] = {'x','y','z','a','b','c','u','v','w'};

// ---------------------------------------------------------------------------
// PreviewData: storage, growth, and the Python object it is handed over in
// ---------------------------------------------------------------------------

PreviewData::~PreviewData() {
    for(int i = 0; i < MAX_PLANES; i++) free(pos[i]);
    free(attrs);
}

bool PreviewData::reserve(size_t extra) {
    size_t need = n + extra;
    if(need <= cap) return true;
    size_t want = cap * 2;
    if(want < 1024) want = 1024;
    while(want < need) want *= 2;
    float *grown[MAX_PLANES] = {};
    for(int i = 0; i < nplanes; i++) {
        grown[i] = (float*)realloc(pos[i], want * P3_COUNT * sizeof(float));
        if(!grown[i]) {
            // Whatever did grow is still valid and still holds the program;
            // only `cap` decides what may be written, so leaving it alone is
            // what makes this safe to fail.
            for(int j = 0; j < i; j++) pos[j] = grown[j];
            return false;
        }
    }
    uint32_t *grown_attrs =
        (uint32_t*)realloc(attrs, want * ATTRS_PER_VERTEX * sizeof(uint32_t));
    if(!grown_attrs) {
        for(int i = 0; i < nplanes; i++) pos[i] = grown[i];
        return false;
    }
    for(int i = 0; i < nplanes; i++) pos[i] = grown[i];
    attrs = grown_attrs;
    cap = want;
    return true;
}

// Give back the doubling slack once the program is complete - up to half the
// array, and this is the copy every reader keeps.
void PreviewData::shrink() {
    if(cap == n) return;
    size_t want = n ? n : 1;
    for(int i = 0; i < nplanes; i++) {
        float *fit = (float*)realloc(pos[i], want * P3_COUNT * sizeof(float));
        if(fit) pos[i] = fit;           // a refused shrink keeps the slack
    }
    uint32_t *fit = (uint32_t*)realloc(attrs, want * ATTRS_PER_VERTEX * sizeof(uint32_t));
    if(fit) attrs = fit;
    cap = want;
}

// A read-only buffer over part of a PreviewGeometry, keeping it alive. What
// numpy wraps, so the program reaches Python without a copy.
struct __attribute__((visibility("hidden"))) ArrayView {
    py::object owner;                   // the PreviewGeometry the memory is in
    void *ptr;
    Py_ssize_t nitems, itemsize;
    const char *format;
};

static py::object array_view(py::object owner, void *ptr, Py_ssize_t nitems,
                             Py_ssize_t itemsize, const char *format) {
    return py::cast(ArrayView{std::move(owner), ptr, nitems, itemsize, format});
}

static py::tuple triple(const Point3 &v) {
    return py::make_tuple(v[P3_X], v[P3_Y], v[P3_Z]);
}

static py::tuple points_tuple(const PlanePoints &pts, int nplanes) {
    py::tuple out(nplanes);
    for(int i = 0; i < nplanes; i++) out[i] = triple(pts[i]);
    return out;
}

py::object preview_geometry_new(PreviewData *data) {
    // The holder takes the program; nothing else may free it after this.
    return py::cast(std::unique_ptr<PreviewData>(data));
}

void preview_geometry_register(py::module_ &m) {
    py::class_<ArrayView>(m, "arrayview", py::buffer_protocol(),
            "Read-only view of a PreviewGeometry array")
        .def_buffer([](ArrayView &v) {
            return py::buffer_info(v.ptr, v.itemsize, v.format, 1,
                                   {v.nitems}, {v.itemsize}, /*readonly=*/true);
        });

    py::class_<PreviewData>(m, "PreviewGeometry",
            "A parsed program: vertex arrays, extents, lengths and records")
        // `self` rather than `PreviewData&`: the view must hold the object
        // that owns the memory, not just a reference into it.
        .def("positions", [](py::object self, int plane) {
                PreviewData &d = py::cast<PreviewData&>(self);
                if(plane < 0 || plane >= d.nplanes)
                    throw py::index_error("no such drawn plane");
                return array_view(std::move(self), d.pos[plane],
                                  (Py_ssize_t)d.n * P3_COUNT, sizeof(float), "f");
            }, py::arg("plane") = 0,
            "Read-only float32 xyz view of one drawn plane")
        .def("attrs", [](py::object self) {
                PreviewData &d = py::cast<PreviewData&>(self);
                return array_view(std::move(self), d.attrs,
                                  (Py_ssize_t)d.n * ATTRS_PER_VERTEX,
                                  sizeof(uint32_t), "I");
            }, "Read-only uint32 (line, kind|tool) view")
        .def("extents", [](const PreviewData &d) {
                py::tuple out(EXTENT_KINDS);
                for(int i = 0; i < EXTENT_KINDS; i++)
                    out[i] = py::make_tuple(triple(d.extents[i][BOX_MIN]),
                                            triple(d.extents[i][BOX_MAX]));
                return out;
            }, "The four machine-frame (min, max) pairs")
        .def("drawn_extents", [](const PreviewData &d) {
                return py::make_tuple(triple(d.drawn[BOX_MIN]),
                                      triple(d.drawn[BOX_MAX]));
            }, "(min, max) over the transformed points in the array")
        .def("cut_lengths", [](const PreviewData &d) {
                py::dict out;
                for(const auto &entry : d.cut_length)
                    out[py::float_(entry.first)] = entry.second;
                return out;
            }, "{commanded rate: cutting length at it}")
        .def("tool_numbers", [](const PreviewData &d) {
                size_t n = d.tool_numbers.size();
                py::list out(n);
                // Ordinal 0 is the state before any tool change: not stated,
                // not T0.
                out[0] = py::none();
                for(size_t i = 1; i < n; i++) out[i] = d.tool_numbers[i];
                return out;
            }, "Ordinal -> T number, entry 0 None")
        .def("dwells", [](const PreviewData &d) {
                py::list out;
                for(const DwellRecord &r : d.dwells)
                    out.append(py::make_tuple(r.lineno, r.plane,
                            py::bool_(r.m1xx), triple(r.raw),
                            points_tuple(r.pts, d.nplanes)));
                return out;
            }, "(lineno, plane, is_m1xx, raw xyz, points per plane) per dwell")
        .def("toolchanges", [](const PreviewData &d) {
                py::list out;
                for(const ToolChangeRecord &r : d.toolchanges)
                    out.append(py::make_tuple(r.lineno, r.tool,
                            points_tuple(r.pts, d.nplanes)));
                return out;
            }, "(lineno, tool number, points per plane) per tool change")
        .def_property_readonly("n_vertices", [](const PreviewData &d) { return d.n; })
        .def_property_readonly("n_moves", [](const PreviewData &d) { return d.moves; })
        .def_property_readonly("n_planes", [](const PreviewData &d) { return d.nplanes; })
        .def_property_readonly("rapid_length", [](const PreviewData &d) { return d.rapid_length; })
        .def_property_readonly("dwell_time", [](const PreviewData &d) { return d.dwell_time; });
}

std::vector<GeomOp> GeomOp::compile(const std::string &geom, long axis_mask,
                                    double rox, double roy, double roz) {
    std::vector<GeomOp> out;
    double sign = 1.0;
    for(const char *ch = geom.c_str(); *ch; ch++) {
        GeomOp op = {};
        op.sign = sign;
        switch(*ch) {
        case '-': sign = -1.0; continue;
        case 'X': op.col = P9_X; op.a = P3_X; break;
        case 'Y': op.col = P9_Y; op.a = P3_Y; break;
        case 'Z': op.col = P9_Z; op.a = P3_Z; break;
        case 'U': op.col = P9_U; op.a = P3_X; break;
        case 'V': op.col = P9_V; op.a = P3_Y; break;
        case 'W': op.col = P9_W; op.a = P3_Z; break;
        case 'A': case 'B': case 'C': {
            // A rotary letter turns a component pair - but only when the
            // config asked for it, which is what the mask says. AXIS_MASK
            // bits are one per 9-DOF axis, in P9 order.
            int bit = *ch == 'A' ? 1 << P9_A
                    : *ch == 'B' ? 1 << P9_B : 1 << P9_C;
            sign = 1.0;
            if(!(axis_mask & bit)) continue;
            op.rotate = true;
            if(*ch == 'A') {
                op.col = P9_A; op.a = P3_Y; op.b = P3_Z;
                op.offa = roy; op.offb = roz;
            } else if(*ch == 'B') {
                op.col = P9_B; op.a = P3_X; op.b = P3_Z;
                op.offa = rox; op.offb = roz;
            } else {
                op.col = P9_C; op.a = P3_X; op.b = P3_Y;
                op.offa = rox; op.offb = roy;
            }
            out.push_back(op);
            continue;
        }
        default: continue;      // '!', ';' and friends, sign preserved
        }
        sign = 1.0;
        out.push_back(op);
    }
    return out;
}

// Read the GEOMETRY strings and the rotation offsets off the canon's
// ProgramGeometry - which is where the widget put them just before the
// parse - and hand each string to GeomOp::compile.
bool GCodeRenderer::read_planes() {
    // Every read below raises rather than reporting; one catch turns the lot
    // into the false-with-an-exception-set that the caller expects.
    try {
        py::object pg = canon_.attr("program_geometry");
        py::object planes = pg.attr("planes");
        py::object ro = pg.attr("ro");

        long mask = ro.attr("axis_mask").cast<long>();
        // The pivot is baked into each rotary op at compile, so `respect` is
        // read here and nowhere else: it cannot change once a parse has
        // started.
        bool respect = PyObject_IsTrue(ro.attr("respect_offsets").ptr());
        double rox = 0.0, roy = 0.0, roz = 0.0;
        if(respect) {
            rox = ro.attr("x").cast<double>();
            roy = ro.attr("y").cast<double>();
            roz = ro.attr("z").cast<double>();
        }

        py::sequence names = planes.cast<py::sequence>();
        size_t n = py::len(names);
        if(n < 1 || n > MAX_PLANES) {
            PyErr_SetString(PyExc_ValueError,
                    "parse: the renderer draws one or two planes");
            throw py::error_already_set();
        }
        data_->nplanes = (int)n;
        for(size_t plane = 0; plane < n; plane++)
            data_->ops[plane] = GeomOp::compile(names[plane].cast<std::string>(),
                                            mask, rox, roy, roz);
    } catch(py::error_already_set &e) {
        e.restore();
        return false;
    }
    return true;
}

// `gcode.RendererCanon`, borrowed from the module that owns it.
static PyTypeObject *renderer_canon_type;

void renderer_canon_register(py::module_ &m) {
    // Plain Python class, not a py::class_: a pybind11 base would oblige
    // every subclass __init__ to call it.
    py::dict ns;
    ns["__module__"] = "gcode";
    ns["__doc__"] =
            "Base class of a canon that wants the finished program.\n\n"
            "Subclass it and define adopt_geometry(program); gcode.parse then\n"
            "builds the whole preview in C++ and hands it over once, instead\n"
            "of calling the per-move canon methods.\n\n"
            "A subclass must also carry a program_geometry, whose `planes` are\n"
            "the GEOMETRY strings to draw and whose `ro` holds the rotation\n"
            "offsets. `arcdivision` is the only other thing a parse reads, and\n"
            "it defaults below.";
    ns["arcdivision"] = py::int_(64);
    py::object cls = py::reinterpret_borrow<py::object>((PyObject *)&PyType_Type)(
            "RendererCanon", py::tuple(), ns);
    m.attr("RendererCanon") = cls;
    renderer_canon_type = (PyTypeObject *)cls.ptr();     // the module owns it
}

std::unique_ptr<GCodeRenderer> GCodeRenderer::make(PyObject *canon_ptr) {
    py::handle canon(canon_ptr);

    // Subclassing is the opt-in; a catch-all `__getattr__` cannot fake it.
    if(!renderer_canon_type || !PyObject_TypeCheck(canon_ptr, renderer_canon_type))
        return nullptr;                 // callback protocol, no complaint

    // Fail fast: a canon that asked for a preview and got callbacks instead
    // would build nothing and look like it worked.
    bool usable = false;
    try {
        usable = PyCallable_Check(canon.attr("adopt_geometry").ptr());
    } catch(py::error_already_set &) {  // absent, or a descriptor that raises
    }
    if(!usable) {
        PyErr_Clear();
        PyErr_SetString(PyExc_TypeError,
                "parse: a gcode.RendererCanon must define a callable "
                "adopt_geometry");
        return nullptr;
    }

    std::unique_ptr<GCodeRenderer> r(new GCodeRenderer(canon));
    r->data_ = new PreviewData();
    for(int i = 0; i < EXTENT_KINDS; i++)
        for(int j = 0; j < P3_COUNT; j++) {
            r->data_->extents[i][BOX_MIN][j] = 9e99;
            r->data_->extents[i][BOX_MAX][j] = -9e99;
        }
    for(int j = 0; j < P3_COUNT; j++) {
        r->data_->drawn[BOX_MIN][j] = 9e99;
        r->data_->drawn[BOX_MAX][j] = -9e99;
    }
    r->data_->tool_numbers.push_back(0);         // ordinal 0 is None
    if(!r->read_planes()) return nullptr;
    // getattr-with-default swallows whatever the read raised, which is what a
    // canon that simply has no progress hook needs.
    r->progress_ = py::getattr(canon, "renderer_progress", py::none());
    if(!PyCallable_Check(r->progress_.ptr())) r->progress_ = py::object();

    // No starting state is read off the canon: a parse begins at zero with
    // nothing drawn yet. Where the machine stands reaches the preview as the
    // caller's initcode - `G53 G0` to the current position, which the
    // first_move drop swallows - and the interpreter re-issues the offsets
    // and the rotation from the parameter file during init().
    py::object div = py::getattr(canon, "arcdivision", py::none());
    if(!div.is_none()) {
        long n = PyLong_AsLong(div.ptr());
        if(n > 0) r->arcdivision_ = (int)n;
    }
    PyErr_Clear();                      // a canon without one keeps the default
    return r;
}

GCodeRenderer::~GCodeRenderer() {
    delete data_;
}

// `(AXIS,hide)` / `(AXIS,show)`: the two words of the comment vocabulary the
// fill depends on, counted as a depth so nested spans close in order. The rest
// of the vocabulary - `stop`, `notify`, the foam Z levels - is the canon's own
// and reached it through the forward that precedes this call.
void GCodeRenderer::comment(const char *text) {
    const char *rest;
    if(!strncmp(text, "AXIS,", 5)) rest = text + 5;
    else if(!strncmp(text, "PREVIEW,", 8)) rest = text + 8;
    else return;
    // The word up to the next comma, as `arg.split(",")[1]` took it.
    size_t n = strcspn(rest, ",");
    if(n != 4) return;
    if(!strncmp(rest, "hide", 4)) suppress_ ++;
    else if(!strncmp(rest, "show", 4)) suppress_ --;
}

void GCodeRenderer::publish_line() {
    canon_guard([&]{ canon_.attr("lineno") = last_line_; });
}

void GCodeRenderer::transform(const Point9 &in, Point9 &out) const {
    out = in + g92_;
    if(rotation_xy_ != 0.0) {
        double rotx = out[P9_X] * rotation_cos_ - out[P9_Y] * rotation_sin_;
        out[P9_Y] = out[P9_X] * rotation_sin_ + out[P9_Y] * rotation_cos_;
        out[P9_X] = rotx;
    }
    out += g5x_;
}

// The GEOMETRY-string transform (the C vertex9), for one point through one
// compiled plane.
static void plane_point(const std::vector<GeomOp> &ops,
                        const Point9 &pts9, Point3 &out) {
    out = {};
    for(const GeomOp &op : ops) op.apply(pts9, out);
}

void GCodeRenderer::write_vertex(const Point9 &pts9, int line_number,
                                unsigned char kind, PlanePoints *points) {
    if(!data_->reserve(1)) {
        if(!parse_state.interp_error) PyErr_NoMemory();
        parse_state.interp_error ++;
        return;
    }
    size_t at = data_->n;
    for(int i = 0; i < data_->nplanes; i++) {
        Point3 p;
        plane_point(data_->ops[i], pts9, p);
        for(int j = 0; j < P3_COUNT; j++) {
            if(p[j] < data_->drawn[BOX_MIN][j]) data_->drawn[BOX_MIN][j] = p[j];
            if(p[j] > data_->drawn[BOX_MAX][j]) data_->drawn[BOX_MAX][j] = p[j];
            data_->pos[i][at * P3_COUNT + j] = (float)p[j];
            if(points) (*points)[i][j] = p[j];
        }
    }
    data_->attrs[at * ATTRS_PER_VERTEX] = (uint32_t)line_number;
    data_->attrs[at * ATTRS_PER_VERTEX + 1] =
            (uint32_t)kind | (data_->tool << 8);
    data_->n = at + 1;
}

void GCodeRenderer::mark(int line_number, const Point9 &at,
                        unsigned char kind, PlanePoints *points) {
    write_vertex(at, line_number, kind, points);
}

// The four machine-frame extent pairs, from one move's raw endpoints.
void GCodeRenderer::accumulate_extents(const Point9 &p1, const Point9 &p2) {
    Box3 box;
    for(int j = 0; j < P3_COUNT; j++) {
        box[BOX_MIN][j] = std::min(p1[j], p2[j]);
        box[BOX_MAX][j] = std::max(p1[j], p2[j]);
    }
    // The tool-corrected box is the raw box shifted: adding a constant is
    // monotonic, so this is the same box, not an approximation of it.
    Point3 shift = {tool_[P9_X], tool_[P9_Y], tool_[P9_Z]};
    Box3 rot;
    if(rotation_xy_ != 0.0) {
        Point3 u1, u2;
        unrotate_xy(p1, u1);
        unrotate_xy(p2, u2);
        for(int j = 0; j < P3_COUNT; j++) {
            rot[BOX_MIN][j] = std::min(u1[j], u2[j]);
            rot[BOX_MAX][j] = std::max(u1[j], u2[j]);
        }
    }
    // With no rotation to remove, the zero_rxy kinds are the plain ones.
    bool rotated = rotation_xy_ != 0.0;
    for(int i = 0; i < EXTENT_KINDS; i++) {
        bool unrotated = (i >= EXT_ZERO_RXY) && rotated;
        const Point3 &lo = unrotated ? rot[BOX_MIN] : box[BOX_MIN];
        const Point3 &hi = unrotated ? rot[BOX_MAX] : box[BOX_MAX];
        bool notool = (i == EXT_NOTOOL || i == EXT_NOTOOL_ZERO_RXY);
        for(int j = 0; j < P3_COUNT; j++) {
            double a = notool ? lo[j] + shift[j] : lo[j];
            double b = notool ? hi[j] + shift[j] : hi[j];
            if(a < data_->extents[i][BOX_MIN][j])
                data_->extents[i][BOX_MIN][j] = a;
            if(b > data_->extents[i][BOX_MAX][j])
                data_->extents[i][BOX_MAX][j] = b;
        }
    }
}

// The g5x XY rotation taken back out about the g5x origin, for the
// rotation-removed extents. Z is left alone.
void GCodeRenderer::unrotate_xy(const Point9 &p, Point3 &out) const {
    double tx = p[P9_X] - g5x_[P9_X];
    double ty = p[P9_Y] - g5x_[P9_Y];
    out[P3_X] = tx * unrot_cos_ - ty * unrot_sin_ + g5x_[P9_X];
    out[P3_Y] = tx * unrot_sin_ + ty * unrot_cos_ + g5x_[P9_Y];
    out[P3_Z] = p[P9_Z];
}

void GCodeRenderer::fill(int line_number, const Point9 &p1, const Point9 &p2,
                        double feedrate, unsigned char cat) {
    data_->moves ++;
    accumulate_extents(p1, p2);

    double dx = p2[P9_X] - p1[P9_X], dy = p2[P9_Y] - p1[P9_Y],
           dz = p2[P9_Z] - p1[P9_Z];
    double len = sqrt(dx * dx + dy * dy + dz * dz);
    if(cat == CAT_TRAVERSE) data_->rapid_length += len;
    else data_->cut_length[feedrate] += len;

    // A move that does not start where the last one ended gets a record vertex
    // at its start; the shaders discard the segment into it.
    bool jump = !data_->has_cur || p1 != data_->cur9;
    // Rotary subdivision: a move that turns A, B or C is drawn as a polyline,
    // since the tool's path through the machine's frame is not a straight line.
    long steps = 1;
    bool turning = false;
    double dc = 0.0;
    for(int i = P9_A; i <= P9_C; i++) {
        if(p1[i] != p2[i]) turning = true;
        double d = fabs(p2[i] - p1[i]);
        if(d > dc) dc = d;
    }
    if(turning) {
        double want = dc / 10.0;
        steps = (long)ceil(want > 10.0 ? want : 10.0);
    }
    long count = steps + (jump ? 1 : 0);
    for(long i = 0; i < count; i++) {
        long sub = i - (jump ? 1 : 0) + 1;
        Point9 pt;
        if(steps == 1 && !jump) {
            pt = p2;
        } else {
            double t = (double)sub / (double)steps;
            for(int k = 0; k < P9_COUNT; k++)
                pt[k] = t * p2[k] + (1.0 - t) * p1[k];
        }
        write_vertex(pt, line_number, sub == 0 ? KIND_NOOP : cat, nullptr);
    }
    data_->cur9 = p2;
    data_->has_cur = true;
}

void GCodeRenderer::move(Kind kind, int line_number, const Point9 &in,
                         double rate) {
    last_line_ = line_number;
    consumed_ = true;
    if(kind >= Dwell) { event(kind, line_number, in); return; }
    // A hidden move touches nothing at all, not even the chain point.
    if(suppress_ > 0) return;

    Point9 p;
    transform(in, p);
    if(kind == RigidTap) {
        // Down and back up the way it came, joined to the chain point's
        // rotary and UVW components, and the chain point does not move.
        Point9 end = lo_;
        end[P9_X] = p[P9_X]; end[P9_Y] = p[P9_Y]; end[P9_Z] = p[P9_Z];
        first_move_ = false;
        fill(line_number, lo_, end, rate / 60., CAT_FEED);
        fill(line_number, end, lo_, rate / 60., CAT_FEED);
        return;
    }
    if(first_move_) {
        // What swallows the `G53 G0 <current position>` AXIS and gmoccapy's
        // gremlin send as initcode: it repositions the tool, it is not a line.
        if(kind == Traverse) { lo_ = p; return; }
        first_move_ = false;
    }
    if(kind == Traverse) fill(line_number, lo_, p, 0.0, CAT_TRAVERSE);
    else fill(line_number, lo_, p, rate / 60., CAT_FEED);
    lo_ = p;
}

// CANON_PLANE to the 0/1/2 code a dwell record carries: XY/UV -> 0,
// XZ/UW -> 1, YZ/VW -> 2.
static int plane_code(int plane) {
    switch(static_cast<CANON_PLANE>(plane)) {
    case CANON_PLANE::YZ: case CANON_PLANE::VW: return 2;
    case CANON_PLANE::XZ: case CANON_PLANE::UW: return 1;
    default: return 0;
    }
}

void GCodeRenderer::event(Kind kind, int line_number,
                         const Point9 &axes) {
    switch(kind) {
    case ToolOffset:
        // Not forwarded: it moved only the chain point and the offset triple,
        // and both live here now.
        first_move_ = true;
        lo_ = lo_ - axes + tool_;
        tool_ = axes;
        return;
    case Dwell:
    case M1xx: {
        // Both are markers at the current position; a hidden one is dropped,
        // as the canon methods they replace drop it.
        if(suppress_ > 0) return;
        if(kind == Dwell) data_->dwell_time += axes[P9_X];
        DwellRecord rec = {};
        rec.lineno = line_number;
        rec.plane = plane_code(plane_);
        rec.m1xx = (kind == M1xx);
        rec.raw = {lo_[P9_X], lo_[P9_Y], lo_[P9_Z]};
        mark(line_number, lo_, KIND_DWELL, &rec.pts);
        data_->dwells.push_back(rec);
        return;
    }
    case ChangeTool: {
        int tool = (int)axes[P9_X];
        // The record vertex carries the *new* ordinal: it marks where the new
        // tool's work begins. 65535 changes in one program reuse the last
        // ordinal rather than wrap onto another tool's entry.
        if(data_->tool_numbers.size() > 0xFFFF) {
            data_->tool = 0xFFFF;
        } else {
            data_->tool = (uint32_t)data_->tool_numbers.size();
            data_->tool_numbers.push_back(tool);
        }
        ToolChangeRecord rec = {};
        rec.lineno = line_number;
        // The T number as commanded, not the ordinal's entry: past 65535
        // changes the ordinal stops advancing and would hand the record the
        // previous tool's number.
        rec.tool = tool;
        mark(line_number, lo_, KIND_TOOLCHANGE, &rec.pts);
        data_->toolchanges.push_back(rec);
        first_move_ = true;

        publish_line();
        canon_guard([&]{ canon_.attr("change_tool")(tool); });

        return;
    }
    default:
        PyErr_Format(PyExc_RuntimeError,
                "gcode renderer: unknown event kind %d", (int)kind);
        parse_state.interp_error ++;
    }
}

void GCodeRenderer::report_progress() {
    if(parse_state.interp_error) return;
    if(!consumed_) return;
    consumed_ = false;
    if(!progress_) return;
    canon_guard([&]{ progress_(last_line_); });
}

void GCodeRenderer::hand_over() {
    if(handed_over_) return;
    handed_over_ = true;
    // The parse may be ending *because* something raised - an abort, a syntax
    // error, a canon callback. Put that aside for the handover and put it
    // back: what was rendered before the failure is still a preview.
    PyObject *type, *value, *tb;
    PyErr_Fetch(&type, &value, &tb);
    int errors = parse_state.interp_error;
    parse_state.interp_error = 0;
    data_->shrink();
    PreviewData *program = data_;
    data_ = nullptr;                    // the holder owns it from here, even
                                        // if the cast below throws
    try {
        canon_.attr("adopt_geometry")(preview_geometry_new(program));
    } catch(py::error_already_set &e) {
        e.restore();                    // read back off the indicator below
    } catch(const std::exception &e) {
        // A failed cast or a failed allocation: the geometry is gone, but the
        // lines below still have to find *some* error on the indicator, and
        // must not be skipped by a C++ exception leaving parse_file.
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
    // A failure here loses the geometry, but never the reason the parse ended:
    // the first exception wins, and the parse stays failed either way.
    if(!type && PyErr_Occurred()) PyErr_Fetch(&type, &value, &tb);
    PyErr_Clear();
    if(type) {
        PyErr_Restore(type, value, tb);
        if(!errors) errors = 1;
    }
    parse_state.interp_error = errors;
    // A bound method, so it holds the canon and through it the program. The
    // renderer outlives the parse - it sits in parse_state until the next one
    // replaces it - so hold nothing past the handover.
    progress_ = py::object();
}

void GCodeRenderer::render_arc(int line_number, double first_end, double second_end,
                       double first_axis, double second_axis, int rotation,
                       double axis_end_point, double a, double b, double c,
                       double u, double v, double w, double rate) {
    last_line_ = line_number;
    consumed_ = true;
    if(suppress_ > 0) return;
    arc_segments(lo_, plane_, rotation_cos_, rotation_sin_,
                 g5x_, g92_, first_end, second_end,
                 first_axis, second_axis, rotation,
                 axis_end_point, a, b, c, u, v, w,
                 arcdivision_, segs_);
    // The segments arrive transformed, so no transform here - and an arc is
    // drawn whether or not it is the program's first move, as the per-move
    // canon draws it.
    first_move_ = false;
    for(const Point9 &p : segs_) {
        fill(line_number, lo_, p, rate / 60., CAT_ARC);
        lo_ = p;
    }
}

// ---------------------------------------------------------------------------
// Arc segmentation
// ---------------------------------------------------------------------------
//
// Shared by gcode.arc_to_segments (the canon-driven Python entry point) and
// the renderer, which segments arcs itself rather than asking Python to.

static void unrotate(double &x, double &y, double c, double s) {
    double tx = x * c + y * s;
    y = -x * s + y * c;
    x = tx;
}

static void rotate(double &x, double &y, double c, double s) {
    double tx = x * c - y * s;
    y = x * s + y * c;
    x = tx;
}

int arc_segments(const Point9 &lo, int plane,
                 double rotation_cos, double rotation_sin,
                 const Point9 &g5xoffset, const Point9 &g92offset,
                 double x1, double y1, double cx, double cy, int rot,
                 double z1, double a, double b, double c,
                 double u, double v, double w,
                 int max_segments, std::vector<Point9> &out) {
    Point9 o = lo, n;
    P9Axis X, Y, Z;

    if(plane == static_cast<int>(CANON_PLANE::XY)) {
        X=P9_X; Y=P9_Y; Z=P9_Z;
    } else if(plane == static_cast<int>(CANON_PLANE::XZ)) {
        X=P9_Z; Y=P9_X; Z=P9_Y;
    } else {
        X=P9_Y; Y=P9_Z; Z=P9_X;
    }
    n[X] = x1;
    n[Y] = y1;
    n[Z] = z1;
    n[P9_A] = a;
    n[P9_B] = b;
    n[P9_C] = c;
    n[P9_U] = u;
    n[P9_V] = v;
    n[P9_W] = w;
    o -= g5xoffset;
    unrotate(o[P9_X], o[P9_Y], rotation_cos, rotation_sin);
    o -= g92offset;

    double theta1 = atan2(o[Y]-cy, o[X]-cx);
    double theta2 = atan2(n[Y]-cy, n[X]-cx);
    /* Issue #1528 1/2/22 andypugh */
    /*_posemath checks for small arcs too, but uses config units */
    double len = hypot(o[X]-n[X], o[Y]-n[Y]) * (25.4 * GET_EXTERNAL_LENGTH_UNITS());
    /* If the signs of the angles differ, make them the same to allow monotonic progress through the arc */
    /* If start and end points are nearly identical, then interpret as a full turn */
    if(rot < 0) { // CW G2
        if (theta1 < theta2) theta2 -= 2*M_PI;
        if (len < CART_FUZZ) theta2 -= 2*M_PI;
    } else { // CCW G3
        if (theta1 > theta2) theta2 += 2*M_PI;
        if (len < CART_FUZZ) theta2 += 2*M_PI;
    }

    // if multi-turn, add the right number of full circles
    if(rot < -1) theta2 += 2*M_PI*(rot+1);
    if(rot > 1) theta2 += 2*M_PI*(rot-1);

    int steps = std::max(3, int(max_segments * fabs(theta1 - theta2) / M_PI));
    double rsteps = 1. / steps;
    out.resize((size_t)steps);

    double dtheta = theta2 - theta1;
    // Only d[Z] and d[P9_A..P9_W] are read below; the plane pair d[X], d[Y]
    // is dead, as those coordinates come from rotating around the centre.
    Point9 d = n - o;

    double tx = o[X] - cx, ty = o[Y] - cy, dc = cos(dtheta*rsteps), ds = sin(dtheta*rsteps);
    for(int i=0; i<steps-1; i++) {
        double f = (i+1) * rsteps;
        Point9 &p = out[(size_t)i];
        rotate(tx, ty, dc, ds);
        p[X] = tx + cx;
        p[Y] = ty + cy;
        p[Z] = o[Z] + d[Z] * f;
        for(int j = P9_A; j < P9_COUNT; j++) p[j] = o[j] + d[j] * f;
        p += g92offset;
        rotate(p[P9_X], p[P9_Y], rotation_cos, rotation_sin);
        p += g5xoffset;
    }
    n += g92offset;
    rotate(n[P9_X], n[P9_Y], rotation_cos, rotation_sin);
    n += g5xoffset;
    out[(size_t)steps - 1] = n;
    return steps;
}

