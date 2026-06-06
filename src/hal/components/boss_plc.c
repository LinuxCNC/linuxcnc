/*
 * boss_plc.c — cmod HAL component: Bridgeport Boss PLC.
 *
 * Hard-coded PLC for a Bridgeport Boss milling machine.
 * Handles feed hold logic, limit conditioning, amp fault detection,
 * spindle state machine, and jog scale selection.
 *
 * Usage:
 *   load boss_plc
 *
 * Original author: Pete Vavaroutsos
 * Converted to cmod API: 2026
 * License: GPL Version 2
 */

#include "gomc_env.h"
#include <string.h>
#include <errno.h>

#define TRUE  1
#define FALSE 0
#define NUM_JOG_SEL 3
#define NUM_AXIS    4

static const char axis_names[NUM_AXIS] = { 'x', 'y', 'z', 'a' };

/* Timer */
typedef struct {
    int enabled;
    uint32_t nSec;
    uint32_t count;
    uint32_t timeout;
    int mode; /* 0=one-shot */
} Timer;

static void Timer_Init(Timer *t) { t->enabled = FALSE; }
static void Timer_Enable(Timer *t) { t->enabled = TRUE; t->count = 0; t->nSec = 0; }
static void Timer_Disable(Timer *t) { t->enabled = FALSE; }
static int Timer_IsEnabled(Timer *t) { return t->enabled; }
static void Timer_SetTimeout(Timer *t, uint32_t ms) { t->count = 0; t->timeout = ms; }
static void Timer_Update(Timer *t, long period) {
    if (!t->enabled) return;
    t->nSec += period;
    if (t->nSec > 1000000) { t->count += t->nSec / 1000000; t->nSec %= 1000000; }
    if (t->count >= t->timeout) t->enabled = FALSE;
}

/* Limit sub-object */
typedef enum { LS_INIT, LS_ON_LIMIT, LS_NO_MOTION, LS_POS_MOTION, LS_NEG_MOTION } LimitState;

typedef struct {
    gomc_hal_float_t *pPositionIn;
    gomc_hal_bit_t   *pJogEnIn;
    gomc_hal_bit_t   *pIn;
    gomc_hal_bit_t   *pPosOut;
    gomc_hal_bit_t   *pNegOut;
    LimitState state;
    double position;
    int limitPos, limitNeg;
} Limit;

static void Limit_Init(Limit *l) { l->state = LS_INIT; }
static int Limit_IsActive(Limit *l) { return *(l->pIn); }

static void Limit_Refresh(Limit *l, int override) {
    switch (l->state) {
    case LS_INIT:
    default:
        l->state = LS_ON_LIMIT;
        l->limitNeg = l->limitPos = 1;
        l->position = *(l->pPositionIn);
        /* fall through */
    case LS_ON_LIMIT:
        if (!*(l->pIn)) {
            l->limitNeg = l->limitPos = 0;
            if (*(l->pPositionIn) == l->position) l->state = LS_NO_MOTION;
            else if (*(l->pPositionIn) > l->position) l->state = LS_POS_MOTION;
            else l->state = LS_NEG_MOTION;
        }
        break;
    case LS_NO_MOTION:
        if (*(l->pIn)) { l->state = LS_ON_LIMIT; l->limitNeg = l->limitPos = 1; }
        else if (*(l->pPositionIn) > l->position) l->state = LS_POS_MOTION;
        else if (*(l->pPositionIn) < l->position) l->state = LS_NEG_MOTION;
        break;
    case LS_POS_MOTION:
        if (*(l->pIn)) { l->state = LS_ON_LIMIT; l->limitPos = 1; }
        else if (*(l->pPositionIn) == l->position) l->state = LS_NO_MOTION;
        else if (*(l->pPositionIn) < l->position) l->state = LS_NEG_MOTION;
        break;
    case LS_NEG_MOTION:
        if (*(l->pIn)) { l->state = LS_ON_LIMIT; l->limitNeg = 1; }
        else if (*(l->pPositionIn) == l->position) l->state = LS_NO_MOTION;
        else if (*(l->pPositionIn) > l->position) l->state = LS_POS_MOTION;
        break;
    }
    l->position = *(l->pPositionIn);
    *(l->pPosOut) = l->limitPos && !(*(l->pJogEnIn) && override);
    *(l->pNegOut) = l->limitNeg && !(*(l->pJogEnIn) && override);
}

/* Amp sub-object */
typedef struct {
    gomc_hal_bit_t *pEnableIn;
    gomc_hal_bit_t *pReadyIn;
    gomc_hal_bit_t *pFaultOut;
    Timer timer;
    int lastEnable;
} Amp;

static void Amp_Init(Amp *a) { a->lastEnable = 0; Timer_Init(&a->timer); }

static void Amp_Refresh(Amp *a, long period, uint32_t readyDelay) {
    Timer_Update(&a->timer, period);
    if (*(a->pEnableIn)) {
        if (!a->lastEnable) { Timer_SetTimeout(&a->timer, readyDelay); Timer_Enable(&a->timer); }
    } else {
        Timer_Disable(&a->timer);
    }
    *(a->pFaultOut) = *(a->pEnableIn) && !*(a->pReadyIn) && !Timer_IsEnabled(&a->timer);
    a->lastEnable = *(a->pEnableIn);
}

/* Spindle state */
typedef enum { SS_OFF, SS_WAIT_BRAKE_OFF, SS_WAIT_ON, SS_ON, SS_WAIT_OFF, SS_WAIT_BRAKE_ON } SpindleState;

/* Main PLC pins */
typedef struct {
    /* params (IO) */
    gomc_hal_u32_t   *ampReadyDelay;
    gomc_hal_u32_t   *brakeOnDelay;
    gomc_hal_u32_t   *brakeOffDelay;
    gomc_hal_float_t *spindleLoToHi;
    gomc_hal_float_t *jogScale[NUM_JOG_SEL];
    /* feed */
    gomc_hal_bit_t   *pCycleStartIn;
    gomc_hal_bit_t   *pCycleHoldIn;
    gomc_hal_bit_t   *pFeedHoldOut;
    gomc_hal_float_t *pAdaptiveFeedIn;
    gomc_hal_float_t *pAdaptiveFeedOut;
    gomc_hal_bit_t   *pToolChangeIn;
    gomc_hal_bit_t   *pToolChangedOut;
    gomc_hal_bit_t   *pWaitUserOut;
    gomc_hal_bit_t   *pMistOnIn;
    gomc_hal_bit_t   *pMistOnOut;
    gomc_hal_bit_t   *pFloodOnIn;
    gomc_hal_bit_t   *pFloodOnOut;
    /* limits */
    gomc_hal_bit_t   *pLimitOverrideIn;
    gomc_hal_bit_t   *pLimitActiveOut;
    gomc_hal_bit_t   *pZJogEnIn;
    gomc_hal_bit_t   *pZLimitPosIn;
    gomc_hal_bit_t   *pZLimitNegIn;
    gomc_hal_bit_t   *pZLimitPosOut;
    gomc_hal_bit_t   *pZLimitNegOut;
    /* spindle */
    gomc_hal_float_t *pSpindleSpeedIn;
    gomc_hal_bit_t   *pSpindleIsOnIn;
    gomc_hal_bit_t   *pSpindleFwdOut;
    gomc_hal_bit_t   *pSpindleRevOut;
    gomc_hal_bit_t   *pSpindleIncIn;
    gomc_hal_bit_t   *pSpindleDecIn;
    gomc_hal_bit_t   *pSpindleIncOut;
    gomc_hal_bit_t   *pSpindleDecOut;
    gomc_hal_bit_t   *pBrakeEnIn;
    gomc_hal_bit_t   *pBrakeEnOut;
    /* jog */
    gomc_hal_bit_t   *pJogSelIn[NUM_JOG_SEL];
    gomc_hal_float_t *pJogScaleOut;
} plc_pins_t;

typedef struct {
    cmod_t base;
    const cmod_env_t *env;
    int comp_id;
    char name[GOMC_HAL_NAME_LEN + 1];
    plc_pins_t *p;
    /* sub-objects */
    Limit xLimit;
    Limit yLimit;
    Amp amps[NUM_AXIS];
    /* spindle state */
    SpindleState spindleState;
    Timer spindleTimer;
    double lastSpindleSpeed;
    int lastCycleStart;
} inst_t;

static void refresh_feed(inst_t *inst) {
    plc_pins_t *p = inst->p;
    int riseCycleStart = !inst->lastCycleStart && *(p->pCycleStartIn);
    inst->lastCycleStart = *(p->pCycleStartIn);

    *(p->pFeedHoldOut) = *(p->pCycleHoldIn)
        || (*(p->pSpindleSpeedIn) != 0 && !*(p->pSpindleIsOnIn))
        || (*(p->pSpindleIsOnIn) && (inst->lastSpindleSpeed != *(p->pSpindleSpeedIn)))
        || (*(p->pFeedHoldOut) && !riseCycleStart);
    inst->lastSpindleSpeed = *(p->pSpindleSpeedIn);

    if (*(p->pLimitOverrideIn) && (*(p->pAdaptiveFeedIn) > 0.01))
        *(p->pAdaptiveFeedOut) = 0.01;
    else
        *(p->pAdaptiveFeedOut) = *(p->pAdaptiveFeedIn);

    *(p->pToolChangedOut) = (*(p->pToolChangeIn) && riseCycleStart)
        || (*(p->pToolChangedOut) && *(p->pToolChangeIn));
    *(p->pWaitUserOut) = *(p->pFeedHoldOut)
        || (*(p->pToolChangeIn) && !*(p->pToolChangedOut));
    *(p->pMistOnOut) = *(p->pMistOnIn) && !*(p->pToolChangeIn);
    *(p->pFloodOnOut) = *(p->pFloodOnIn) && !*(p->pToolChangeIn);
}

static void refresh_limits(inst_t *inst) {
    plc_pins_t *p = inst->p;
    Limit_Refresh(&inst->xLimit, *(p->pLimitOverrideIn));
    Limit_Refresh(&inst->yLimit, *(p->pLimitOverrideIn));
    *(p->pZLimitPosOut) = *(p->pZLimitPosIn) && !(*(p->pZJogEnIn) && *(p->pLimitOverrideIn));
    *(p->pZLimitNegOut) = *(p->pZLimitNegIn) && !(*(p->pZJogEnIn) && *(p->pLimitOverrideIn));
    *(p->pLimitActiveOut) = Limit_IsActive(&inst->xLimit) || Limit_IsActive(&inst->yLimit)
        || *(p->pZLimitPosIn) || *(p->pZLimitNegIn);
}

static void refresh_amps(inst_t *inst, long period) {
    int i;
    for (i = 0; i < NUM_AXIS; i++)
        Amp_Refresh(&inst->amps[i], period, *(inst->p->ampReadyDelay));
}

static void refresh_spindle(inst_t *inst, long period) {
    plc_pins_t *p = inst->p;
    Timer_Update(&inst->spindleTimer, period);

    switch (inst->spindleState) {
    case SS_OFF:
        if (!*(p->pBrakeEnIn)) {
            inst->spindleState = SS_WAIT_BRAKE_OFF;
            *(p->pBrakeEnOut) = 0;
            Timer_SetTimeout(&inst->spindleTimer, *(p->brakeOffDelay));
            Timer_Enable(&inst->spindleTimer);
        }
        break;
    case SS_WAIT_BRAKE_OFF:
        if (*(p->pBrakeEnIn)) {
            inst->spindleState = SS_OFF;
            *(p->pBrakeEnOut) = 1;
            Timer_Disable(&inst->spindleTimer);
        } else if (*(p->pSpindleSpeedIn) != 0.0 && !Timer_IsEnabled(&inst->spindleTimer)) {
            inst->spindleState = SS_WAIT_ON;
            if (*(p->pSpindleSpeedIn) > *(p->spindleLoToHi)
                || (*(p->pSpindleSpeedIn) < 0.0 && *(p->pSpindleSpeedIn) >= -*(p->spindleLoToHi)))
                *(p->pSpindleFwdOut) = 1;
            else
                *(p->pSpindleRevOut) = 1;
        }
        break;
    case SS_WAIT_ON:
        if (*(p->pSpindleIsOnIn)) {
            inst->spindleState = SS_ON;
        } else if (*(p->pSpindleSpeedIn) == 0.0) {
            inst->spindleState = SS_WAIT_BRAKE_OFF;
            *(p->pSpindleFwdOut) = 0; *(p->pSpindleRevOut) = 0;
        }
        break;
    case SS_ON:
        if (*(p->pSpindleSpeedIn) == 0.0) {
            inst->spindleState = SS_WAIT_OFF;
            *(p->pSpindleFwdOut) = 0; *(p->pSpindleRevOut) = 0;
        }
        break;
    case SS_WAIT_OFF:
        if (!*(p->pSpindleIsOnIn)) {
            inst->spindleState = SS_WAIT_BRAKE_ON;
            Timer_SetTimeout(&inst->spindleTimer, *(p->brakeOnDelay));
            Timer_Enable(&inst->spindleTimer);
        }
        break;
    case SS_WAIT_BRAKE_ON:
        if (!Timer_IsEnabled(&inst->spindleTimer))
            inst->spindleState = SS_WAIT_BRAKE_OFF;
        break;
    default:
        inst->spindleState = SS_WAIT_OFF;
        *(p->pSpindleFwdOut) = 0; *(p->pSpindleRevOut) = 0;
        break;
    }

    *(p->pSpindleIncOut) = *(p->pSpindleIncIn) && !*(p->pSpindleDecIn) && *(p->pSpindleIsOnIn);
    *(p->pSpindleDecOut) = *(p->pSpindleDecIn) && !*(p->pSpindleIncIn) && *(p->pSpindleIsOnIn);
}

static void refresh_jog(inst_t *inst) {
    int i;
    for (i = 0; i < NUM_JOG_SEL; i++) {
        if (*(inst->p->pJogSelIn[i])) {
            *(inst->p->pJogScaleOut) = *(inst->p->jogScale[i]);
            break;
        }
    }
}

static void plc_refresh(void *arg, long period) {
    inst_t *inst = (inst_t *)arg;
    refresh_feed(inst);
    refresh_limits(inst);
    refresh_amps(inst, period);
    refresh_spindle(inst, period);
    refresh_jog(inst);
}

static void inst_destroy(cmod_t *self) {
    inst_t *inst = (inst_t *)self;
    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

/* Helper macros for pin creation */
#define PIN_BIT_IN(ptr, fmt, ...) do { \
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IN, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_BIT_OUT(ptr, fmt, ...) do { \
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_BIT_IO(ptr, fmt, ...) do { \
    r = gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_IO, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_FLOAT_IN(ptr, fmt, ...) do { \
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_FLOAT_OUT(ptr, fmt, ...) do { \
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_FLOAT_IO(ptr, fmt, ...) do { \
    r = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)
#define PIN_U32_IO(ptr, fmt, ...) do { \
    r = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &(ptr), cid, fmt, __VA_ARGS__); \
    if (r) goto err; } while(0)

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    inst_t *inst;
    plc_pins_t *p;
    int r, i;
    int cid;
    char buf[GOMC_HAL_NAME_LEN + 1];

    (void)argc; (void)argv;

    inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;

    inst->base.Destroy = inst_destroy;
    inst->env = env;
    strncpy(inst->name, name, sizeof(inst->name) - 1);
    inst->spindleState = SS_OFF;
    inst->lastCycleStart = 1;
    Timer_Init(&inst->spindleTimer);
    Limit_Init(&inst->xLimit);
    Limit_Init(&inst->yLimit);
    for (i = 0; i < NUM_AXIS; i++) Amp_Init(&inst->amps[i]);

    cid = env->hal->init(env->hal->ctx, name, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (cid < 0) goto err;
    inst->comp_id = cid;

    inst->p = env->hal->malloc(env->hal->ctx, sizeof(plc_pins_t));
    if (!inst->p) goto err;
    memset(inst->p, 0, sizeof(plc_pins_t));
    p = inst->p;

    /* params (IO pins) */
    PIN_U32_IO(p->ampReadyDelay, "%s.amp-ready-delay", name);
    PIN_U32_IO(p->brakeOnDelay, "%s.brake-on-delay", name);
    PIN_U32_IO(p->brakeOffDelay, "%s.brake-off-delay", name);
    PIN_FLOAT_IO(p->spindleLoToHi, "%s.spindle-lo-to-hi", name);
    for (i = 0; i < NUM_JOG_SEL; i++) {
        PIN_FLOAT_IO(p->jogScale[i], "%s.jog-scale-%d", name, i);
    }

    /* feed pins */
    PIN_BIT_IN(p->pCycleStartIn, "%s.cycle-start-in", name);
    PIN_BIT_IN(p->pCycleHoldIn, "%s.cycle-hold-in", name);
    PIN_BIT_OUT(p->pFeedHoldOut, "%s.feed-hold-out", name);
    PIN_FLOAT_IN(p->pAdaptiveFeedIn, "%s.adaptive-feed-in", name);
    PIN_FLOAT_OUT(p->pAdaptiveFeedOut, "%s.adaptive-feed-out", name);
    PIN_BIT_IN(p->pToolChangeIn, "%s.tool-change-in", name);
    PIN_BIT_OUT(p->pToolChangedOut, "%s.tool-changed-out", name);
    PIN_BIT_OUT(p->pWaitUserOut, "%s.wait-user-out", name);
    PIN_BIT_IN(p->pMistOnIn, "%s.mist-on-in", name);
    PIN_BIT_OUT(p->pMistOnOut, "%s.mist-on-out", name);
    PIN_BIT_IN(p->pFloodOnIn, "%s.flood-on-in", name);
    PIN_BIT_OUT(p->pFloodOnOut, "%s.flood-on-out", name);

    /* limit pins */
    PIN_BIT_IN(p->pLimitOverrideIn, "%s.limit-override-in", name);
    PIN_BIT_OUT(p->pLimitActiveOut, "%s.limit-active-out", name);
    /* x/y limits */
    PIN_FLOAT_IN(inst->xLimit.pPositionIn, "%s.x-position-in", name);
    PIN_BIT_IN(inst->xLimit.pJogEnIn, "%s.x-jog-en-in", name);
    PIN_BIT_IN(inst->xLimit.pIn, "%s.x-limit-in", name);
    PIN_BIT_OUT(inst->xLimit.pPosOut, "%s.x-limit-pos-out", name);
    PIN_BIT_OUT(inst->xLimit.pNegOut, "%s.x-limit-neg-out", name);
    PIN_FLOAT_IN(inst->yLimit.pPositionIn, "%s.y-position-in", name);
    PIN_BIT_IN(inst->yLimit.pJogEnIn, "%s.y-jog-en-in", name);
    PIN_BIT_IN(inst->yLimit.pIn, "%s.y-limit-in", name);
    PIN_BIT_OUT(inst->yLimit.pPosOut, "%s.y-limit-pos-out", name);
    PIN_BIT_OUT(inst->yLimit.pNegOut, "%s.y-limit-neg-out", name);
    /* z limits */
    PIN_BIT_IN(p->pZJogEnIn, "%s.z-jog-en-in", name);
    PIN_BIT_IN(p->pZLimitPosIn, "%s.z-limit-pos-in", name);
    PIN_BIT_IN(p->pZLimitNegIn, "%s.z-limit-neg-in", name);
    PIN_BIT_OUT(p->pZLimitPosOut, "%s.z-limit-pos-out", name);
    PIN_BIT_OUT(p->pZLimitNegOut, "%s.z-limit-neg-out", name);

    /* amp pins */
    for (i = 0; i < NUM_AXIS; i++) {
        PIN_BIT_IN(inst->amps[i].pEnableIn, "%s.%c-amp-enable-in", name, axis_names[i]);
        PIN_BIT_IN(inst->amps[i].pReadyIn, "%s.%c-amp-ready-in", name, axis_names[i]);
        PIN_BIT_OUT(inst->amps[i].pFaultOut, "%s.%c-amp-fault-out", name, axis_names[i]);
    }

    /* spindle pins */
    PIN_FLOAT_IN(p->pSpindleSpeedIn, "%s.spindle-speed-in", name);
    PIN_BIT_IN(p->pSpindleIsOnIn, "%s.spindle-is-on-in", name);
    PIN_BIT_OUT(p->pSpindleFwdOut, "%s.spindle-fwd-out", name);
    PIN_BIT_OUT(p->pSpindleRevOut, "%s.spindle-rev-out", name);
    PIN_BIT_IN(p->pSpindleIncIn, "%s.spindle-inc-in", name);
    PIN_BIT_IN(p->pSpindleDecIn, "%s.spindle-dec-in", name);
    PIN_BIT_OUT(p->pSpindleIncOut, "%s.spindle-inc-out", name);
    PIN_BIT_OUT(p->pSpindleDecOut, "%s.spindle-dec-out", name);
    PIN_BIT_IN(p->pBrakeEnIn, "%s.brake-en-in", name);
    PIN_BIT_OUT(p->pBrakeEnOut, "%s.brake-en-out", name);

    /* jog pins */
    for (i = 0; i < NUM_JOG_SEL; i++) {
        PIN_BIT_IN(p->pJogSelIn[i], "%s.jog-sel-in-%d", name, i);
    }
    PIN_FLOAT_OUT(p->pJogScaleOut, "%s.jog-scale-out", name);

    /* defaults */
    *(p->brakeOffDelay) = 500;
    *(p->brakeOnDelay) = 300;
    *(p->ampReadyDelay) = 50;
    *(p->spindleLoToHi) = 500.0;
    *(p->jogScale[0]) = 0.0001;
    *(p->jogScale[1]) = 0.001;
    *(p->jogScale[2]) = 0.01;
    *(p->pAdaptiveFeedIn) = 1.0;

    /* export function */
    snprintf(buf, sizeof(buf), "%s.refresh", name);
    r = env->hal->export_funct(env->hal->ctx, buf, plc_refresh, inst, 1, 0, cid);
    if (r) goto err;

    r = env->hal->ready(env->hal->ctx, cid);
    if (r) goto err;

    *out = &inst->base;
    return 0;

err:
    if (inst->comp_id > 0)
        env->hal->exit(env->hal->ctx, inst->comp_id);
    env->rtapi->free(env->rtapi->ctx, inst);
    return -1;
}
