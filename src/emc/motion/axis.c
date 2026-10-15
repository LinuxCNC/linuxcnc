
#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>   // memset
#include <emcmotcfg.h>      // EMCMOT_MAX_AXIS

#include "axis.h"
#include "simple_tp.h"
#ifndef ULAPI
// motion-logger links axis.c in userspace without homing or kinematics
#include "motion.h"         // emcmot_joint_t (via homing.h), kinematics.h
#include "homing.h"         // get_homed()

// Weak: kinematics modules that do not link kins_util.c (matrixkins,
// out-of-tree modules built on older headers) export no such symbol.
// They never run teleop with an unhomed joint, so the all-homed
// fallback in axis_is_homed() is the right answer for them.
extern int identityKinematicsAxisJointsBitmap(int axis_num) __attribute__((weak));
#endif

typedef struct {
    double pos_cmd;                 /* commanded axis position */
    double teleop_vel_cmd;          /* commanded axis velocity */
    double max_pos_limit;           /* upper soft limit on axis pos */
    double min_pos_limit;           /* lower soft limit on axis pos */
    double vel_limit;               /* upper limit of axis speed */
    double acc_limit;               /* upper limit of axis accel */
    double jerk_limit;	/* upper limit of axis jerk */
    simple_tp_t teleop_tp;          /* planner for teleop mode motion */
    double teleop_vel_req;          /* the speed the jog asked for */
    double teleop_acc_req;          /* and the acceleration it may use */

    rtapi_sint old_ajog_counts;            /* prior value, used for deltas */
    int kb_ajog_active;             /* non-zero during a keyboard jog */
    int wheel_ajog_active;          /* non-zero during a wheel jog */
    int locking_joint;              /* locking_joint number, -1 ==> notused */

    double ext_offset_vel_limit;    /* upper limit of axis speed for ext offset */
    double ext_offset_acc_limit;    /* upper limit of axis accel for ext offset */
    rtapi_sint old_eoffset_counts;
    simple_tp_t ext_offset_tp;      /* planner for external coordinate offsets*/
} emcmot_axis_t;

typedef struct {
    hal_real_t pos_cmd;           /* RPI: commanded position */
    hal_real_t teleop_vel_cmd;    /* RPI: commanded velocity */
    hal_real_t teleop_pos_cmd;    /* RPI: teleop traj planner pos cmd */
    hal_real_t teleop_vel_lim;    /* RPI: teleop traj planner vel limit */
    hal_bool_t teleop_tp_enable;  /* RPI: teleop traj planner is running */

    hal_sint_t ajog_counts;       /* WPI: jogwheel position input */
    hal_bool_t ajog_enable;       /* RPI: enable jogwheel */
    hal_real_t ajog_scale;        /* RPI: distance to jog on each count */
    hal_real_t ajog_accel_fraction;  /* RPI: to limit wheel jog accel */
    hal_bool_t ajog_vel_mode;     /* RPI: true for "velocity mode" jogwheel */
    hal_bool_t kb_ajog_active;    /* RPI: executing keyboard jog */
    hal_bool_t wheel_ajog_active; /* RPI: executing handwheel jog */

    hal_bool_t eoffset_enable;
    hal_bool_t eoffset_clear;
    hal_sint_t eoffset_counts;
    hal_real_t eoffset_scale;
    hal_real_t external_offset;
    hal_real_t external_offset_requested;
} axis_hal_t;


typedef struct {
    axis_hal_t axis[EMCMOT_MAX_AXIS];   /* data for each axis */
} axis_hal_data_t;

static emcmot_axis_t axis_array[EMCMOT_MAX_AXIS];
static axis_hal_data_t *hal_data = NULL;

/* A world jog of X, Y and Z along the axes of a frame turned against the
   world, the work plane or the tool.  A planner per frame axis plans the
   jog along it; each cycle its step is turned into the world and added to
   the X, Y and Z teleop planners, position and command alike, so those
   always hold where the machine is.  One frame axis moves at a time, and a
   jog of another waits until the one under way has stopped: two at once
   would move each world axis by their sum, past what the limits along
   either allow, and into the travel along neither one's way.  The rotation
   is taken when a frame jog starts and kept until the planners are at
   rest: a tool frame turning with the rotaries would bend the jog under
   way.  A frame axis gets the limits of the world axes over the share of
   the jog each takes. */
typedef struct {
    int selected;                   /* X Y Z jogs go to the frame */
    double rot_now[3][3];           /* the frame as last reported, columns
                                       its axes in world coordinates */
    double rot[3][3];               /* the frame the planners run in */
    int latched;                    /* rot is in use */
    int owner;                      /* the planner that moves, -1 none */
    unsigned int asked;             /* counts the jogs asked for */
    simple_tp_t tp[3];              /* planners along the frame's axes */
    unsigned int order[3];          /* when each was asked */
    int cont[3];                    /* a continuous jog, to the travel */
    double vel_req[3];              /* the speed the jog asked for */
    double acc_req[3];              /* and the acceleration it may use */
    double vel_cap[3];              /* the most the joints allow of them */
    double acc_cap[3];
    double vel_limit[3];            /* the world axes' limits along the */
    double acc_limit[3];            /* frame axis */
    double jerk_limit[3];
    double min_lim[3];              /* the travel of X, Y and Z, taken */
    double max_lim[3];              /* with the frame */
} jog_frame_t;

static jog_frame_t frame;


// Mark strings for translation, but defer translation to userspace
#define _(s) (s)

void axis_init_all(void)
{
    int n;
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        emcmot_axis_t *axis = &axis_array[n];
        axis->locking_joint = -1;
    }
    for (n = 0; n < 3; n++) {
        frame.rot_now[n][n] = 1.0;
        frame.rot[n][n] = 1.0;
    }
}

void axis_initialize_external_offsets(void)
{
    int n;
    axis_hal_t *axis_data;

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis_data = &hal_data->axis[n];

        hal_set_real(axis_data->external_offset, 0);
        hal_set_real(axis_data->external_offset_requested, 0);
        axis_array[n].ext_offset_tp.pos_cmd  = 0;
        axis_array[n].ext_offset_tp.curr_pos = 0;
        axis_array[n].ext_offset_tp.curr_vel = 0;
    }
}

#define CALL_CHECK(expr) do {           \
        int _retval;                    \
        _retval = expr;                 \
        if (_retval) return _retval;    \
    } while (0);

static int export_axis(int mot_comp_id, char c, axis_hal_t * addr)
{
    int msg;

    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_IN, &(addr->ajog_enable), 0, "axis.%c.jog-enable", c));
    CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_IN, &(addr->ajog_scale), 0.0, "axis.%c.jog-scale", c));
    CALL_CHECK(hal_pin_new_sint(mot_comp_id, HAL_IN, &(addr->ajog_counts), 0, "axis.%c.jog-counts", c));
    CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_IN, &(addr->ajog_vel_mode), 0, "axis.%c.jog-vel-mode", c));
    CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_OUT, &(addr->kb_ajog_active), 0, "axis.%c.kb-jog-active", c));
    CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_OUT, &(addr->wheel_ajog_active), 0, "axis.%c.wheel-jog-active", c));

    // init: 1.0  fraction of accel for wheel ajogs
    CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_IN, &(addr->ajog_accel_fraction), 1.0, "axis.%c.jog-accel-fraction", c));

    rtapi_set_msg_level(msg);
    return 0;
}

int axis_init_hal_io(int mot_comp_id)
{
    int n, retval;

    hal_data = hal_malloc(sizeof(axis_hal_data_t));
    if (!hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, _("MOTION: axis_hal_data hal_malloc() failed\n"));
        return -1;
    }

    // export axis pins and parameters
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        char c = "xyzabcuvw"[n];
        axis_hal_t *axis_data = &(hal_data->axis[n]);
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->pos_cmd, 0.0, "axis.%c.pos-cmd", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->teleop_vel_cmd, 0.0, "axis.%c.teleop-vel-cmd", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->teleop_pos_cmd, 0.0, "axis.%c.teleop-pos-cmd", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->teleop_vel_lim, 0.0, "axis.%c.teleop-vel-lim", c));
        CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_OUT, &axis_data->teleop_tp_enable, 0, "axis.%c.teleop-tp-enable",c));
        CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_IN, &axis_data->eoffset_enable, 0, "axis.%c.eoffset-enable", c));
        CALL_CHECK(hal_pin_new_bool(mot_comp_id, HAL_IN, &axis_data->eoffset_clear, 0, "axis.%c.eoffset-clear", c));
        CALL_CHECK(hal_pin_new_sint(mot_comp_id, HAL_IN, &axis_data->eoffset_counts, 0, "axis.%c.eoffset-counts", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_IN, &axis_data->eoffset_scale, 0.0, "axis.%c.eoffset-scale", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->external_offset, 0.0, "axis.%c.eoffset", c));
        CALL_CHECK(hal_pin_new_real(mot_comp_id, HAL_OUT, &axis_data->external_offset_requested,
           0.0, "axis.%c.eoffset-request", c));

        retval = export_axis(mot_comp_id, c, axis_data);
        if (retval) {
            rtapi_print_msg(RTAPI_MSG_ERR, _("MOTION: axis %c pin/param export failed\n"), c);
            return -1;
        }
    }

    return 0;
}

void axis_output_to_hal(double *pcmd_p[])
{
    int n;

    // output axis info to HAL for scoping, etc
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        emcmot_axis_t *axis = &axis_array[n];
        axis_hal_t *axis_data = &hal_data->axis[n];
        hal_set_real(axis_data->teleop_vel_cmd,    axis->teleop_vel_cmd);
        hal_set_real(axis_data->teleop_pos_cmd,    axis->teleop_tp.pos_cmd);
        hal_set_real(axis_data->teleop_vel_lim,    axis->teleop_tp.max_vel);
        hal_set_bool(axis_data->teleop_tp_enable,  axis->teleop_tp.enable
                                                   || (n < 3 && frame.tp[n].enable));
        hal_set_bool(axis_data->kb_ajog_active,    axis->kb_ajog_active);
        hal_set_bool(axis_data->wheel_ajog_active, axis->wheel_ajog_active);

        // hal pins: axis.L.pos-cmd reported without applied offsets:
        hal_set_real(axis_data->pos_cmd, *pcmd_p[n]
                              - axis->ext_offset_tp.curr_pos);
     }
}

void axis_set_max_pos_limit(int axis_num, double maxLimit)
{
    axis_array[axis_num].max_pos_limit = maxLimit;
}

void axis_set_min_pos_limit(int axis_num, double minLimit)
{
    axis_array[axis_num].min_pos_limit = minLimit;
}

void axis_set_vel_limit(int axis_num, double vel)
{
    axis_array[axis_num].vel_limit = vel;
}

void axis_set_acc_limit(int axis_num, double acc)
{
    axis_array[axis_num].acc_limit = acc;
}

void axis_set_jerk_limit(int axis_num, double jerk)
{
    axis_array[axis_num].jerk_limit = jerk;
}

void axis_set_ext_offset_vel_limit(int axis_num, double vel)
{
    axis_array[axis_num].ext_offset_vel_limit = vel;
}

void axis_set_ext_offset_acc_limit(int axis_num, double acc)
{
    axis_array[axis_num].ext_offset_acc_limit = acc;
}

void axis_set_locking_joint(int axis_num, int joint)
{
    axis_array[axis_num].locking_joint = joint;
}


double axis_get_min_pos_limit(int axis_num)
{
    return axis_array[axis_num].min_pos_limit;
}

double axis_get_max_pos_limit(int axis_num)
{
    return axis_array[axis_num].max_pos_limit;
}

double axis_get_vel_limit(int axis_num)
{
    return axis_array[axis_num].vel_limit;
}

double axis_get_acc_limit(int axis_num)
{
    return axis_array[axis_num].acc_limit;
}

double axis_get_teleop_vel_cmd(int axis_num)
{
    return axis_array[axis_num].teleop_vel_cmd;
}

int axis_get_locking_joint(int axis_num)
{
    return axis_array[axis_num].locking_joint;
}

double axis_get_compound_velocity(void)
{
    double v2 = 0.0;
    int n;

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        emcmot_axis_t *axis = &axis_array[n];
        if (axis->teleop_tp.active || (n < 3 && frame.latched)) {
            v2 += axis->teleop_vel_cmd * axis->teleop_vel_cmd;
        }
    }

    if (v2 > 0.0)
        return sqrt(v2);
    return 0.0;
}

double axis_get_ext_offset_curr_pos(int axis_num)
{
    return axis_array[axis_num].ext_offset_tp.curr_pos;
}

/* True when the axis may use its INI soft limits.  Axes are not homed,
** joints are: an axis is homed when every joint mapped to its coordinate
** letter is homed.  Only identity kinematics can be in teleop mode with
** an unhomed joint (see EMCMOT_COORD handling in command.c), and those
** modules map letters to joints, so ask the kinematics for the map.
** Modules without a map report 0 and the answer is all-homed, which
** teleop mode guarantees for them. */
static bool axis_is_homed(int axis_num)
{
#ifdef ULAPI
    // motion-logger never jogs; treat every axis as homed
    (void)axis_num;
    return 1;
#else
    int bitmap, jno;

    if (!identityKinematicsAxisJointsBitmap) { return get_allhomed(); }
    bitmap = identityKinematicsAxisJointsBitmap(axis_num);
    if (!bitmap) { return get_allhomed(); }  /* no mapping: fall back */
    for (jno = 0; jno < EMCMOT_MAX_JOINTS; jno++) {
        if ((bitmap & (1 << jno)) && !get_homed(jno)) { return 0; }
    }
    return 1;
#endif
}

/* Jog limits for an axis.  A homed axis jogs within its INI soft limits.
** An unhomed axis has no known machine position, so it jogs within one
** limit range of its current position, the same rule joint jogs use in
** refresh_jog_limits() (command.c). */
static void axis_jog_limits(int axis_num, double *min_lim, double *max_lim)
{
    emcmot_axis_t *axis = &axis_array[axis_num];

    if (axis_is_homed(axis_num)) {
        *min_lim = axis->min_pos_limit;
        *max_lim = axis->max_pos_limit;
    } else {
        double range = axis->max_pos_limit - axis->min_pos_limit;
        *min_lim = axis->teleop_tp.curr_pos - range;
        *max_lim = axis->teleop_tp.curr_pos + range;
    }
}

// The frame for jogs of X, Y and Z, its axes in world coordinates as the
// columns of rot, row major; selected 0 jogs them along the world axes
void axis_set_jog_frame(int selected, const double rot[9])
{
    int i, k;

    frame.selected = selected;
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 3; k++) { frame.rot_now[i][k] = rot[3 * i + k]; }
    }
}

// Whether frame planner k moves, or has been asked to
static int frame_busy(int k);

// Whether any teleop planner moves, a frame jog's included
bool axis_jog_is_moving(void)
{
    int n;

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        if (axis_array[n].teleop_tp.active || axis_array[n].teleop_tp.curr_vel != 0.0) {
            return 1;
        }
    }
    return frame.latched;
}

// Whether a planner has somewhere to go this cycle, and which way
static int planner_request(const simple_tp_t *tp, double *dir)
{
    double togo = tp->pos_cmd - tp->curr_pos;

    if (!tp->enable) { return 0; }
    if (fabs(togo) < TINY_DP(tp->max_acc, 0.001) && tp->curr_vel == 0.0) { return 0; }
    *dir = togo > 0.0 ? 1.0 : togo < 0.0 ? -1.0 : tp->curr_vel > 0.0 ? 1.0 : -1.0;
    return 1;
}

static int frame_busy(int k)
{
    double dir;

    return frame.tp[k].active || frame.tp[k].curr_vel != 0.0
        || planner_request(&frame.tp[k], &dir);
}

// Where world axis n is commanded, external offset included
static double world_pos(int n)
{
    return axis_array[n].teleop_tp.curr_pos + axis_array[n].ext_offset_tp.curr_pos;
}

// Take the frame for a jog about to start, unless one is under way
static void frame_latch(void)
{
    int i, k;

    if (frame.latched) { return; }
    for (k = 0; k < 3; k++) {
        double vel = 1e99, acc = 1e99, jerk = 1e99;
        for (i = 0; i < 3; i++) {
            double u = frame.rot_now[i][k];
            // a rounding error is no share of the jog, and would hold an
            // axis standing on its limit
            if (fabs(u) < 1e-12) { u = 0.0; }
            frame.rot[i][k] = u;
            u = fabs(u);
            if (u == 0.0) { continue; }
            if (axis_array[i].vel_limit / u < vel) { vel = axis_array[i].vel_limit / u; }
            if (axis_array[i].acc_limit / u < acc) { acc = axis_array[i].acc_limit / u; }
            if (axis_array[i].jerk_limit / u < jerk) { jerk = axis_array[i].jerk_limit / u; }
        }
        frame.vel_limit[k] = vel;
        frame.acc_limit[k] = acc;
        frame.jerk_limit[k] = jerk;
    }
    // an axis not homed jogs within one range of where it stands, as a
    // plain jog of it does
    for (i = 0; i < 3; i++) { axis_jog_limits(i, &frame.min_lim[i], &frame.max_lim[i]); }
    memset(frame.tp, 0, sizeof(frame.tp));
    memset(frame.cont, 0, sizeof(frame.cont));
    frame.owner = -1;
    frame.asked = 0;
    frame.latched = 1;
}

// Stop the frame planners where they are and let the frame go
static void frame_release(void)
{
    memset(frame.tp, 0, sizeof(frame.tp));
    memset(frame.cont, 0, sizeof(frame.cont));
    frame.owner = -1;
    frame.latched = 0;
}

// How far frame axis k may go, s the way, before the jog leaves the box
static double frame_reach(int k, double s)
{
    double reach = 1e99;
    int i;

    for (i = 0; i < 3; i++) {
        double u = s * frame.rot[i][k], room;
        if (u == 0.0) { continue; }
        room = ((u > 0.0 ? frame.max_lim[i] : frame.min_lim[i])
                - world_pos(i)) / u;
        if (room < reach) { reach = room; }
    }
    return reach > 0.0 ? reach : 0.0;
}

// The planner that moves: the one under way until it has stopped, then the
// one asked for first of those waiting.  A continuous jog runs to the box
// from where it starts
static int frame_owner(void)
{
    int k, next = -1;

    if (frame.owner >= 0 && frame_busy(frame.owner)) { return frame.owner; }
    for (k = 0; k < 3; k++) {
        if (frame_busy(k) && (next < 0 || frame.order[k] < frame.order[next])) { next = k; }
    }
    if (next >= 0 && frame.cont[next]) {
        simple_tp_t *tp = &frame.tp[next];
        double s = tp->pos_cmd > tp->curr_pos ? 1.0 : -1.0;
        tp->pos_cmd = tp->curr_pos + s * frame_reach(next, s);
    }
    frame.owner = next;
    return next;
}

// Whether the box holds the frame jogs once frame axis k is sent to target,
// each jog waiting its turn: where each ends, in the order they were asked
// for, inside it or no further out than now.  A continuous jog waiting is
// left out, it stops at the box from wherever it starts
static int frame_ends_inside(int k, double target)
{
    double now[3], end[3];
    int i, j, done[3] = {0, 0, 0};

    for (i = 0; i < 3; i++) { now[i] = end[i] = world_pos(i); }
    for (;;) {
        int next = -1;
        double to;
        for (j = 0; j < 3; j++) {
            if (done[j] || (j != k && (!frame_busy(j) || frame.cont[j]))) { continue; }
            if (next < 0 || (j != k && (next == k || frame.order[j] < frame.order[next]))) {
                next = j;
            }
        }
        if (next < 0) { return 1; }
        done[next] = 1;
        to = next == k ? target : frame.tp[next].pos_cmd;
        for (i = 0; i < 3; i++) {
            end[i] += frame.rot[i][next] * (to - frame.tp[next].curr_pos);
            if (end[i] > frame.max_lim[i] && end[i] > now[i]) { return 0; }
            if (end[i] < frame.min_lim[i] && end[i] < now[i]) { return 0; }
        }
    }
}

// A jog of frame axis k under way, or one waiting its turn, keeps its
// place; a new one goes after those asked for before it
static void frame_start(int k, int queued, double vel, double acc)
{
    simple_tp_t *tp = &frame.tp[k];

    if (!queued) { frame.order[k] = ++frame.asked; }
    frame.vel_req[k] = vel < frame.vel_limit[k] ? vel : frame.vel_limit[k];
    frame.acc_req[k] = acc;
    frame.vel_cap[k] = frame.vel_req[k];
    frame.acc_cap[k] = frame.acc_req[k];
    tp->max_vel = frame.vel_req[k];
    tp->max_acc = frame.acc_req[k];
    tp->enable = 1;
}

static void frame_jog_cont(int k, double vel)
{
    double s = vel > 0.0 ? 1.0 : -1.0;
    simple_tp_t *tp = &frame.tp[k];
    int queued;

    frame_latch();
    queued = frame_busy(k);
    frame.cont[k] = 1;
    // how far is read from the box when the jog gets to move
    tp->pos_cmd = tp->curr_pos + s * (frame.owner == k ? frame_reach(k, s) : 1.0);
    frame_start(k, queued, fabs(vel), frame.acc_limit[k]);
    axis_array[k].kb_ajog_active = 1;
}

static void frame_jog_incr(int k, double offset, double vel)
{
    simple_tp_t *tp = &frame.tp[k];
    double target;
    int queued;

    frame_latch();
    queued = frame_busy(k);
    target = (frame.cont[k] ? tp->curr_pos : tp->pos_cmd) + (vel > 0.0 ? offset : -offset);
    if (!frame_ends_inside(k, target)) { return; }
    frame.cont[k] = 0;
    tp->pos_cmd = target;
    frame_start(k, queued, fabs(vel), frame.acc_limit[k]);
    axis_array[k].kb_ajog_active = 1;
}

// A jog wheel's counts on X, Y or Z, as axis_handle_jogwheels() takes them
// in the world
static int frame_jog_wheel(int k, double distance, double fraction, bool vel_mode)
{
    simple_tp_t *tp = &frame.tp[k];
    double pos, acc;
    int queued;

    frame_latch();
    queued = frame_busy(k);
    acc = fraction * frame.acc_limit[k];
    pos = (frame.cont[k] ? tp->curr_pos : tp->pos_cmd) + distance;
    if (vel_mode) {
        double v = frame.vel_limit[k];
        double stop_dist = v * v / (2 * acc);
        if (pos > tp->curr_pos + stop_dist) {
            pos = tp->curr_pos + stop_dist;
        } else if (pos < tp->curr_pos - stop_dist) {
            pos = tp->curr_pos - stop_dist;
        }
    }
    if (!frame_ends_inside(k, pos)) { return 0; }
    frame.cont[k] = 0;
    tp->pos_cmd = pos;
    frame_start(k, queued, frame.vel_limit[k], acc);
    axis_array[k].wheel_ajog_active = 1;
    return 1;
}

// Run the frame planner whose turn it is and carry its step into the world
// planners; 1 if the step would have taken an axis out of its box and was
// held back
static int frame_step(double period)
{
    simple_tp_t *tp;
    double was, step[3];
    int i, k, out = 0;

    if (!frame.latched) { return 0; }
    k = frame_owner();
    if (k < 0) {
        // all at rest: the next frame jog takes the frame afresh
        frame_release();
        return 0;
    }
    tp = &frame.tp[k];
    tp->max_vel = frame.vel_cap[k] < frame.vel_req[k] ? frame.vel_cap[k] : frame.vel_req[k];
    tp->max_acc = frame.acc_cap[k] < frame.acc_req[k] ? frame.acc_cap[k] : frame.acc_req[k];
    tp->max_jerk = frame.jerk_limit[k];
    was = tp->curr_pos;
    simple_tp_update(tp, period);
    for (i = 0; i < 3; i++) {
        double to;
        step[i] = frame.rot[i][k] * (tp->curr_pos - was);
        to = world_pos(i) + step[i];
        if ((step[i] > 0.0 && to >= frame.max_lim[i])
            || (step[i] < 0.0 && to <= frame.min_lim[i])) {
            out = 1;
        }
    }
    if (out) {
        // the jog ends where it is
        tp->curr_pos = tp->pos_cmd = was;
        tp->curr_vel = tp->curr_acc = 0.0;
        tp->active = 0;
        return 1;
    }
    for (i = 0; i < 3; i++) {
        axis_array[i].teleop_tp.curr_pos += step[i];
        axis_array[i].teleop_tp.pos_cmd += step[i];
    }
    return 0;
}

// The velocity the frame planners give world axis n
static double frame_vel(int n)
{
    double v = 0.0;
    int k;

    if (n >= 3 || !frame.latched) { return 0.0; }
    for (k = 0; k < 3; k++) { v += frame.rot[n][k] * frame.tp[k].curr_vel; }
    return v;
}

void axis_jog_cont(int axis_num, double vel, long servo_period)
{
    (void)servo_period;
    emcmot_axis_t *axis = &axis_array[axis_num];
    double min_lim, max_lim;

    if (frame.selected && axis_num < 3) {
        frame_jog_cont(axis_num, vel);
        return;
    }
    axis_jog_limits(axis_num, &min_lim, &max_lim);
    if (vel > 0.0) {
        axis->teleop_tp.pos_cmd = max_lim;
    } else {
        axis->teleop_tp.pos_cmd = min_lim;
    }

    axis->teleop_tp.max_vel = fabs(vel);
    axis->teleop_tp.max_acc = axis->acc_limit;
    axis->teleop_vel_req = axis->teleop_tp.max_vel;
    axis->teleop_acc_req = axis->teleop_tp.max_acc;
    axis->kb_ajog_active = 1;
    axis->teleop_tp.enable = 1;
}

void axis_jog_incr(int axis_num, double offset, double vel, long servo_period)
{
    (void)servo_period;
    emcmot_axis_t *axis = &axis_array[axis_num];
    double tmp1, min_lim, max_lim;

    if (frame.selected && axis_num < 3) {
        frame_jog_incr(axis_num, offset, vel);
        return;
    }
    if (vel > 0.0) {
        tmp1 = axis->teleop_tp.pos_cmd + offset;
    } else {
        tmp1 = axis->teleop_tp.pos_cmd - offset;
    }

    axis_jog_limits(axis_num, &min_lim, &max_lim);
    if (tmp1 > max_lim) { return; }
    if (tmp1 < min_lim) { return; }

    axis->teleop_tp.pos_cmd = tmp1;
    axis->teleop_tp.max_vel = fabs(vel);
    axis->teleop_tp.max_acc = axis->acc_limit;
    axis->teleop_vel_req = axis->teleop_tp.max_vel;
    axis->teleop_acc_req = axis->teleop_tp.max_acc;
    axis->kb_ajog_active = 1;
    axis->teleop_tp.enable = 1;
}

void axis_jog_abs(int axis_num, double offset, double vel)
{
    emcmot_axis_t *axis = &axis_array[axis_num];
    double tmp1, min_lim, max_lim;

    axis->kb_ajog_active = 1;
    if (axis->wheel_ajog_active) { return; }
    if (vel > 0.0) {
        tmp1 = axis->teleop_tp.pos_cmd + offset;
    } else {
        tmp1 = axis->teleop_tp.pos_cmd - offset;
    }
    axis_jog_limits(axis_num, &min_lim, &max_lim);
    if (tmp1 > max_lim) { return; }
    if (tmp1 < min_lim) { return; }
    axis->teleop_tp.pos_cmd = tmp1;
    axis->teleop_tp.max_vel = fabs(vel);
    axis->teleop_tp.max_acc = axis->acc_limit;
    axis->teleop_vel_req = axis->teleop_tp.max_vel;
    axis->teleop_acc_req = axis->teleop_tp.max_acc;
    axis->kb_ajog_active = 1;
    axis->teleop_tp.enable = 1;
}

bool axis_jog_abort(int axis_num, bool immediate)
{
    bool aborted = 0;
    emcmot_axis_t *axis = &axis_array[axis_num];
    if (axis->teleop_tp.enable) {
        aborted = 1;
    }
    axis->teleop_tp.enable = 0;
    axis->kb_ajog_active = 0;
    axis->wheel_ajog_active = 0;
    if (immediate) {
        axis->teleop_tp.curr_vel = 0.0;
    }
    if (axis_num < 3) {
        simple_tp_t *tp = &frame.tp[axis_num];
        if (tp->enable) {
            aborted = 1;
        }
        tp->enable = 0;
        if (immediate) {
            tp->curr_vel = 0.0;
        }
    }
    return aborted;
}

bool axis_jog_abort_all(bool immediate)
{
    int n;
    bool aborted = 0;
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        if (axis_jog_abort(n, immediate)) {aborted = 1;}
    }
    return aborted;
}

bool axis_jog_is_active(void)
{
    int n;
    emcmot_axis_t *axis;
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis = &axis_array[n];
        if (axis->kb_ajog_active || axis->wheel_ajog_active) {
            return 1;
        }
    }
    return 0;
}

void axis_handle_jogwheels(bool motion_teleop_flag, bool motion_enable_flag, bool homing_is_active)
{
    int axis_num;
    emcmot_axis_t *axis;
    axis_hal_t *axis_data;
    rtapi_sint new_ajog_counts, delta;
    double distance, pos, stop_dist;
    static int first_pass = 1;	/* used to set initial conditions */

    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        double aaccel_limit;
        axis = &axis_array[axis_num];
        axis_data = &hal_data->axis[axis_num];

        // disallow accel bogus fractions
        rtapi_real ajog_accel_fraction = hal_get_real(axis_data->ajog_accel_fraction);
        if (   (ajog_accel_fraction > 1)
            || (ajog_accel_fraction < 0) ) {
            aaccel_limit = axis->acc_limit;
        } else {
            aaccel_limit = ajog_accel_fraction * axis->acc_limit;
        }

        new_ajog_counts = hal_get_si32(axis_data->ajog_counts);
        delta = new_ajog_counts - axis->old_ajog_counts;
        axis->old_ajog_counts = new_ajog_counts;
        if ( first_pass ) { continue; }
        if ( delta == 0 ) {
            //just update counts
            continue;
        }
        if (!motion_teleop_flag) {
            axis->teleop_tp.enable = 0;
            return;
        }
        if (!motion_enable_flag)              { continue; }
        if (!hal_get_bool(axis_data->ajog_enable)) { continue; }
        if (homing_is_active)                 { continue; }
        if (axis->kb_ajog_active)             { continue; }

        if (axis->locking_joint >= 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
            "Cannot wheel jog a locking indexer AXIS_%c\n",
            "XYZABCUVW"[axis_num]);
            continue;
        }

        distance = delta * hal_get_real(axis_data->ajog_scale);
        if (frame.selected && axis_num < 3) {
            if (   (ajog_accel_fraction > 1)
                || (ajog_accel_fraction < 0) ) {
                ajog_accel_fraction = 1;
            }
            if (!frame_jog_wheel(axis_num, distance, ajog_accel_fraction,
                                 hal_get_bool(axis_data->ajog_vel_mode))) {
                break;
            }
            continue;
        }
        pos = axis->teleop_tp.pos_cmd + distance;
        if ( hal_get_bool(axis_data->ajog_vel_mode) ) {
            double v = axis->vel_limit;
            /* compute stopping distance at max speed */
            stop_dist = v * v / ( 2 * aaccel_limit);
            /* if commanded position leads the actual position by more
               than stopping distance, discard excess command */
            if ( pos > axis->pos_cmd + stop_dist ) {
                pos = axis->pos_cmd + stop_dist;
            } else if ( pos < axis->pos_cmd - stop_dist ) {
                pos = axis->pos_cmd - stop_dist;
            }
        }
        if (pos > axis->max_pos_limit) { break; }
        if (pos < axis->min_pos_limit) { break; }
        axis->teleop_tp.pos_cmd = pos;
        axis->teleop_tp.max_vel = axis->vel_limit;
        axis->teleop_tp.max_acc = aaccel_limit;
        axis->teleop_vel_req = axis->teleop_tp.max_vel;
        axis->teleop_acc_req = axis->teleop_tp.max_acc;
        axis->wheel_ajog_active = 1;
        axis->teleop_tp.enable  = 1;
    }
    first_pass = 0;
}

void axis_sync_teleop_tp_to_carte_pos(int extfactor, double *pcmd_p[])
{
    int n;
    // expect extfactor =  -1 || 0 || +1
    //
    // This function initializes the teleop trajectory planner to a REST
    // state at the current cartesian position. Both callers (enabling
    // motion, entering teleop mode) expect the joint to be stationary
    // at the synced position. Without resetting curr_vel/curr_acc, stale
    // values from a previous motion (e.g. a jog that was aborted with
    // immediate=0, leaving a non-zero ramp-down velocity) would survive
    // a mode transition and cause the trajectory to drift away from the
    // synced curr_pos the next time simple_tp_update integrates one
    // cycle of motion.
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis_array[n].teleop_tp.curr_pos = *pcmd_p[n]
                            + extfactor * axis_array[n].ext_offset_tp.curr_pos;
        axis_array[n].teleop_tp.pos_cmd = axis_array[n].teleop_tp.curr_pos;
        axis_array[n].teleop_tp.curr_vel = 0.0;
        axis_array[n].teleop_tp.curr_acc = 0.0;
    }
    frame_release();
}

void axis_sync_carte_pos_to_teleop_tp(int extfactor, double *pcmd_p[])
{
    int n;
    // expect extfactor =  -1 || 0 || +1
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        *pcmd_p[n] = axis_array[n].teleop_tp.curr_pos
                            + extfactor * axis_array[n].ext_offset_tp.curr_pos;
    }
}

void axis_apply_ext_offsets_to_carte_pos(int extfactor, double *pcmd_p[])
{
    int n;
    // expect extfactor =  -1 || 0 || +1
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        *pcmd_p[n] = *pcmd_p[n]
                            + extfactor * axis_array[n].ext_offset_tp.curr_pos;
    }
}

bool axis_plan_external_offsets(double servo_period, bool motion_enable_flag, bool all_homed)
{
    static int first_pass = 1;
    int n;
    emcmot_axis_t *axis;
    axis_hal_t *axis_data;
    rtapi_sint new_eoffset_counts, delta;
    static int last_eoffset_enable[EMCMOT_MAX_AXIS];
    double ext_offset_epsilon;
    bool eoffset_active = 0;

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis = &axis_array[n];
        // coord,teleop updates done in get_pos_cmds()
        axis->ext_offset_tp.max_vel = axis->ext_offset_vel_limit;
        axis->ext_offset_tp.max_acc = axis->ext_offset_acc_limit;

        axis_data = &hal_data->axis[n];

        new_eoffset_counts       = hal_get_sint(axis_data->eoffset_counts);
        delta                    = new_eoffset_counts - axis->old_eoffset_counts;
        axis->old_eoffset_counts = new_eoffset_counts;

        hal_set_real(axis_data->external_offset, axis->ext_offset_tp.curr_pos);
        axis->ext_offset_tp.enable = 1;
        if ( first_pass ) {
            hal_set_real(axis_data->external_offset, 0);
            continue;
        }

        // Use stopping criterion of simple_tp.c:
        ext_offset_epsilon = TINY_DP(axis->ext_offset_tp.max_acc, servo_period);
        if (fabs(hal_get_real(axis_data->external_offset)) > ext_offset_epsilon) {
            eoffset_active = 1;
        }
        if ( !hal_get_bool(axis_data->eoffset_enable) ) {
            axis->ext_offset_tp.enable = 0;
            // Detect disabling of eoffsets:
            //   At very high accel, simple planner may terminate with
            //   a larger position value than occurs at more realistic accels.
            if (last_eoffset_enable[n]
                && (fabs(hal_get_real(axis_data->external_offset)) > ext_offset_epsilon)
                && motion_enable_flag
                && axis->ext_offset_tp.enable) {
                // to stdout only:
                rtapi_print_msg(RTAPI_MSG_NONE,
                           "*** Axis_%c External Offset=%.4g eps=%.4g\n"
                           "*** External Offset disabled while NON-zero\n"
                           "*** To clear: re-enable & zero or use Machine-Off\n",
                           "XYZABCUVW"[n],
                           hal_get_real(axis_data->external_offset),
                           ext_offset_epsilon);
            }
            last_eoffset_enable[n] = 0;
            continue; // Note: if   not eoffset_enable
                      //       then planner disabled and no pos_cmd updates
                      //       useful for eoffset_pid hold
        }
        last_eoffset_enable[n] = 1;
        if (hal_get_bool(axis_data->eoffset_clear)) {
            axis->ext_offset_tp.pos_cmd             = 0;
            hal_set_real(axis_data->external_offset_requested, 0);
            continue;
        }
        if (delta == 0)           { continue; }
        if (!all_homed)           { continue; }
        if (!motion_enable_flag)  { continue; }

        axis->ext_offset_tp.pos_cmd   += delta *  hal_get_real(axis_data->eoffset_scale);
        hal_set_real(axis_data->external_offset_requested, axis->ext_offset_tp.pos_cmd);
    } // for n
    first_pass = 0;

    return eoffset_active;
}

/* For each axis, return -1 if over negative limit, 1 if over positive limit,
   or 0 if in range */
void axis_check_constraints(double pos[], int failing_axes[])
{
    int axis_num;
    double eps = 1e-308;

    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num += 1) {
        double nl = axis_array[axis_num].min_pos_limit;
        double pl = axis_array[axis_num].max_pos_limit;
        failing_axes[axis_num] = 0;

        if (   (fabs(pos[axis_num]) < eps)
            && (fabs(axis_array[axis_num].min_pos_limit) < eps)
            && (fabs(axis_array[axis_num].max_pos_limit) < eps) ) {
            continue;
        }

        if (pos[axis_num] < (nl - 0.000000000001)) { // see pull request #1047
            failing_axes[axis_num] = -1;
        }

        if (pos[axis_num] > (pl + 0.000000000001)) { // see pull request #1047
            failing_axes[axis_num] = 1;
        }
    }
}

int axis_update_coord_with_bound(double *pcmd_p[], double servo_period)
{
    int n;
    int ans = 0;
    emcmot_axis_t *axis;
    double save_pos_cmd[EMCMOT_MAX_AXIS];
    double save_offset_cmd[EMCMOT_MAX_AXIS];

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis = &axis_array[n];
        save_pos_cmd[n]     = *pcmd_p[n];
        save_offset_cmd[n]  = axis->ext_offset_tp.pos_cmd;
        axis->ext_offset_tp.max_jerk = axis->jerk_limit;
        simple_tp_update(&(axis->ext_offset_tp), servo_period);
    }
    axis_apply_ext_offsets_to_carte_pos(+1, pcmd_p); // add external offsets

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        axis = &axis_array[n];
        //workaround: axis letters not in [TRAJ]COORDINATES
        //            have min_pos_limit == max_pos_lim == 0
        if ( (0 == axis->max_pos_limit) && (0 == axis->min_pos_limit) ) {
            continue;
        }
        if (axis->ext_offset_tp.curr_pos == 0) {
           continue; // don't claim violation if no offset
        }

        if (*pcmd_p[n] >= axis->max_pos_limit) {
            // hold carte_pos_cmd at the limit:
            *pcmd_p[n]  = axis->max_pos_limit;
            // stop growth of offsetting position:
            axis->ext_offset_tp.curr_pos = axis->max_pos_limit
                                         - save_pos_cmd[n];
            if (axis->ext_offset_tp.pos_cmd > save_offset_cmd[n]) {
                axis->ext_offset_tp.pos_cmd = save_offset_cmd[n];
            }
            axis->ext_offset_tp.curr_vel = 0;
            ans++;
            continue;
        }
        if (*pcmd_p[n] <= axis->min_pos_limit) {
            *pcmd_p[n]  = axis->min_pos_limit;
            axis->ext_offset_tp.curr_pos = axis->min_pos_limit
                                         - save_pos_cmd[n];
            if (axis->ext_offset_tp.pos_cmd < save_offset_cmd[n]) {
                axis->ext_offset_tp.pos_cmd = save_offset_cmd[n];
            }
            axis->ext_offset_tp.curr_vel = 0;
            ans++;
        }
    }
    if (ans > 0) { return 1; }
    return 0;
}

static int update_teleop_with_check(int axis_num, simple_tp_t *the_tp, double servo_period)
{
    // 'the_tp' is the planner to update
    // the tests herein apply to the sum of the offsets for both
    // planners (teleop_tp and ext_offset_tp)
    double save_curr_pos;
    emcmot_axis_t *axis = &axis_array[axis_num];

    save_curr_pos = the_tp->curr_pos;
    the_tp->max_jerk = axis->jerk_limit;
    simple_tp_update(the_tp, servo_period);

    //workaround: axis letters not in [TRAJ]COORDINATES
    //            have min_pos_limit == max_pos_lim == 0
    if  ( (0 == axis->max_pos_limit) && (0 == axis->min_pos_limit) ) {
        return 0;
    }
    // an unhomed axis has no known machine position; the INI limits do
    // not apply and the jog limits (axis_jog_limits) bound the motion
    if (!axis_is_homed(axis_num)) { return 0; }
    if  ( (axis->ext_offset_tp.curr_pos + axis->teleop_tp.curr_pos)
          >= axis->max_pos_limit) {
        // positive error, restore save_curr_pos
        the_tp->curr_pos = save_curr_pos;
        the_tp->curr_vel = 0;
        return 1;
    }
    if  ( (axis->ext_offset_tp.curr_pos + axis->teleop_tp.curr_pos)
           <= axis->min_pos_limit) {
        // negative error, restore save_curr_pos
        the_tp->curr_pos = save_curr_pos;
        the_tp->curr_vel = 0;
        return 1;
    }
    return 0;
}

// What the teleop planners that have somewhere to go this cycle ask of
// the world axes, the velocity and the acceleration each jog asked for
// along its way, and the velocity they are at: what the cap on the joints
// reads before the planners run.  0 if none has anywhere to go
int axis_teleop_request(double vel[], double acc[], double now[])
{
    double dir;
    int n, k, any = 0;

    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        emcmot_axis_t *axis = &axis_array[n];
        vel[n] = acc[n] = now[n] = 0.0;
        if (planner_request(&axis->teleop_tp, &dir)) {
            vel[n] = dir * axis->teleop_vel_req;
            acc[n] = dir * axis->teleop_acc_req;
            now[n] = axis->teleop_tp.curr_vel;
            any = 1;
        }
    }
    // the frame planner whose turn it is, the others wait
    k = frame.latched ? frame_owner() : -1;
    if (k >= 0 && planner_request(&frame.tp[k], &dir)) {
        for (n = 0; n < 3; n++) {
            vel[n] += frame.rot[n][k] * dir * frame.vel_req[k];
            acc[n] += frame.rot[n][k] * dir * frame.acc_req[k];
            now[n] += frame.rot[n][k] * frame.tp[k].curr_vel;
        }
        any = 1;
    }
    return any;
}

// The most the planners with somewhere to go may do this cycle: the given
// share of what their jog asked, where the joints cannot follow all of it
void axis_teleop_cap(double vel_scale, double acc_scale)
{
    double dir;
    int n, k;

    if (vel_scale > 1.0) { vel_scale = 1.0; }
    if (acc_scale > 1.0) { acc_scale = 1.0; }
    for (n = 0; n < EMCMOT_MAX_AXIS; n++) {
        emcmot_axis_t *axis = &axis_array[n];
        if (!planner_request(&axis->teleop_tp, &dir)) { continue; }
        axis->teleop_tp.max_vel = vel_scale * axis->teleop_vel_req;
        axis->teleop_tp.max_acc = acc_scale * axis->teleop_acc_req;
    }
    k = frame.latched ? frame.owner : -1;
    if (k >= 0 && planner_request(&frame.tp[k], &dir)) {
        frame.vel_cap[k] = vel_scale * frame.vel_req[k];
        frame.acc_cap[k] = acc_scale * frame.acc_req[k];
    }
}

int axis_calc_motion(double servo_period)
{
    int axis_num;
    int violated_teleop_limit = 0;
    emcmot_axis_t *axis;

    // the frame planner first, so the frame's state is this cycle's when
    // the jog flags it shares with X, Y and Z are read below
    if (frame_step(servo_period)) {
        violated_teleop_limit = 1;
    }
    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        axis = &axis_array[axis_num];

        // teleop_tp.max_vel is always positive
        if (axis->teleop_tp.max_vel > axis->vel_limit) {
            axis->teleop_tp.max_vel = axis->vel_limit;
        }
        if (update_teleop_with_check(axis_num, &(axis->teleop_tp), servo_period)) {
            violated_teleop_limit = 1;
        }

        axis->teleop_vel_cmd = axis->teleop_tp.curr_vel + frame_vel(axis_num);
        axis->pos_cmd = axis->teleop_tp.curr_pos;

        if (!axis->teleop_tp.active && !(axis_num < 3 && frame.latched && frame_busy(axis_num))) {
            axis->kb_ajog_active = 0;
            axis->wheel_ajog_active = 0;
        }

        if (axis->ext_offset_tp.enable) {
            if (update_teleop_with_check(axis_num, &(axis->ext_offset_tp), servo_period)) {
                violated_teleop_limit = 1;
            }
        }
    }
    return violated_teleop_limit;
}
