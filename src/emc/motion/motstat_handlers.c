/********************************************************************
* Description: motstat_handlers.c
*   Implementation of the motstat GMI status API.
*   Reads from emcmot_status_t and converts to GMI types.
*
* License: GPL Version 2
********************************************************************/

#include <string.h>
#include <stdlib.h>
#include "motion.h"
#include "motion_struct.h"
#include "mot_priv.h"
#include "axis.h"
#include "state_tag.h"

#define MOTSTAT_API_CGO
#include "motstat_api.h"
#undef MOTSTAT_API_CGO

typedef struct motstat_ctx {
    emcmot_struct_t *mot;
    axis_inst_t *axis_inst;
} motstat_ctx_t;

#define CTX motstat_ctx_t *mc = (motstat_ctx_t *)ctx

/* ================================================================
 * Helpers
 * ================================================================ */

/* Copy emcmot_status_t with split-read protection (head/tail check).
   Returns 0 on success, -1 if split read detected after retries. */
static int read_status(motstat_ctx_t *mc, emcmot_status_t *out)
{
    emcmot_status_t *src = &mc->mot->status;
    for (int tries = 0; tries < 3; tries++) {
        __sync_synchronize();
        *out = *src;
        __sync_synchronize();
        if (out->head == out->tail)
            return 0;
    }
    return -1;
}

static inline motstat_pose_t pose_to_motstat(const EmcPose *p)
{
    motstat_pose_t r;
    r.x = p->tran.x; r.y = p->tran.y; r.z = p->tran.z;
    r.a = p->a; r.b = p->b; r.c = p->c;
    r.u = p->u; r.v = p->v; r.w = p->w;
    return r;
}

static inline motstat_state_tag_t tag_to_motstat(const struct state_tag_t *t)
{
    motstat_state_tag_t r;
    for (int i = 0; i < MOTSTAT_TAG_FIELDS_FLOAT && i < GM_FIELD_FLOAT_MAX_FIELDS; i++)
        r.fields_float[i] = t->fields_float[i];
    for (int i = 0; i < MOTSTAT_TAG_FIELDS_INT && i < GM_FIELD_MAX_FIELDS; i++)
        r.fields[i] = t->fields[i];
    r.packed_flags = t->packed_flags;
    return r;
}

/* ================================================================
 * Full status read
 * ================================================================ */

static int32_t h_get_status(void *ctx, motstat_motion_status_t *status)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0)
        return -1;

    emcmot_config_t *cfg = &mc->mot->config;

    memset(status, 0, sizeof(*status));

    /* Command echo */
    status->command_echo = s.commandEcho;
    status->command_num_echo = s.commandNumEcho;
    status->command_status = s.commandStatus;

    /* Motion state: decode the motionFlag bitfield */
    status->motion_state = s.motion_state;
    status->enabled  = (s.motionFlag & EMCMOT_MOTION_ENABLE_BIT) ? 1 : 0;
    status->inpos    = (s.motionFlag & EMCMOT_MOTION_INPOS_BIT)  ? 1 : 0;
    status->coord    = (s.motionFlag & EMCMOT_MOTION_COORD_BIT)  ? 1 : 0;
    status->teleop   = (s.motionFlag & EMCMOT_MOTION_TELEOP_BIT) ? 1 : 0;
    status->error    = (s.motionFlag & EMCMOT_MOTION_ERROR_BIT)  ? 1 : 0;

    status->on_soft_limit    = s.on_soft_limit;
    status->paused           = s.paused;
    status->reverse_run      = s.reverse_run;
    status->stepping         = s.stepping;
    status->jogging_active   = s.jogging_active;
    status->override_limit_mask = s.overrideLimitMask;

    /* Overrides / enables */
    status->feed_scale  = s.feed_scale;
    status->rapid_scale = s.rapid_scale;
    status->net_feed_scale = s.net_feed_scale;
    status->feed_scale_enabled    = (s.enables_new & FS_ENABLED) ? 1 : 0;
    status->spindle_scale_enabled = (s.enables_new & SS_ENABLED) ? 1 : 0;
    status->adaptive_feed_enabled = (s.enables_new & AF_ENABLED) ? 1 : 0;
    status->feed_hold_enabled     = (s.enables_new & FH_ENABLED) ? 1 : 0;

    /* Positions */
    status->carte_pos_cmd = pose_to_motstat(&s.carte_pos_cmd);
    status->carte_pos_fb  = pose_to_motstat(&s.carte_pos_fb);
    status->carte_pos_cmd_ok = s.carte_pos_cmd_ok;
    status->carte_pos_fb_ok  = s.carte_pos_fb_ok;

    /* Velocity */
    status->vel           = s.vel;
    status->acc           = s.acc;
    status->current_vel   = s.current_vel;
    status->requested_vel = s.requested_vel;

    /* Queue */
    status->queue_depth  = s.depth;
    status->active_depth = s.activeDepth;
    status->queue_full   = s.queueFull;
    status->tcqlen       = s.tcqlen;

    /* Current motion */
    status->id            = s.id;
    status->motion_type   = s.motionType;
    status->distance_to_go = s.distance_to_go;
    status->dtg           = pose_to_motstat(&s.dtg);
    status->tag           = tag_to_motstat(&s.tag);

    /* Probe */
    status->probe.val        = s.probeVal;
    status->probe.tripped    = s.probeTripped;
    status->probe.probing    = s.probing;
    status->probe.probe_type = s.probe_type;
    status->probe.pos        = pose_to_motstat(&s.probedPos);

    /* Tool / offsets */
    status->tool_offset = pose_to_motstat(&s.tool_offset);
    status->atspeed_next_feed = s.atspeed_next_feed;
    status->external_offsets_applied = s.external_offsets_applied;
    status->eoffset_pose = pose_to_motstat(&s.eoffset_pose);

    /* Misc */
    status->heartbeat  = s.heartbeat;
    status->config_num = s.config_num;

    /* Config snapshot */
    status->kin_type        = cfg->kinType;
    status->traj_cycle_time = cfg->trajCycleTime;
    status->limit_vel       = cfg->limitVel;
    status->debug           = cfg->debug;

    /* Joints */
    int nj = cfg->numJoints;
    if (nj > MOTSTAT_MAX_JOINTS) nj = MOTSTAT_MAX_JOINTS;
    for (int i = 0; i < nj; i++) {
        emcmot_joint_status_t *js = &s.joint_status[i];
        motstat_joint_status_t *d = &status->joints[i];
        d->enabled    = (js->flag & EMCMOT_JOINT_ENABLE_BIT) ? 1 : 0;
        d->active     = (js->flag & EMCMOT_JOINT_ACTIVE_BIT) ? 1 : 0;
        d->inpos      = (js->flag & EMCMOT_JOINT_INPOS_BIT)  ? 1 : 0;
        d->error      = (js->flag & EMCMOT_JOINT_ERROR_BIT)  ? 1 : 0;
        d->on_pos_limit    = (js->flag & EMCMOT_JOINT_MAX_HARD_LIMIT_BIT) ? 1 : 0;
        d->on_neg_limit    = (js->flag & EMCMOT_JOINT_MIN_HARD_LIMIT_BIT) ? 1 : 0;
        d->ferror_exceeded = (js->flag & EMCMOT_JOINT_FERROR_BIT)  ? 1 : 0;
        d->fault           = (js->flag & EMCMOT_JOINT_FAULT_BIT)   ? 1 : 0;
        d->homed  = js->homed;
        d->homing = js->homing;
        d->pos_cmd = js->pos_cmd;
        d->pos_fb  = js->pos_fb;
        d->vel_cmd = js->vel_cmd;
        d->ferror  = js->ferror;
        d->ferror_high_mark = js->ferror_high_mark;
        d->min_pos_limit = js->min_pos_limit;
        d->max_pos_limit = js->max_pos_limit;
        d->min_ferror = js->min_ferror;
        d->max_ferror = js->max_ferror;
    }

    /* Spindles */
    int ns = cfg->numSpindles;
    if (ns > MOTSTAT_MAX_SPINDLES) ns = MOTSTAT_MAX_SPINDLES;
    for (int i = 0; i < ns; i++) {
        spindle_status_t *ss = &s.spindle_status[i];
        motstat_spindle_status_t *d = &status->spindles[i];
        d->speed     = ss->speed;
        d->scale     = ss->scale;
        d->direction = ss->direction;
        d->state     = ss->state;
        d->brake     = ss->brake;
        d->orient_fault = ss->orient_fault;
        d->orient_state = ss->orient_state;
        d->at_speed  = ss->at_speed;
        d->fault     = ss->fault;
        d->homed     = 0; /* spindle home status not in spindle_status_t */
        d->css_factor     = ss->css_factor;
        d->max_pos_speed  = ss->max_pos_speed;
        d->min_pos_speed  = ss->min_pos_speed;
        d->max_neg_speed  = ss->max_neg_speed;
        d->min_neg_speed  = ss->min_neg_speed;
    }

    /* Axes — pull limits from the axis module (internal, same cmod) */
    for (int i = 0; i < MOTSTAT_MAX_AXIS && i < EMCMOT_MAX_AXIS; i++) {
        motstat_axis_status_t *d = &status->axes[i];
        d->min_pos_limit = s.axis_status[i].min_pos_limit;
        d->max_pos_limit = s.axis_status[i].max_pos_limit;
        d->vel_limit = axis_get_vel_limit(mc->axis_inst, i);
        d->acc_limit = axis_get_acc_limit(mc->axis_inst, i);
    }

    /* I/O arrays */
    for (int i = 0; i < MOTSTAT_MAX_DIO && i < EMCMOT_MAX_DIO; i++) {
        status->synch_di[i] = s.synch_di[i];
        status->synch_do[i] = s.synch_do[i];
    }
    for (int i = 0; i < MOTSTAT_MAX_AIO && i < EMCMOT_MAX_AIO; i++) {
        status->analog_input[i]  = s.analog_input[i];
        status->analog_output[i] = s.analog_output[i];
    }

    return 0;
}

/* ================================================================
 * Hot-path accessors (no full status copy)
 * ================================================================ */

static int32_t h_get_pos_cmd(void *ctx, motstat_pose_t *pos)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    *pos = pose_to_motstat(&s.carte_pos_cmd);
    return 0;
}

static int32_t h_get_pos_fb(void *ctx, motstat_pose_t *pos)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    *pos = pose_to_motstat(&s.carte_pos_fb);
    return 0;
}

static int32_t h_get_exec_id(void *ctx)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    return s.id;
}

static int32_t h_get_queue_depth(void *ctx)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    return s.depth;
}

static int32_t h_get_inpos(void *ctx)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    return (s.motionFlag & EMCMOT_MOTION_INPOS_BIT) ? 1 : 0;
}

static int32_t h_get_command_num_echo(void *ctx)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    return s.commandNumEcho;
}

static int32_t h_get_command_status(void *ctx)
{
    CTX;
    emcmot_status_t s;
    if (read_status(mc, &s) < 0) return -1;
    return s.commandStatus;
}

/* ================================================================
 * Public: build the callback table
 * ================================================================ */

motstat_callbacks_t motstat_get_callbacks(motstat_ctx_t **ctx_out)
{
    motstat_ctx_t *mc = calloc(1, sizeof(*mc));
    if (!mc) {
        motstat_callbacks_t empty = {0};
        if (ctx_out) *ctx_out = NULL;
        return empty;
    }
    mc->mot = NULL;
    if (ctx_out) *ctx_out = mc;

    motstat_callbacks_t cb = {
        .ctx                  = mc,
        .get_status           = h_get_status,
        .get_pos_cmd          = h_get_pos_cmd,
        .get_pos_fb           = h_get_pos_fb,
        .get_exec_id          = h_get_exec_id,
        .get_queue_depth      = h_get_queue_depth,
        .get_inpos            = h_get_inpos,
        .get_command_num_echo = h_get_command_num_echo,
        .get_command_status   = h_get_command_status,
    };
    return cb;
}

void motstat_init_ctx(motstat_ctx_t *mc, emcmot_struct_t *mot, axis_inst_t *ai)
{
    if (mc) {
        mc->mot = mot;
        mc->axis_inst = ai;
    }
}
