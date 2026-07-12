/* motion-logger — gomc interceptor/proxy cmod.
 *
 * Stands in for motmod: milltask is configured with [EMCMOT]EMCMOT=motion-logger,
 * so it looks up the motctl/motstat providers registered here.  This module
 * registers those providers under its own instance name, looks up the REAL
 * motmod by mot_instance=<name> (default "motmod"), and for every call it
 * LOGS the motctl command and FORWARDS to the real motmod (which does the real
 * trajectory planning and supplies real status).  No status faking.
 *
 * The log captures the gomc motctl command stream a program produces; the test
 * `expected.*` files are re-captured from gomc (self-regression), while
 * tests/milltask-parity remains the cross-tree C-vs-gomc check.
 *
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gomc_env.h"
#include "gomc_hal.h"
#include "gomc_log.h"
#include "motctl_api.h"
#include "motstat_api.h"

typedef struct {
    const cmod_env_t *env;
    const char *name;
    int comp_id;
    FILE *log;
    char mot_inst[64];
    const motctl_callbacks_t *real_ctl;
    const motstat_callbacks_t *real_stat;
    motctl_callbacks_t *ctl_cb;   /* our provider table (ctx = this) */
    motstat_callbacks_t *stat_cb;
} ml_t;

static void ml_log(ml_t *m, const char *fmt, ...)
{
    if (!m->log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(m->log, fmt, ap);
    va_end(ap);
    fflush(m->log);
}

/* Convenience: fetch the interceptor from a provider ctx. */
#define ML ((ml_t *)ctx)
/* Forward helper: call the real motmod's motctl callback. */
#define RC (ML->real_ctl)
#define RS (ML->real_stat)

/* ---- motctl provider wrappers: log + forward ---------------------------- */

static int32_t w_set_line(void *ctx, const motctl_pose_t *p, double vel,
        double ini_maxvel, double acc, int32_t mt, int32_t id,
        double feed, int32_t idx) {
    ml_log(ML, "SET_LINE x=%.6g, y=%.6g, z=%.6g, a=%.6g, b=%.6g, c=%.6g, u=%.6g, v=%.6g, w=%.6g, id=%d, motion_type=%d, vel=%.6g, ini_maxvel=%.6g, acc=%.6g, turn=%d\n",
        p->x, p->y, p->z, p->a, p->b, p->c, p->u, p->v, p->w, id, mt, vel, ini_maxvel, acc, -1);
    return RC->set_line(RC->ctx, p, vel, ini_maxvel, acc, mt, id, feed, idx);
}
static int32_t w_set_circle(void *ctx, const motctl_pose_t *p,
        const motctl_cartesian_t *center, const motctl_cartesian_t *normal,
        int32_t turn, double vel, double ini_maxvel, double acc,
        int32_t mt, int32_t id, double feed) {
    ml_log(ML, "SET_CIRCLE:\n    center: x=%.6g, y=%.6g, z=%.6g\n    normal: x=%.6g, y=%.6g, z=%.6g\n    id=%d, motion_type=%d, vel=%.6g, ini_maxvel=%.6g, acc=%.6g, turn=%d\n",
        center->x, center->y, center->z, normal->x, normal->y, normal->z, id, mt, vel, ini_maxvel, acc, turn);
    return RC->set_circle(RC->ctx, p, center, normal, turn, vel, ini_maxvel, acc, mt, id, feed);
}
static int32_t w_probe(void *ctx, const motctl_pose_t *p, double vel,
        double ini_maxvel, double acc, int32_t mt, uint8_t pt, int32_t id, double feed) {
    ml_log(ML, "PROBE\n");
    return RC->probe(RC->ctx, p, vel, ini_maxvel, acc, mt, pt, id, feed);
}
static int32_t w_rigid_tap(void *ctx, const motctl_pose_t *p, double vel,
        double ini_maxvel, double acc, double scale, int32_t id, double feed) {
    ml_log(ML, "RIGID_TAP\n");
    return RC->rigid_tap(RC->ctx, p, vel, ini_maxvel, acc, scale, id, feed);
}
static int32_t w_set_vel(void *ctx, double vel) {
    ml_log(ML, "SET_VEL vel=%.6g\n", vel);
    return RC->set_vel(RC->ctx, vel);
}
static int32_t w_set_vel_limit(void *ctx, double vel) {
    ml_log(ML, "SET_VEL_LIMIT vel=%.6g\n", vel);
    return RC->set_vel_limit(RC->ctx, vel);
}
static int32_t w_set_acc(void *ctx, double acc) {
    ml_log(ML, "SET_ACC acc=%.6g\n", acc);
    return RC->set_acc(RC->ctx, acc);
}
static int32_t w_set_term_cond(void *ctx, int32_t cond, double tol) {
    ml_log(ML, "SET_TERM_COND termCond=%d, tolerance=%.6g\n", cond, tol);
    return RC->set_term_cond(RC->ctx, cond, tol);
}
static int32_t w_set_spindlesync(void *ctx, double sync, int32_t mt) {
    ml_log(ML, "SET_SPINDLESYNC sync=%.6f, motion_type=%d\n", sync, mt);
    return RC->set_spindlesync(RC->ctx, sync, mt);
}
static int32_t w_set_offset(void *ctx, const motctl_pose_t *o) {
    ml_log(ML, "SET_OFFSET x=%.6g, y=%.6g, z=%.6g, a=%.6g, b=%.6g, c=%.6g, u=%.6g, v=%.6g, w=%.6g\n",
        o->x, o->y, o->z, o->a, o->b, o->c, o->u, o->v, o->w);
    return RC->set_offset(RC->ctx, o);
}
static int32_t w_abort(void *ctx)   { ml_log(ML, "ABORT\n");   return RC->abort(RC->ctx); }
static int32_t w_pause(void *ctx)   { ml_log(ML, "PAUSE\n");   return RC->pause(RC->ctx); }
static int32_t w_resume(void *ctx)  { ml_log(ML, "RESUME\n");  return RC->resume(RC->ctx); }
static int32_t w_step(void *ctx, int32_t id) { ml_log(ML, "STEP\n"); return RC->step(RC->ctx, id); }
static int32_t w_reverse(void *ctx) { ml_log(ML, "REVERSE\n"); return RC->reverse(RC->ctx); }
static int32_t w_forward(void *ctx) { ml_log(ML, "FORWARD\n"); return RC->forward(RC->ctx); }
static int32_t w_set_free(void *ctx)   { ml_log(ML, "FREE\n");   return RC->set_free(RC->ctx); }
static int32_t w_set_coord(void *ctx)  { ml_log(ML, "COORD\n");  return RC->set_coord(RC->ctx); }
static int32_t w_set_teleop(void *ctx) { ml_log(ML, "TELEOP\n"); return RC->set_teleop(RC->ctx); }
static int32_t w_jog_cont(void *ctx, int32_t n, double vel, int32_t t) {
    ml_log(ML, "JOG_CONT\n"); return RC->jog_cont(RC->ctx, n, vel, t);
}
static int32_t w_jog_incr(void *ctx, int32_t n, double vel, double incr, int32_t t) {
    ml_log(ML, "JOG_INCR\n"); return RC->jog_incr(RC->ctx, n, vel, incr, t);
}
static int32_t w_jog_abs(void *ctx, int32_t n, double vel, double pos, int32_t t) {
    ml_log(ML, "JOG_ABS\n"); return RC->jog_abs(RC->ctx, n, vel, pos, t);
}
static int32_t w_jog_abort(void *ctx, int32_t n, int32_t t) {
    ml_log(ML, "JOG_ABORT joint=%d\n", n); return RC->jog_abort(RC->ctx, n, t);
}
static int32_t w_spindle_on(void *ctx, int32_t s, double speed, double cf, double co, int32_t w) {
    ml_log(ML, "SPINDLE_ON speed=%f, css_factor=%f, xoffset=%f\n", speed, cf, co);
    return RC->spindle_on(RC->ctx, s, speed, cf, co, w);
}
static int32_t w_spindle_off(void *ctx, int32_t s) { ml_log(ML, "SPINDLE_OFF\n"); return RC->spindle_off(RC->ctx, s); }
static int32_t w_spindle_orient(void *ctx, int32_t s, double o, int32_t m) { ml_log(ML, "SPINDLE_ORIENT\n"); return RC->spindle_orient(RC->ctx, s, o, m); }
static int32_t w_spindle_increase(void *ctx, int32_t s) { ml_log(ML, "SPINDLE_INCREASE\n"); return RC->spindle_increase(RC->ctx, s); }
static int32_t w_spindle_decrease(void *ctx, int32_t s) { ml_log(ML, "SPINDLE_DECREASE\n"); return RC->spindle_decrease(RC->ctx, s); }
static int32_t w_spindle_brake_engage(void *ctx, int32_t s) { ml_log(ML, "SPINDLE_BRAKE_ENGAGE\n"); return RC->spindle_brake_engage(RC->ctx, s); }
static int32_t w_spindle_brake_release(void *ctx, int32_t s) { ml_log(ML, "SPINDLE_BRAKE_RELEASE\n"); return RC->spindle_brake_release(RC->ctx, s); }
static int32_t w_set_spindle_params(void *ctx, int32_t s, double a, double b, double c, double d, double hv, int32_t hs, double inc) {
    ml_log(ML, "SET_SPINDLE_PARAMS, %.2e, %.2e, %.2e, %.2e\n", a, b, c, d);
    return RC->set_spindle_params(RC->ctx, s, a, b, c, d, hv, hs, inc);
}
static int32_t w_set_spindle_scale(void *ctx, int32_t s, double sc) { ml_log(ML, "SPINDLE_SCALE\n"); return RC->set_spindle_scale(RC->ctx, s, sc); }
static int32_t w_spindle_scale_enable(void *ctx, int32_t s, int32_t e) { ml_log(ML, "SS_ENABLE\n"); return RC->spindle_scale_enable(RC->ctx, s, e); }
static int32_t w_set_dout(void *ctx, int32_t i, int32_t v) { ml_log(ML, "SET_DOUT\n"); return RC->set_dout(RC->ctx, i, v); }
static int32_t w_set_dout_synched(void *ctx, int32_t i, int32_t s, int32_t e) { ml_log(ML, "SET_DOUT\n"); return RC->set_dout_synched(RC->ctx, i, s, e); }
static int32_t w_set_aout(void *ctx, int32_t i, double v) { ml_log(ML, "SET_AOUT\n"); return RC->set_aout(RC->ctx, i, v); }
static int32_t w_set_aout_synched(void *ctx, int32_t i, double s, double e) { ml_log(ML, "SET_AOUT\n"); return RC->set_aout_synched(RC->ctx, i, s, e); }
static int32_t w_set_feed_scale(void *ctx, double s) { ml_log(ML, "FEED_SCALE\n"); return RC->set_feed_scale(RC->ctx, s); }
static int32_t w_set_rapid_scale(void *ctx, double s) { ml_log(ML, "RAPID_SCALE\n"); return RC->set_rapid_scale(RC->ctx, s); }
static int32_t w_feed_scale_enable(void *ctx, int32_t e) { ml_log(ML, "FS_ENABLE\n"); return RC->feed_scale_enable(RC->ctx, e); }
static int32_t w_feed_hold_enable(void *ctx, int32_t e) { ml_log(ML, "FH_ENABLE\n"); return RC->feed_hold_enable(RC->ctx, e); }
static int32_t w_adaptive_feed_enable(void *ctx, int32_t e) { ml_log(ML, "AF_ENABLE\n"); return RC->adaptive_feed_enable(RC->ctx, e); }
static int32_t w_set_max_feed_override(void *ctx, double m) { ml_log(ML, "SET_MAX_FEED_OVERRIDE %.6g\n", m); return RC->set_max_feed_override(RC->ctx, m); }
static int32_t w_enable(void *ctx)  { ml_log(ML, "ENABLE\n");  return RC->enable(RC->ctx); }
static int32_t w_disable(void *ctx) { ml_log(ML, "DISABLE\n"); return RC->disable(RC->ctx); }
static int32_t w_joint_activate(void *ctx, int32_t j) { ml_log(ML, "JOINT_ACTIVATE joint=%d\n", j); return RC->joint_activate(RC->ctx, j); }
static int32_t w_set_joint_position_limits(void *ctx, int32_t j, double mn, double mx) {
    ml_log(ML, "SET_JOINT_POSITION_LIMITS joint=%d, min=%.6g, max=%.6g\n", j, mn, mx);
    return RC->set_joint_position_limits(RC->ctx, j, mn, mx);
}
static int32_t w_set_joint_backlash(void *ctx, int32_t j, double b) { ml_log(ML, "SET_JOINT_BACKLASH joint=%d, backlash=%.6g\n", j, b); return RC->set_joint_backlash(RC->ctx, j, b); }
static int32_t w_set_joint_max_ferror(void *ctx, int32_t j, double f) { ml_log(ML, "SET_JOINT_MAX_FERROR joint=%d, maxFerror=%.6g\n", j, f); return RC->set_joint_max_ferror(RC->ctx, j, f); }
static int32_t w_set_joint_min_ferror(void *ctx, int32_t j, double f) { ml_log(ML, "SET_JOINT_MIN_FERROR joint=%d, minFerror=%.6g\n", j, f); return RC->set_joint_min_ferror(RC->ctx, j, f); }
static int32_t w_set_joint_vel_limit(void *ctx, int32_t j, double v) { ml_log(ML, "SET_JOINT_VEL_LIMIT joint=%d, vel=%.6g\n", j, v); return RC->set_joint_vel_limit(RC->ctx, j, v); }
static int32_t w_set_joint_acc_limit(void *ctx, int32_t j, double a) { ml_log(ML, "SET_JOINT_ACC_LIMIT joint=%d, acc=%.6g\n", j, a); return RC->set_joint_acc_limit(RC->ctx, j, a); }
static int32_t w_set_joint_jerk_limit(void *ctx, int32_t j, double k) { ml_log(ML, "SET_JOINT_JERK_LIMIT joint=%d, jerk=%.6g\n", j, k); return RC->set_joint_jerk_limit(RC->ctx, j, k); }
static int32_t w_set_joint_motor_offset(void *ctx, int32_t j, double o) { ml_log(ML, "SET_JOINT_MOTOR_OFFSET\n"); return RC->set_joint_motor_offset(RC->ctx, j, o); }
static int32_t w_set_joint_comp(void *ctx, int32_t j, double n, double f, double r) { ml_log(ML, "SET_JOINT_COMP\n"); return RC->set_joint_comp(RC->ctx, j, n, f, r); }
static int32_t w_override_limits(void *ctx, int32_t j) { ml_log(ML, "OVERRIDE_LIMITS\n"); return RC->override_limits(RC->ctx, j); }
static int32_t w_set_axis_position_limits(void *ctx, int32_t a, double mn, double mx) {
    ml_log(ML, "SET_AXIS_POSITION_LIMITS axis=%d, min=%.6g, max=%.6g\n", a, mn, mx);
    return RC->set_axis_position_limits(RC->ctx, a, mn, mx);
}
static int32_t w_set_axis_vel_limit(void *ctx, int32_t a, double v, double eov) { ml_log(ML, "SET_AXIS_VEL_LIMIT axis=%d vel=%.6g\n", a, v); return RC->set_axis_vel_limit(RC->ctx, a, v, eov); }
static int32_t w_set_axis_acc_limit(void *ctx, int32_t a, double ac, double eoa) { ml_log(ML, "SET_AXIS_ACC_LIMIT axis=%d, acc=%.6g\n", a, ac); return RC->set_axis_acc_limit(RC->ctx, a, ac, eoa); }
static int32_t w_set_axis_locking_joint(void *ctx, int32_t a, int32_t j) { ml_log(ML, "SET_AXIS_LOCKING_JOINT axis=%d joint=%d\n", a, j); return RC->set_axis_locking_joint(RC->ctx, a, j); }
static int32_t w_set_joint_homing_params(void *ctx, int32_t j, double off, double home, double hfv, double sv, double lv, int32_t fl, int32_t seq, int32_t vh) {
    ml_log(ML, "SET_JOINT_HOMING_PARAMS joint=%d, offset=%.6g, home=%.6g, search_vel=%.6g, latch_vel=%.6g, sequence=%d\n", j, off, home, sv, lv, seq);
    return RC->set_joint_homing_params(RC->ctx, j, off, home, hfv, sv, lv, fl, seq, vh);
}
static int32_t w_update_joint_homing_params(void *ctx, int32_t j, double off, double home, int32_t seq) {
    ml_log(ML, "UPDATE_JOINT_HOMING_PARAMS joint=%d, sequence=%d\n", j, seq);
    return RC->update_joint_homing_params(RC->ctx, j, off, home, seq);
}
static int32_t w_joint_home(void *ctx, int32_t j) { ml_log(ML, "JOINT_HOME joint=%d\n", j); return RC->joint_home(RC->ctx, j); }
static int32_t w_joint_unhome(void *ctx, int32_t j) { ml_log(ML, "JOINT_UNHOME joint=%d\n", j); return RC->joint_unhome(RC->ctx, j); }
static int32_t w_clear_probe_flags(void *ctx) { ml_log(ML, "CLEAR_PROBE_FLAGS\n"); return RC->clear_probe_flags(RC->ctx); }
static int32_t w_set_probe_err_inhibit(void *ctx, int32_t ji, int32_t hi) { ml_log(ML, "SETUP_SET_PROBE_ERR_INHIBIT %d %d\n", ji, hi); return RC->set_probe_err_inhibit(RC->ctx, ji, hi); }
static int32_t w_set_world_home(void *ctx, const motctl_pose_t *p) {
    ml_log(ML, "SET_WORLD_HOME x=%.6g, y=%.6g, z=%.6g, a=%.6g, b=%.6g, c=%.6g, u=%.6g, v=%.6g, w=%.6g\n",
        p->x, p->y, p->z, p->a, p->b, p->c, p->u, p->v, p->w);
    return RC->set_world_home(RC->ctx, p);
}
static int32_t w_set_debug(void *ctx, int32_t lvl) { ml_log(ML, "SET_DEBUG\n"); return RC->set_debug(RC->ctx, lvl); }

/* ---- motstat provider wrappers: pure forward (real status) -------------- */

static int32_t s_get_status(void *ctx, motstat_motion_status_t *st) { return RS->get_status(RS->ctx, st); }
static int32_t s_get_pos_cmd(void *ctx, motstat_pose_t *p) { return RS->get_pos_cmd(RS->ctx, p); }
static int32_t s_get_pos_fb(void *ctx, motstat_pose_t *p) { return RS->get_pos_fb(RS->ctx, p); }
static int32_t s_get_exec_id(void *ctx) { return RS->get_exec_id(RS->ctx); }
static int32_t s_get_queue_depth(void *ctx) { return RS->get_queue_depth(RS->ctx); }
static int32_t s_get_inpos(void *ctx) { return RS->get_inpos(RS->ctx); }
static int32_t s_get_command_num_echo(void *ctx) { return RS->get_command_num_echo(RS->ctx); }
static int32_t s_get_command_status(void *ctx) { return RS->get_command_status(RS->ctx); }
static int32_t s_get_synch_di(void *ctx, int32_t i) { return RS->get_synch_di(RS->ctx, i); }
static double s_get_analog_input(void *ctx, int32_t i) { return RS->get_analog_input(RS->ctx, i); }

/* ---- cmod lifecycle ----------------------------------------------------- */

static int ml_Init(cmod_t *self)
{
    ml_t *m = (ml_t *)self->priv;
    const gomc_log_t *log = (const gomc_log_t *)m->env->log;

    m->real_ctl = motctl_api_get(m->env->api, m->mot_inst);
    m->real_stat = motstat_api_get(m->env->api, m->mot_inst);
    if (!m->real_ctl || !m->real_stat) {
        gomc_log_errorf(log, m->name,
            "motion-logger: could not find real motion instance '%s' (motctl=%p motstat=%p)\n",
            m->mot_inst, (void *)m->real_ctl, (void *)m->real_stat);
        return -1;
    }
    gomc_log_infof(log, m->name, "motion-logger: proxying to '%s'\n", m->mot_inst);
    return 0;
}

static void ml_Destroy(cmod_t *self)
{
    ml_t *m = (ml_t *)self->priv;
    if (!m) return;
    if (m->log) { fclose(m->log); m->log = NULL; }
    if (m->comp_id >= 0 && m->env && m->env->hal) {
        const gomc_hal_t *hal = (const gomc_hal_t *)m->env->hal;
        hal->exit(hal->ctx, m->comp_id);
    }
    free(m->ctl_cb);
    free(m->stat_cb);
    free(m);
    free(self);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    const gomc_log_t *log = (const gomc_log_t *)env->log;
    const gomc_hal_t *hal = (const gomc_hal_t *)env->hal;

    ml_t *m = calloc(1, sizeof(*m));
    if (!m) return -1;
    m->env = env;
    m->name = name;
    m->comp_id = -1;
    strncpy(m->mot_inst, "motmod", sizeof(m->mot_inst) - 1);

    char logfile[256] = "out.motion-logger";
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "mot_instance=", 13) == 0) {
            strncpy(m->mot_inst, a + 13, sizeof(m->mot_inst) - 1);
        } else if (strncmp(a, "logfile=", 8) == 0) {
            strncpy(logfile, a + 8, sizeof(logfile) - 1);
        } else if (strchr(a, '=') == NULL) {
            /* bare arg = logfile name (classic `motion-logger LOGFILE`) */
            strncpy(logfile, a, sizeof(logfile) - 1);
        }
    }

    m->log = fopen(logfile, "w");
    if (!m->log) {
        gomc_log_errorf(log, name, "motion-logger: cannot open logfile '%s'\n", logfile);
        free(m);
        return -1;
    }

    /* Register a HAL component so we are a well-formed RT module. */
    m->comp_id = hal->init(hal->ctx, name, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (m->comp_id < 0) {
        gomc_log_errorf(log, name, "motion-logger: hal init failed\n");
        fclose(m->log);
        free(m);
        return -1;
    }

    /* Register motctl + motstat providers under our instance name so milltask
       (EMCMOT=<name>) looks us up. */
    m->ctl_cb = calloc(1, sizeof(*m->ctl_cb));
    m->stat_cb = calloc(1, sizeof(*m->stat_cb));
    if (!m->ctl_cb || !m->stat_cb) { hal->exit(hal->ctx, m->comp_id); fclose(m->log); free(m->ctl_cb); free(m->stat_cb); free(m); return -1; }

    *m->ctl_cb = (motctl_callbacks_t){
        .ctx = m,
        .set_line = w_set_line, .set_circle = w_set_circle, .probe = w_probe,
        .rigid_tap = w_rigid_tap, .set_vel = w_set_vel, .set_vel_limit = w_set_vel_limit,
        .set_acc = w_set_acc, .set_term_cond = w_set_term_cond,
        .set_spindlesync = w_set_spindlesync, .set_offset = w_set_offset,
        .abort = w_abort, .pause = w_pause, .resume = w_resume, .step = w_step,
        .reverse = w_reverse, .forward = w_forward, .set_free = w_set_free,
        .set_coord = w_set_coord, .set_teleop = w_set_teleop,
        .jog_cont = w_jog_cont, .jog_incr = w_jog_incr, .jog_abs = w_jog_abs, .jog_abort = w_jog_abort,
        .spindle_on = w_spindle_on, .spindle_off = w_spindle_off, .spindle_orient = w_spindle_orient,
        .spindle_increase = w_spindle_increase, .spindle_decrease = w_spindle_decrease,
        .spindle_brake_engage = w_spindle_brake_engage, .spindle_brake_release = w_spindle_brake_release,
        .set_spindle_params = w_set_spindle_params, .set_spindle_scale = w_set_spindle_scale,
        .spindle_scale_enable = w_spindle_scale_enable,
        .set_dout = w_set_dout, .set_dout_synched = w_set_dout_synched,
        .set_aout = w_set_aout, .set_aout_synched = w_set_aout_synched,
        .set_feed_scale = w_set_feed_scale, .set_rapid_scale = w_set_rapid_scale,
        .feed_scale_enable = w_feed_scale_enable, .feed_hold_enable = w_feed_hold_enable,
        .adaptive_feed_enable = w_adaptive_feed_enable, .set_max_feed_override = w_set_max_feed_override,
        .enable = w_enable, .disable = w_disable, .joint_activate = w_joint_activate,
        .set_joint_position_limits = w_set_joint_position_limits, .set_joint_backlash = w_set_joint_backlash,
        .set_joint_max_ferror = w_set_joint_max_ferror, .set_joint_min_ferror = w_set_joint_min_ferror,
        .set_joint_vel_limit = w_set_joint_vel_limit, .set_joint_acc_limit = w_set_joint_acc_limit,
        .set_joint_jerk_limit = w_set_joint_jerk_limit, .set_joint_motor_offset = w_set_joint_motor_offset,
        .set_joint_comp = w_set_joint_comp, .override_limits = w_override_limits,
        .set_axis_position_limits = w_set_axis_position_limits, .set_axis_vel_limit = w_set_axis_vel_limit,
        .set_axis_acc_limit = w_set_axis_acc_limit, .set_axis_locking_joint = w_set_axis_locking_joint,
        .set_joint_homing_params = w_set_joint_homing_params, .update_joint_homing_params = w_update_joint_homing_params,
        .joint_home = w_joint_home, .joint_unhome = w_joint_unhome,
        .clear_probe_flags = w_clear_probe_flags, .set_probe_err_inhibit = w_set_probe_err_inhibit,
        .set_world_home = w_set_world_home, .set_debug = w_set_debug,
    };
    if (motctl_api_register(env->api, name, m->ctl_cb) != 0) {
        gomc_log_errorf(log, name, "motion-logger: motctl register failed\n");
        hal->exit(hal->ctx, m->comp_id); fclose(m->log); free(m->ctl_cb); free(m->stat_cb); free(m); return -1;
    }

    *m->stat_cb = (motstat_callbacks_t){
        .ctx = m,
        .get_status = s_get_status, .get_pos_cmd = s_get_pos_cmd, .get_pos_fb = s_get_pos_fb,
        .get_exec_id = s_get_exec_id, .get_queue_depth = s_get_queue_depth, .get_inpos = s_get_inpos,
        .get_command_num_echo = s_get_command_num_echo, .get_command_status = s_get_command_status,
        .get_synch_di = s_get_synch_di, .get_analog_input = s_get_analog_input,
    };
    if (motstat_api_register(env->api, name, m->stat_cb) != 0) {
        gomc_log_errorf(log, name, "motion-logger: motstat register failed\n");
        hal->exit(hal->ctx, m->comp_id); fclose(m->log); free(m->ctl_cb); free(m->stat_cb); free(m); return -1;
    }

    cmod_t *c = calloc(1, sizeof(*c));
    if (!c) { hal->exit(hal->ctx, m->comp_id); fclose(m->log); free(m->ctl_cb); free(m->stat_cb); free(m); return -1; }
    c->Init = ml_Init;
    c->Start = NULL;
    c->Stop = NULL;
    c->Destroy = ml_Destroy;
    c->priv = m;

    gomc_log_infof(log, name, "motion-logger: New('%s') logfile='%s' mot_instance='%s'\n", name, logfile, m->mot_inst);
    *out = c;
    return 0;
}
