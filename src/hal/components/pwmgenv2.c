/********************************************************************
* Description:  pwmgen.c
*               A HAL component that generates Pulse Width and
*               Pulse Density Modulation
*
* Author: John Kasunich
* License: GPL Version 2
*
* Copyright (c) 2006 All rights reserved.
*
* Last change:  Michael Haberler 9/2015
*
*   SMP-safe - does not assume priority scheduling any more
*   sanitized paramter passing - no shared state, atomic
*   passing of the complete parameter set
*   instantiable - supports an arbitrary number of instances
********************************************************************/

/** This file, 'pwmgen.c', is a HAL component that generates
    Pulse Width Modulation or Pulse Density Modulation signals in
    software.  Since the timing granularity of a software based
    scheme is rather large compared to a hardware, either the
    output frequency, or the resolution, or both, will be less
    than expected from hardware implementations.

    The driver exports two functions.  'pwmgen.make-pulses', is
    responsible for actually generating the PWM/PDM signals.  It
    must be executed in a fast thread to reduce pulse jitter and
    improve resolution.  The other function, pwmgen.update, is
    normally called from a much slower thread.  It reads the
    command and sets internal variables used by 'pwm.make-pulses'.
    'update' uses floating point, 'make-pulses' does not.


    Polarity:

    All signals from this module have fixed polarity (active high)
    If the driver needs the opposite polarity, the signals can be
    inverted using parameters exported by the hardware driver(s)
    such as ParPort.


*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "triple-buffer.h"
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"           // SHMPTR,SHMOFF
#include "hal_list.h"
#include "hal_logging.h"

#undef TRACE_TB       // trace parameter passing operations
#undef VERBOSE_SETUP  // pin creation

MODULE_AUTHOR("John Kasunich, Michael Haberler");
MODULE_DESCRIPTION("PWM/PDM Generator for EMC HAL");
MODULE_LICENSE("GPL");

RTAPI_TAG(HAL,HC_INSTANTIABLE);
RTAPI_TAG(HAL,HC_SMP_SAFE);

static int output_type = -1;
RTAPI_IP_INT(output_type, "output type ,0:single, 1:pwm/direction, 2:up/down");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* values for pwm_mode */
#define PWM_DISABLED 0
#define PWM_PURE 1
#define PWM_DITHER 2
#define PWM_PDM 3

// parameter set for make_pulses()
struct mp_params {
    hal_s32_t     pwm_period;	        // length of PWM period, ns
    hal_s32_t     high_time;	        // desired high time, ns
    unsigned char direction;
    unsigned char pwm_mode;
    hal_bit_t     jitter_correct;
};

typedef struct {
    hal_list_t  list;                  // list of instances
    int inst_id;

    // per-instance config data, not changed by functs
    struct config {
	unsigned char output_type;
    } cfg;

    // make_pulses() private state
    struct mp_state {
	struct mp_params *mpp;           // current make_pulses params
	hal_s32_t        period_timer;	// timer for PWM period
	unsigned char    curr_output;	// current state of output
	hal_s32_t        high_timer;	// timer for high time
    } mp;

    // update() private state
    struct upd_state {
	bit_pin_ptr   enable;		/* pin for enable signal */
	bit_pin_ptr   jitter_correct;	// use actual cycle time for turnoff decision
	float_pin_ptr value;		/* command value */
	float_pin_ptr scale;		/* pin: scaling from value to duty cycle */
	float_pin_ptr pwm_freq;	        /* pin: (max) output frequency in Hz */
	bit_pin_ptr   dither_pwm;       /* 0 = pure PWM, 1 = dithered PWM */
	float_pin_ptr min_dc;       	/* pin: minimum duty cycle */
	float_pin_ptr max_dc;	        /* pin: maximum duty cycle */
	float_pin_ptr curr_dc;	        /* pin: current duty cycle */
	float_pin_ptr offset;	        /* pin: offset: this is added to duty cycle */

	// tracking values of in or io pins
	hal_bit_t   old_enable;
	hal_bit_t   old_dither_pwm;
	hal_bit_t   old_jitter_correct;
	hal_float_t old_value;
	hal_float_t old_scale;
	hal_float_t old_pwm_freq;
	hal_float_t old_min_dc;
	hal_float_t old_max_dc;
	hal_float_t old_offset;

	double     scale_recip;		/* reciprocal value used for scaling */
	double    periods_recip;        /* reciprocal */
	hal_s32_t periods;	        /* number of periods in PWM cycle */
    } upd;

    // triple buffer for param messages update() -> make_pulses()
    TB_FLAG_FAST(tb);
    struct mp_params tb_state[3];

    // out pins are only written to so defacto unshared
    bit_pin_ptr  out[2];		/* pins for output signals */

} pwmgen_t;

static  hal_list_t head;

#define PWM_PIN		0	/* output phase used for PWM signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */

/* other globals */
static int comp_id;		/* component ID */
static long periodns = -1;	/* makepulses function period in nanosec */

static const char *compname = "pwmgenv2";
static const char *prefix = "pwmgenv2"; 

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pwmgen(const char *name, const int inst_id,
			 pwmgen_t * addr, const int output_type);
static int make_pulses(void *arg, const hal_funct_args_t *fa);
static int update(void *arg, const hal_funct_args_t *fa);
static int instantiate_pwmgen(const int argc, char* const *argv);
static int delete_pwmgen(const char *name, void *inst, const int inst_size);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/
int rtapi_app_main(void)
{
    int retval;
    dlist_init_entry(&head);

    if ((comp_id = hal_xinit(TYPE_RT, 0, 0,
			     instantiate_pwmgen,
			     delete_pwmgen,
			     compname)) < 0)
	return comp_id;

    hal_export_xfunct_args_t u = {
        .type = FS_XTHREADFUNC,
        .funct.x = update,
        .arg = &head,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&u, "%s.update",
				     prefix)) < 0)
	return retval;

    hal_export_xfunct_args_t mp = {
        .type = FS_XTHREADFUNC,
        .funct.x = make_pulses,
        .arg = &head,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&mp, "%s.make-pulses",
				     prefix)) < 0)
	return retval;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static int instantiate_pwmgen(const int argc, char* const *argv)
{
    pwmgen_t *p;
    int retval;
    const char* name;
    
    if(argc >= 2)
        name = argv[1];
    else
        HALFAIL_RC(EINVAL, "ERROR: insufficient args in argv");
    
    if ((retval = hal_inst_create(name, comp_id, sizeof(pwmgen_t), (void **)&p)) < 0)
	return retval;

    p->inst_id = retval;
    if ((retval = export_pwmgen(name, p->inst_id, p, output_type)) != 0)
	HALFAIL_RC(retval, "%s: ERROR: export(%s) failed", compname, name);

    // append to instance list
    dlist_init_entry(&p->list);
    dlist_add_after(&p->list, &head);
    return 0;
}

static int delete_pwmgen(const char *name, void *inst, const int inst_size)
{
    pwmgen_t *p = inst;

    // disable PWM outputs
    if (p->cfg.output_type < 2) {
	set_bit_pin(p->out[PWM_PIN], 0);
    } else {
	// up/down: drive both outputs low
	set_bit_pin(p->out[UP_PIN], 0);
	set_bit_pin(p->out[DOWN_PIN], 0);
    }

    // delete from instance list
    dlist_remove_entry(&p->list);
    return 0;
}

/***********************************************************************
*              REALTIME STEP PULSE GENERATION FUNCTIONS                *
************************************************************************/

/** The pwm generator works by adding a positive value proportional
    to the desired duty cycle to accumulator.  When the accumulator
    is greater than zero, the output goes high (if permitted by the
    maximum frequency).  When the output is high, a larger value is
    subtracted from the accumulator.  As a result, it oscillates
    around zero to generate PWM or PDM, based on the values that are
    added and subtracted.
*/

static int make_pulses(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    pwmgen_t *self;

    // foreach pwmgen instance: generate output(s)
    dlist_for_each_entry(self, insts, list) {

	struct config *cfg = &self->cfg;
	struct mp_state *mp = &self->mp;

	if (rtapi_tb_snapshot(&self->tb)) {

	    // read barrier before accessing new snapshot
	    rtapi_smp_rmb();

	    // new parameter set available, fetch it
	    self->mp.mpp = &self->tb_state[rtapi_tb_snap_idx(&self->tb)];

#ifdef TRACE_TB
	    HALDBG("SNAP inst=%d pwm_mode=%d pwm_period=%d high_time=%d direction=%d jc=%d",
		   self->inst_id,
		   self->mp.mpp->pwm_mode,
		   self->mp.mpp->pwm_period,
		   self->mp.mpp->high_time,
		   self->mp.mpp->direction,
		   self->mp.mpp->jitter_correct);
#endif
	    // store period for use in update() function - activates
	    // parameter calculation
	    // no need to do this every thread cycle
	    periodns = fa_period(fa);
	}

	struct mp_params *mparams = self->mp.mpp;

	hal_s32_t delta_t = (mparams->jitter_correct) ? fa_current_period(fa) : periodns;

	switch ( mparams->pwm_mode ) {

	case PWM_PURE:
	    if ( mp->curr_output ) {
		/* current state is high, update cumlative high time */
		mp->high_timer += delta_t;
		/* have we been high long enough? */
		if ( mp->high_timer >= mparams->high_time ) {
		    /* yes, terminate the high time */
		    mp->curr_output = 0;
		}
	    }
	    /* update period timer */
	    mp->period_timer += delta_t;
	    /* have we reached the end of a period? */
	    if ( mp->period_timer >= mparams->pwm_period ) {
		/* reset both timers to zero for jitter-free output */
		mp->period_timer = 0;
		mp->high_timer = 0;
		/* start the next period */
		if ( mparams->high_time > 0 ) {
		    mp->curr_output = 1;
		}
	    }
	    break;

	case PWM_DITHER:
	    if ( mp->curr_output ) {
		/* current state is high, update cumlative high time */
		mp->high_timer -= delta_t;
		/* have we been high long enough? */
		if ( mp->high_timer <= 0 ) {
		    /* yes, terminate the high time */
		    mp->curr_output = 0;
		}
	    }
	    /* update period timer */
	    mp->period_timer += delta_t;
	    /* have we reached the end of a period? */
	    if ( mp->period_timer >= mparams->pwm_period ) {
		/* update both timers, retain remainder from last period */
		/* this allows dithering for finer resolution */
		mp->period_timer -= mparams->pwm_period;
		mp->high_timer += mparams->high_time;
		/* start the next period */
		if ( mp->high_timer > 0 ) {
		    mp->curr_output = 1;
		}
	    }
	    break;

	case PWM_PDM:
	    /* add desired high time to running total */
	    mp->high_timer += mparams->high_time;
	    if ( mp->curr_output ) {
		/* current state is high, subtract actual high time */
		mp->high_timer -= delta_t;
	    }
	    if ( mp->high_timer > 0 ) {
		mp->curr_output = 1;
	    } else {
		mp->curr_output = 0;
	    }
	    break;

	case PWM_DISABLED:
	default:
	    /* disabled, drive output off and zero accumulator */
	    mp->curr_output = 0;
	    mp->high_timer = 0;
	    mp->period_timer = 0;
	    break;
	}

	/* generate output, based on output type */
	if (cfg->output_type < 2) {
	    /* PWM (and maybe DIR) output */
	    /* DIR is set by update(), we only do PWM */
	    set_bit_pin(self->out[PWM_PIN], mp->curr_output);
	} else {
	    /* UP and DOWN output */
	    set_bit_pin(self->out[UP_PIN], mp->curr_output & ~mparams->direction);
	    set_bit_pin(self->out[DOWN_PIN], mp->curr_output & mparams->direction);
	}
    }
    return 0;
}

static int update(void *arg,  const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    pwmgen_t *self;
    int high_periods;
    double tmpdc, outdc;

    // foreach pwmgen instance: update the PWM parameters
    dlist_for_each_entry(self, insts, list) {

	struct config *cfg = &self->cfg;
	struct upd_state *upd = &self->upd;

	// validate duty cycle limits, both limits must be between
	// 0.0 and 1.0 (inclusive) and max must be greater then min
	hal_float_t max_dc     = get_float_pin(upd->max_dc);
	hal_float_t min_dc     = get_float_pin(upd->min_dc);
	if (max_dc > 1.0) {
	    max_dc = 1.0;
	    set_float_pin(upd->max_dc, max_dc);
	}
	if (min_dc > max_dc) {
	    min_dc = max_dc;
	    set_float_pin(upd->min_dc, min_dc);
	}
	if (min_dc < 0.0) {
	    min_dc = 0.0;
	    set_float_pin(upd->min_dc, min_dc);
	}
	if (max_dc < min_dc) {
	    max_dc = min_dc;
	    set_float_pin(upd->max_dc, max_dc);
	}

	// cop out until we know the thread period from make_pulses
	if (periodns < 0)
	    return 0;

	// change-detect any driving pins
	hal_bit_t   enable         = get_bit_pin(upd->enable);
	hal_bit_t   dither_pwm     = get_bit_pin(upd->dither_pwm);
	hal_float_t value          = get_float_pin(upd->value);
	hal_float_t scale          = get_float_pin(upd->scale);
	hal_float_t pwm_freq       = get_float_pin(upd->pwm_freq);
	hal_float_t offset         = get_float_pin(upd->offset);
	hal_bit_t   jitter_correct = get_bit_pin(upd->jitter_correct);

#define TRACK(name)  (name != upd->old_##name) ? \
	    (upd->old_##name = name) , true : false

	bool changed = false;
	changed |= TRACK(min_dc);
	changed |= TRACK(max_dc);

	changed |= TRACK(enable);
	changed |= TRACK(dither_pwm);
	changed |= TRACK(value);
	changed |= TRACK(scale);
	changed |= TRACK(pwm_freq);
	changed |= TRACK(offset);
	changed |= TRACK(jitter_correct);
#undef TRACK

	if (!changed)
	    continue;

	// compute a new parameter set

	// get a handle on the currently write buffer
	struct mp_params *uparams =  &self->tb_state[rtapi_tb_write_idx(&self->tb)];

	/* validate the new scale value */
	if ((scale < 1e-20) && (scale > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    set_float_pin(upd->scale, 1.0);
	}
	/* we will need the reciprocal */
	upd->scale_recip = 1.0 / scale;

	// set pwm_mode:
	if ( enable == 0 ) {
	    uparams->pwm_mode = PWM_DISABLED;
	} else if ( pwm_freq == 0 ) {
	    uparams->pwm_mode = PWM_PDM;
	} else if ( dither_pwm != 0 ) {
	    uparams->pwm_mode = PWM_DITHER;
	} else {
	    uparams->pwm_mode = PWM_PURE;
	}

	// set pwm_period:

	/* validate max_freq */
	if (pwm_freq <= 0.0 ) {
	    /* zero or negative means PDM mode */
	    set_float_pin(upd->pwm_freq, 0.0);
	    uparams->pwm_period = periodns;
	} else {
	    /* positive means PWM mode */
	    if ( pwm_freq < 0.5 ) {
		/* min freq is 0.5 Hz (2 billion nsec period) */
		set_float_pin(upd->pwm_freq, 0.5);
	    } else if ( pwm_freq > ((1e9/2.0) / periodns) ) {
		/* max freq is 2 base periods */
		set_float_pin(upd->pwm_freq, (1e9/2.0) / periodns);
	    }
	    if ( uparams->pwm_mode == PWM_PURE ) {
		/* period must be integral multiple of periodns */
		upd->periods = (( 1e9 / pwm_freq ) / periodns ) + 0.5;
		upd->periods_recip = 1.0 / upd->periods;
		uparams->pwm_period = upd->periods * periodns;
		/* actual max freq after rounding */
		set_float_pin(upd->pwm_freq, 1.0e9 / uparams->pwm_period);
	    } else {
		uparams->pwm_period = 1.0e9 / pwm_freq;
	    }
	}

	// set direction
	/* convert value command to duty cycle */
	tmpdc = value * upd->scale_recip + offset;
	if ( cfg->output_type == 0 ) {
	    /* unidirectional mode, no negative output */
	    if ( tmpdc < 0.0 ) {
		tmpdc = 0.0;
	    }
	}
	/* limit the duty cycle */
	if (tmpdc >= 0.0) {
	    if ( tmpdc > max_dc) {
		tmpdc = max_dc;
	    } else if ( tmpdc < min_dc ) {
		tmpdc = min_dc;
	    }
	    uparams->direction = 0;
	    outdc = tmpdc;
	} else {
	    if ( tmpdc < -max_dc ) {
		tmpdc = -max_dc;
	    } else if ( tmpdc > -min_dc ) {
		tmpdc = -min_dc;
	    }
	    uparams->direction = 1;
	    outdc = -tmpdc;
	}

	// set high_time
	if ( uparams->pwm_mode == PWM_PURE ) {
	    /* round to nearest pure PWM duty cycle */
	    high_periods = (upd->periods * outdc) + 0.5;
	    uparams->high_time = high_periods * periodns;
	    /* save rounded value to curr_dc param */
	    if ( tmpdc >= 0 ) {
		set_float_pin(upd->curr_dc, high_periods * upd->periods_recip);
	    } else {
		set_float_pin(upd->curr_dc, -high_periods * upd->periods_recip);
	    }
	} else {
	    uparams->high_time = ( uparams->pwm_period * outdc ) + 0.5;
	    /* save duty cycle to curr_dc param */
	    set_float_pin(upd->curr_dc, tmpdc);
	}
	/* if using PWM/DIR outputs, set DIR pin */
	if ( cfg->output_type == 1 ) {
	    set_bit_pin(self->out[DIR_PIN], uparams->direction);
	}
	uparams->jitter_correct = jitter_correct;

	// all fields in mp_params now set

#ifdef TRACE_TB
	HALDBG("FLIP");
#endif
        // write barrier before we flip the index
	rtapi_smp_wmb();

	// flip the write index
	rtapi_tb_flip(&self->tb);
    }
    return 0;
}


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pwmgen(const char *name,
			 const int inst_id,
			 pwmgen_t *p,
			 const int output_type)
{
    int msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
#ifndef VERBOSE_SETUP
    rtapi_set_msg_level(RTAPI_MSG_WARN);
#endif

#define FCHECK(rvalue) if (float_pin_null(rvalue)) return _halerrno;
#define BCHECK(rvalue) if (bit_pin_null(rvalue)) return _halerrno;

    FCHECK(p->upd.scale          = halxd_pin_float_newf(HAL_IO, inst_id, 1.0, "%s.scale", name));
    FCHECK(p->upd.offset         = halxd_pin_float_newf(HAL_IO, inst_id, 0.0, "%s.offset", name));
    FCHECK(p->upd.pwm_freq       = halxd_pin_float_newf(HAL_IO, inst_id, 0.0, "%s.pwm-freq", name));
    FCHECK(p->upd.min_dc         = halxd_pin_float_newf(HAL_IO, inst_id, 0.0, "%s.min-dc", name));
    FCHECK(p->upd.max_dc         = halxd_pin_float_newf(HAL_IO, inst_id, 1.0, "%s.max-dc", name));
    FCHECK(p->upd.curr_dc        = halxd_pin_float_newf(HAL_OUT,inst_id, 0.0, "%s.curr-dc", name));
    FCHECK(p->upd.value          = halxd_pin_float_newf(HAL_IN, inst_id, 0.0, "%s.value", name));

    BCHECK(p->upd.jitter_correct = halxd_pin_bit_newf(HAL_IN,   inst_id, 0,   "%s.jitter-correct", name));
    BCHECK(p->upd.dither_pwm     = halxd_pin_bit_newf(HAL_IO,   inst_id, 0,   "%s.dither-pwm", name));
    BCHECK(p->upd.enable         = halxd_pin_bit_newf(HAL_IN,   inst_id, 0,   "%s.enable", name));


    if (output_type == 2) {
	  BCHECK(p->out[UP_PIN]   = halxd_pin_bit_newf(HAL_OUT, inst_id, 0, "%s.up", name));
	  BCHECK(p->out[DOWN_PIN] = halxd_pin_bit_newf(HAL_OUT, inst_id, 0, "%s.down", name));
    } else {
	  BCHECK(p->out[PWM_PIN]  = halxd_pin_bit_newf(HAL_OUT, inst_id, 0, "%s.pwm", name));

	  if (output_type == 1) {
	      BCHECK(p->out[DIR_PIN]   = halxd_pin_bit_newf(HAL_OUT, inst_id, 0, "%s.dir", name));
	}
    }
    p->cfg.output_type = output_type;

    // set triple buffer initial state
    rtapi_tb_init(&p->tb);

    // supply startup params to make_pulses
    struct mp_params *mpp =  &p->tb_state[rtapi_tb_write_idx(&p->tb)];
    mpp->high_time = 0;
    mpp->pwm_mode = PWM_DISABLED;
    mpp->direction = 0;
    mpp->pwm_period = 50000;
    rtapi_tb_flip(&p->tb); // commit

    // update() private state
    p->upd.old_scale = 1.0; // trigger change detection
    p->upd.old_pwm_freq = -1;

    // make_pulses() private state
    p->mp.period_timer = 0;
    p->mp.curr_output = 0;
    p->mp.high_timer = 0;

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
