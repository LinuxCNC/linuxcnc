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
    if (g_code == G_68_3) {
        CHKS((s->g68_seq_code != 0), _("G68.3 cannot interrupt a G68.2 sequence"));
        return convert_work_plane_from_tool(block, s);
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

//----------------------------------------------------------------------
// The kinematics.  G68.3 and the orientation moves need the frames and
// the tool frame inverse of the module motion runs, evaluated here, ahead
// of motion, through the loader in kinematics_userspace/.  The loader
// binds its pins to a HAL component, so the interpreter makes one, named
// by its process, the first time it is asked.
//----------------------------------------------------------------------

#include <unistd.h>
#include <posemath.h>
#include "units.h"
// kinematics.h has no linkage guards of its own and posemath.h cannot take
// them, so the latter goes first
extern "C" {
#include <hal.h>
#include <kinematics_user.h>
}

#define KINS_CTX(s) ((KinematicsUserContext *)(s)->kins_ctx)

// the loaded module, on the kinematics type the program is in
int Interp::kins_context(setup_pointer s, void **out)
{
    KinematicsUserContext *ctx;

    *out = NULL;
    if (!s->kins_ctx) {
        char name[HAL_NAME_LEN + 1];
        int comp;

        CHKS((!s->kins_module[0]),
             _("the INI file names no [KINS] KINEMATICS, so the kinematics cannot be evaluated here"));
        CHKS((s->kins_joints < 1),
             _("the INI file gives no [KINS] JOINTS, so the kinematics cannot be evaluated here"));
        snprintf(name, sizeof(name), "interp.%d", (int)getpid());
        comp = hal_init(name);
        CHKS((comp < 0),
             _("cannot connect to HAL to evaluate the kinematics (is realtime running?)"));
        s->kins_comp_id = comp;
        ctx = kinematicsUserInitString(s->kins_module, s->kins_joints, comp, name);
        hal_ready(comp);
        CHKS((!ctx), _("kinematics module %s cannot be loaded here"), s->kins_module);
        s->kins_ctx = ctx;
        for (int i = 0; i < EMCMOT_MAX_JOINTS; i++) { s->kins_seed[i] = 0.0; }
    }
    ctx = KINS_CTX(s);
    CHKS((kinematicsUserIsRtOnly(ctx)),
         _("kinematics module %s cannot be evaluated outside realtime"), s->kins_module);
    CHKS((kinematicsUserSetType(ctx, s->kins_type) != 0),
         _("kinematics type %d is not available outside realtime"), s->kins_type);
    *out = ctx;
    return INTERP_OK;
}

void Interp::kins_release(setup_pointer s)
{
    if (s->kins_ctx) {
        kinematicsUserFree(KINS_CTX(s));
        s->kins_ctx = NULL;
    }
    if (s->kins_comp_id > 0) {
        hal_exit(s->kins_comp_id);
        s->kins_comp_id = 0;
    }
}

// the current point as the machine sees it: the absolute frame, in the
// machine's units, which is what the kinematics works in
void Interp::current_machine_pose(setup_pointer s, EmcPose *pose)
{
    double abs_pos[9];

    get_abs_position(s, abs_pos);
    pose->tran.x = PROGRAM_TO_USER_LEN(abs_pos[0]);
    pose->tran.y = PROGRAM_TO_USER_LEN(abs_pos[1]);
    pose->tran.z = PROGRAM_TO_USER_LEN(abs_pos[2]);
    pose->a = PROGRAM_TO_USER_ANG(abs_pos[3]);
    pose->b = PROGRAM_TO_USER_ANG(abs_pos[4]);
    pose->c = PROGRAM_TO_USER_ANG(abs_pos[5]);
    pose->u = PROGRAM_TO_USER_LEN(abs_pos[6]);
    pose->v = PROGRAM_TO_USER_LEN(abs_pos[7]);
    pose->w = PROGRAM_TO_USER_LEN(abs_pos[8]);
}

// and back: a machine pose as program coordinates, through the chain
void Interp::machine_pose_to_program(setup_pointer s, const EmcPose *pose, double prog[9])
{
    world_to_program_xyz(s,
                         USER_TO_PROGRAM_LEN(pose->tran.x),
                         USER_TO_PROGRAM_LEN(pose->tran.y),
                         USER_TO_PROGRAM_LEN(pose->tran.z),
                         &prog[0], &prog[1], &prog[2]);
    prog[3] = USER_TO_PROGRAM_ANG(pose->a) - s->tool_offset.a - s->AA_origin_offset - s->AA_axis_offset;
    prog[4] = USER_TO_PROGRAM_ANG(pose->b) - s->tool_offset.b - s->BB_origin_offset - s->BB_axis_offset;
    prog[5] = USER_TO_PROGRAM_ANG(pose->c) - s->tool_offset.c - s->CC_origin_offset - s->CC_axis_offset;
    prog[6] = USER_TO_PROGRAM_LEN(pose->u) - s->tool_offset.u - s->u_origin_offset - s->u_axis_offset;
    prog[7] = USER_TO_PROGRAM_LEN(pose->v) - s->tool_offset.v - s->v_origin_offset - s->v_axis_offset;
    prog[8] = USER_TO_PROGRAM_LEN(pose->w) - s->tool_offset.w - s->w_origin_offset - s->w_axis_offset;
}

// the joints the machine is at, as far as the interpreter can know ahead
// of motion: the inverse of its current point, seeded with the last answer.
// Some modules read the joints they are handed (a nutating head takes its
// rotary angles from them), so one pass from a stale seed answers for the
// wrong angles; running again from its own answer settles it.
int Interp::current_joints(setup_pointer s, void *vctx, double *joints)
{
    KinematicsUserContext *ctx = (KinematicsUserContext *)vctx;
    EmcPose pose;
    int pass, i;

    current_machine_pose(s, &pose);
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { joints[i] = s->kins_seed[i]; }
    for (pass = 0; pass < 8; pass++) {
        double prev[EMCMOT_MAX_JOINTS], worst = 0.0;
        for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { prev[i] = joints[i]; }
        CHKS((kinematicsUserInverse(ctx, &pose, joints) != 0),
             _("the kinematics cannot invert the current position"));
        for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { worst = fmax(worst, fabs(joints[i] - prev[i])); }
        if (worst < 1e-9) { break; }
    }
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { s->kins_seed[i] = joints[i]; }
    return INTERP_OK;
}

// a direction of the plane in world coordinates: the plane's rotation
// then the XY rotation of the coordinate system it sits on
static void plane_axis_in_world(setup_pointer s, int column, double rotation_xy, PmCartesian *out)
{
    double x = s->g68_rotation[0][column];
    double y = s->g68_rotation[1][column];
    double z = s->g68_rotation[2][column];
    double t = rotation_xy * M_PI / 180.0;

    out->x = x * cos(t) - y * sin(t);
    out->y = x * sin(t) + y * cos(t);
    out->z = z;
}

static void rotate_about(const PmCartesian *axis, double rad, PmCartesian *v)
{
    // Rodrigues, for a unit axis
    PmCartesian c;
    double d = axis->x*v->x + axis->y*v->y + axis->z*v->z;

    pmCartCartCross(axis, v, &c);
    v->x = v->x*cos(rad) + c.x*sin(rad) + axis->x*d*(1 - cos(rad));
    v->y = v->y*cos(rad) + c.y*sin(rad) + axis->y*d*(1 - cos(rad));
    v->z = v->z*cos(rad) + c.z*sin(rad) + axis->z*d*(1 - cos(rad));
}

// G68.3: the plane from the tool.  Z is the tool axis as the joints have
// it now; X is the default tool X of the conventions chapter, tool x
// turned about the tool axis by the smaller angle that makes it parallel
// to the machine XY plane, and machine X when the tool is vertical; R
// turns the plane from there.  X Y Z are the origin, in the coordinate
// system the plane sits on like G68.2's.
int Interp::convert_work_plane_from_tool(block_pointer block, setup_pointer s)
{
    void *vctx;
    KinematicsUserContext *ctx;
    double joints[EMCMOT_MAX_JOINTS];
    PmRotationMatrix work, tool, in_work;
    PmCartesian zt, xt, yt, zm, xm, x, y;
    double origin[3], rotation[3][3], r, t, along;

    CHKS((s->cutter_comp_side != CUTTER_COMP::OFF),
         _("Cannot define a tilted work plane with cutter radius compensation on"));
    CHP(kins_context(s, &vctx));
    ctx = (KinematicsUserContext *)vctx;
    CHKS((kinematicsUserIsIdentity(ctx)),
         _("G68.3 needs a kinematics type that describes the machine; select it with G12.1 first"));
    CHP(current_joints(s, ctx, joints));
    CHKS((kinematicsUserWorkFrame(ctx, joints, &work) != 0
          || kinematicsUserToolFrame(ctx, joints, &tool) != 0),
         _("the kinematics reports no tool frame, so G68.3 cannot read the tool direction"));
    toolFrameInWork(&work, &tool, &in_work);
    zt = in_work.z;
    xt = in_work.x;
    yt = in_work.y;
    // machine Z and X seen from the work: the rows of the work frame
    zm.x = work.x.z; zm.y = work.y.z; zm.z = work.z.z;
    xm.x = work.x.x; xm.y = work.y.x; xm.z = work.z.x;

    pmCartCartCross(&zt, &zm, &x);
    if (sqrt(x.x*x.x + x.y*x.y + x.z*x.z) < 1e-9) {
        // vertical: tool x is machine x, less whatever of it lies along
        // the tool axis, which is rounding
        along = xm.x*zt.x + xm.y*zt.y + xm.z*zt.z;
        x.x = xm.x - along*zt.x; x.y = xm.y - along*zt.y; x.z = xm.z - along*zt.z;
        pmCartUnitEq(&x);
    } else {
        // the turn about the tool axis that takes tool x into the
        // machine XY plane: (cos t x + sin t y) . zm = 0, the root nearer
        // to no turn at all
        double xz = xt.x*zm.x + xt.y*zm.y + xt.z*zm.z;
        double yz = yt.x*zm.x + yt.y*zm.y + yt.z*zm.z;

        t = atan2(-xz, yz);
        if (t > M_PI / 2) { t -= M_PI; }
        if (t < -M_PI / 2) { t += M_PI; }
        x = xt;
        rotate_about(&zt, t, &x);
    }
    r = block->r_flag ? block->r_number : 0.0;
    rotate_about(&zt, r * M_PI / 180.0, &x);
    pmCartCartCross(&zt, &x, &y);

    // from world directions to the system the plane is defined in: the
    // XY rotation comes off
    {
        PmCartesian cols[3] = { x, y, zt };
        double c = cos(-s->rotation_xy * M_PI / 180.0), sn = sin(-s->rotation_xy * M_PI / 180.0);

        for (int j = 0; j < 3; j++) {
            rotation[0][j] = cols[j].x * c - cols[j].y * sn;
            rotation[1][j] = cols[j].x * sn + cols[j].y * c;
            rotation[2][j] = cols[j].z;
        }
    }
    origin[0] = block->x_flag ? block->x_number : 0.0;
    origin[1] = block->y_flag ? block->y_number : 0.0;
    origin[2] = block->z_flag ? block->z_number : 0.0;
    return work_plane_set(s, G_68_3, origin, rotation);
}

// G53.1, G53.3 and G53.6: the rotaries to the plane's normal.
//
// P picks the solution, nearest to the present rotary position first; Q
// says whether the joints that carry the work may take part: Q0 holds
// them and lets the head do it, with the residual turn about the tool
// being a rotation the frame already carries (Heidenhain COORD ROT), and
// falls back to everything free when nothing is reachable that way; Q1
// frees them from the start (TABLE ROT).
//
// G53.1 moves the rotaries alone, the linear joints stay where they are,
// interpolated in joint space.  G53.6 keeps the tool centre point where
// it is, a Cartesian move.  G53.3 goes to X Y Z in the plane with the
// tool oriented, interpolated in joint space.
int Interp::convert_orient_tool(int code, block_pointer block, setup_pointer s)
{
    void *vctx;
    KinematicsUserContext *ctx;
    double now[EMCMOT_MAX_JOINTS];
    double solutions[TOOL_FRAME_MAX_SOLUTIONS * EMCMOT_MAX_JOINTS];
    double spin[TOOL_FRAME_MAX_SOLUTIONS];
    double distance[TOOL_FRAME_MAX_SOLUTIONS];
    int order[TOOL_FRAME_MAX_SOLUTIONS], free_dirs[TOOL_FRAME_MAX_SOLUTIONS];
    PmCartesian axis, xdir;
    EmcPose end_pose;
    double end_prog[9];
    unsigned int held = 0;
    int p, q, n, i, j, chosen, njoints;
    const double *sol;
    const char *name = (code == G_53_1) ? "G53.1" : (code == G_53_3) ? "G53.3" : "G53.6";

    CHKS((!s->g68_active), _("%s needs a tilted work plane; define one with G68.2 first"), name);
    CHKS((s->cutter_comp_side != CUTTER_COMP::OFF),
         _("Cannot orient the tool with cutter radius compensation on"));
    p = block->p_flag ? (int)round(block->p_number) : 0;
    CHKS((block->p_flag && (fabs(block->p_number - p) > 1e-9 || p < 0 || p > 2)),
         _("P word with %s must be 0, 1 or 2"), name);
    q = block->q_flag ? (int)round(block->q_number) : 0;
    CHKS((block->q_flag && (fabs(block->q_number - q) > 1e-9 || (q != 0 && q != 1))),
         _("Q word with %s must be 0 or 1"), name);

    CHP(kins_context(s, &vctx));
    ctx = (KinematicsUserContext *)vctx;
    CHKS((kinematicsUserIsIdentity(ctx)),
         _("%s needs a kinematics type that describes the machine; select it with G12.1 first"), name);
    njoints = kinematicsUserGetNumJoints(ctx);
    CHP(current_joints(s, ctx, now));

    plane_axis_in_world(s, 2, s->rotation_xy, &axis);
    plane_axis_in_world(s, 0, s->rotation_xy, &xdir);
    if (q == 0) {
        if (kinematicsUserWorkJoints(ctx, now, &held) != 0) { held = 0; }
    }
    n = kinematicsUserToolFrameInverse(ctx, &axis, &xdir, now, held,
                                       solutions, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
    if (n == 0 && held) {
        // nothing reachable with the work held still: let it move
        held = 0;
        n = kinematicsUserToolFrameInverse(ctx, &axis, &xdir, now, held,
                                           solutions, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
    }
    CHKS((n < 0), _("%s: the kinematics cannot answer the orientation"), name);
    CHKS((n == 0), _("%s: the plane's normal cannot be reached by the rotary joints"), name);

    // nearest first, by rotary travel in joint units
    for (i = 0; i < n; i++) {
        distance[i] = 0.0;
        for (j = 0; j < njoints; j++) { distance[i] += fabs(solutions[i*njoints + j] - now[j]); }
        order[i] = i;
    }
    for (i = 1; i < n; i++) {
        int k = order[i];
        for (j = i; j > 0 && distance[order[j-1]] > distance[k]; j--) { order[j] = order[j-1]; }
        order[j] = k;
    }
    if (p == 0) {
        chosen = order[0];
    } else {
        // P names the pose rather than its rank, so that the same program
        // reaches the same pose from wherever the machine is standing
        int primary, secondary;
        CHKS((kinematicsUserOrientJoints(ctx, now, &primary, &secondary) != 0),
             _("%s P%d: the poses of this machine cannot be told apart by a tilting"
               " joint, so leave P out and take the nearest"), name, p);
        chosen = -1;
        for (i = 0; i < n; i++) {
            double value = solutions[order[i]*njoints + secondary];
            if ((p == 1 && value > 1e-9) || (p == 2 && value < -1e-9)) {
                chosen = order[i];
                break;
            }
        }
        CHKS((chosen < 0), _("%s P%d: no reachable pose has joint %d %s"),
             name, p, secondary, (p == 1) ? "positive" : "negative");
    }
    sol = solutions + chosen * njoints;

    // where that puts the machine, and what the program calls it
    end_pose = (EmcPose){};
    current_machine_pose(s, &end_pose);
    {
        double full[EMCMOT_MAX_JOINTS];
        for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { full[i] = (i < njoints) ? sol[i] : 0.0; }
        CHKS((kinematicsUserForward(ctx, full, &end_pose) != 0),
             _("%s: the kinematics cannot place the orientation it found"), name);
        for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { s->kins_seed[i] = full[i]; }
    }
    machine_pose_to_program(s, &end_pose, end_prog);

    write_canon_state_tag(block, s);
    if (code == G_53_1) {
        // the rotaries alone: the linear joints are where they are, since
        // the solver left them at the seed, and the tool goes wherever
        // that carries it
        JOINT_TRAVERSE(block->line_number, sol, 1,
                       end_prog[0], end_prog[1], end_prog[2],
                       end_prog[3], end_prog[4], end_prog[5],
                       end_prog[6], end_prog[7], end_prog[8]);
        s->current_x = end_prog[0];
        s->current_y = end_prog[1];
        s->current_z = end_prog[2];
    } else if (code == G_53_6) {
        // the tool centre point stays: a Cartesian move of the rotaries
        STRAIGHT_TRAVERSE(block->line_number, s->current_x, s->current_y, s->current_z,
                          end_prog[3], end_prog[4], end_prog[5],
                          s->u_current, s->v_current, s->w_current);
    } else {
        double x = block->x_flag ? block->x_number : s->current_x;
        double y = block->y_flag ? block->y_number : s->current_y;
        double z = block->z_flag ? block->z_number : s->current_z;

        JOINT_TRAVERSE(block->line_number, NULL, 0, x, y, z,
                       end_prog[3], end_prog[4], end_prog[5],
                       s->u_current, s->v_current, s->w_current);
        s->current_x = x;
        s->current_y = y;
        s->current_z = z;
    }
    s->AA_current = end_prog[3];
    s->BB_current = end_prog[4];
    s->CC_current = end_prog[5];
    if (code == G_53_1) {
        s->u_current = end_prog[6];
        s->v_current = end_prog[7];
        s->w_current = end_prog[8];
    }
    return INTERP_OK;
}

// how long a point-to-point feed is to take: the time the same straight
// move would take at the programmed feed
int Interp::ptp_seconds(block_pointer block, setup_pointer s,
                        double x, double y, double z, double a, double b, double c,
                        double u, double v, double w, double *seconds)
{
    if (s->feed_mode == FEED_MODE::INVERSE_TIME) {
        CHKS((block->f_number <= 0.0), _("F must be positive with G93"));
        *seconds = 60.0 / block->f_number;
    } else if (s->feed_mode == FEED_MODE::UNITS_PER_MINUTE) {
        double length = find_straight_length(x, y, z, a, b, c, u, v, w,
                                             s->current_x, s->current_y, s->current_z,
                                             s->AA_current, s->BB_current, s->CC_current,
                                             s->u_current, s->v_current, s->w_current);
        CHKS((length <= 0.0),
             _("a point-to-point feed with no displacement has nothing to apply F to in G94 mode; use G93 or G0"));
        *seconds = 60.0 * length / s->feed_rate;
    } else {
        ERS(_("Cannot use feed per revolution with a point-to-point move"));
    }
    return INTERP_OK;
}

// The two point-to-point codes that name joints rather than a point.  G53.5
// takes axis letters through the module's identity mapping, in program units
// like any other axis word, and refuses a letter whose joints are not the
// kind the letter implies, since on a robot X is the first rotary joint.
// G53.7 takes J<n>=<value>, the joint number and the joint's own position in
// its own units, which every machine can answer.  A joint left out keeps its
// position either way.
int Interp::convert_ptp_joints(int code, int move, block_pointer block, setup_pointer s)
{
    void *vctx;
    KinematicsUserContext *ctx;
    const kins_params *p;
    double joints[EMCMOT_MAX_JOINTS];
    EmcPose pose;
    double prog[9];
    const int flags[9] = { block->x_flag, block->y_flag, block->z_flag,
                           block->a_flag, block->b_flag, block->c_flag,
                           block->u_flag, block->v_flag, block->w_flag };
    const double words[9] = { block->x_number, block->y_number, block->z_number,
                              block->a_number, block->b_number, block->c_number,
                              block->u_number, block->v_number, block->w_number };
    static const char letters[9] = { 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W' };
    const char *name = (code == G_53_5) ? "G53.5" : "G53.7";
    int a, j, njoints, given = 0;

    CHKS((s->cutter_comp_side != CUTTER_COMP::OFF),
         _("Cannot use %s with cutter radius compensation on"), name);
    CHP(kins_context(s, &vctx));
    ctx = (KinematicsUserContext *)vctx;
    njoints = kinematicsUserGetNumJoints(ctx);
    p = kinematicsUserParams(ctx);
    CHP(current_joints(s, ctx, joints));

    if (code == G_53_7) {
        for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
            if (!block->joint_flag[j]) { continue; }
            CHKS((j >= njoints), _("G53.7: this kinematics has no joint %d"), j);
            joints[j] = block->joint_value[j];
            given++;
        }
        CHKS((given == 0), _("G53.7 needs at least one J<n>=<value> joint word"));
    } else {
        CHKS((!p), _("G53.5: the kinematics module gives no joint mapping"));

        // A letter carries a unit class and a joint does not, so the letters
        // are only a way to name joints where the mapping agrees with them.
        // On a serial robot the first joint answers to X and turns in degrees,
        // and the whole machine is refused rather than that one letter, since
        // a mapping that lies about X is telling nothing useful about A.
        for (a = 0; a < 9; a++) {
            const int angular = (a >= 3 && a <= 5);
            for (j = 0; j < njoints; j++) {
                int turns;
                if (!(p->joints_of_axis[a] & (1 << j))) { continue; }
                turns = (s->kins_angular_joints & (1 << j)) ? 1 : 0;
                CHKS((turns != angular),
                     _("G53.5: on this machine %c names joint %d, which the INI file"
                       " declares %s, so the axis letters do not name the joints they"
                       " look like; give joints by number with G53.7 J%d="),
                     letters[a], j, turns ? "angular" : "linear", j);
            }
        }

        for (a = 0; a < 9; a++) {
            const int angular = (a >= 3 && a <= 5);
            double value;
            if (!flags[a]) { continue; }
            CHKS((p->joints_of_axis[a] == 0),
                 _("G53.5: %c is not a joint of this kinematics"), letters[a]);
            value = angular ? PROGRAM_TO_USER_ANG(words[a]) : PROGRAM_TO_USER_LEN(words[a]);
            for (j = 0; j < njoints; j++) {
                if (p->joints_of_axis[a] & (1 << j)) { joints[j] = value; }
            }
            given++;
        }
        CHKS((given == 0), _("G53.5 needs at least one axis word"));
    }

    // the joints of a gantry pair move together: both given, one value
    if (code == G_53_7) {
        for (a = 0; p && a < EMCMOT_MAX_AXIS; a++) {
            int bits = p->joints_of_axis[a];
            int first = -1;
            if (!(bits & (bits - 1))) { continue; }
            for (j = 0; j < njoints; j++) {
                if (!(bits & (1 << j))) { continue; }
                if (first < 0) { first = j; continue; }
                CHKS((block->joint_flag[j] != block->joint_flag[first]),
                     _("G53.7: joints %d and %d are a pair on this kinematics, give both"), first, j);
                CHKS((block->joint_flag[j] && block->joint_value[j] != block->joint_value[first]),
                     _("G53.7: joints %d and %d are a pair on this kinematics, give them one value"), first, j);
            }
        }
    }

    // where that puts the tool, and what the program calls it
    current_machine_pose(s, &pose);
    CHKS((kinematicsUserForward(ctx, joints, &pose) != 0),
         _("%s: the kinematics cannot place those joints"), name);
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) { s->kins_seed[j] = joints[j]; }
    machine_pose_to_program(s, &pose, prog);

    write_canon_state_tag(block, s);
    if (move == G_0) {
        JOINT_TRAVERSE(block->line_number, joints, 1,
                       prog[0], prog[1], prog[2], prog[3], prog[4], prog[5],
                       prog[6], prog[7], prog[8]);
    } else {
        double seconds;
        CHP(ptp_seconds(block, s, prog[0], prog[1], prog[2], prog[3], prog[4], prog[5],
                        prog[6], prog[7], prog[8], &seconds));
        JOINT_FEED(block->line_number, joints, 1,
                   prog[0], prog[1], prog[2], prog[3], prog[4], prog[5],
                   prog[6], prog[7], prog[8], seconds);
    }
    s->current_x = prog[0];
    s->current_y = prog[1];
    s->current_z = prog[2];
    s->AA_current = prog[3];
    s->BB_current = prog[4];
    s->CC_current = prog[5];
    s->u_current = prog[6];
    s->v_current = prog[7];
    s->w_current = prog[8];
    return INTERP_OK;
}
