/********************************************************************
* Description: motctl_handlers.c
*   Implementation of the motctl GMI command API.
*   Each handler fills an emcmot_command_t and sends it to the RT
*   side via the shared command buffer.
*
* License: GPL Version 2
********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "motion.h"
#include "motion_struct.h"
#include "mot_priv.h"
#include "state_tag.h"

#define MOTCTL_API_CGO
#include "motctl_api.h"
#undef MOTCTL_API_CGO

#define MOTSTAT_API_CGO
#include "motstat_api.h"
#undef MOTSTAT_API_CGO

/* ================================================================
 * Context and command dispatch
 * ================================================================ */

typedef struct motctl_ctx {
    emcmot_struct_t *mot;
    int command_num;
    double comm_timeout; /* seconds */
} motctl_ctx_t;

/* Send a command to the RT side: mutex-protected write, then poll
   for acknowledgment.  Returns 0 on success, -1 on error/timeout. */
static int send_command(motctl_ctx_t *mc, emcmot_command_t *cmd)
{
    emcmot_struct_t *m = mc->mot;
    cmd->commandNum = ++mc->command_num;

    rtapi_mutex_get(&m->command_mutex);
    m->command = *cmd;
    rtapi_mutex_give(&m->command_mutex);

    /* Poll until RT echoes our command number or timeout. */
    long long end = rtapi_get_time() + (long long)(mc->comm_timeout * 1e9);
    while (rtapi_get_time() < end) {
        /* Read from triple buffer — always consistent. */
        int idx = atomic_load_explicit(&m->status_buf.readable, memory_order_acquire);
        emcmot_status_t *s = &m->status_buf.slots[idx];
        if (s->commandNumEcho == cmd->commandNum) {
            return (s->commandStatus == EMCMOT_COMMAND_OK) ? 0 : -1;
        }
        rtapi_delay(rtapi_delay_max()); /* ~10 µs yield */
    }
    return -1; /* timeout */
}

/* Helper: zero a command struct and set the opcode */
static inline void cmd_init(emcmot_command_t *cmd, cmd_code_t code)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->command = code;
}

/* Convert motctl_pose_t ↔ EmcPose (identical layout) */
static inline EmcPose pose_from_motctl(const motctl_pose_t *p)
{
    EmcPose e;
    e.tran.x = p->x; e.tran.y = p->y; e.tran.z = p->z;
    e.a = p->a; e.b = p->b; e.c = p->c;
    e.u = p->u; e.v = p->v; e.w = p->w;
    return e;
}

/* Convert motctl_state_tag_t → state_tag_t */
static inline struct state_tag_t tag_from_motctl(const motctl_state_tag_t *t)
{
    struct state_tag_t st;
    memset(&st, 0, sizeof(st));
    for (int i = 0; i < MOTCTL_TAG_FIELDS_FLOAT && i < GM_FIELD_FLOAT_MAX_FIELDS; i++)
        st.fields_float[i] = t->fields_float[i];
    for (int i = 0; i < MOTCTL_TAG_FIELDS_INT && i < GM_FIELD_MAX_FIELDS; i++)
        st.fields[i] = t->fields[i];
    st.packed_flags = t->packed_flags;
    return st;
}

#define CTX motctl_ctx_t *mc = (motctl_ctx_t *)ctx

/* ================================================================
 * Motion Queue
 * ================================================================ */

static int32_t h_set_line(void *ctx, const motctl_pose_t *pos,
    double vel, double ini_maxvel, double acc,
    int32_t motion_type, int32_t id, const motctl_state_tag_t *tag,
    int32_t indexer_jnum)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_LINE);
    cmd.pos = pose_from_motctl(pos);
    cmd.vel = vel;
    cmd.ini_maxvel = ini_maxvel;
    cmd.acc = acc;
    cmd.motion_type = motion_type;
    cmd.id = id;
    cmd.tag = tag_from_motctl(tag);
    cmd.turn = indexer_jnum;
    return send_command(mc, &cmd);
}

static int32_t h_set_circle(void *ctx, const motctl_pose_t *pos,
    const motctl_cartesian_t *center, const motctl_cartesian_t *normal,
    int32_t turn, double vel, double ini_maxvel, double acc,
    int32_t motion_type, int32_t id, const motctl_state_tag_t *tag)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_CIRCLE);
    cmd.pos = pose_from_motctl(pos);
    cmd.center.x = center->x;
    cmd.center.y = center->y;
    cmd.center.z = center->z;
    cmd.normal.x = normal->x;
    cmd.normal.y = normal->y;
    cmd.normal.z = normal->z;
    cmd.turn = turn;
    cmd.vel = vel;
    cmd.ini_maxvel = ini_maxvel;
    cmd.acc = acc;
    cmd.motion_type = motion_type;
    cmd.id = id;
    cmd.tag = tag_from_motctl(tag);
    return send_command(mc, &cmd);
}

static int32_t h_probe(void *ctx, const motctl_pose_t *pos,
    double vel, double ini_maxvel, double acc,
    int32_t motion_type, uint8_t probe_type, int32_t id,
    const motctl_state_tag_t *tag)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_PROBE);
    cmd.pos = pose_from_motctl(pos);
    cmd.vel = vel;
    cmd.ini_maxvel = ini_maxvel;
    cmd.acc = acc;
    cmd.motion_type = motion_type;
    cmd.probe_type = probe_type;
    cmd.id = id;
    cmd.tag = tag_from_motctl(tag);
    return send_command(mc, &cmd);
}

static int32_t h_rigid_tap(void *ctx, const motctl_pose_t *pos,
    double vel, double ini_maxvel, double acc,
    double scale, int32_t id, const motctl_state_tag_t *tag)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_RIGID_TAP);
    cmd.pos = pose_from_motctl(pos);
    cmd.vel = vel;
    cmd.ini_maxvel = ini_maxvel;
    cmd.acc = acc;
    cmd.scale = scale;
    cmd.id = id;
    cmd.tag = tag_from_motctl(tag);
    return send_command(mc, &cmd);
}

/* ================================================================
 * Motion Parameters
 * ================================================================ */

static int32_t h_set_vel(void *ctx, double vel)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_VEL);
    cmd.vel = vel;
    return send_command(mc, &cmd);
}

static int32_t h_set_vel_limit(void *ctx, double vel)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_VEL_LIMIT);
    cmd.vel = vel;
    return send_command(mc, &cmd);
}

static int32_t h_set_acc(void *ctx, double acc)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_ACC);
    cmd.acc = acc;
    return send_command(mc, &cmd);
}

static int32_t h_set_term_cond(void *ctx, int32_t cond, double tolerance)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_TERM_COND);
    cmd.termCond = cond;
    cmd.tolerance = tolerance;
    return send_command(mc, &cmd);
}

static int32_t h_set_spindlesync(void *ctx, double sync, int32_t motion_type)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_SPINDLESYNC);
    cmd.spindlesync = sync;
    cmd.motion_type = motion_type;
    return send_command(mc, &cmd);
}

static int32_t h_set_offset(void *ctx, const motctl_pose_t *offset)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_OFFSET);
    cmd.tool_offset = pose_from_motctl(offset);
    return send_command(mc, &cmd);
}

/* ================================================================
 * Flow Control
 * ================================================================ */

static int32_t h_abort(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_ABORT);
    return send_command(mc, &cmd);
}

static int32_t h_pause(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_PAUSE);
    return send_command(mc, &cmd);
}

static int32_t h_resume(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_RESUME);
    return send_command(mc, &cmd);
}

static int32_t h_step(void *ctx, int32_t id)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_STEP);
    cmd.id = id;
    return send_command(mc, &cmd);
}

static int32_t h_reverse(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_REVERSE);
    return send_command(mc, &cmd);
}

static int32_t h_forward(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_FORWARD);
    return send_command(mc, &cmd);
}

/* ================================================================
 * Mode
 * ================================================================ */

static int32_t h_set_free(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_FREE);
    return send_command(mc, &cmd);
}

static int32_t h_set_coord(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_COORD);
    return send_command(mc, &cmd);
}

static int32_t h_set_teleop(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_TELEOP);
    return send_command(mc, &cmd);
}

/* ================================================================
 * Jog
 * ================================================================ */

static int32_t h_jog_cont(void *ctx, int32_t num, double vel, int32_t is_teleop)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOG_CONT);
    if (is_teleop) {
        cmd.joint = -1;
        cmd.axis = num;
    } else {
        cmd.joint = num;
        cmd.axis = -1;
    }
    cmd.vel = vel;
    return send_command(mc, &cmd);
}

static int32_t h_jog_incr(void *ctx, int32_t num, double vel, double incr,
                           int32_t is_teleop)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOG_INCR);
    if (is_teleop) {
        cmd.joint = -1;
        cmd.axis = num;
    } else {
        cmd.joint = num;
        cmd.axis = -1;
    }
    cmd.vel = vel;
    cmd.offset = incr;
    return send_command(mc, &cmd);
}

static int32_t h_jog_abs(void *ctx, int32_t num, double vel, double pos,
                          int32_t is_teleop)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOG_ABS);
    if (is_teleop) {
        cmd.joint = -1;
        cmd.axis = num;
    } else {
        cmd.joint = num;
        cmd.axis = -1;
    }
    cmd.vel = vel;
    cmd.offset = pos;
    return send_command(mc, &cmd);
}

static int32_t h_jog_abort(void *ctx, int32_t num, int32_t is_teleop)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOG_ABORT);
    if (is_teleop) {
        cmd.joint = -1;
        cmd.axis = num;
    } else {
        cmd.joint = num;
        cmd.axis = -1;
    }
    return send_command(mc, &cmd);
}

/* ================================================================
 * Spindle
 * ================================================================ */

static int32_t h_spindle_on(void *ctx, int32_t spindle, double speed,
    double css_factor, double css_offset, int32_t wait_for_atspeed)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_ON);
    cmd.spindle = spindle;
    cmd.vel = speed;
    cmd.state = 1;
    cmd.ini_maxvel = css_factor;
    cmd.acc = css_offset;
    cmd.wait_for_spindle_at_speed = (unsigned char)wait_for_atspeed;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_off(void *ctx, int32_t spindle)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_OFF);
    cmd.spindle = spindle;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_orient(void *ctx, int32_t spindle,
    double orientation, int32_t mode)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_ORIENT);
    cmd.spindle = spindle;
    cmd.orientation = orientation;
    cmd.mode = (unsigned char)mode;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_increase(void *ctx, int32_t spindle)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_INCREASE);
    cmd.spindle = spindle;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_decrease(void *ctx, int32_t spindle)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_DECREASE);
    cmd.spindle = spindle;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_brake_engage(void *ctx, int32_t spindle)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_BRAKE_ENGAGE);
    cmd.spindle = spindle;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_brake_release(void *ctx, int32_t spindle)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_BRAKE_RELEASE);
    cmd.spindle = spindle;
    return send_command(mc, &cmd);
}

static int32_t h_set_spindle_params(void *ctx, int32_t spindle,
    double max_pos_speed, double min_pos_speed,
    double max_neg_speed, double min_neg_speed,
    double home_search_vel, int32_t home_sequence,
    double increment)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_SPINDLE_PARAMS);
    cmd.spindle = spindle;
    cmd.maxLimit = max_pos_speed;
    cmd.min_pos_speed = min_pos_speed;
    cmd.max_neg_speed = min_neg_speed;
    cmd.minLimit = max_neg_speed;
    cmd.search_vel = home_search_vel;
    cmd.home_sequence = home_sequence;
    cmd.offset = increment;
    return send_command(mc, &cmd);
}

static int32_t h_set_spindle_scale(void *ctx, int32_t spindle, double scale)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SPINDLE_SCALE);
    cmd.spindle = spindle;
    cmd.scale = scale;
    return send_command(mc, &cmd);
}

static int32_t h_spindle_scale_enable(void *ctx, int32_t spindle, int32_t enable)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SS_ENABLE);
    cmd.spindle = spindle;
    cmd.mode = (unsigned char)enable;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Digital/Analog I/O
 * ================================================================ */

static int32_t h_set_dout(void *ctx, int32_t index, int32_t value)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_DOUT);
    cmd.now = 1;
    cmd.out = (unsigned char)index;
    cmd.start = (unsigned char)value;
    cmd.end = (unsigned char)value;
    return send_command(mc, &cmd);
}

static int32_t h_set_dout_synched(void *ctx, int32_t index,
    int32_t start_value, int32_t end_value)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_DOUT);
    cmd.now = 0;
    cmd.out = (unsigned char)index;
    cmd.start = (unsigned char)start_value;
    cmd.end = (unsigned char)end_value;
    return send_command(mc, &cmd);
}

static int32_t h_set_aout(void *ctx, int32_t index, double value)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AOUT);
    cmd.now = 1;
    cmd.out = (unsigned char)index;
    cmd.minLimit = value;
    return send_command(mc, &cmd);
}

static int32_t h_set_aout_synched(void *ctx, int32_t index,
    double start_value, double end_value)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AOUT);
    cmd.now = 0;
    cmd.out = (unsigned char)index;
    cmd.minLimit = start_value;
    cmd.maxLimit = end_value;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Overrides
 * ================================================================ */

static int32_t h_set_feed_scale(void *ctx, double scale)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_FEED_SCALE);
    cmd.scale = scale;
    return send_command(mc, &cmd);
}

static int32_t h_set_rapid_scale(void *ctx, double scale)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_RAPID_SCALE);
    cmd.scale = scale;
    return send_command(mc, &cmd);
}

static int32_t h_feed_scale_enable(void *ctx, int32_t enable)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_FS_ENABLE);
    cmd.mode = (unsigned char)enable;
    return send_command(mc, &cmd);
}

static int32_t h_feed_hold_enable(void *ctx, int32_t enable)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_FH_ENABLE);
    cmd.mode = (unsigned char)enable;
    return send_command(mc, &cmd);
}

static int32_t h_adaptive_feed_enable(void *ctx, int32_t enable)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_AF_ENABLE);
    cmd.mode = (unsigned char)enable;
    return send_command(mc, &cmd);
}

static int32_t h_set_max_feed_override(void *ctx, double max)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_MAX_FEED_OVERRIDE);
    cmd.maxFeedScale = max;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Enable/Disable
 * ================================================================ */

static int32_t h_enable(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_ENABLE);
    return send_command(mc, &cmd);
}

static int32_t h_disable(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_DISABLE);
    return send_command(mc, &cmd);
}

/* ================================================================
 * Joint Configuration
 * ================================================================ */

static int32_t h_joint_activate(void *ctx, int32_t joint)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOINT_ACTIVATE);
    cmd.joint = joint;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_position_limits(void *ctx, int32_t joint,
    double min, double max)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_POSITION_LIMITS);
    cmd.joint = joint;
    cmd.minLimit = min;
    cmd.maxLimit = max;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_backlash(void *ctx, int32_t joint, double backlash)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_BACKLASH);
    cmd.joint = joint;
    cmd.backlash = backlash;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_max_ferror(void *ctx, int32_t joint, double ferror)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_MAX_FERROR);
    cmd.joint = joint;
    cmd.maxFerror = ferror;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_min_ferror(void *ctx, int32_t joint, double ferror)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_MIN_FERROR);
    cmd.joint = joint;
    cmd.minFerror = ferror;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_vel_limit(void *ctx, int32_t joint, double vel)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_VEL_LIMIT);
    cmd.joint = joint;
    cmd.vel = vel;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_acc_limit(void *ctx, int32_t joint, double acc)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_ACC_LIMIT);
    cmd.joint = joint;
    cmd.acc = acc;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_jerk_limit(void *ctx, int32_t joint, double jerk)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_JERK_LIMIT);
    cmd.joint = joint;
    cmd.jerk = jerk;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_motor_offset(void *ctx, int32_t joint, double offset)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_MOTOR_OFFSET);
    cmd.joint = joint;
    cmd.motor_offset = offset;
    return send_command(mc, &cmd);
}

static int32_t h_set_joint_comp(void *ctx, int32_t joint,
    double nominal, double fwd, double rev)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_COMP);
    cmd.joint = joint;
    cmd.comp_nominal = nominal;
    cmd.comp_forward = fwd;
    cmd.comp_reverse = rev;
    return send_command(mc, &cmd);
}

static int32_t h_override_limits(void *ctx, int32_t joint)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_OVERRIDE_LIMITS);
    cmd.joint = joint;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Axis Configuration
 * ================================================================ */

static int32_t h_set_axis_position_limits(void *ctx, int32_t axis,
    double min, double max)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AXIS_POSITION_LIMITS);
    cmd.axis = axis;
    cmd.minLimit = min;
    cmd.maxLimit = max;
    return send_command(mc, &cmd);
}

static int32_t h_set_axis_vel_limit(void *ctx, int32_t axis, double vel, double ext_offset_vel)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AXIS_VEL_LIMIT);
    cmd.axis = axis;
    cmd.vel = vel;
    cmd.ext_offset_vel = ext_offset_vel;
    return send_command(mc, &cmd);
}

static int32_t h_set_axis_acc_limit(void *ctx, int32_t axis, double acc, double ext_offset_acc)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AXIS_ACC_LIMIT);
    cmd.axis = axis;
    cmd.acc = acc;
    cmd.ext_offset_acc = ext_offset_acc;
    return send_command(mc, &cmd);
}

static int32_t h_set_axis_locking_joint(void *ctx, int32_t axis, int32_t joint)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_AXIS_LOCKING_JOINT);
    cmd.axis = axis;
    cmd.joint = joint;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Homing
 * ================================================================ */

static int32_t h_set_joint_homing_params(void *ctx, int32_t joint,
    double offset, double home, double home_final_vel,
    double search_vel, double latch_vel,
    int32_t flags, int32_t sequence, int32_t volatile_home)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_JOINT_HOMING_PARAMS);
    cmd.joint = joint;
    cmd.offset = offset;
    cmd.home = home;
    cmd.home_final_vel = home_final_vel;
    cmd.search_vel = search_vel;
    cmd.latch_vel = latch_vel;
    cmd.flags = flags;
    cmd.home_sequence = sequence;
    cmd.volatile_home = volatile_home;
    return send_command(mc, &cmd);
}

static int32_t h_update_joint_homing_params(void *ctx, int32_t joint,
    double offset, double home, int32_t sequence)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_UPDATE_JOINT_HOMING_PARAMS);
    cmd.joint = joint;
    cmd.offset = offset;
    cmd.home = home;
    cmd.home_sequence = sequence;
    return send_command(mc, &cmd);
}

static int32_t h_joint_home(void *ctx, int32_t joint)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOINT_HOME);
    cmd.joint = joint;
    return send_command(mc, &cmd);
}

static int32_t h_joint_unhome(void *ctx, int32_t joint)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_JOINT_UNHOME);
    cmd.joint = joint;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Probe
 * ================================================================ */

static int32_t h_clear_probe_flags(void *ctx)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_CLEAR_PROBE_FLAGS);
    return send_command(mc, &cmd);
}

static int32_t h_set_probe_err_inhibit(void *ctx,
    int32_t jog_inhibit, int32_t home_inhibit)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_PROBE_ERR_INHIBIT);
    cmd.probe_jog_err_inhibit = jog_inhibit;
    cmd.probe_home_err_inhibit = home_inhibit;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Miscellaneous
 * ================================================================ */

static int32_t h_set_world_home(void *ctx, const motctl_pose_t *pos)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_WORLD_HOME);
    cmd.pos = pose_from_motctl(pos);
    return send_command(mc, &cmd);
}

static int32_t h_set_debug(void *ctx, int32_t level)
{
    CTX; emcmot_command_t cmd;
    cmd_init(&cmd, EMCMOT_SET_DEBUG);
    cmd.debug = level;
    return send_command(mc, &cmd);
}

/* ================================================================
 * Public: build the callback table
 * ================================================================ */

motctl_callbacks_t motctl_get_callbacks(motctl_ctx_t **ctx_out)
{
    motctl_ctx_t *mc = calloc(1, sizeof(*mc));
    if (!mc) {
        motctl_callbacks_t empty = {0};
        if (ctx_out) *ctx_out = NULL;
        return empty;
    }
    mc->mot = NULL;
    mc->command_num = 0;
    mc->comm_timeout = 0;
    if (ctx_out) *ctx_out = mc;

    motctl_callbacks_t cb = {
        .ctx                       = mc,
        .set_line                  = h_set_line,
        .set_circle                = h_set_circle,
        .probe                     = h_probe,
        .rigid_tap                 = h_rigid_tap,
        .set_vel                   = h_set_vel,
        .set_vel_limit             = h_set_vel_limit,
        .set_acc                   = h_set_acc,
        .set_term_cond             = h_set_term_cond,
        .set_spindlesync           = h_set_spindlesync,
        .set_offset                = h_set_offset,
        .abort                     = h_abort,
        .pause                     = h_pause,
        .resume                    = h_resume,
        .step                      = h_step,
        .reverse                   = h_reverse,
        .forward                   = h_forward,
        .set_free                  = h_set_free,
        .set_coord                 = h_set_coord,
        .set_teleop                = h_set_teleop,
        .jog_cont                  = h_jog_cont,
        .jog_incr                  = h_jog_incr,
        .jog_abs                   = h_jog_abs,
        .jog_abort                 = h_jog_abort,
        .spindle_on                = h_spindle_on,
        .spindle_off               = h_spindle_off,
        .spindle_orient            = h_spindle_orient,
        .spindle_increase          = h_spindle_increase,
        .spindle_decrease          = h_spindle_decrease,
        .spindle_brake_engage      = h_spindle_brake_engage,
        .spindle_brake_release     = h_spindle_brake_release,
        .set_spindle_params        = h_set_spindle_params,
        .set_spindle_scale         = h_set_spindle_scale,
        .spindle_scale_enable      = h_spindle_scale_enable,
        .set_dout                  = h_set_dout,
        .set_dout_synched          = h_set_dout_synched,
        .set_aout                  = h_set_aout,
        .set_aout_synched          = h_set_aout_synched,
        .set_feed_scale            = h_set_feed_scale,
        .set_rapid_scale           = h_set_rapid_scale,
        .feed_scale_enable         = h_feed_scale_enable,
        .feed_hold_enable          = h_feed_hold_enable,
        .adaptive_feed_enable      = h_adaptive_feed_enable,
        .set_max_feed_override     = h_set_max_feed_override,
        .enable                    = h_enable,
        .disable                   = h_disable,
        .joint_activate            = h_joint_activate,
        .set_joint_position_limits = h_set_joint_position_limits,
        .set_joint_backlash        = h_set_joint_backlash,
        .set_joint_max_ferror      = h_set_joint_max_ferror,
        .set_joint_min_ferror      = h_set_joint_min_ferror,
        .set_joint_vel_limit       = h_set_joint_vel_limit,
        .set_joint_acc_limit       = h_set_joint_acc_limit,
        .set_joint_jerk_limit      = h_set_joint_jerk_limit,
        .set_joint_motor_offset    = h_set_joint_motor_offset,
        .set_joint_comp            = h_set_joint_comp,
        .override_limits           = h_override_limits,
        .set_axis_position_limits  = h_set_axis_position_limits,
        .set_axis_vel_limit        = h_set_axis_vel_limit,
        .set_axis_acc_limit        = h_set_axis_acc_limit,
        .set_axis_locking_joint    = h_set_axis_locking_joint,
        .set_joint_homing_params   = h_set_joint_homing_params,
        .update_joint_homing_params = h_update_joint_homing_params,
        .joint_home                = h_joint_home,
        .joint_unhome              = h_joint_unhome,
        .clear_probe_flags         = h_clear_probe_flags,
        .set_probe_err_inhibit     = h_set_probe_err_inhibit,
        .set_world_home            = h_set_world_home,
        .set_debug                 = h_set_debug,
    };
    return cb;
}

void motctl_init_ctx(motctl_ctx_t *mc, emcmot_struct_t *mot, double timeout)
{
    if (mc) {
        mc->mot = mot;
        mc->comm_timeout = timeout;
    }
}
