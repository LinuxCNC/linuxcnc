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

#define TRACE_TB  // trace parameter passing operations

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
	struct mp_params *mp;           // current make_pulses params
	hal_s32_t        period_timer;	// timer for PWM period
	unsigned char    curr_output;	// current state of output
	hal_s32_t        high_timer;	// timer for high time
    } mp;

    // update() private state
    struct upd_state {
	hal_bit_t   *enable;		/* pin for enable signal */
	hal_bit_t   *jitter_correct;	// use actual cycle time for turnoff decision
	hal_float_t *value;		/* command value */
	hal_float_t *scale;		/* pin: scaling from value to duty cycle */
	hal_float_t *pwm_freq;	        /* pin: (max) output frequency in Hz */
	hal_bit_t   *dither_pwm;        /* 0 = pure PWM, 1 = dithered PWM */
	hal_float_t *min_dc;       	/* pin: minimum duty cycle */
	hal_float_t *max_dc;	        /* pin: maximum duty cycle */
	hal_float_t *curr_dc;	        /* pin: current duty cycle */
	hal_float_t *offset;	        /* pin: offset: this is added to duty cycle */

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
    hal_bit_t *out[2];		/* pins for output signals */

} pwmgen_t;

static  hal_list_t head;

#define PWM_PIN		0	/* output phase used for PWM signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */

/* other globals */
static int comp_id;		/* component ID */
static long periodns = -1;	/* makepulses function period in nanosec */

static const char *compname = "pwmgen-v2";
static const char *prefix = "pwmgen"; // less surprises on funct names

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_pwmgen(const char *name, pwmgen_t * addr, const int output_type);
static int make_pulses(void *arg, const hal_funct_args_t *fa);
static int update(void *arg, const hal_funct_args_t *fa);
static int instantiate_pwmgen(const char *name, const int argc,
			      const char**argv);
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
    if ((retval = hal_export_xfunctf(&u,
				     "%s.update",
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
    if ((retval = hal_export_xfunctf(&mp,
				     "%s.make-pulses",
				     prefix)) < 0)
	return retval;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static int instantiate_pwmgen(const char *name,
			      const int argc,
			      const char**argv)
{
    pwmgen_t *p;

    int retval = hal_inst_create(name, comp_id, sizeof(pwmgen_t), (void **)&p);
    if (retval < 0)
	return retval;

    p->inst_id = retval;
    if ((retval = export_pwmgen(name, p, output_type)) != 0)
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
	    *(p->out[PWM_PIN]) = 0;
    } else {
	// up/down: drive both outputs low
	*(p->out[UP_PIN]) = 0;
	*(p->out[DOWN_PIN]) = 0;
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

	if (rtapi_tb_new_snap(&self->tb)) {
	    // new parameter set available, fetch it
	    self->mp.mp = &self->tb_state[rtapi_tb_snap(&self->tb)];

#ifdef TRACE_TB
	    HALDBG("SNAP inst=%d pwm_mode=%d pwm_period=%d high_time=%d direction=%d jc=%d",
		   self->inst_id,
		   self->mp.mp->pwm_mode,
		   self->mp.mp->pwm_period,
		   self->mp.mp->high_time,
		   self->mp.mp->direction,
		   self->mp.mp->jitter_correct);
#endif
	    // store period for use in update() function - activates
	    // parameter calculation
	    // no need to do this every thread cycle
	    periodns = fa_period(fa);
	}

	struct mp_params *mparams = self->mp.mp;

	hal_s32_t delta_t = (self->mp.mp->jitter_correct) ? fa_current_period(fa) : periodns;

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
	    *(self->out[PWM_PIN]) = mp->curr_output;
	} else {
	    /* UP and DOWN output */
	    *(self->out[UP_PIN]) = mp->curr_output & ~mparams->direction;
	    *(self->out[DOWN_PIN]) = mp->curr_output & mparams->direction;
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

	/* validate duty cycle limits, both limits must be between
	   0.0 and 1.0 (inclusive) and max must be greater then min */
	if ( *(upd->max_dc) > 1.0 ) {
	    *(upd->max_dc) = 1.0;
	}
	if ( *(upd->min_dc) > *(upd->max_dc) ) {
	    *(upd->min_dc) = *(upd->max_dc);
	}
	if ( *(upd->min_dc) < 0.0 ) {
	    *(upd->min_dc) = 0.0;
	}
	if ( *(upd->max_dc) < *(upd->min_dc) ) {
	    *(upd->max_dc) = *(upd->min_dc);
	}

	// cop out until we know the thread period from make_pulses
	if (periodns < 0)
	    return 0;

	// change-detect the driving pins
#define TRACK(name)  (*(upd->name) != upd->old_##name) ? upd->old_##name = *(upd->name), true : false
	bool changed = false;
	changed |= TRACK(enable);
	changed |= TRACK(dither_pwm);
	changed |= TRACK(value);
	changed |= TRACK(scale);
	changed |= TRACK(pwm_freq);
	changed |= TRACK(min_dc);
	changed |= TRACK(max_dc);
	changed |= TRACK(offset);
	changed |= TRACK(jitter_correct);
#undef TRACK
	if (!changed)
	    continue;

	// compute a new parameter set

	// get a handle on the currently unused param buffer
	struct mp_params *uparams =  &self->tb_state[rtapi_tb_write(&self->tb)];

	/* validate the new scale value */
	if ((*(upd->scale) < 1e-20)
	    && (*(upd->scale) > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    *(upd->scale) = 1.0;
	}
	/* we will need the reciprocal */
	upd->scale_recip = 1.0 / *(upd->scale);

	// set pwm_mode:
	if ( *(upd->enable) == 0 ) {
	    uparams->pwm_mode = PWM_DISABLED;
	} else if ( *(upd->pwm_freq) == 0 ) {
	    uparams->pwm_mode = PWM_PDM;
	} else if ( *(upd->dither_pwm) != 0 ) {
	    uparams->pwm_mode = PWM_DITHER;
	} else {
	    uparams->pwm_mode = PWM_PURE;
	}

	// set pwm_period:
	/* validate max_freq */
	if ( *(upd->pwm_freq) <= 0.0 ) {
	    /* zero or negative means PDM mode */
	    *(upd->pwm_freq) = 0.0;
	    uparams->pwm_period = periodns;
	} else {
	    /* positive means PWM mode */
	    if ( *(upd->pwm_freq) < 0.5 ) {
		/* min freq is 0.5 Hz (2 billion nsec period) */
		*(upd->pwm_freq) = 0.5;
	    } else if ( *(upd->pwm_freq) > ((1e9/2.0) / periodns) ) {
		/* max freq is 2 base periods */
		*(upd->pwm_freq) = (1e9/2.0) / periodns;
	    }
	    if ( uparams->pwm_mode == PWM_PURE ) {
		/* period must be integral multiple of periodns */
		upd->periods = (( 1e9 / *(upd->pwm_freq) ) / periodns ) + 0.5;
		upd->periods_recip = 1.0 / upd->periods;
		uparams->pwm_period = upd->periods * periodns;
		/* actual max freq after rounding */
		*(upd->pwm_freq) = 1.0e9 / uparams->pwm_period;
	    } else {
		uparams->pwm_period = 1.0e9 / *(upd->pwm_freq);
	    }
	}

	// set direction
	/* convert value command to duty cycle */
	tmpdc = *(upd->value) * upd->scale_recip + *(upd->offset);
	if ( cfg->output_type == 0 ) {
	    /* unidirectional mode, no negative output */
	    if ( tmpdc < 0.0 ) {
		tmpdc = 0.0;
	    }
	}
	/* limit the duty cycle */
	if (tmpdc >= 0.0) {
	    if ( tmpdc > *(upd->max_dc) ) {
		tmpdc = *(upd->max_dc);
	    } else if ( tmpdc < *(upd->min_dc) ) {
		tmpdc = *(upd->min_dc);
	    }
	    uparams->direction = 0;
	    outdc = tmpdc;
	} else {
	    if ( tmpdc < -*(upd->max_dc) ) {
		tmpdc = -*(upd->max_dc);
	    } else if ( tmpdc > -*(upd->min_dc) ) {
		tmpdc = -*(upd->min_dc);
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
		*(upd->curr_dc) = high_periods * upd->periods_recip;
	    } else {
		*(upd->curr_dc) = -high_periods * upd->periods_recip;
	    }
	} else {
	    uparams->high_time = ( uparams->pwm_period * outdc ) + 0.5;
	    /* save duty cycle to curr_dc param */
	    *(upd->curr_dc) = tmpdc;
	}
	/* if using PWM/DIR outputs, set DIR pin */
	if ( cfg->output_type == 1 ) {
	    *(self->out[DIR_PIN]) = uparams->direction;
	}
	uparams->jitter_correct = *(upd->jitter_correct);

	// all fields in mp_params now set

#ifdef TRACE_TB
	HALDBG("FLIP");
#endif
        // write barrier before we flip the index
	rtapi_smp_wmb();

	// flip the write index
	rtapi_tb_flip_writer(&self->tb);
    }
    return 0;
}


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pwmgen(const char *name, pwmgen_t * addr, const int output_type)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export paramameters */
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.scale), comp_id,
	    "%s.scale", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.offset), comp_id,
	    "%s.offset", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IO, &(addr->upd.dither_pwm), comp_id,
	    "%s.dither-pwm", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.pwm_freq), comp_id,
	    "%s.pwm-freq", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.min_dc), comp_id,
	    "%s.min-dc", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.max_dc), comp_id,
	    "%s.max-dc", name);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->upd.curr_dc), comp_id,
	    "%s.curr-dc", name);
    if (retval != 0) {
	return retval;
    }
    /* export pins */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.enable), comp_id,
	    "%s.enable", name);
    if (retval != 0) {
	return retval;
    }
    *(addr->upd.enable) = 0;
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.jitter_correct), comp_id,
	    "%s.jitter_correct", name);
    if (retval != 0) {
	return retval;
    }
    *(addr->upd.enable) = 0;
    retval = hal_pin_float_newf(HAL_IN, &(addr->upd.value), comp_id,
	    "%s.value", name);
    if (retval != 0) {
	return retval;
    }
    *(addr->upd.value) = 0.0;
    if (output_type == 2) {
	/* export UP/DOWN pins */
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[UP_PIN]), comp_id,
		"%s.up", name);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[UP_PIN]) = 0;
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[DOWN_PIN]), comp_id,
		"%s.down", name);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[DOWN_PIN]) = 0;
    } else {
	/* export PWM pin */
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[PWM_PIN]), comp_id,
		"%s.pwm", name);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[PWM_PIN]) = 0;
	if ( output_type == 1 ) {
	    /* export DIR pin */
	    retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[DIR_PIN]), comp_id,
		    "%s.dir", name);
	    if (retval != 0) {
		return retval;
	    }
	    /* init the pin */
	    *(addr->out[DIR_PIN]) = 0;
	}
    }

    /* init other fields */
    addr->cfg.output_type = output_type;

    // set triple buffer initial state
    rtapi_tb_init(&addr->tb);


    // supply startup params to make_pulses
    struct mp_params *p =  &addr->tb_state[rtapi_tb_write(&addr->tb)];
    p->high_time = 0;
    p->pwm_mode = PWM_DISABLED;
    p->direction = 0;
    p->pwm_period = 50000;
    rtapi_tb_flip_writer(&addr->tb); // commit

    // update() private state, including pins
    *(addr->upd.scale) = 1.0;
    *(addr->upd.offset) = 0.0;
    *(addr->upd.dither_pwm) = 0;
    *(addr->upd.pwm_freq) = 0;
    *(addr->upd.min_dc) = 0.0;
    *(addr->upd.max_dc) = 1.0;
    *(addr->upd.curr_dc) = 0.0;
    *(addr->upd.jitter_correct) = 0;
    addr->upd.old_scale = *(addr->upd.scale) + 1.0; // trigger change detection
    addr->upd.old_pwm_freq = -1;


    // make_pulses private state
    addr->mp.period_timer = 0;
    addr->mp.curr_output = 0;
    addr->mp.high_timer = 0;

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
