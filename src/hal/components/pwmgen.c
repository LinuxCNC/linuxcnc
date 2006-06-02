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
# $Revision$
* $Author$
* $Date$
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
    modify it under the terms of version 2.1 of the GNU General
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

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/ctype.h>	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#define MAX_CHAN 8

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("PWM/PDM Generator for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#define MAX_OUTPUT_TYPE 2
int output_type[MAX_CHAN] = { -1, -1, -1, -1, -1, -1, -1, -1 };
MODULE_PARM(output_type, "1-8i");
MODULE_PARM_DESC(output_type, "output types for up to 8 channels");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct {
    hal_bit_t *enable;		/* pin for enable signal */
    unsigned long k1;		/* value added to accum every period */
    unsigned long k2;		/* value sub'ed from accum when high */
    unsigned long lockout_time;
    unsigned long lockout;	/* max frequency lockout timer */
    signed long accum;		/* PWM generator accumulator */
    unsigned char curr_output;	/* current state of output */
    unsigned char output_type;
    unsigned char direction;
    hal_bit_t *out[2];		/* pins for output signals */

    hal_float_t *value;		/* command value */
    hal_float_t scale;		/* param: scaling from value to duty cycle */
    float old_scale;		/* stored scale value */
    double scale_recip;		/* reciprocal value used for scaling */
    hal_bit_t pure_pwm;		/* 0 = PDM, 1 = PWM */
    char old_ppwm;		/* used to detect changes */
    hal_float_t max_freq;	/* param: (max) output frequency in Hz */
    float old_max_freq;		/* used to detect changes */
    hal_float_t min_dc;		/* param: minimum duty cycle */
    hal_float_t max_dc;		/* param: maximum duty cycle */
    hal_float_t curr_dc;	/* param: current duty cycle */
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
    periodns = 50000;
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
    pwmgen_t *pwmgen;
    int n;

    /* store period for use in update() function */
    periodns = period;
    /* point to pwmgen data structures */
    pwmgen = arg;
    for (n = 0; n < num_chan; n++) {
	/* decrement lockout timer if non-zero */
	if ( pwmgen->lockout > 0 ) {
	    pwmgen->lockout--;
	}
	if ( *(pwmgen->enable) ) {
	    /* update accumulator */
	    pwmgen->accum += pwmgen->k1;
	} else {
	    /* reset accum */
	    pwmgen->accum = 0;
	}
	/* should output be high? */
	if ( pwmgen->accum > 0 ) {
	    /* Yes. is it already high? */
	    if ( pwmgen->curr_output == 0 ) {
		/* No. is it allowed to go high now? */
		if ( pwmgen->lockout == 0 ) {
		    /* Yes. make it high */
		    pwmgen->curr_output = 1;
		    /* and restart rising edge lockout timer */
		    pwmgen->lockout = pwmgen->lockout_time;
		}
	    }
	} else {
	    /* output should be low, make it so */
	    pwmgen->curr_output = 0;
	}
	/* subtract K2 only when output is high */
	if ( pwmgen->curr_output ) {
	    pwmgen->accum -= pwmgen->k2;
	}
	/* generate output, based on output type */
	if (pwmgen->output_type < 2) {
	    /* PWM (and maybe DIR) output */
	    /* DIR is set by update(), we only do PWM */
	    *(pwmgen->out[PWM_PIN]) = pwmgen->curr_output;
	} else {
	    /* UP and DOWN output */
	    *(pwmgen->out[UP_PIN]) = pwmgen->curr_output & ~pwmgen->direction;
	    *(pwmgen->out[DOWN_PIN]) = pwmgen->curr_output & pwmgen->direction;
	}
	/* move on to next PWM generator */
	pwmgen++;
    }
    /* done */
}

static void update(void *arg, long period)
{
    pwmgen_t *pwmgen;
    int n;
    float tmpdc, outdc;

    /* update the PWM generators */
    pwmgen = arg;
    for (n = 0; n < num_chan; n++) {

	/* validate duty cycle limits, both limits must be between
	   0.0 and 1.0 (inclusive) and max must be greater then min */
	if ( pwmgen->max_dc > 1.0 ) {
	    pwmgen->max_dc = 1.0;
	}
	if ( pwmgen->min_dc > pwmgen->max_dc ) {
	    pwmgen->min_dc = pwmgen->max_dc;
	}
	if ( pwmgen->min_dc < 0.0 ) {
	    pwmgen->min_dc = 0.0;
	}
	if ( pwmgen->max_dc < pwmgen->min_dc ) {
	    pwmgen->max_dc = pwmgen->min_dc;
	}
	/* do scale calcs only when scale changes */
	if ( pwmgen->scale != pwmgen->old_scale ) {
	    /* get ready to detect future scale changes */
	    pwmgen->old_scale = pwmgen->scale;
	    /* validate the new scale value */
	    if ((pwmgen->scale < 1e-20)
		&& (pwmgen->scale > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		pwmgen->scale = 1.0;
	    }
	    /* we will need the reciprocal */
	    pwmgen->scale_recip = 1.0 / pwmgen->scale;
	}
	/* do lockout time calcs only when max_freq is changed */
	if ( pwmgen->max_freq != pwmgen->old_max_freq ) {
	    /* test param to avoid zero or negative freq */
	    if ( pwmgen->max_freq > 0.001 ) {
		/* calc min period, round up to multiple of thread period */
		pwmgen->lockout_time =
		    (1.0e9 / (pwmgen->max_freq * periodns)) + 0.99;
	    } else {
		/* zero or negative, use max possible frequency */
		pwmgen->lockout_time = 0;
	    }
	    /* minimum PWM period is two thread periods */
	    if ( pwmgen->lockout_time < 2 ) {
		pwmgen->lockout_time = 2;
	    }
	    /* calc actual max freq after rounding, limiting, etc */
	    pwmgen->max_freq = 1.0e9 / (pwmgen->lockout_time * periodns);
	    /* save to detect changes */
	    pwmgen->old_max_freq = pwmgen->max_freq;
	}
	/* reset state if mode changed */
	if ( pwmgen->pure_pwm != pwmgen->old_ppwm ) {
	    pwmgen->accum = 0;
	    pwmgen->lockout = 0;
	    pwmgen->old_ppwm = pwmgen->pure_pwm;
	    pwmgen->curr_output = 0;
	}

	/* convert value command to duty cycle */
	tmpdc = *(pwmgen->value) * pwmgen->scale_recip;
	if ( pwmgen->output_type == 0 ) {
	    /* unidirectional mode, no negative output */
	    if ( tmpdc < 0.0 ) {
		tmpdc = 0.0;
	    }
	}
	/* limit the duty cycle */
	if (tmpdc >= 0.0) {
	    if ( tmpdc > pwmgen->max_dc ) {
		tmpdc = pwmgen->max_dc;
	    } else if ( tmpdc < pwmgen->min_dc ) {
		tmpdc = pwmgen->min_dc;
	    }
	    pwmgen->direction = 0;
	    outdc = tmpdc;
	} else {
	    if ( tmpdc < -pwmgen->max_dc ) {
		tmpdc = -pwmgen->max_dc;
	    } else if ( tmpdc > -pwmgen->min_dc ) {
		tmpdc = -pwmgen->min_dc;
	    }
	    pwmgen->direction = 1;
	    outdc = -tmpdc;
	}
	/* save duty cycle */
	pwmgen->curr_dc = tmpdc;
	/* calculate constants for make_pulses */
	if ( pwmgen->pure_pwm ) {
	    /* calculate K1 and K2 that will give constant duty cycle
	       even if it results in large error due to low resolution */
	    pwmgen->k2 = pwmgen->lockout_time;
	    pwmgen->k1 = pwmgen->lockout_time * outdc + 0.5;
	} else {
	    /* calculate K1 and K2 that will allow duty cycle to dither
	       to more closely approach the desired value */
	    pwmgen->k2 = periodns;
	    pwmgen->k1 = periodns * outdc + 0.5;
	}
	/* if using PWM/DIR outputs, set DIR pin */
	if ( pwmgen->output_type == 1 ) {
	    *(pwmgen->out[DIR_PIN]) = pwmgen->direction;
	}
	/* move on to next channel */
	pwmgen++;
    }
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
    retval = hal_param_float_newf(HAL_WR, &(addr->scale), comp_id,
	    "pwmgen.%d.scale", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_param_bit_newf(HAL_WR, &(addr->pure_pwm), comp_id,
	    "pwmgen.%d.pure-pwm", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_param_float_newf(HAL_WR, &(addr->max_freq), comp_id,
	    "pwmgen.%d.max-freq", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_param_float_newf(HAL_WR, &(addr->min_dc), comp_id,
	    "pwmgen.%d.min-dc", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_param_float_newf(HAL_WR, &(addr->max_dc), comp_id,
	    "pwmgen.%d.max-dc", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_param_float_newf(HAL_RD, &(addr->curr_dc), comp_id,
	    "pwmgen.%d.curr-dc", num);
    if (retval != 0) {
	return retval;
    }
    /* export pins */
    retval = hal_pin_bit_newf(HAL_RD, &(addr->enable), comp_id,
	    "pwmgen.%d.enable", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_RD, &(addr->value), comp_id,
	    "pwmgen.%d.value", num);
    if (retval != 0) {
	return retval;
    }
    /* set default parameter values */
    addr->scale = 1.0;
    addr->pure_pwm = 0;
    addr->max_freq = 0;
    addr->min_dc = 0.0;
    addr->max_dc = 1.0;
    addr->output_type = output_type;
    /* init the step generator core to zero output */
    addr->k1 = 0;
    addr->k2 = 0;
    addr->lockout_time = 2;
    addr->lockout = 0;
    addr->accum = 0;
    addr->curr_output = 0;
    addr->direction = 0;
    addr->old_scale = addr->scale + 1.0;
    addr->old_ppwm = 3;
    addr->old_max_freq = -1;
    addr->curr_dc = 0.0;
    if (output_type == 2) {
	/* export UP/DOWN pins */
	retval = hal_pin_bit_newf(HAL_WR, &(addr->out[UP_PIN]), comp_id,
		"pwmgen.%d.up", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[UP_PIN]) = 0;
	retval = hal_pin_bit_newf(HAL_WR, &(addr->out[DOWN_PIN]), comp_id,
		"pwmgen.%d.down", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[DOWN_PIN]) = 0;
    } else {
	/* export PWM pin */
	retval = hal_pin_bit_newf(HAL_WR, &(addr->out[PWM_PIN]), comp_id,
		"pwmgen.%d.pwm", num);
	if (retval != 0) {
	    return retval;
	}
	/* init the pin */
	*(addr->out[PWM_PIN]) = 0;
	if ( output_type == 1 ) {
	    /* export DIR pin */
	    retval = hal_pin_bit_newf(HAL_WR, &(addr->out[DIR_PIN]), comp_id,
		    "pwmgen.%d.dir", num);
	    if (retval != 0) {
		return retval;
	    }
	    /* init the pin */
	    *(addr->out[DIR_PIN]) = 0;
	}
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
