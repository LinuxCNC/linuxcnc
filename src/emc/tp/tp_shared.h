#ifndef _TP_SHARED_H
#define _TP_SHARED_H

#include "hal.h"
#include "rtapi_math.h"
#include "emcpose.h"

typedef void (*emcmotDioWrite_t)(unsigned int index, hal_bit_t   value);
typedef void (*emcmotAioWrite_t)(unsigned int index, hal_float_t value);

typedef void (*emcmotSetRotaryUnlock_t)(int axis, hal_bit_t unlock);
typedef hal_bit_t  (*emcmotGetRotaryIsUnlocked_t)(int axis);


// this holds all shared data between using code and the tp
// data items can be pins if so desired,
// in this case the tp_shared struct must live in HAL memory, and
// be allocated by hal_malloc()

typedef struct tp_shared_t {

    // read-only by tp, config items
    hal_s32_t   *num_dio;
    hal_s32_t   *num_aio;

    hal_s32_t   *arcBlendGapCycles;
    hal_s32_t   *arcBlendOptDepth;
    hal_bit_t   *arcBlendEnable;
    hal_float_t *arcBlendRampFreq;
    hal_bit_t   *arcBlendFallbackEnable;
    hal_float_t *arcBlendTangentKinkRatio;
    hal_float_t *maxFeedScale;
    hal_float_t *net_feed_scale;

    hal_float_t *acc_limit[3];
    hal_float_t *vel_limit[3];

    hal_bit_t  *stepping;
    hal_u32_t  *enables_new;

    // read/write by tp:
    // NB: this is the direction field in struct spindle_status
    // (there's another direction field in emcmot_command_t)
    hal_s32_t  *spindle_direction;	// 0 stopped, 1 forward, -1 reverse
    hal_float_t *spindleRevs;
    hal_float_t *spindleSpeedIn;
    hal_float_t *spindle_speed;  // in struct spindle_status
    hal_bit_t   *spindle_index_enable;
    hal_bit_t   *spindle_is_atspeed; // emcmotStatus->spindle_is_atspeed

    hal_bit_t   *spindleSync;
    hal_float_t *current_vel;
    hal_float_t *dtg[9]; // an EmcPose

    // write-only by tp:
    hal_float_t *requested_vel;
    hal_float_t *distance_to_go;
    hal_u32_t   *enables_queued;
    hal_u32_t   *tcqlen;

    // upcalls by the tp into using code to set pin values:
    emcmotDioWrite_t dioWrite;
    emcmotAioWrite_t aioWrite;

    emcmotSetRotaryUnlock_t SetRotaryUnlock;
    emcmotGetRotaryIsUnlocked_t GetRotaryIsUnlocked;

} tp_shared_t;

static inline int get_num_dio(tp_shared_t *ts)  { return *(ts->num_dio); }
static inline void set_num_dio(tp_shared_t *ts, int n)  { *(ts->num_dio) = n; }

static inline int get_num_aio(tp_shared_t *ts)  { return *(ts->num_aio); }
static inline void set_num_aio(tp_shared_t *ts, int n)  { *(ts->num_aio) = n; }

static inline hal_float_t get_acc_limit(tp_shared_t *ts, int n)
{ return *(ts->acc_limit[n]); }
static inline void set_acc_limit(tp_shared_t *ts, int n, hal_float_t val)
{ *(ts->acc_limit[n]) = val; }

static inline hal_float_t get_vel_limit(tp_shared_t *ts, int n)
{ return *(ts->vel_limit[n]); }
static inline void set_vel_limit(tp_shared_t *ts, int n, hal_float_t val)
{ *(ts->vel_limit[n]) = val; }

static inline hal_float_t get_net_feed_scale(tp_shared_t *ts)
{ return *(ts->net_feed_scale); }
static inline hal_float_t get_maxFeedScale(tp_shared_t *ts)
{ return *(ts->maxFeedScale); }

static inline hal_bit_t get_stepping(tp_shared_t *ts)
{ return *(ts->stepping); }

static inline hal_float_t get_current_vel(tp_shared_t *ts)
{ return *(ts->current_vel); }
static inline void set_current_vel(tp_shared_t *ts, hal_float_t n)
{ *(ts->current_vel) = n; }

static inline hal_float_t get_requested_vel(tp_shared_t *ts)
{ return *(ts->requested_vel); }
static inline void set_requested_vel(tp_shared_t *ts, hal_float_t n)
{ *(ts->requested_vel) = n; }

static inline hal_float_t get_distance_to_go(tp_shared_t *ts)
{ return *(ts->distance_to_go); }
static inline void set_distance_to_go(tp_shared_t *ts, hal_float_t n)
{ *(ts->distance_to_go) = n; }

static inline hal_bit_t get_spindleSync(tp_shared_t *ts)
{ return *(ts->spindleSync); }
static inline void set_spindleSync(tp_shared_t *ts, hal_bit_t n)
{ *(ts->spindleSync) = n; }

static inline void zero_dtg(tp_shared_t *ts)
{
    unsigned i;
    for (i = 0; i < sizeof(ts->dtg)/sizeof(ts->dtg[0]); i++)
	*(ts->dtg[i]) = 0.0;
}

static inline hal_s32_t get_arcBlendGapCycles(tp_shared_t *ts)
{ return *(ts->arcBlendGapCycles); }
static inline void set_arcBlendGapCycles(tp_shared_t *ts, hal_s32_t n)
{ *(ts->arcBlendGapCycles) = n; }

static inline hal_s32_t get_arcBlendOptDepth(tp_shared_t *ts)
{ return *(ts->arcBlendOptDepth); }
static inline void set_arcBlendOptDepth(tp_shared_t *ts, hal_s32_t n)
{ *(ts->arcBlendOptDepth) = n; }

static inline hal_bit_t get_arcBlendEnable(tp_shared_t *ts)
{ return *(ts->arcBlendEnable); }
static inline void set_arcBlendEnable(tp_shared_t *ts, hal_bit_t n)
{ *(ts->arcBlendEnable) = n; }

static inline hal_float_t get_arcBlendRampFreq(tp_shared_t *ts)
{ return *(ts->arcBlendRampFreq); }
static inline void set_arcBlendRampFreq(tp_shared_t *ts, hal_float_t n)
{ *(ts->arcBlendRampFreq) = n; }

static inline hal_float_t get_arcBlendTangentKinkRatio(tp_shared_t *ts)
{ 
    const double max_ratio = 0.7071;
    const double min_ratio = 0.001;
    return rtapi_fmax(rtapi_fmin(*(ts->arcBlendTangentKinkRatio), max_ratio), min_ratio);
}

static inline void dioWrite(tp_shared_t *ts, unsigned int index, char value)
{ if (ts->dioWrite) ts->dioWrite(index, value); }
static inline void aioWrite(tp_shared_t *ts, unsigned int index, double value)
{ if (ts->aioWrite) ts->aioWrite(index, value); }

static inline hal_float_t get_spindleRevs(tp_shared_t *ts)
{ return *(ts->spindleRevs); }
static inline void set_spindleRevs(tp_shared_t *ts, hal_float_t n)
{ *(ts->spindleRevs) = n; }

static inline hal_float_t get_spindleSpeedIn(tp_shared_t *ts)
{ return *(ts->spindleSpeedIn); }
static inline void set_spindleSpeedIn(tp_shared_t *ts, hal_float_t n)
{ *(ts->spindleSpeedIn) = n; }

static inline hal_s32_t get_spindle_direction(tp_shared_t *ts)
{ return *(ts->spindle_direction); }
static inline void set_spindle_direction(tp_shared_t *ts, hal_s32_t n)
{ *(ts->spindle_direction) = n; }

static inline hal_float_t get_spindle_speed(tp_shared_t *ts)
{ return *(ts->spindle_speed); }
static inline void set_spindle_speed(tp_shared_t *ts, hal_float_t n)
{ *(ts->spindle_speed) = n; }

static inline hal_bit_t get_spindle_is_atspeed(tp_shared_t *ts)
{ return *(ts->spindle_is_atspeed); }
static inline void set_spindle_is_atspeed(tp_shared_t *ts, hal_bit_t n)
{ *(ts->spindle_is_atspeed) = n; }

static inline hal_bit_t get_spindle_index_enable(tp_shared_t *ts)
{ return *(ts->spindle_index_enable); }
static inline void set_spindle_index_enable(tp_shared_t *ts, hal_bit_t n)
{ *(ts->spindle_index_enable) = n; }

static inline hal_u32_t get_enables_queued(tp_shared_t *ts)
{ return *(ts->enables_queued); }
static inline void set_enables_queued(tp_shared_t *ts, hal_u32_t n)
{ *(ts->enables_queued) = n; }

static inline hal_u32_t get_tcqlen(tp_shared_t *ts)
{ return *(ts->tcqlen); }
static inline void set_tcqlen(tp_shared_t *ts, hal_u32_t n)
{ *(ts->tcqlen) = n; }

static inline hal_u32_t get_enables_new(tp_shared_t *ts)
{ return *(ts->enables_new); }
static inline void set_enables_new(tp_shared_t *ts, hal_u32_t n)
{ *(ts->enables_new) = n; }

// subtract two emcPose values and deposit the result
// in a hal_float_t * array
static inline int emcPoseSub2fp(EmcPose const * const p1,
				EmcPose const * const p2,
				hal_float_t ** const out)
{
#ifdef EMCPOSE_PEDANTIC
    if (!p1 || !p2) {
        return EMCPOSE_ERR_INPUT_MISSING;
    }
#endif
    *(out[0]) = p1->tran.x - p2->tran.x;
    *(out[1]) = p1->tran.y - p2->tran.y;
    *(out[2]) = p1->tran.z - p2->tran.z;
    *(out[3]) = p1->a - p2->a;
    *(out[4]) = p1->b - p2->b;
    *(out[5]) = p1->c - p2->c;
    *(out[6]) = p1->u - p2->u;
    *(out[7]) = p1->v - p2->v;
    *(out[8]) = p1->w - p2->w;
    return EMCPOSE_ERR_OK;
}

static inline int emcPoseZero2fp(hal_float_t ** const out)
{
#ifdef EMCPOSE_PEDANTIC
    if (!pos) {
        return EMCPOSE_ERR_INPUT_MISSING;
    }
#endif
    *(out[0]) = 0.0;
    *(out[1]) = 0.0;
    *(out[2]) = 0.0;
    *(out[3]) = 0.0;
    *(out[4]) = 0.0;
    *(out[5]) = 0.0;
    *(out[6]) = 0.0;
    *(out[7]) = 0.0;
    *(out[8]) = 0.0;
    return EMCPOSE_ERR_OK;
}

static inline void SetRotaryUnlock(tp_shared_t *ts,
				   int axis,
				   hal_bit_t unlock)
{
    if (ts->SetRotaryUnlock)
	ts->SetRotaryUnlock(axis, unlock);
}

static inline int GetRotaryIsUnlocked(tp_shared_t *ts,
				      int axis)
{
    if (ts->GetRotaryIsUnlocked)
	return ts->GetRotaryIsUnlocked(axis);
    return 0;
}

#endif //_TP_SHARED_H
