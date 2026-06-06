/*
 * pid.c — cmod HAL component: PID controller with auto-tune.
 *
 * Single PID loop with P/I/D/FF0/FF1/FF2/FF3 and relay auto-tuner.
 *
 * Usage:
 *   load pid
 *   load pid debug=1    (exports optional debug pins)
 *
 * Original author: John Kasunich, Peter G. Vavaroutsos
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>

#define PI 3.141592653589

typedef enum {
    STATE_PID,
    STATE_TUNE_IDLE,
    STATE_TUNE_START,
    STATE_TUNE_POS,
    STATE_TUNE_NEG,
    STATE_TUNE_ABORT,
} State;

typedef enum {
    TYPE_PID,
    TYPE_PI_FF1,
} Mode;

typedef struct {
    gomc_hal_bit_t   *enable;
    gomc_hal_float_t *command;
    gomc_hal_float_t *commandv;
    gomc_hal_float_t *feedback;
    gomc_hal_float_t *feedbackv;
    gomc_hal_float_t *error;
    gomc_hal_float_t *output;
    gomc_hal_bit_t   *saturated;
    gomc_hal_float_t *saturated_s;
    gomc_hal_s32_t   *saturated_count;
    gomc_hal_float_t *pgain;
    gomc_hal_float_t *igain;
    gomc_hal_float_t *dgain;
    gomc_hal_float_t *ff0gain;
    gomc_hal_float_t *ff1gain;
    gomc_hal_float_t *ff2gain;
    gomc_hal_float_t *ff3gain;
    gomc_hal_float_t *deadband;
    gomc_hal_float_t *maxerror;
    gomc_hal_float_t *maxerror_i;
    gomc_hal_float_t *maxerror_d;
    gomc_hal_float_t *maxcmd_d;
    gomc_hal_float_t *maxcmd_dd;
    gomc_hal_float_t *maxcmd_ddd;
    gomc_hal_float_t *bias;
    gomc_hal_float_t *maxoutput;
    gomc_hal_bit_t   *index_enable;
    gomc_hal_bit_t   *error_previous_target;
    /* auto-tune */
    gomc_hal_float_t *tuneEffort;
    gomc_hal_u32_t   *tuneCycles;
    gomc_hal_u32_t   *tuneType;
    gomc_hal_bit_t   *pTuneMode;
    gomc_hal_bit_t   *pTuneStart;
    /* debug (may point to hal_malloc'd storage if not exported) */
    gomc_hal_float_t *error_i;
    gomc_hal_float_t *error_d;
    gomc_hal_float_t *cmd_d;
    gomc_hal_float_t *cmd_dd;
    gomc_hal_float_t *cmd_ddd;
    gomc_hal_float_t *ultimateGain;
    gomc_hal_float_t *ultimatePeriod;
} pid_hal_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    pid_hal_t *hal;
    int debug;
    /* internal state */
    double prev_error;
    double prev_cmd;
    double prev_fb;
    double limit_state;
    char prev_ie;
    gomc_hal_float_t *commandvds;
    gomc_hal_float_t *feedbackvds;
    /* auto-tune */
    State state;
    uint32_t cycleCount;
    uint32_t cyclePeriod;
    double cycleAmplitude;
    double totalTime;
    double avgAmplitude;
} inst_t;

static void pid_autotune(inst_t *inst, long period) {
    pid_hal_t *h = inst->hal;
    double error = *(h->command) - *(h->feedback);
    *(h->error) = error;

    if (!*(h->enable) || !*(h->pTuneMode))
        inst->state = STATE_TUNE_ABORT;

    switch (inst->state) {
    case STATE_TUNE_IDLE:
        if (*(h->pTuneStart)) inst->state = STATE_TUNE_START;
        break;
    case STATE_TUNE_START:
        inst->state = STATE_TUNE_POS;
        inst->cycleCount = 0;
        inst->cyclePeriod = 0;
        inst->cycleAmplitude = 0;
        inst->totalTime = 0;
        inst->avgAmplitude = 0;
        *(h->ultimateGain) = 0;
        *(h->ultimatePeriod) = 0;
        *(h->output) = *(h->bias) + fabs(*(h->tuneEffort));
        break;
    case STATE_TUNE_POS:
    case STATE_TUNE_NEG:
        inst->cyclePeriod += period;
        if (error < 0.0) {
            if (-error > inst->cycleAmplitude) inst->cycleAmplitude = -error;
            if (inst->state == STATE_TUNE_POS) {
                inst->state = STATE_TUNE_NEG;
                inst->cycleCount++;
                inst->avgAmplitude += inst->cycleAmplitude / *(h->tuneCycles);
                inst->cycleAmplitude = 0;
                inst->totalTime += inst->cyclePeriod * 0.000000001;
                inst->cyclePeriod = 0;
            }
            *(h->output) = *(h->bias) - fabs(*(h->tuneEffort));
        } else {
            if (error > inst->cycleAmplitude) inst->cycleAmplitude = error;
            if (inst->state == STATE_TUNE_NEG) {
                inst->state = STATE_TUNE_POS;
                inst->cycleCount++;
                inst->avgAmplitude += inst->cycleAmplitude / *(h->tuneCycles);
                inst->cycleAmplitude = 0;
                inst->totalTime += inst->cyclePeriod * 0.000000001;
                inst->cyclePeriod = 0;
            }
            *(h->output) = *(h->bias) + fabs(*(h->tuneEffort));
        }
        if (inst->cycleCount < *(h->tuneCycles)) break;

        *(h->ultimateGain) = (4.0 * fabs(*(h->tuneEffort))) / (PI * inst->avgAmplitude);
        *(h->ultimatePeriod) = 2.0 * inst->totalTime / *(h->tuneCycles);
        *(h->ff0gain) = 0;
        *(h->ff2gain) = 0;
        if (*(h->tuneType) == TYPE_PID) {
            *(h->pgain) = 0.6 * *(h->ultimateGain);
            *(h->igain) = 1.2 * *(h->ultimateGain) / *(h->ultimatePeriod);
            *(h->dgain) = (3.0/40.0) * *(h->ultimateGain) * *(h->ultimatePeriod);
            *(h->ff1gain) = 0;
        } else {
            *(h->pgain) = 0.45 * *(h->ultimateGain);
            *(h->igain) = 0.54 * *(h->ultimateGain) / *(h->ultimatePeriod);
            *(h->dgain) = 0;
            *(h->ff1gain) = 1;
        }
        /* fall through */
    case STATE_TUNE_ABORT:
    default:
        *(h->output) = *(h->bias);
        *(h->pTuneStart) = 0;
        inst->state = *(h->pTuneMode) ? STATE_TUNE_IDLE : STATE_PID;
        break;
    }
}

static void calc_pid(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    pid_hal_t *h = inst->hal;
    double tmp1, tmp2, tmp3, command, feedback;
    int enable;
    double periodfp, periodrecip;

    if (inst->state != STATE_PID) {
        pid_autotune(inst, period);
        return;
    }

    periodfp = period * 0.000000001;
    periodrecip = 1.0 / periodfp;
    enable = *(h->enable);
    command = *(h->command);
    feedback = *(h->feedback);

    if ((!(inst->prev_ie && !*(h->index_enable))) && *(h->error_previous_target))
        tmp1 = inst->prev_cmd - feedback;
    else
        tmp1 = command - feedback;

    *(h->error) = tmp1;

    if (*(h->pTuneMode)) {
        *(h->error_i) = 0; inst->prev_error = 0; *(h->error_d) = 0;
        inst->prev_cmd = 0; inst->limit_state = 0;
        *(h->cmd_d) = 0; *(h->cmd_dd) = 0;
        *(h->output) = 0;
        inst->state = STATE_TUNE_IDLE;
        return;
    }

    if (*(h->maxerror) != 0.0) {
        if (tmp1 > *(h->maxerror)) tmp1 = *(h->maxerror);
        else if (tmp1 < -*(h->maxerror)) tmp1 = -*(h->maxerror);
    }
    if (tmp1 > *(h->deadband)) tmp1 -= *(h->deadband);
    else if (tmp1 < -*(h->deadband)) tmp1 += *(h->deadband);
    else tmp1 = 0;

    if (enable) {
        if (tmp1 * inst->limit_state <= 0.0)
            *(h->error_i) += tmp1 * periodfp;
        if (*(h->maxerror_i) != 0.0) {
            if (*(h->error_i) > *(h->maxerror_i)) *(h->error_i) = *(h->maxerror_i);
            else if (*(h->error_i) < -*(h->maxerror_i)) *(h->error_i) = -*(h->maxerror_i);
        }
    } else {
        *(h->error_i) = 0;
    }

    if (!(inst->prev_ie && !*(h->index_enable))) {
        *(inst->commandvds) = (command - inst->prev_cmd) * periodrecip;
        *(inst->feedbackvds) = (feedback - inst->prev_fb) * periodrecip;
    }

    *(h->error_d) = *(h->commandv) - *(h->feedbackv);
    inst->prev_error = tmp1;
    if (*(h->maxerror_d) != 0.0) {
        if (*(h->error_d) > *(h->maxerror_d)) *(h->error_d) = *(h->maxerror_d);
        else if (*(h->error_d) < -*(h->maxerror_d)) *(h->error_d) = -*(h->maxerror_d);
    }

    tmp2 = *(h->cmd_d);
    *(h->cmd_d) = *(h->commandv);
    inst->prev_ie = *(h->index_enable);
    inst->prev_cmd = command;
    inst->prev_fb = feedback;

    if (*(h->maxcmd_d) != 0.0) {
        if (*(h->cmd_d) > *(h->maxcmd_d)) *(h->cmd_d) = *(h->maxcmd_d);
        else if (*(h->cmd_d) < -*(h->maxcmd_d)) *(h->cmd_d) = -*(h->maxcmd_d);
    }

    tmp3 = *(h->cmd_dd);
    *(h->cmd_dd) = (*(h->cmd_d) - tmp2) * periodrecip;
    if (*(h->maxcmd_dd) != 0.0) {
        if (*(h->cmd_dd) > *(h->maxcmd_dd)) *(h->cmd_dd) = *(h->maxcmd_dd);
        else if (*(h->cmd_dd) < -*(h->maxcmd_dd)) *(h->cmd_dd) = -*(h->maxcmd_dd);
    }

    *(h->cmd_ddd) = (*(h->cmd_dd) - tmp3) * periodrecip;
    if (*(h->maxcmd_ddd) != 0.0) {
        if (*(h->cmd_ddd) > *(h->maxcmd_ddd)) *(h->cmd_ddd) = *(h->maxcmd_ddd);
        else if (*(h->cmd_ddd) < -*(h->maxcmd_ddd)) *(h->cmd_ddd) = -*(h->maxcmd_ddd);
    }

    if (enable) {
        tmp1 = *(h->bias) + *(h->pgain) * tmp1 + *(h->igain) * *(h->error_i) +
               *(h->dgain) * *(h->error_d);
        tmp1 += command * *(h->ff0gain) + *(h->cmd_d) * *(h->ff1gain) +
                *(h->cmd_dd) * *(h->ff2gain) + *(h->cmd_ddd) * *(h->ff3gain);
        if (*(h->maxoutput) != 0.0) {
            if (tmp1 > *(h->maxoutput)) { tmp1 = *(h->maxoutput); inst->limit_state = 1.0; }
            else if (tmp1 < -*(h->maxoutput)) { tmp1 = -*(h->maxoutput); inst->limit_state = -1.0; }
            else inst->limit_state = 0.0;
        }
    } else {
        tmp1 = 0.0;
        inst->limit_state = 0.0;
    }

    *(h->output) = tmp1;
    if (inst->limit_state) {
        *(h->saturated) = 1;
        *(h->saturated_s) += period * 1e-9;
        if (*(h->saturated_count) != 2147483647) (*h->saturated_count)++;
    } else {
        *(h->saturated) = 0;
        *(h->saturated_s) = 0;
        *(h->saturated_count) = 0;
    }
}

static void inst_destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    pid_hal_t *h;
    int r, i;
    int debug = 0;
    char buf[GOMC_HAL_NAME_LEN + 1];

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "debug=", 6) == 0)
            debug = atoi(argv[i] + 6);
    }

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->debug = debug;
    inst->state = STATE_PID;

    inst->comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                                   GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) goto err;

    inst->hal = env->hal->malloc(env->hal->ctx, sizeof(pid_hal_t));
    if (!inst->hal) goto err;
    memset(inst->hal, 0, sizeof(pid_hal_t));
    h = inst->hal;

    /* standard pins */
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->enable, inst->comp_id, "%s.enable", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->command, inst->comp_id, "%s.command", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->commandv, inst->comp_id, "%s.command-deriv", name);
    if (r) goto err;
    inst->commandvds = h->commandv;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->feedback, inst->comp_id, "%s.feedback", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->feedbackv, inst->comp_id, "%s.feedback-deriv", name);
    if (r) goto err;
    inst->feedbackvds = h->feedbackv;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->error, inst->comp_id, "%s.error", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->output, inst->comp_id, "%s.output", name);
    if (r) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &h->saturated, inst->comp_id, "%s.saturated", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->saturated_s, inst->comp_id, "%s.saturated-s", name);
    if (r) goto err;
    r = gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_OUT, &h->saturated_count, inst->comp_id, "%s.saturated-count", name);
    if (r) goto err;

    /* gains */
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->pgain, inst->comp_id, "%s.Pgain", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->igain, inst->comp_id, "%s.Igain", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->dgain, inst->comp_id, "%s.Dgain", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->ff0gain, inst->comp_id, "%s.FF0", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->ff1gain, inst->comp_id, "%s.FF1", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->ff2gain, inst->comp_id, "%s.FF2", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->ff3gain, inst->comp_id, "%s.FF3", name);
    if (r) goto err;

    /* limits */
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->deadband, inst->comp_id, "%s.deadband", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxerror, inst->comp_id, "%s.maxerror", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxerror_i, inst->comp_id, "%s.maxerrorI", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxerror_d, inst->comp_id, "%s.maxerrorD", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxcmd_d, inst->comp_id, "%s.maxcmdD", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxcmd_dd, inst->comp_id, "%s.maxcmdDD", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxcmd_ddd, inst->comp_id, "%s.maxcmdDDD", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->bias, inst->comp_id, "%s.bias", name);
    if (r) goto err;
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &h->maxoutput, inst->comp_id, "%s.maxoutput", name);
    if (r) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->index_enable, inst->comp_id, "%s.index-enable", name);
    if (r) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->error_previous_target, inst->comp_id, "%s.error-previous-target", name);
    if (r) goto err;

    /* auto-tune */
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &h->tuneEffort, inst->comp_id, "%s.tune-effort", name);
    if (r) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->tuneCycles, inst->comp_id, "%s.tune-cycles", name);
    if (r) goto err;
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &h->tuneType, inst->comp_id, "%s.tune-type", name);
    if (r) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &h->pTuneMode, inst->comp_id, "%s.tune-mode", name);
    if (r) goto err;
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &h->pTuneStart, inst->comp_id, "%s.tune-start", name);
    if (r) goto err;

    /* debug/internal pins */
    if (debug) {
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->error_i, inst->comp_id, "%s.errorI", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->error_d, inst->comp_id, "%s.errorD", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->cmd_d, inst->comp_id, "%s.commandD", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->cmd_dd, inst->comp_id, "%s.commandDD", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->cmd_ddd, inst->comp_id, "%s.commandDDD", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->ultimateGain, inst->comp_id, "%s.ultimate-gain", name);
        if (r) goto err;
        r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &h->ultimatePeriod, inst->comp_id, "%s.ultimate-period", name);
        if (r) goto err;
    } else {
        /* allocate hidden storage */
        h->error_i = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->error_d = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->cmd_d = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->cmd_dd = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->cmd_ddd = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->ultimateGain = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        h->ultimatePeriod = env->hal->malloc(env->hal->ctx, sizeof(gomc_hal_float_t));
        if (!h->error_i || !h->error_d || !h->cmd_d || !h->cmd_dd ||
            !h->cmd_ddd || !h->ultimateGain || !h->ultimatePeriod)
            goto err;
    }

    /* defaults */
    *(h->error_previous_target) = 1;
    *(h->pgain) = 1.0;
    *(h->tuneCycles) = 50;
    *(h->tuneEffort) = 0.5;

    snprintf(buf, sizeof(buf), "%s.do-pid-calcs", name);
    r = env->hal->export_funct(env->hal->ctx, buf, calc_pid, inst, 1, 0, inst->comp_id);
    if (r) goto err;

    r = env->hal->ready(env->hal->ctx, inst->comp_id);
    if (r) goto err;

    *out = &inst->base;
    return 0;

err:
    if (inst->comp_id > 0)
        env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
