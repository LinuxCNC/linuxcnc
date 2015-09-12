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
* Last change:
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
#include "hal.h"		/* HAL public API decls */

#define MAX_CHAN 8

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("PWM/PDM Generator for EMC HAL");
MODULE_LICENSE("GPL");
#define MAX_OUTPUT_TYPE 2
int output_type[MAX_CHAN] = { -1, -1, -1, -1, -1, -1, -1, -1 };
RTAPI_MP_ARRAY_INT(output_type, 8, "output types for up to 8 channels");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* values for pwm_mode */
#define PWM_DISABLED 0
#define PWM_PURE 1
#define PWM_DITHER 2
#define PWM_PDM 3


typedef struct {
    // per-instance config data, not changed by functs
    struct config {
	unsigned char output_type;
    } cfg;

    // make_pulses() private state
    struct mp_state {
	long period_timer;		// timer for PWM period
	unsigned char curr_output;	// current state of output
	long high_timer;		// timer for high time
    } mp;

    // update() private state
    struct upd_state {
	hal_bit_t   *enable;		/* pin for enable signal */
	hal_float_t *value;		/* command value */
	hal_float_t *scale;		/* pin: scaling from value to duty cycle */
	hal_float_t *pwm_freq;	        /* pin: (max) output frequency in Hz */
	hal_bit_t   *dither_pwm;        /* 0 = pure PWM, 1 = dithered PWM */
	hal_float_t *min_dc;       	/* pin: minimum duty cycle */
	hal_float_t *max_dc;	        /* pin: maximum duty cycle */
	hal_float_t *curr_dc;	        /* pin: current duty cycle */
	hal_float_t *offset;	        /* pin: offset: this is added to duty cycle */

	double old_scale;		/* stored scale value */
	double scale_recip;		/* reciprocal value used for scaling */
	double old_pwm_freq;	        /* used to detect changes */
	double periods_recip;	        /* reciprocal */
	int periods;		        /* number of periods in PWM cycle */
    } upd;

    // shared between make_pulses() and update()
    // requiring coordination
    struct shared_state {
	unsigned char pwm_mode;  // m:r u:rw
	long period;		 // m:r u:rw length of PWM period, ns

	long high_time;		 // m:r u:w desired high time, ns
	unsigned char direction; // m:r u:w
    } ss;

    // out pins are only written to so defacto unshared
    hal_bit_t *out[2];		/* pins for output signals */

} pwmgen_t;

/* ptr to array of pwmgen_t structs in shared memory, 1 per channel */
static pwmgen_t *pwmgen_array;

#define PWM_PIN		0	/* output phase used for PWM signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */

/* other globals */
static int comp_id;		/* component ID */
static int num_chan;		/* number of pwm generators configured */
static long periodns;		/* makepulses function period in nanosec */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_pwmgen(int num, pwmgen_t * addr, int output_type);
static void make_pulses(void *arg, long period);
static void update(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;

    for (n = 0; n < MAX_CHAN && output_type[n] != -1 ; n++) {
	if ((output_type[n] > MAX_OUTPUT_TYPE) || (output_type[n] < 0)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PWMGEN: ERROR: bad output type '%i', channel %i\n",
		output_type[n], n);
	    return -1;
	} else {
	    num_chan++;
	}
    }
    if (num_chan == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PWMGEN: ERROR: no channels configured\n");
	return -1;
    }
    /* periodns will be set to the proper value when 'make_pulses()' runs for
       the first time.  We load a default value here to avoid glitches at
       startup */
    periodns = -1;
    /* have good config info, connect to the HAL */
    comp_id = hal_init("pwmgen");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PWMGEN: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for generator data */
    pwmgen_array = hal_malloc(num_chan * sizeof(pwmgen_t));
    if (pwmgen_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PWMGEN: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each PWM generator */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_pwmgen(n, &(pwmgen_array[n]), output_type[n]);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PWMGEN: ERROR: pwmgen %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions */
    retval = hal_export_funct("pwmgen.make-pulses", make_pulses,
	pwmgen_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PWMGEN: ERROR: makepulses funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("pwmgen.update", update,
	pwmgen_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PWMGEN: ERROR: update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"PWMGEN: installed %d PWM/PDM generators\n", num_chan);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
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

static void make_pulses(void *arg, long period)
{
    pwmgen_t *self;
    int n;

    /* store period for use in update() function */
    periodns = period;
    /* point to pwmgen data structures */
    self = arg;
    for (n = 0; n < num_chan; n++) {

	struct config *cfg = &self->cfg;
	struct mp_state *mp = &self->mp;
	struct shared_state *ss = &self->ss;

	switch ( ss->pwm_mode ) {

	case PWM_PURE:
	    if ( mp->curr_output ) {
		/* current state is high, update cumlative high time */
		mp->high_timer += periodns;
		/* have we been high long enough? */
		if ( mp->high_timer >= ss->high_time ) {
		    /* yes, terminate the high time */
		    mp->curr_output = 0;
		}
	    }
	    /* update period timer */
	    mp->period_timer += periodns;
	    /* have we reached the end of a period? */
	    if ( mp->period_timer >= ss->period ) {
		/* reset both timers to zero for jitter-free output */
		mp->period_timer = 0;
		mp->high_timer = 0;
		/* start the next period */
		if ( ss->high_time > 0 ) {
		    mp->curr_output = 1;
		}
	    }
	    break;
	case PWM_DITHER:
	    if ( mp->curr_output ) {
		/* current state is high, update cumlative high time */
		mp->high_timer -= periodns;
		/* have we been high long enough? */
		if ( mp->high_timer <= 0 ) {
		    /* yes, terminate the high time */
		    mp->curr_output = 0;
		}
	    }
	    /* update period timer */
	    mp->period_timer += periodns;
	    /* have we reached the end of a period? */
	    if ( mp->period_timer >= ss->period ) {
		/* update both timers, retain remainder from last period */
		/* this allows dithering for finer resolution */
		mp->period_timer -= ss->period;
		mp->high_timer += ss->high_time;
		/* start the next period */
		if ( mp->high_timer > 0 ) {
		    mp->curr_output = 1;
		}
	    }
	    break;
	case PWM_PDM:
	    /* add desired high time to running total */
	    mp->high_timer += ss->high_time;
	    if ( mp->curr_output ) {
		/* current state is high, subtract actual high time */
		mp->high_timer -= periodns;
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
	    *(self->out[UP_PIN]) = mp->curr_output & ~ss->direction;
	    *(self->out[DOWN_PIN]) = mp->curr_output & ss->direction;
	}
	/* move on to next PWM generator */
	self++;
    }
    /* done */
}

static void update(void *arg, long period)
{
	static long oldperiodns=-1;

    pwmgen_t *self;
    int n, high_periods;
    unsigned char new_pwm_mode;
    double tmpdc, outdc;

    /* update the PWM generators */
    self = arg;
    for (n = 0; n < num_chan; n++) {

	struct config *cfg = &self->cfg;
	struct upd_state *upd = &self->upd;
	struct shared_state *ss = &self->ss;

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
	/* do scale calcs only when scale changes */
	if ( *(upd->scale) != upd->old_scale ) {
	    /* get ready to detect future scale changes */
	    upd->old_scale = *(upd->scale);
	    /* validate the new scale value */
	    if ((*(upd->scale) < 1e-20)
		&& (*(upd->scale) > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		*(upd->scale) = 1.0;
	    }
	    /* we will need the reciprocal */
	    upd->scale_recip = 1.0 / *(upd->scale);
	}
	if ( *(upd->enable) == 0 ) {
	    new_pwm_mode = PWM_DISABLED;
	} else if ( *(upd->pwm_freq) == 0 ) {
	    new_pwm_mode = PWM_PDM;
	} else if ( *(upd->dither_pwm) != 0 ) {
	    new_pwm_mode = PWM_DITHER;
	} else {
	    new_pwm_mode = PWM_PURE;
	}
	/* force recalc if max_freq is changed */
	if ( *(upd->pwm_freq) != upd->old_pwm_freq ) {
	    ss->pwm_mode = PWM_DISABLED;
	}
	/* do the period calcs when mode, pwm_freq, or periodns changes */
	if ( ( ss->pwm_mode != new_pwm_mode )
		|| ( periodns != oldperiodns ) ) {
	    /* disable output during calcs */
	    ss->pwm_mode = PWM_DISABLED;
	    /* validate max_freq */
	    if ( *(upd->pwm_freq) <= 0.0 ) {
		/* zero or negative means PDM mode */
		*(upd->pwm_freq) = 0.0;
		ss->period = periodns;
	    } else {
		/* positive means PWM mode */
		if ( *(upd->pwm_freq) < 0.5 ) {
		    /* min freq is 0.5 Hz (2 billion nsec period) */
		    *(upd->pwm_freq) = 0.5;
		} else if ( *(upd->pwm_freq) > ((1e9/2.0) / periodns) ) {
		    /* max freq is 2 base periods */
		    *(upd->pwm_freq) = (1e9/2.0) / periodns;
		}
		if ( new_pwm_mode == PWM_PURE ) {
		    /* period must be integral multiple of periodns */
		    upd->periods = (( 1e9 / *(upd->pwm_freq) ) / periodns ) + 0.5;
		    upd->periods_recip = 1.0 / upd->periods;
		    ss->period = upd->periods * periodns;
		    /* actual max freq after rounding */
		    *(upd->pwm_freq) = 1.0e9 / ss->period;
		} else {
		    ss->period = 1.0e9 / *(upd->pwm_freq);
		}
	    }
	    /* save freq to detect changes */
	    upd->old_pwm_freq = *(upd->pwm_freq);
	}
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
	    ss->direction = 0;
	    outdc = tmpdc;
	} else {
	    if ( tmpdc < -*(upd->max_dc) ) {
		tmpdc = -*(upd->max_dc);
	    } else if ( tmpdc > -*(upd->min_dc) ) {
		tmpdc = -*(upd->min_dc);
	    }
	    ss->direction = 1;
	    outdc = -tmpdc;
	}
	if ( new_pwm_mode == PWM_PURE ) {
	    /* round to nearest pure PWM duty cycle */
	    high_periods = (upd->periods * outdc) + 0.5;
	    ss->high_time = high_periods * periodns;
	    /* save rounded value to curr_dc param */
	    if ( tmpdc >= 0 ) {
		*(upd->curr_dc) = high_periods * upd->periods_recip;
	    } else {
		*(upd->curr_dc) = -high_periods * upd->periods_recip;
	    }
	} else {
	    ss->high_time = ( ss->period * outdc ) + 0.5;
	    /* save duty cycle to curr_dc param */
	    *(upd->curr_dc) = tmpdc;
	}
	/* if using PWM/DIR outputs, set DIR pin */
	if ( cfg->output_type == 1 ) {
	    *(self->out[DIR_PIN]) = ss->direction;
	}
	/* save new mode */
	ss->pwm_mode = new_pwm_mode;
	/* move on to next channel */
	self++;
    }
    oldperiodns = periodns;
    /* done */
}


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pwmgen(int num, pwmgen_t * addr, int output_type)
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
	    "pwmgen.%d.scale", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.offset), comp_id,
	    "pwmgen.%d.offset", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IO, &(addr->upd.dither_pwm), comp_id,
	    "pwmgen.%d.dither-pwm", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.pwm_freq), comp_id,
	    "pwmgen.%d.pwm-freq", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.min_dc), comp_id,
	    "pwmgen.%d.min-dc", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->upd.max_dc), comp_id,
	    "pwmgen.%d.max-dc", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->upd.curr_dc), comp_id,
	    "pwmgen.%d.curr-dc", num);
    if (retval != 0) {
	return retval;
    }
    /* export pins */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.enable), comp_id,
	    "pwmgen.%d.enable", num);
    if (retval != 0) {
	return retval;
    }
    *(addr->upd.enable) = 0;
    retval = hal_pin_float_newf(HAL_IN, &(addr->upd.value), comp_id,
	    "pwmgen.%d.value", num);
    if (retval != 0) {
	return retval;
    }
    *(addr->upd.value) = 0.0;
    if (output_type == 2) {
	/* export UP/DOWN pins */
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[UP_PIN]), comp_id,
		"pwmgen.%d.up", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[UP_PIN]) = 0;
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[DOWN_PIN]), comp_id,
		"pwmgen.%d.down", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[DOWN_PIN]) = 0;
    } else {
	/* export PWM pin */
	retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[PWM_PIN]), comp_id,
		"pwmgen.%d.pwm", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[PWM_PIN]) = 0;
	if ( output_type == 1 ) {
	    /* export DIR pin */
	    retval = hal_pin_bit_newf(HAL_OUT, &(addr->out[DIR_PIN]), comp_id,
		    "pwmgen.%d.dir", num);
	    if (retval != 0) {
		return retval;
	    }
	    /* init the pin */
	    *(addr->out[DIR_PIN]) = 0;
	}
    }

    /* init other fields */

    addr->cfg.output_type = output_type;

    // update() state, including pins
    *(addr->upd.scale) = 1.0;
    *(addr->upd.offset) = 0.0;
    *(addr->upd.dither_pwm) = 0;
    *(addr->upd.pwm_freq) = 0;
    *(addr->upd.min_dc) = 0.0;
    *(addr->upd.max_dc) = 1.0;
    *(addr->upd.curr_dc) = 0.0;
    addr->upd.old_scale = *(addr->upd.scale) + 1.0;
    addr->upd.old_pwm_freq = -1;

    // shared state
    addr->ss.high_time = 0;
    addr->ss.pwm_mode = PWM_DISABLED;
    addr->ss.direction = 0;
    addr->ss.period = 50000;

    // make_pulses state
    addr->mp.period_timer = 0;
    addr->mp.curr_output = 0;
    addr->mp.high_timer = 0;


    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
