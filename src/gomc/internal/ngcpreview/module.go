// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package ngcpreview provides server-side G-code preview interpretation.
// Each preview request creates a fresh Interp instance with a recording
// canon that captures geometry as structured segments.
package ngcpreview

/*
#cgo CFLAGS: -I${SRCDIR}/../../../emc/rs274ngc -I${SRCDIR}/../../../emc/nml_intf -I${SRCDIR}/../../../emc/motion -I${SRCDIR}/../../../emc/task -I${SRCDIR}/../../../rtapi -I${SRCDIR}/../../../../include -I${SRCDIR}/../../generated/gmi/canon -I${SRCDIR}/../../.. -I${SRCDIR}/../../generated/gmi/interp_ext -I${SRCDIR}/../../generated/gmi/interp_ctx -I${SRCDIR}/../../pkg/cmodule
#cgo LDFLAGS: -L${SRCDIR}/../../../../lib -Wl,--allow-shlib-undefined -lrs274 -lposemath -lstdc++ -lm

#include <stdlib.h>
#include <string.h>
#include "emctool.h"
#include "interp_shim.h"
#include "canon_api.h"

// Forward declarations for canon callback implementations (defined below).
// These are the recording canon — they capture geometry into a C struct.

// Preview state passed as ctx to all canon callbacks.
typedef struct {
    // Segment storage (dynamically grown)
    int seg_count;
    int seg_cap;
    struct preview_segment *segments;

    // Dwell storage
    int dwell_count;
    int dwell_cap;
    struct preview_dwell *dwells;

    // Tool change storage
    int tc_count;
    int tc_cap;
    struct preview_tool_change *tool_changes;

    // Current position in display units (inches) for segment starts
    double pos[9];

    // Current position in program units (for get_external_position_*)
    double prog_pos[9];

    // Current tool offset
    double tool_offset[9];

    // Current feedrate (in inches/min for display)
    double feedrate;

    // XY rotation angle (radians)
    double xy_rotation;

    // Active plane (1=XY, 2=YZ, 3=XZ)
    int plane;

    // Current line number
    int line_no;

    // Parameter file name (stored by set_parameter_file_name)
    char param_file[1024];

    // Metric flag: true when G21 active (program units = mm)
    int metric;

    // Active G5x offset (set by SET_G5X_OFFSET canon call)
    int g5x_index;
    double g5x_offset[9];

    // Active G92 offset (set by SET_G92_OFFSET canon call)
    double g92_offset[9];

    // Machine linear units (from [TRAJ]LINEAR_UNITS): 1.0 for mm, 1/25.4 for inch
    double linear_units;

    // Tool table cache (indexed by pocket)
    int tool_count; // number of populated entries
    struct {
        int32_t toolno;
        int32_t pocketno;
        double offset[9];
        double diameter;
        double frontangle;
        double backangle;
        int32_t orientation;
    } tools[CANON_POCKETS_MAX];
} preview_ctx_t;

typedef struct preview_segment {
    int type;       // 1=traverse, 2=feed, 3=arc, 4=probe
    int line_no;
    double start[9];
    double end[9];
    double feedrate;
    double tool_offset[9];
    // Arc-specific (only for type==3)
    double center_x;
    double center_y;
    int rotation;
    int plane;      // active plane at time of arc (1=XY, 2=YZ, 3=XZ)
} preview_segment_t;

typedef struct preview_dwell {
    int line_no;
    double pos[9];
    double seconds;
    int plane;
} preview_dwell_t;

typedef struct preview_tool_change {
    int line_no;
    int tool_no;
} preview_tool_change_t;

static void ctx_ensure_seg_cap(preview_ctx_t *ctx) {
    if (ctx->seg_count >= ctx->seg_cap) {
        int newcap = ctx->seg_cap == 0 ? 1024 : ctx->seg_cap * 2;
        ctx->segments = (preview_segment_t*)realloc(ctx->segments,
            newcap * sizeof(preview_segment_t));
        ctx->seg_cap = newcap;
    }
}

static void ctx_ensure_dwell_cap(preview_ctx_t *ctx) {
    if (ctx->dwell_count >= ctx->dwell_cap) {
        int newcap = ctx->dwell_cap == 0 ? 64 : ctx->dwell_cap * 2;
        ctx->dwells = (preview_dwell_t*)realloc(ctx->dwells,
            newcap * sizeof(preview_dwell_t));
        ctx->dwell_cap = newcap;
    }
}

static void ctx_ensure_tc_cap(preview_ctx_t *ctx) {
    if (ctx->tc_count >= ctx->tc_cap) {
        int newcap = ctx->tc_cap == 0 ? 16 : ctx->tc_cap * 2;
        ctx->tool_changes = (preview_tool_change_t*)realloc(ctx->tool_changes,
            newcap * sizeof(preview_tool_change_t));
        ctx->tc_cap = newcap;
    }
}

static void add_segment(preview_ctx_t *ctx, int type,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    // Store program-unit position for get_external_position_* (interpreter feedback)
    ctx->prog_pos[0] = x; ctx->prog_pos[1] = y; ctx->prog_pos[2] = z;
    ctx->prog_pos[3] = a; ctx->prog_pos[4] = b; ctx->prog_pos[5] = c;
    ctx->prog_pos[6] = u; ctx->prog_pos[7] = v; ctx->prog_pos[8] = w;
    // AXIS GL display uses inches internally (see to_internal_units in bin/axis).
    // Convert from program units to inches:
    //   G21 (metric=1): positions in mm → divide by 25.4 → inches
    //   G20 (metric=0): positions already in inches → no conversion
    if (ctx->metric) {
        x /= 25.4; y /= 25.4; z /= 25.4;
        u /= 25.4; v /= 25.4; w /= 25.4;
    }
    ctx_ensure_seg_cap(ctx);
    preview_segment_t *s = &ctx->segments[ctx->seg_count++];
    memset(s, 0, sizeof(*s));
    s->type = type;
    s->line_no = ctx->line_no;
    memcpy(s->start, ctx->pos, sizeof(s->start));
    s->end[0] = x; s->end[1] = y; s->end[2] = z;
    s->end[3] = a; s->end[4] = b; s->end[5] = c;
    s->end[6] = u; s->end[7] = v; s->end[8] = w;
    s->feedrate = ctx->feedrate;
    memcpy(s->tool_offset, ctx->tool_offset, sizeof(s->tool_offset));
    // Update current position (in inches — AXIS internal unit)
    ctx->pos[0] = x; ctx->pos[1] = y; ctx->pos[2] = z;
    ctx->pos[3] = a; ctx->pos[4] = b; ctx->pos[5] = c;
    ctx->pos[6] = u; ctx->pos[7] = v; ctx->pos[8] = w;
}

// --- Canon callback implementations ---

static void pc_init_canon(void *ctx) { (void)ctx; }

static void pc_straight_traverse(void *vctx, int32_t ln,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->line_no = ln;
    add_segment(ctx, 1, x, y, z, a, b, c, u, v, w);
}

static void pc_straight_feed(void *vctx, int32_t ln,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->line_no = ln;
    add_segment(ctx, 2, x, y, z, a, b, c, u, v, w);
}

static void pc_arc_feed(void *vctx, int32_t ln,
    double first_end, double second_end,
    double first_axis, double second_axis,
    int32_t rotation, double axis_end_point,
    double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->line_no = ln;
    // Map in-plane coordinates back to XYZ based on active plane.
    // Plane 1 (XY): first=X, second=Y, axis=Z
    // Plane 2 (YZ): first=Y, second=Z, axis=X
    // Plane 3 (XZ): first=Z, second=X, axis=Y
    double x, y, z;
    switch (ctx->plane) {
    case 2: // YZ
        x = axis_end_point; y = first_end; z = second_end;
        break;
    case 3: // XZ
        x = second_end; y = axis_end_point; z = first_end;
        break;
    default: // XY (plane 1)
        x = first_end; y = second_end; z = axis_end_point;
        break;
    }
    add_segment(ctx, 3, x, y, z, a, b, c, u, v, w);
    // Store in-plane arc center and rotation on the last-added segment.
    // center_x = first_axis (in-plane), center_y = second_axis (in-plane).
    preview_segment_t *s = &ctx->segments[ctx->seg_count - 1];
    s->center_x = ctx->metric ? first_axis / 25.4 : first_axis;
    s->center_y = ctx->metric ? second_axis / 25.4 : second_axis;
    s->rotation = rotation;
    s->plane = ctx->plane;
}

static void pc_straight_probe(void *vctx, int32_t ln,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w, uint8_t ptype) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->line_no = ln;
    (void)ptype;
    add_segment(ctx, 4, x, y, z, a, b, c, u, v, w);
}

static void pc_set_feed_rate(void *vctx, double rate) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // Feed rate comes in program units/min. Convert to inches/min for display.
    if (ctx->metric) { rate /= 25.4; }
    ctx->feedrate = rate;
}

static void pc_use_length_units(void *vctx, int32_t units) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // CANON_UNITS_MM = 2
    ctx->metric = (units == 2);
}

static void pc_dwell(void *vctx, double seconds) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx_ensure_dwell_cap(ctx);
    preview_dwell_t *d = &ctx->dwells[ctx->dwell_count++];
    d->line_no = ctx->line_no;
    memcpy(d->pos, ctx->pos, sizeof(d->pos));
    d->seconds = seconds;
    d->plane = 0;
}

static void pc_change_tool(void *vctx, int32_t slot) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx_ensure_tc_cap(ctx);
    preview_tool_change_t *tc = &ctx->tool_changes[ctx->tc_count++];
    tc->line_no = ctx->line_no;
    tc->tool_no = slot;
}

static void pc_use_tool_length_offset(void *vctx,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // Convert linear axes to inches for display (angular axes stay as-is)
    if (ctx->metric) {
        x /= 25.4; y /= 25.4; z /= 25.4;
        u /= 25.4; v /= 25.4; w /= 25.4;
    }
    ctx->tool_offset[0] = x; ctx->tool_offset[1] = y; ctx->tool_offset[2] = z;
    ctx->tool_offset[3] = a; ctx->tool_offset[4] = b; ctx->tool_offset[5] = c;
    ctx->tool_offset[6] = u; ctx->tool_offset[7] = v; ctx->tool_offset[8] = w;
}

static void pc_update_end_point(void *vctx,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // Store program-unit position for interpreter feedback
    ctx->prog_pos[0] = x; ctx->prog_pos[1] = y; ctx->prog_pos[2] = z;
    ctx->prog_pos[3] = a; ctx->prog_pos[4] = b; ctx->prog_pos[5] = c;
    ctx->prog_pos[6] = u; ctx->prog_pos[7] = v; ctx->prog_pos[8] = w;
    // Also update display position (in inches)
    if (ctx->metric) {
        x /= 25.4; y /= 25.4; z /= 25.4;
        u /= 25.4; v /= 25.4; w /= 25.4;
    }
    ctx->pos[0] = x; ctx->pos[1] = y; ctx->pos[2] = z;
    ctx->pos[3] = a; ctx->pos[4] = b; ctx->pos[5] = c;
    ctx->pos[6] = u; ctx->pos[7] = v; ctx->pos[8] = w;
}

// Stubs for callbacks we don't care about in preview
static void pc_nop_v(void *ctx) { (void)ctx; }
static void pc_nop_vi(void *ctx, int32_t a) { (void)ctx; (void)a; }
static void pc_nop_vii(void *ctx, int32_t a, int32_t b) { (void)ctx; (void)a; (void)b; }
static void pc_nop_vd(void *ctx, double a) { (void)ctx; (void)a; }
static void pc_nop_vid(void *ctx, int32_t a, double b) { (void)ctx; (void)a; (void)b; }
static void pc_nop_viid(void *ctx, int32_t a, int32_t b, double c) { (void)ctx; (void)a; (void)b; (void)c; }
static void pc_nop_vs(void *ctx, const char *s) { (void)ctx; (void)s; }
static int32_t pc_nop_ri(void *ctx) { (void)ctx; return 0; }
static int32_t pc_nop_rii(void *ctx, int32_t a) { (void)ctx; (void)a; return 0; }
static double pc_nop_rd(void *ctx) { (void)ctx; return 0.0; }
static double pc_nop_rdi(void *ctx, int32_t a) { (void)ctx; (void)a; return 0.0; }
static double pc_nop_rdid(void *ctx, int32_t a, double b) { (void)ctx; (void)a; (void)b; return b; }
static int32_t pc_nop_riid(void *ctx, int32_t a, int32_t b) { (void)ctx; (void)a; return b; }

static double pc_nop_hal_value(void *ctx, const char *name, int32_t *found) {
    (void)ctx; (void)name; *found = 0; return 0.0;
}

static void pc_get_position(void *vctx, double pos[9]) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    memcpy(pos, ctx->pos, 9 * sizeof(double));
}

// Per-axis position getters return program-unit values (what the interpreter expects)
static double pc_get_ext_pos_x(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[0]; }
static double pc_get_ext_pos_y(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[1]; }
static double pc_get_ext_pos_z(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[2]; }
static double pc_get_ext_pos_a(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[3]; }
static double pc_get_ext_pos_b(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[4]; }
static double pc_get_ext_pos_c(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[5]; }
static double pc_get_ext_pos_u(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[6]; }
static double pc_get_ext_pos_v(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[7]; }
static double pc_get_ext_pos_w(void *vctx) { return ((preview_ctx_t*)vctx)->prog_pos[8]; }

static void pc_set_xy_rotation(void *vctx, double r) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->xy_rotation = r;
}

static void pc_select_plane(void *vctx, int32_t plane) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->plane = plane;
}

static void pc_get_probe_position(void *ctx, double pos[9]) {
    memset(pos, 0, 9 * sizeof(double));
    (void)ctx;
}

static void pc_get_tool_offset(void *vctx, double off[9]) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    memcpy(off, ctx->tool_offset, 9 * sizeof(double));
}

static void pc_get_offsets(void *ctx, double off[9]) {
    memset(off, 0, 9 * sizeof(double));
    (void)ctx;
}

static int32_t pc_get_tool_table(void *vctx, int32_t pocket,
    int32_t *toolno, double offset[9], double *diameter,
    double *frontangle, double *backangle, int32_t *orientation) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    if (pocket >= 0 && pocket < CANON_POCKETS_MAX) {
        *toolno = ctx->tools[pocket].toolno;
        memcpy(offset, ctx->tools[pocket].offset, 9 * sizeof(double));
        *diameter = ctx->tools[pocket].diameter;
        *frontangle = ctx->tools[pocket].frontangle;
        *backangle = ctx->tools[pocket].backangle;
        *orientation = ctx->tools[pocket].orientation;
    } else {
        *toolno = 0;
        memset(offset, 0, 9 * sizeof(double));
        *diameter = 0; *frontangle = 0; *backangle = 0; *orientation = 0;
    }
    return 0;
}

static int32_t pc_get_tool_by_number(void *vctx, int32_t toolno,
    int32_t *pocket, double offset[9], double *diameter,
    double *frontangle, double *backangle, int32_t *orientation) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    for (int i = 1; i < CANON_POCKETS_MAX; i++) {
        if (ctx->tools[i].toolno == toolno) {
            *pocket = ctx->tools[i].pocketno;
            memcpy(offset, ctx->tools[i].offset, 9 * sizeof(double));
            *diameter = ctx->tools[i].diameter;
            *frontangle = ctx->tools[i].frontangle;
            *backangle = ctx->tools[i].backangle;
            *orientation = ctx->tools[i].orientation;
            return 0;
        }
    }
    return -1;  // not found
}

static void ctx_set_tool(preview_ctx_t *ctx, int32_t pocket, int32_t toolno,
    double *offset, double diameter, double frontangle, double backangle,
    int32_t orientation) {
    if (pocket < 0 || pocket >= CANON_POCKETS_MAX) return;
    ctx->tools[pocket].toolno = toolno;
    ctx->tools[pocket].pocketno = pocket;
    memcpy(ctx->tools[pocket].offset, offset, 9 * sizeof(double));
    ctx->tools[pocket].diameter = diameter;
    ctx->tools[pocket].frontangle = frontangle;
    ctx->tools[pocket].backangle = backangle;
    ctx->tools[pocket].orientation = orientation;
}

static void pc_set_param_file(void *vctx, const char *name) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    if (name) {
        strncpy(ctx->param_file, name, sizeof(ctx->param_file) - 1);
        ctx->param_file[sizeof(ctx->param_file) - 1] = '\0';
    }
}

static void pc_get_param_file(void *vctx, const char **buf) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    *buf = ctx->param_file;
}

static int32_t pc_get_axis_mask(void *ctx) {
    (void)ctx;
    return 0x1FF; // all 9 axes enabled
}

static int32_t pc_get_queue_empty(void *ctx) {
    (void)ctx;
    return 1; // queue always empty for preview
}

static int32_t pc_get_length_unit_type(void *vctx) {
    (void)vctx;
    return 1; // CANON_UNITS_INCHES — matches gcodemodule.cc; G20/G21 in initcodes handles unit switching
}

static double pc_get_external_length_units(void *vctx) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    return ctx->linear_units;
}

static double pc_get_external_angle_units(void *vctx) {
    (void)vctx;
    return 1.0; // degrees
}

static int32_t pc_get_plane(void *ctx) {
    (void)ctx;
    return 1; // CANON_PLANE_XY
}

static void pc_set_g5x_offset(void *vctx, int32_t o,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // Convert to inches (AXIS internal unit), same as segments
    if (ctx->metric) {
        x /= 25.4; y /= 25.4; z /= 25.4;
        u /= 25.4; v /= 25.4; w /= 25.4;
    }
    ctx->g5x_index = o;
    ctx->g5x_offset[0] = x; ctx->g5x_offset[1] = y; ctx->g5x_offset[2] = z;
    ctx->g5x_offset[3] = a; ctx->g5x_offset[4] = b; ctx->g5x_offset[5] = c;
    ctx->g5x_offset[6] = u; ctx->g5x_offset[7] = v; ctx->g5x_offset[8] = w;
}

static void pc_set_g92_offset(void *vctx,
    double x, double y, double z, double a, double b, double c,
    double u, double v, double w) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    // Convert to inches (AXIS internal unit), same as segments
    if (ctx->metric) {
        x /= 25.4; y /= 25.4; z /= 25.4;
        u /= 25.4; v /= 25.4; w /= 25.4;
    }
    ctx->g92_offset[0] = x; ctx->g92_offset[1] = y; ctx->g92_offset[2] = z;
    ctx->g92_offset[3] = a; ctx->g92_offset[4] = b; ctx->g92_offset[5] = c;
    ctx->g92_offset[6] = u; ctx->g92_offset[7] = v; ctx->g92_offset[8] = w;
}

static void pc_nurbs_feed(void *ctx, int32_t ln,
    const canon_control_point_t *pts, size_t npts, uint32_t order) {
    // NURBS is handled by the interpreter internally — it decomposes
    // into straight_feed calls, so we don't need to handle this.
    (void)ctx; (void)ln; (void)pts; (void)npts; (void)order;
}

static void pc_rigid_tap(void *vctx, int32_t ln,
    double x, double y, double z, double scale) {
    preview_ctx_t *ctx = (preview_ctx_t*)vctx;
    ctx->line_no = ln;
    (void)scale;
    // Record as feed to tap point and back
    add_segment(ctx, 2, x, y, z,
                ctx->pos[3], ctx->pos[4], ctx->pos[5],
                ctx->pos[6], ctx->pos[7], ctx->pos[8]);
}

static int32_t pc_wait_input(void *ctx, int32_t index, int32_t input_type,
    int32_t wait_type, double timeout) {
    (void)ctx; (void)index; (void)input_type; (void)wait_type; (void)timeout;
    return 0;
}

static int32_t pc_lock_rotary(void *ctx, int32_t ln, int32_t joint) {
    (void)ctx; (void)ln; (void)joint;
    return 0;
}

static int32_t pc_unlock_rotary(void *ctx, int32_t ln, int32_t joint) {
    (void)ctx; (void)ln; (void)joint;
    return 0;
}

static void pc_update_tag(void *ctx, uint64_t tag_ptr) {
    (void)ctx; (void)tag_ptr;
}

static void pc_set_tool_table_entry(void *ctx, int32_t pocket, int32_t toolno,
    double ox, double oy, double oz, double oa, double ob, double oc,
    double ou, double ov, double ow,
    double diameter, double frontangle, double backangle, int32_t orientation) {
    (void)ctx; (void)pocket; (void)toolno;
    (void)ox; (void)oy; (void)oz; (void)oa; (void)ob; (void)oc;
    (void)ou; (void)ov; (void)ow;
    (void)diameter; (void)frontangle; (void)backangle; (void)orientation;
}

static double pc_get_user_defined_result(void *ctx) {
    (void)ctx;
    return 0.0;
}

// Build the preview canon callback table
static canon_callbacks_t make_preview_canon(preview_ctx_t *ctx) {
    canon_callbacks_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.ctx = ctx;
    cb.init_canon = pc_init_canon;
    cb.straight_traverse = pc_straight_traverse;
    cb.straight_feed = pc_straight_feed;
    cb.arc_feed = pc_arc_feed;
    cb.nurbs_feed = pc_nurbs_feed;
    cb.rigid_tap = pc_rigid_tap;
    cb.straight_probe = pc_straight_probe;
    cb.set_feed_rate = pc_set_feed_rate;
    cb.dwell = pc_dwell;
    cb.change_tool = pc_change_tool;
    cb.use_tool_length_offset = pc_use_tool_length_offset;
    cb.update_end_point = pc_update_end_point;

    // Coordinate/state setters — no-ops for preview
    cb.set_g5x_offset = pc_set_g5x_offset;
    cb.set_g92_offset = pc_set_g92_offset;
    cb.set_xy_rotation = pc_set_xy_rotation;
    cb.use_length_units = pc_use_length_units;
    cb.select_plane = pc_select_plane;
    cb.set_traverse_rate = (void (*)(void*, double))pc_nop_vd;
    cb.set_feed_reference = (void (*)(void*, int32_t))pc_nop_vi;
    cb.set_feed_mode = (void (*)(void*, int32_t, int32_t))pc_nop_vii;
    cb.set_motion_control_mode = (void (*)(void*, int32_t, double))pc_nop_vid;
    cb.set_naivecam_tolerance = (void (*)(void*, double))pc_nop_vd;
    cb.set_cutter_radius_compensation = (void (*)(void*, double))pc_nop_vd;
    cb.start_cutter_radius_compensation = (void (*)(void*, int32_t))pc_nop_vi;
    cb.stop_cutter_radius_compensation = pc_nop_v;
    cb.start_speed_feed_synch = (void (*)(void*, int32_t, double, int32_t))pc_nop_viid;
    cb.stop_speed_feed_synch = pc_nop_v;

    // Spindle — no-ops
    cb.set_spindle_mode = (void (*)(void*, int32_t, double))pc_nop_vid;
    cb.set_spindle_speed = (void (*)(void*, int32_t, double))pc_nop_vid;
    cb.start_spindle_clockwise = (void (*)(void*, int32_t, int32_t))pc_nop_vii;
    cb.start_spindle_counterclockwise = (void (*)(void*, int32_t, int32_t))pc_nop_vii;
    cb.stop_spindle_turning = (void (*)(void*, int32_t))pc_nop_vi;
    cb.orient_spindle = (void (*)(void*, int32_t, double, int32_t))pc_nop_viid;
    cb.wait_spindle_orient_complete = (void (*)(void*, int32_t, double))pc_nop_vid;

    // Tool — most are no-ops except change_tool
    cb.select_tool = (void (*)(void*, int32_t))pc_nop_vi;
    cb.start_change = pc_nop_v;
    cb.change_tool_number = (void (*)(void*, int32_t))pc_nop_vi;
    cb.reload_tooldata = pc_nop_v;
    cb.set_tool_table_entry = pc_set_tool_table_entry;

    // Coolant/overrides — no-ops
    cb.flood_on = pc_nop_v;
    cb.flood_off = pc_nop_v;
    cb.mist_on = pc_nop_v;
    cb.mist_off = pc_nop_v;
    cb.enable_feed_override = pc_nop_v;
    cb.disable_feed_override = pc_nop_v;
    cb.enable_speed_override = (void (*)(void*, int32_t))pc_nop_vi;
    cb.disable_speed_override = (void (*)(void*, int32_t))pc_nop_vi;
    cb.enable_feed_hold = pc_nop_v;
    cb.disable_feed_hold = pc_nop_v;
    cb.enable_adaptive_feed = pc_nop_v;
    cb.disable_adaptive_feed = pc_nop_v;

    // IO — no-ops
    cb.set_motion_output_bit = (void (*)(void*, int32_t))pc_nop_vi;
    cb.clear_motion_output_bit = (void (*)(void*, int32_t))pc_nop_vi;
    cb.set_aux_output_bit = (void (*)(void*, int32_t))pc_nop_vi;
    cb.clear_aux_output_bit = (void (*)(void*, int32_t))pc_nop_vi;
    cb.set_motion_output_value = (void (*)(void*, int32_t, double))pc_nop_vid;
    cb.set_aux_output_value = (void (*)(void*, int32_t, double))pc_nop_vid;
    cb.wait_input = pc_wait_input;
    cb.clamp_axis = (void (*)(void*, int32_t))pc_nop_vi;
    cb.unclamp_axis = (void (*)(void*, int32_t))pc_nop_vi;
    cb.lock_rotary = pc_lock_rotary;
    cb.unlock_rotary = pc_unlock_rotary;

    // Program flow — no-ops
    cb.stop = pc_nop_v;
    cb.finish = pc_nop_v;
    cb.program_stop = pc_nop_v;
    cb.optional_program_stop = pc_nop_v;
    cb.program_end = pc_nop_v;
    cb.pallet_shuttle = pc_nop_v;

    // Messages — no-ops
    cb.comment = (void (*)(void*, const char*))pc_nop_vs;
    cb.message = (void (*)(void*, const char*))pc_nop_vs;
    cb.log_msg = (void (*)(void*, const char*))pc_nop_vs;
    cb.logopen = (void (*)(void*, const char*))pc_nop_vs;
    cb.logappend = (void (*)(void*, const char*))pc_nop_vs;
    cb.logclose = pc_nop_v;
    cb.canon_error = (void (*)(void*, const char*))pc_nop_vs;

    // Probe
    cb.turn_probe_on = pc_nop_v;
    cb.turn_probe_off = pc_nop_v;

    // Block delete / optional stop
    cb.set_block_delete = (void (*)(void*, int32_t))pc_nop_vi;
    cb.get_block_delete = pc_nop_ri;
    cb.set_optional_program_stop = (void (*)(void*, int32_t))pc_nop_vi;
    cb.get_optional_program_stop = pc_nop_ri;

    // State tag
    cb.update_tag = pc_update_tag;

    // Parameter file
    cb.set_parameter_file_name = pc_set_param_file;
    cb.on_reset = pc_nop_v;

    // Getters
    cb.get_external_feed_rate = pc_nop_rd;
    cb.get_external_traverse_rate = pc_nop_rd;
    cb.get_external_length_unit_type = pc_get_length_unit_type;
    cb.get_external_length_units = pc_get_external_length_units;
    cb.get_external_angle_units = pc_get_external_angle_units;
    cb.get_external_motion_control_mode = pc_nop_ri;
    cb.get_external_motion_control_tolerance = pc_nop_rd;
    cb.get_external_motion_control_naivecam_tolerance = pc_nop_rd;
    cb.get_external_flood = pc_nop_ri;
    cb.get_external_mist = pc_nop_ri;
    cb.get_external_position_x = pc_get_ext_pos_x;
    cb.get_external_position_y = pc_get_ext_pos_y;
    cb.get_external_position_z = pc_get_ext_pos_z;
    cb.get_external_position_a = pc_get_ext_pos_a;
    cb.get_external_position_b = pc_get_ext_pos_b;
    cb.get_external_position_c = pc_get_ext_pos_c;
    cb.get_external_position_u = pc_get_ext_pos_u;
    cb.get_external_position_v = pc_get_ext_pos_v;
    cb.get_external_position_w = pc_get_ext_pos_w;
    cb.get_external_probe_position_x = pc_nop_rd;
    cb.get_external_probe_position_y = pc_nop_rd;
    cb.get_external_probe_position_z = pc_nop_rd;
    cb.get_external_probe_position_a = pc_nop_rd;
    cb.get_external_probe_position_b = pc_nop_rd;
    cb.get_external_probe_position_c = pc_nop_rd;
    cb.get_external_probe_position_u = pc_nop_rd;
    cb.get_external_probe_position_v = pc_nop_rd;
    cb.get_external_probe_position_w = pc_nop_rd;
    cb.get_external_probe_value = pc_nop_rd;
    cb.get_external_probe_tripped_value = pc_nop_ri;
    cb.get_external_speed = (double (*)(void*, int32_t))pc_nop_rdi;
    cb.get_external_spindle = (int32_t (*)(void*, int32_t))pc_nop_rii;
    cb.get_external_tool_length_xoffset = pc_nop_rd;
    cb.get_external_tool_length_yoffset = pc_nop_rd;
    cb.get_external_tool_length_zoffset = pc_nop_rd;
    cb.get_external_tool_length_aoffset = pc_nop_rd;
    cb.get_external_tool_length_boffset = pc_nop_rd;
    cb.get_external_tool_length_coffset = pc_nop_rd;
    cb.get_external_tool_length_uoffset = pc_nop_rd;
    cb.get_external_tool_length_voffset = pc_nop_rd;
    cb.get_external_tool_length_woffset = pc_nop_rd;
    cb.get_external_tool_slot = pc_nop_ri;
    cb.get_external_selected_tool_slot = pc_nop_ri;
    cb.get_external_tool_table = pc_get_tool_table;
    cb.get_tool_by_number = pc_get_tool_by_number;
    cb.get_external_tc_fault = pc_nop_ri;
    cb.get_external_tc_reason = pc_nop_ri;
    cb.get_external_queue_empty = pc_get_queue_empty;
    cb.get_external_axis_mask = pc_get_axis_mask;
    cb.get_external_digital_input = pc_nop_riid;
    cb.get_external_analog_input = pc_nop_rdid;
    cb.get_external_feed_override_enable = pc_nop_ri;
    cb.get_external_spindle_override_enable = (int32_t (*)(void*, int32_t))pc_nop_rii;
    cb.get_external_adaptive_feed_enable = pc_nop_ri;
    cb.get_external_feed_hold_enable = pc_nop_ri;
    cb.get_external_plane = pc_get_plane;
    cb.get_external_parameter_file_name = pc_get_param_file;
    cb.get_external_offset_applied = pc_nop_ri;
    cb.get_external_offsets = pc_get_offsets;
    cb.get_user_defined_result = pc_get_user_defined_result;
    cb.get_external_hal_value = pc_nop_hal_value;

    return cb;
}
*/
import "C"

import (
	"fmt"
	"log/slog"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/ngcpreview"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("ngcpreview", newNgcPreview)
	apiserver.RegisterMeta(ngcpreview.NgcpreviewMeta)
}

type ngcPreview struct {
	logger         *slog.Logger
	name           string  // module instance name
	parameterFile  string  // from [RS274NGC]PARAMETER_FILE
	linearUnits    float64 // from [TRAJ]LINEAR_UNITS: 1.0 for mm, 1/25.4 for inch
	ttInstanceName string  // tooltable instance to look up (default "tooltable")
	ttClient       *tooltable.TooltableClient
	allowedDirs    []string // resolved absolute paths where get_file may read
}

func parseLinearUnits(s string) float64 {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "mm", "metric":
		return 1.0
	case "in", "inch", "imperial":
		return 1.0 / 25.4
	default:
		if v, err := strconv.ParseFloat(s, 64); err == nil && v != 0 {
			return v
		}
		return 1.0
	}
}

func newNgcPreview(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	// Allow overriding INI namespace via "namespace=xxx" argument.
	ns := name
	ttInst := "tooltable"
	for _, arg := range args {
		if strings.HasPrefix(arg, "namespace=") {
			ns = strings.TrimPrefix(arg, "namespace=")
		} else if strings.HasPrefix(arg, "tooltable_instance=") {
			ttInst = strings.TrimPrefix(arg, "tooltable_instance=")
		}
	}
	nsIni := ini.WithNamespace(ns)
	paramFile := nsIni.Get("RS274NGC", "PARAMETER_FILE")
	// Resolve relative parameter file path against the INI file's directory
	if paramFile != "" && !filepath.IsAbs(paramFile) {
		iniDir := filepath.Dir(ini.SourceFile())
		paramFile = filepath.Join(iniDir, paramFile)
	}
	linearUnits := parseLinearUnits(nsIni.Get("TRAJ", "LINEAR_UNITS"))
	// Build allowed directories for get_file path restriction
	iniDir := filepath.Dir(ini.SourceFile())
	allowedDirs := collectAllowedDirs(nsIni, iniDir)
	m := &ngcPreview{logger: logger, name: name, parameterFile: paramFile, linearUnits: linearUnits, ttInstanceName: ttInst, allowedDirs: allowedDirs}
	ngcpreview.RegisterNgcpreviewAPI(apiserver.DefaultRegistry(), name, m)
	logger.Info("ngcpreview module loaded and API registered", "instance", name, "parameterFile", paramFile)
	return m, nil
}

func (m *ngcPreview) Start() error {
	// Look up tooltable API for tool data during preview generation.
	reg := apiserver.DefaultRegistry()
	ttCbs, err := reg.GetAPIFor(m.name, "tooltable", m.ttInstanceName, 1)
	if err != nil {
		m.logger.Warn("ngcpreview: tooltable API not available, tool data will be empty", "err", err)
	} else {
		m.ttClient = tooltable.NewTooltableClient(unsafe.Pointer(ttCbs))
	}
	return nil
}

func (m *ngcPreview) Stop()    {}
func (m *ngcPreview) Destroy() {}

// sanitize replaces NaN/Inf with 0 to avoid JSON serialization errors.
func sanitize(v float64) float64 {
	if math.IsNaN(v) || math.IsInf(v, 0) {
		return 0
	}
	return v
}

func shimErrorText(h *C.interp_handle_t, rc C.int) string {
	var buf [256]C.char
	C.interp_shim_error_text(h, rc, &buf[0], 256)
	return C.GoString(&buf[0])
}

// GenPreview implements ngcpreview.NgcpreviewCallbacks.
func (m *ngcPreview) GenPreview(filename string, initcodes string, unitcode string) (*ngcpreview.PreviewResult, error) {
	// Create a fresh interpreter
	h := C.interp_shim_new()
	if h == nil {
		return nil, fmt.Errorf("failed to create interpreter")
	}
	defer C.interp_shim_destroy(h)

	// Set up preview context (C heap to satisfy cgo pointer rules)
	ctx := (*C.preview_ctx_t)(C.calloc(1, C.size_t(unsafe.Sizeof(C.preview_ctx_t{}))))
	// Pre-populate parameter file path from INI
	if m.parameterFile != "" {
		cPF := C.CString(m.parameterFile)
		C.strncpy(&ctx.param_file[0], cPF, 1023)
		C.free(unsafe.Pointer(cPF))
	}
	ctx.linear_units = C.double(m.linearUnits)
	ctx.plane = 1 // default XY plane

	// Pre-populate tool table from tooltable API
	if m.ttClient != nil {
		entries, err := m.ttClient.ListTools()
		if err == nil {
			for i := range entries {
				pocket := entries[i].Pocketno
				if pocket < 0 || int(pocket) >= int(C.CANON_POCKETS_MAX) {
					continue
				}
				off := [9]C.double{
					C.double(entries[i].XOffset), C.double(entries[i].YOffset), C.double(entries[i].ZOffset),
					C.double(entries[i].AOffset), C.double(entries[i].BOffset), C.double(entries[i].COffset),
					C.double(entries[i].UOffset), C.double(entries[i].VOffset), C.double(entries[i].WOffset),
				}
				C.ctx_set_tool(ctx, C.int32_t(pocket), C.int32_t(entries[i].Toolno),
					&off[0], C.double(entries[i].Diameter),
					C.double(entries[i].Frontangle), C.double(entries[i].Backangle),
					C.int32_t(entries[i].Orientation))
			}
		}
	}

	defer func() {
		C.free(unsafe.Pointer(ctx.segments))
		C.free(unsafe.Pointer(ctx.dwells))
		C.free(unsafe.Pointer(ctx.tool_changes))
		C.free(unsafe.Pointer(ctx))
	}()

	// Build and set recording canon (cb on C heap too)
	cbHeap := (*C.canon_callbacks_t)(C.malloc(C.size_t(unsafe.Sizeof(C.canon_callbacks_t{}))))
	defer C.free(unsafe.Pointer(cbHeap))
	*cbHeap = C.make_preview_canon(ctx)
	C.interp_shim_set_callbacks(h, cbHeap)

	// Initialize interpreter
	rc := C.interp_shim_init(h)
	if rc != C.INTERP_SHIM_OK {
		errText := shimErrorText(h, rc)
		return &ngcpreview.PreviewResult{
			Error: fmt.Sprintf("interpreter init failed: %d (%s)", rc, errText),
		}, nil
	}

	// Open file first (sets up parameter file, subroutine paths, etc.)
	cFile := C.CString(filename)
	rc = C.interp_shim_open(h, cFile)
	C.free(unsafe.Pointer(cFile))
	if rc != C.INTERP_SHIM_OK {
		errText := shimErrorText(h, rc)
		return &ngcpreview.PreviewResult{
			Error: fmt.Sprintf("open failed: %d (%s)", rc, errText),
		}, nil
	}

	// Execute initcodes after open (matches gcodemodule behavior)
	if initcodes != "" {
		for _, line := range strings.Split(initcodes, "\n") {
			line = strings.TrimSpace(line)
			if line == "" {
				continue
			}
			cCode := C.CString(line)
			rc = C.interp_shim_read_string(h, cCode)
			C.free(unsafe.Pointer(cCode))
			if rc == C.INTERP_SHIM_OK {
				rc = C.interp_shim_execute(h)
			}
			if rc > C.INTERP_SHIM_ENDFILE {
				return &ngcpreview.PreviewResult{
					Error: fmt.Sprintf("initcodes execution failed: %d", rc),
				}, nil
			}
		}
	}

	// Execute unitcode after initcodes
	if unitcode != "" {
		cCode := C.CString(unitcode)
		rc = C.interp_shim_read_string(h, cCode)
		C.free(unsafe.Pointer(cCode))
		if rc == C.INTERP_SHIM_OK {
			rc = C.interp_shim_execute(h)
		}
		if rc != C.INTERP_SHIM_OK {
			return &ngcpreview.PreviewResult{
				Error: fmt.Sprintf("unitcode execution failed: %d", rc),
			}, nil
		}
	}

	// Read/execute loop
	var maxLine int32
	var readCount, execCount int
	var lastReadRC, lastExecRC C.int
	for {
		rc = C.interp_shim_read(h)
		lastReadRC = rc
		if rc == C.INTERP_SHIM_ENDFILE {
			break
		}
		if rc != C.INTERP_SHIM_OK {
			break
		}
		readCount++
		rc = C.interp_shim_execute(h)
		lastExecRC = rc
		if rc != C.INTERP_SHIM_OK && rc != C.INTERP_SHIM_ENDFILE && rc != C.INTERP_SHIM_EXIT {
			break
		}
		if rc == C.INTERP_SHIM_EXIT {
			execCount++
			break
		}
		execCount++
		seq := int32(C.interp_shim_sequence_number(h))
		if seq > maxLine {
			maxLine = seq
		}
	}

	C.interp_shim_close(h)

	// Convert C results to Go types
	var errMsg string
	if lastExecRC > C.INTERP_SHIM_ENDFILE {
		errText := shimErrorText(h, lastExecRC)
		errMsg = fmt.Sprintf("line %d: execute error %d: %s", maxLine+1, lastExecRC, errText)
	} else if lastReadRC != C.INTERP_SHIM_OK && lastReadRC != C.INTERP_SHIM_ENDFILE {
		errText := shimErrorText(h, lastReadRC)
		errMsg = fmt.Sprintf("line %d: read error %d: %s", maxLine+1, lastReadRC, errText)
	}
	result := &ngcpreview.PreviewResult{
		MaxLine: maxLine,
		Error:   errMsg,
	}

	// Convert segments
	nSegs := int(ctx.seg_count)
	if nSegs > 0 {
		result.Segments = make([]ngcpreview.Segment, nSegs)
		segs := unsafe.Slice(ctx.segments, nSegs)
		for i := 0; i < nSegs; i++ {
			s := &segs[i]
			result.Segments[i] = ngcpreview.Segment{
				Type:   ngcpreview.SegmentType(s._type),
				LineNo: int32(s.line_no),
				Start: ngcpreview.Position{
					X: sanitize(float64(s.start[0])), Y: sanitize(float64(s.start[1])), Z: sanitize(float64(s.start[2])),
					A: sanitize(float64(s.start[3])), B: sanitize(float64(s.start[4])), C: sanitize(float64(s.start[5])),
					U: sanitize(float64(s.start[6])), V: sanitize(float64(s.start[7])), W: sanitize(float64(s.start[8])),
				},
				End: ngcpreview.Position{
					X: sanitize(float64(s.end[0])), Y: sanitize(float64(s.end[1])), Z: sanitize(float64(s.end[2])),
					A: sanitize(float64(s.end[3])), B: sanitize(float64(s.end[4])), C: sanitize(float64(s.end[5])),
					U: sanitize(float64(s.end[6])), V: sanitize(float64(s.end[7])), W: sanitize(float64(s.end[8])),
				},
				Feedrate: sanitize(float64(s.feedrate)),
				ToolOffset: ngcpreview.Position{
					X: sanitize(float64(s.tool_offset[0])), Y: sanitize(float64(s.tool_offset[1])), Z: sanitize(float64(s.tool_offset[2])),
					A: sanitize(float64(s.tool_offset[3])), B: sanitize(float64(s.tool_offset[4])), C: sanitize(float64(s.tool_offset[5])),
					U: sanitize(float64(s.tool_offset[6])), V: sanitize(float64(s.tool_offset[7])), W: sanitize(float64(s.tool_offset[8])),
				},
				CenterX:  sanitize(float64(s.center_x)),
				CenterY:  sanitize(float64(s.center_y)),
				Rotation: int32(s.rotation),
				Plane:    int32(s.plane),
			}
		}
	}

	// Convert dwells
	nDwells := int(ctx.dwell_count)
	if nDwells > 0 {
		result.Dwells = make([]ngcpreview.Dwell, nDwells)
		dwells := unsafe.Slice(ctx.dwells, nDwells)
		for i := 0; i < nDwells; i++ {
			d := &dwells[i]
			result.Dwells[i] = ngcpreview.Dwell{
				LineNo:  int32(d.line_no),
				Seconds: float64(d.seconds),
				Plane:   int32(d.plane),
				Pos: ngcpreview.Position{
					X: float64(d.pos[0]), Y: float64(d.pos[1]), Z: float64(d.pos[2]),
					A: float64(d.pos[3]), B: float64(d.pos[4]), C: float64(d.pos[5]),
					U: float64(d.pos[6]), V: float64(d.pos[7]), W: float64(d.pos[8]),
				},
			}
		}
	}

	// Convert tool changes
	nTC := int(ctx.tc_count)
	if nTC > 0 {
		result.ToolChanges = make([]ngcpreview.ToolChange, nTC)
		tcs := unsafe.Slice(ctx.tool_changes, nTC)
		for i := 0; i < nTC; i++ {
			tc := &tcs[i]
			result.ToolChanges[i] = ngcpreview.ToolChange{
				LineNo: int32(tc.line_no),
				ToolNo: int32(tc.tool_no),
			}
		}
	}

	// Set active offsets (captured from SET_G5X_OFFSET / SET_G92_OFFSET canon calls)
	result.G5xIndex = int32(ctx.g5x_index)
	result.G5xOffset = ngcpreview.Position{
		X: sanitize(float64(ctx.g5x_offset[0])), Y: sanitize(float64(ctx.g5x_offset[1])), Z: sanitize(float64(ctx.g5x_offset[2])),
		A: sanitize(float64(ctx.g5x_offset[3])), B: sanitize(float64(ctx.g5x_offset[4])), C: sanitize(float64(ctx.g5x_offset[5])),
		U: sanitize(float64(ctx.g5x_offset[6])), V: sanitize(float64(ctx.g5x_offset[7])), W: sanitize(float64(ctx.g5x_offset[8])),
	}
	result.G92Offset = ngcpreview.Position{
		X: sanitize(float64(ctx.g92_offset[0])), Y: sanitize(float64(ctx.g92_offset[1])), Z: sanitize(float64(ctx.g92_offset[2])),
		A: sanitize(float64(ctx.g92_offset[3])), B: sanitize(float64(ctx.g92_offset[4])), C: sanitize(float64(ctx.g92_offset[5])),
		U: sanitize(float64(ctx.g92_offset[6])), V: sanitize(float64(ctx.g92_offset[7])), W: sanitize(float64(ctx.g92_offset[8])),
	}
	result.XyRotation = float64(ctx.xy_rotation)
	result.Plane = int32(ctx.plane)

	return result, nil
}

// collectAllowedDirs builds the whitelist of directories from which get_file
// may serve files. Directories are resolved to absolute paths.
func collectAllowedDirs(ini *inifile.IniFile, iniDir string) []string {
	var dirs []string
	resolve := func(p string) string {
		if p == "" {
			return ""
		}
		if !filepath.IsAbs(p) {
			p = filepath.Join(iniDir, p)
		}
		// EvalSymlinks resolves ".." and symlinks
		if abs, err := filepath.EvalSymlinks(p); err == nil {
			return abs
		}
		// Fall back to Abs if the dir doesn't exist yet
		abs, _ := filepath.Abs(p)
		return abs
	}

	// [DISPLAY] PROGRAM_PREFIX — the NC_FILES directory
	if pp := ini.Get("DISPLAY", "PROGRAM_PREFIX"); pp != "" {
		if d := resolve(pp); d != "" {
			dirs = append(dirs, d)
		}
	}
	// [RS274NGC] SUBROUTINE_PATH — colon-separated list
	if sp := ini.Get("RS274NGC", "SUBROUTINE_PATH"); sp != "" {
		for _, p := range strings.Split(sp, ":") {
			p = strings.TrimSpace(p)
			if d := resolve(p); d != "" {
				dirs = append(dirs, d)
			}
		}
	}
	// [FILTER] PROGRAM_EXTENSION lines define filtered file types;
	// the filtered output typically goes to a tempdir, but we allow
	// the PROGRAM_PREFIX which already covers it.

	// System share directory (contains splash screen NGC files, etc.)
	if emcHome := os.Getenv("EMC2_HOME"); emcHome != "" {
		shareDir := filepath.Join(emcHome, "share")
		if d := resolve(shareDir); d != "" {
			dirs = append(dirs, d)
		}
	}

	return dirs
}

// isAllowedPath checks whether the resolved path resides under one of the
// allowed directories.
func (m *ngcPreview) isAllowedPath(resolvedPath string) bool {
	for _, dir := range m.allowedDirs {
		if strings.HasPrefix(resolvedPath, dir+string(filepath.Separator)) || resolvedPath == dir {
			return true
		}
	}
	return false
}

// GetFile implements ngcpreview.NgcpreviewCallbacks — serves file contents
// with path restriction.
func (m *ngcPreview) GetFile(filename string) (*ngcpreview.FileResult, error) {
	if filename == "" {
		return &ngcpreview.FileResult{Error: "empty filename"}, nil
	}

	abs, err := filepath.Abs(filename)
	if err != nil {
		return &ngcpreview.FileResult{Error: "invalid path"}, nil
	}
	real, err := filepath.EvalSymlinks(abs)
	if err != nil {
		return &ngcpreview.FileResult{Error: "file not found"}, nil
	}

	if !m.isAllowedPath(real) {
		m.logger.Warn("get_file: access denied", "requested", filename, "resolved", real)
		return &ngcpreview.FileResult{Error: "access denied: file is not in an allowed directory"}, nil
	}

	data, err := os.ReadFile(real)
	if err != nil {
		return &ngcpreview.FileResult{Error: fmt.Sprintf("read error: %v", err)}, nil
	}

	// Split into lines, preserving empty trailing line if present
	content := string(data)
	lines := strings.Split(content, "\n")
	// Remove last empty element from trailing newline
	if len(lines) > 0 && lines[len(lines)-1] == "" {
		lines = lines[:len(lines)-1]
	}

	return &ngcpreview.FileResult{Lines: lines}, nil
}
