/********************************************************************
* Description: interp_workplane.cc
*
*   The tilted work plane: G68.2, G68.4 and G69, and the frame they put
*   inside the offset chain.
*
*   The chain, as canon applies it:
*
*       world = TLO + G5x + Rz(rotation_xy) * (G92 + O + R * program)
*
*   O and R are the plane's origin and rotation, expressed in the
*   coordinate system that was active when the plane was defined: G5x
*   with G92 and the XY rotation in place, which is what the operator
*   sees on the display and what G68.2 X Y Z means on every control.
*   Rotary and UVW words do not pass through the plane: on a TCP
*   kinematics the rotary world coordinates are the rotary joints, and a
*   plane does not change what a joint is.
*
*   The interpreter keeps its current position in program coordinates
*   and only needs the chain where it reasons about absolute coordinates
*   itself (G53, G28/G30, #5021, G28.1, a G43 change).  Those places
*   call program_to_world_xyz() and world_to_program_xyz() from here
*   rather than repeating the stages.
*
*   The plane is not persistent: Interp::init(), M2/M30 and G69 clear
*   it.  Nothing is written to the var file.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2026 All rights reserved.
********************************************************************/

#include <math.h>
#include <string.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"

//----------------------------------------------------------------------
// small matrix helpers, row major double[3][3]
//----------------------------------------------------------------------

static void mat_identity(double m[3][3])
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) { m[i][j] = (i == j) ? 1.0 : 0.0; }
    }
}

// rotation about axis 1, 2 or 3 (X, Y, Z) by an angle in degrees
static void mat_rotation(int axis, double deg, double m[3][3])
{
    double c = cos(deg * M_PI / 180.0), s = sin(deg * M_PI / 180.0);
    mat_identity(m);
    switch (axis) {
    case 1: m[1][1] = c; m[1][2] = -s; m[2][1] = s; m[2][2] = c; break;
    case 2: m[0][0] = c; m[0][2] = s; m[2][0] = -s; m[2][2] = c; break;
    default: m[0][0] = c; m[0][1] = -s; m[1][0] = s; m[1][1] = c; break;
    }
}

static void mat_mul(const double a[3][3], const double b[3][3], double out[3][3])
{
    double r[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j];
        }
    }
    memcpy(out, r, sizeof(r));
}

static void mat_apply(const double m[3][3], double *x, double *y, double *z)
{
    double px = *x, py = *y, pz = *z;
    *x = m[0][0]*px + m[0][1]*py + m[0][2]*pz;
    *y = m[1][0]*px + m[1][1]*py + m[1][2]*pz;
    *z = m[2][0]*px + m[2][1]*py + m[2][2]*pz;
}

static void mat_apply_transposed(const double m[3][3], double *x, double *y, double *z)
{
    double px = *x, py = *y, pz = *z;
    *x = m[0][0]*px + m[1][0]*py + m[2][0]*pz;
    *y = m[0][1]*px + m[1][1]*py + m[2][1]*pz;
    *z = m[0][2]*px + m[1][2]*py + m[2][2]*pz;
}

static double vec_norm(const double v[3])
{
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static void vec_cross(const double a[3], const double b[3], double out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

// a rotation whose columns are the three axes
static void mat_from_axes(const double x[3], const double y[3], const double z[3], double m[3][3])
{
    for (int i = 0; i < 3; i++) { m[i][0] = x[i]; m[i][1] = y[i]; m[i][2] = z[i]; }
}

//----------------------------------------------------------------------
// the chain
//----------------------------------------------------------------------

// the plane stage alone: program coordinates to the system the plane was
// defined in, and back
void Interp::g68_apply(setup_pointer s, double *x, double *y, double *z)
{
    if (!s->g68_active) { return; }
    mat_apply(s->g68_rotation, x, y, z);
    *x += s->g68_offset[0];
    *y += s->g68_offset[1];
    *z += s->g68_offset[2];
}

void Interp::g68_remove(setup_pointer s, double *x, double *y, double *z)
{
    if (!s->g68_active) { return; }
    *x -= s->g68_offset[0];
    *y -= s->g68_offset[1];
    *z -= s->g68_offset[2];
    mat_apply_transposed(s->g68_rotation, x, y, z);
}

// a displacement in the system the plane was defined in, seen from the
// program: the rotation without the origin
void Interp::g68_unrotate(setup_pointer s, double *x, double *y, double *z)
{
    if (!s->g68_active) { return; }
    mat_apply_transposed(s->g68_rotation, x, y, z);
}

// The whole chain for X Y Z, program coordinates to the absolute (G53)
// frame: the plane, G92, the XY rotation, G5x and the tool offset.
void Interp::program_to_world_xyz(setup_pointer s,
                                  double px, double py, double pz,
                                  double *wx, double *wy, double *wz)
{
    double x = px, y = py, z = pz;

    g68_apply(s, &x, &y, &z);
    x += s->axis_offset_x;
    y += s->axis_offset_y;
    z += s->axis_offset_z;
    rotate(&x, &y, s->rotation_xy);
    *wx = x + s->origin_offset_x + s->tool_offset.tran.x;
    *wy = y + s->origin_offset_y + s->tool_offset.tran.y;
    *wz = z + s->origin_offset_z + s->tool_offset.tran.z;
}

void Interp::world_to_program_xyz(setup_pointer s,
                                  double wx, double wy, double wz,
                                  double *px, double *py, double *pz)
{
    double x = wx - s->origin_offset_x - s->tool_offset.tran.x;
    double y = wy - s->origin_offset_y - s->tool_offset.tran.y;
    double z = wz - s->origin_offset_z - s->tool_offset.tran.z;

    rotate(&x, &y, -s->rotation_xy);
    x -= s->axis_offset_x;
    y -= s->axis_offset_y;
    z -= s->axis_offset_z;
    g68_remove(s, &x, &y, &z);
    *px = x;
    *py = y;
    *pz = z;
}

//----------------------------------------------------------------------
// setting and clearing the plane
//----------------------------------------------------------------------

// Install a plane.  The tool does not move, so its program coordinates
// change: take the current point through the old chain to the absolute
// frame and back through the new one.
int Interp::work_plane_set(setup_pointer s, int code,
                           const double origin[3], const double rotation[3][3])
{
    double wx, wy, wz, flat[9];

    program_to_world_xyz(s, s->current_x, s->current_y, s->current_z, &wx, &wy, &wz);

    for (int i = 0; i < 3; i++) {
        s->g68_offset[i] = origin[i];
        for (int j = 0; j < 3; j++) {
            s->g68_rotation[i][j] = rotation[i][j];
            flat[3*i + j] = rotation[i][j];
        }
    }
    s->g68_active = true;
    s->g68_code = code;

    world_to_program_xyz(s, wx, wy, wz, &s->current_x, &s->current_y, &s->current_z);

    SET_G68_FRAME(origin[0], origin[1], origin[2], flat, 1);
    return INTERP_OK;
}

// Cancel the plane if one is in effect.  Canon is told only when there was
// something to cancel, so a program that never tilts sees no new canon
// call.  tell_canon_anyway says to send the cancel even so, which is what
// an abort and an explicit G69 do: status carries the plane the executed
// canon stream last set, and an abort throws away everything the read
// ahead had queued, so the two can disagree and only canon can settle it.
int Interp::work_plane_cancel(setup_pointer s, bool tell_canon_anyway)
{
    double wx, wy, wz;
    static const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

    s->g68_seq_code = 0;
    if (!s->g68_active) {
        if (tell_canon_anyway) { SET_G68_FRAME(0.0, 0.0, 0.0, identity, 0); }
        return INTERP_OK;
    }

    program_to_world_xyz(s, s->current_x, s->current_y, s->current_z, &wx, &wy, &wz);
    s->g68_active = false;
    s->g68_code = 0;
    for (int i = 0; i < 3; i++) { s->g68_offset[i] = 0.0; }
    mat_identity(s->g68_rotation);
    world_to_program_xyz(s, wx, wy, wz, &s->current_x, &s->current_y, &s->current_z);

    SET_G68_FRAME(0.0, 0.0, 0.0, identity, 0);
    return INTERP_OK;
}

// A block that is not part of a pending three-point or two-vector
// sequence: the sequence was left incomplete.
int Interp::work_plane_check_sequence(block_pointer block, setup_pointer s)
{
    if (s->g68_seq_code == 0) { return INTERP_OK; }
    if (block->g_modes[GM_WORK_PLANE] == s->g68_seq_code) { return INTERP_OK; }
    s->g68_seq_code = 0;
    ERS(_("G68.2 P%d sequence is incomplete: the next block must carry the next Q"), s->g68_seq_p);
}

//----------------------------------------------------------------------
// the definitions
//----------------------------------------------------------------------

// Q names the axes of a three-angle definition, three digits from 1 to 3,
// no two adjacent alike: 313 is Z X Z, 123 is X Y Z.
static int parse_axis_order(double q, int order[3])
{
    int n = (int)round(q);
    if (fabs(q - n) > 1e-9 || n < 111 || n > 333) { return -1; }
    order[0] = n / 100;
    order[1] = (n / 10) % 10;
    order[2] = n % 10;
    for (int i = 0; i < 3; i++) {
        if (order[i] < 1 || order[i] > 3) { return -1; }
    }
    if (order[0] == order[1] || order[1] == order[2]) { return -1; }
    return 0;
}

// The rotation of a G68.2 or G68.4 block, and whether the block completes
// a definition.  The three-point and two-vector forms arrive over several
// blocks with Q; the words are kept in the setup until the last one.
int Interp::work_plane_build(block_pointer block, setup_pointer s,
                             double origin[3], double rotation[3][3], int *complete)
{
    int p = block->p_flag ? (int)round(block->p_number) : 0;
    double r = block->r_flag ? block->r_number : 0.0;
    double rz[3][3];

    *complete = 0;
    CHKS((block->p_flag && (fabs(block->p_number - p) > 1e-9 || p < 0 || p > 3)),
         _("P word with G68.2 must be 0, 1, 2 or 3"));

    if (p == 0 || p == 1) {
        // three angles.  P0: each about an axis of the frame as rotated so
        // far (Euler, ZXZ by default).  P1: each about a fixed axis of the
        // system the plane is defined in, in the order Q gives (XYZ by
        // default).
        int order[3];
        double angle[3], m[3][3];

        CHKS((s->g68_seq_code != 0), _("G68.2 P%d cannot interrupt a P%d sequence"), p, s->g68_seq_p);
        CHKS((parse_axis_order(block->q_flag ? block->q_number : (p == 0 ? 313.0 : 123.0), order) != 0),
             _("Q word with G68.2 P%d must be three axis digits 1 to 3 with no two adjacent alike"), p);
        angle[0] = block->i_flag ? block->i_number : 0.0;
        angle[1] = block->j_flag ? block->j_number : 0.0;
        angle[2] = block->k_flag ? block->k_number : 0.0;

        mat_identity(rotation);
        for (int i = 0; i < 3; i++) {
            mat_rotation(order[i], angle[i], m);
            if (p == 0) {
                mat_mul(rotation, m, rotation);
            } else {
                mat_mul(m, rotation, rotation);
            }
        }
        origin[0] = block->x_flag ? block->x_number : 0.0;
        origin[1] = block->y_flag ? block->y_number : 0.0;
        origin[2] = block->z_flag ? block->z_number : 0.0;
        mat_rotation(3, r, rz);
        mat_mul(rotation, rz, rotation);
        *complete = 1;
        return INTERP_OK;
    }

    // the sequences
    {
        int q = block->q_flag ? (int)round(block->q_number) : -1;
        int code = block->g_modes[GM_WORK_PLANE];
        int first = (p == 2) ? 0 : 1, last = (p == 2) ? 3 : 2;
        int expect;

        CHKS((q < 0 || fabs(block->q_number - q) > 1e-9), _("Q word missing with G68.2 P%d"), p);
        if (s->g68_seq_code == 0) {
            // the first block of a sequence; a three-point definition may
            // leave out Q0 and take the first point as origin
            CHKS((q != first && !(p == 2 && q == 1)),
                 _("G68.2 P%d sequence must start with Q%d"), p, first);
            s->g68_seq_code = code;
            s->g68_seq_p = p;
            s->g68_seq_have = 0;
        } else {
            CHKS((s->g68_seq_code != code || s->g68_seq_p != p),
                 _("G68.2 P%d cannot interrupt a P%d sequence"), p, s->g68_seq_p);
        }
        expect = -1;
        for (int i = first; i <= last; i++) {
            if (!(s->g68_seq_have & (1 << i))) { expect = i; break; }
        }
        if (!(q == expect || (p == 2 && expect == 0 && q == 1))) {
            s->g68_seq_code = 0;
            ERS(_("G68.2 P%d expects Q%d here"), p, expect);
        }
        s->g68_seq_have |= 1 << q;
        s->g68_seq_word[q][0] = block->x_flag ? block->x_number : 0.0;
        s->g68_seq_word[q][1] = block->y_flag ? block->y_number : 0.0;
        s->g68_seq_word[q][2] = block->z_flag ? block->z_number : 0.0;
        s->g68_seq_word[q][3] = block->i_flag ? block->i_number : 0.0;
        s->g68_seq_word[q][4] = block->j_flag ? block->j_number : 0.0;
        s->g68_seq_word[q][5] = block->k_flag ? block->k_number : 0.0;
        s->g68_seq_word[q][6] = r;
        if (q != last) { return INTERP_OK; }
    }

    // the sequence is complete
    s->g68_seq_code = 0;
    if (p == 2) {
        // three points: the first to the second is +X, the third lies on
        // the +Y side; Q0 gives the origin and R, else the origin is the
        // first point
        const double *p1 = s->g68_seq_word[1], *p2 = s->g68_seq_word[2], *p3 = s->g68_seq_word[3];
        double x[3], v[3], y[3], z[3], len;

        for (int i = 0; i < 3; i++) { x[i] = p2[i] - p1[i]; v[i] = p3[i] - p1[i]; }
        len = vec_norm(x);
        CHKS((len < 1e-9), _("G68.2 P2: the first two points coincide"));
        for (int i = 0; i < 3; i++) { x[i] /= len; }
        vec_cross(x, v, z);
        len = vec_norm(z);
        CHKS((len < 1e-9 * fmax(1.0, vec_norm(v))), _("G68.2 P2: the three points are on one line"));
        for (int i = 0; i < 3; i++) { z[i] /= len; }
        vec_cross(z, x, y);
        mat_from_axes(x, y, z, rotation);
        if (s->g68_seq_have & 1) {
            for (int i = 0; i < 3; i++) { origin[i] = s->g68_seq_word[0][i]; }
            r = s->g68_seq_word[0][6];
        } else {
            for (int i = 0; i < 3; i++) { origin[i] = p1[i]; }
            r = 0.0;
        }
    } else {
        // two vectors: the origin and +X on the first block, +Z on the
        // second; X is projected onto the plane so that a request a few
        // digits off square still names a frame
        const double *q1 = s->g68_seq_word[1], *q2 = s->g68_seq_word[2];
        double x[3], y[3], z[3], len, along;

        for (int i = 0; i < 3; i++) { z[i] = q2[3 + i]; x[i] = q1[3 + i]; }
        len = vec_norm(z);
        CHKS((len < 1e-12), _("G68.2 P3: the Z direction is a zero vector"));
        for (int i = 0; i < 3; i++) { z[i] /= len; }
        len = vec_norm(x);
        CHKS((len < 1e-12), _("G68.2 P3: the X direction is a zero vector"));
        along = x[0]*z[0] + x[1]*z[1] + x[2]*z[2];
        for (int i = 0; i < 3; i++) { x[i] -= along * z[i]; }
        CHKS((vec_norm(x) < 1e-6 * len), _("G68.2 P3: the X direction lies along the Z direction"));
        len = vec_norm(x);
        for (int i = 0; i < 3; i++) { x[i] /= len; }
        vec_cross(z, x, y);
        mat_from_axes(x, y, z, rotation);
        for (int i = 0; i < 3; i++) { origin[i] = q1[i]; }
        r = q1[6];
    }
    mat_rotation(3, r, rz);
    mat_mul(rotation, rz, rotation);
    *complete = 1;
    return INTERP_OK;
}

// G68.2, G68.4 and G69 from convert_g
int Interp::convert_work_plane(int g_code, block_pointer block, setup_pointer s)
{
    double origin[3], rotation[3][3];
    int complete;

    if (g_code == G_69) {
        CHKS((s->cutter_comp_side != CUTTER_COMP::OFF),
             _("Cannot cancel a tilted work plane with cutter radius compensation on"));
        return work_plane_cancel(s, true);
    }

    CHKS((g_code != G_68_2 && g_code != G_68_4), "BUG: code not G68.2, G68.4 or G69");
    CHKS((s->cutter_comp_side != CUTTER_COMP::OFF),
         _("Cannot define a tilted work plane with cutter radius compensation on"));
    CHKS((g_code == G_68_4 && !s->g68_active),
         _("G68.4 needs an active tilted work plane to build on"));

    CHP(work_plane_build(block, s, origin, rotation, &complete));
    if (!complete) { return INTERP_OK; }

    if (g_code == G_68_4) {
        // composed onto the active plane: the new origin is a point of the
        // old plane and the new rotation follows the old one
        double ox = origin[0], oy = origin[1], oz = origin[2];

        g68_apply(s, &ox, &oy, &oz);
        origin[0] = ox;
        origin[1] = oy;
        origin[2] = oz;
        mat_mul(s->g68_rotation, rotation, rotation);
    }
    return work_plane_set(s, g_code, origin, rotation);
}
