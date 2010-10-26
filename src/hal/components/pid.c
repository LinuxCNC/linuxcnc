/********************************************************************
* Description:  pid.c
*               This file, 'pid.c', is a HAL component that provides 
*               Proportional/Integeral/Derivative control loops.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/
/** This file, 'pid.c', is a HAL component that provides Proportional/
    Integeral/Derivative control loops.  It is a realtime component.

    It supports a maximum of 16 PID loops, as set by the insmod parameter
    'num_chan'.

    In this documentation, it is assumed that we are discussing position
    loops.  However this component can be used to implement other loops
    such as speed loops, torch height control, and others.

    Each loop has a number of pins and parameters, whose names begin
    with 'pid.x.', where 'x' is the channel number.  Channel numbers
    start at zero.

    The three most important pins are 'command', 'feedback', and
    'output'.  For a position loop, 'command' and 'feedback' are
    in position units.  For a linear axis, this could be inches,
    mm, metres, or whatever is relavent.  Likewise, for a angular
    axis, it could be degrees, radians, etc.  The units of the
    'output' pin represent the change needed to make the feedback
    match the command.  As such, for a position loop 'Output' is
    a velocity, in inches/sec, mm/sec, degrees/sec, etc.

    Each loop has several other pins as well.  'error' is equal to
    'command' minus 'feedback'.  'enable' is a bit that enables
    the loop.  If 'enable' is false, all integrators are reset,
    and the output is forced to zero.  If 'enable' is true, the
    loop operates normally.

    The PID gains, limits, and other 'tunable' features of the
    loop are implemented as parameters.  These are as follows:

    Pgain	Proportional gain
    Igain	Integral gain
    Dgain	Derivative gain
    bias	Constant offset on output
    FF0		Zeroth order Feedforward gain
    FF1		First order Feedforward gain
    FF2		Second order Feedforward gain
    deadband	Amount of error that will be ignored
    maxerror	Limit on error
    maxerrorI	Limit on error integrator
    maxerrorD	Limit on error differentiator
    maxcmdD	Limit on command differentiator
    maxcmdDD	Limit on command 2nd derivative
    maxoutput	Limit on output value

    All of the limits (max____) are implemented such that if the
    parameter value is zero, there is no limit.

    A number of internal values which may be usefull for testing
    and tuning are also available as parameters.  To avoid cluttering
    the parameter list, these are only exported if "debug=1" is
    specified on the insmod command line.

    errorI	Integral of error
    errorD	Derivative of error
    commandD	Derivative of the command
    commandDD	2nd derivative of the command

    The PID loop calculations are as follows (see the code for
    all the nitty gritty details):

    error = command - feedback
    if ( abs(error) < deadband ) then error = 0
    limit error to +/- maxerror
    errorI += error * period
    limit errorI to +/- maxerrorI
    errorD = (error - previouserror) / period
    limit errorD to +/- maxerrorD
    commandD = (command - previouscommand) / period
    limit commandD to +/- maxcmdD
    commandDD = (commandD - previouscommandD) / period
    limit commandDD to +/- maxcmdDD
    output = bias + error * Pgain + errorI * Igain +
             errorD * Dgain + command * FF0 + commandD * FF1 +
             commandDD * FF2
    limit output to +/- maxoutput

    This component exports one function called 'pid.x.do-pid-calcs'
    for each PID loop.  This allows loops to be included in different
    threads and execute at different rates.
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

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("PID Loop Component for EMC HAL");
MODULE_LICENSE("GPL");
static int num_chan = 3;	/* number of channels - default = 3 */
RTAPI_MP_INT(num_chan, "number of channels");
static int debug = 0;		/* flag to export optional params */
RTAPI_MP_INT(debug, "enables optional params");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** This structure contains the runtime data for a single PID loop.
    The data is arranged to optimize speed - they are placed in the
    order in which they will be accessed, so that when one item is
    accessed, the next item(s) will be pulled into the cache.  In
    addition, items that are written are grouped together, so only
    a few cache lines will need to be written back to main memory.
*/

typedef struct {
    hal_bit_t *enable;		/* pin: enable input */
    hal_float_t *command;	/* pin: commanded value */
    hal_float_t *feedback;	/* pin: feedback value */
    hal_float_t *error;		/* pin: command - feedback */
    hal_float_t *deadband;	/* pin: deadband */
    hal_float_t *maxerror;	/* pin: limit for error */
    hal_float_t *maxerror_i;	/* pin: limit for integrated error */
    hal_float_t *maxerror_d;	/* pin: limit for differentiated error */
    hal_float_t *maxcmd_d;	/* pin: limit for differentiated cmd */
    hal_float_t *maxcmd_dd;	/* pin: limit for 2nd derivative of cmd */
    hal_float_t *error_i;	/* opt. pin: integrated error */
    double prev_error;		/* previous error for differentiator */
    hal_float_t *error_d;	/* opt. pin: differentiated error */
    double prev_cmd;		/* previous command for differentiator */
    double limit_state;		/* +1 or -1 if in limit, else 0.0 */
    hal_float_t *cmd_d;		/* opt. pin: differentiated command */
    hal_float_t *cmd_dd;	/* opt. pin: 2nd derivative of command */
    hal_float_t *bias;		/* param: steady state offset */
    hal_float_t *pgain;		/* pin: proportional gain */
    hal_float_t *igain;		/* pin: integral gain */
    hal_float_t *dgain;		/* pin: derivative gain */
    hal_float_t *ff0gain;	/* pin: feedforward proportional */
    hal_float_t *ff1gain;	/* pin: feedforward derivative */
    hal_float_t *ff2gain;	/* pin: feedforward 2nd derivative */
    hal_float_t *maxoutput;	/* pin: limit for PID output */
    hal_float_t *output;	/* pin: the output value */
    hal_bit_t   *saturated;	/* pin: TRUE when the output is saturated */
    hal_float_t *saturated_s;  /* pin: the time the output has been saturated */
    hal_s32_t   *saturated_count;
			       /* pin: the time the output has been saturated */
    hal_bit_t *index_enable;   /* pin: to monitor for step changes that would
                                       otherwise screw up FF */
    char prev_ie;
} hal_pid_t;

/* pointer to array of pid_t structs in shared memory, 1 per loop */
static hal_pid_t *pid_array;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_pid(int num, hal_pid_t * addr);
static void calc_pid(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 16

int rtapi_app_main(void)
{
    int n, retval;

    /* test for number of channels */
    if ((num_chan <= 0) || (num_chan > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PID: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("pid");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PID: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for pid loop data */
    pid_array = hal_malloc(num_chan * sizeof(hal_pid_t));
    if (pid_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PID: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export variables and function for each PID loop */
    for (n = 0; n < num_chan; n++) {
	/* export everything for this loop */
	retval = export_pid(n, &(pid_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PID: ERROR: loop %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "PID: installed %d PID loops\n",
	num_chan);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                   REALTIME PID LOOP CALCULATIONS                     *
************************************************************************/

static void calc_pid(void *arg, long period)
{
    hal_pid_t *pid;
    double tmp1, tmp2, command, feedback;
    int enable;
    double periodfp, periodrecip;

    /* point to the data for this PID loop */
    pid = arg;
    /* precalculate some timing constants */
    periodfp = period * 0.000000001;
    periodrecip = 1.0 / periodfp;
    /* get the enable bit */
    enable = *(pid->enable);
    /* read the command and feedback only once */
    command = *(pid->command);
    feedback = *(pid->feedback);
    /* calculate the error */
    tmp1 = command - feedback;
    /* store error to error pin */
    *(pid->error) = tmp1;
    /* apply error limits */
    if (*(pid->maxerror) != 0.0) {
	if (tmp1 > *(pid->maxerror)) {
	    tmp1 = *(pid->maxerror);
	} else if (tmp1 < -*(pid->maxerror)) {
	    tmp1 = -*(pid->maxerror);
	}
    }
    /* apply the deadband */
    if (tmp1 > *(pid->deadband)) {
	tmp1 -= *(pid->deadband);
    } else if (tmp1 < -*(pid->deadband)) {
	tmp1 += *(pid->deadband);
    } else {
	tmp1 = 0;
    }
    /* do integrator calcs only if enabled */
    if (enable != 0) {
	/* if output is in limit, don't let integrator wind up */
	if ( ( tmp1 * pid->limit_state ) <= 0.0 ) {
	    /* compute integral term */
	    *(pid->error_i) += tmp1 * periodfp;
	}
	/* apply integrator limits */
	if (*(pid->maxerror_i) != 0.0) {
	    if (*(pid->error_i) > *(pid->maxerror_i)) {
		*(pid->error_i) = *(pid->maxerror_i);
	    } else if (*(pid->error_i) < -*(pid->maxerror_i)) {
		*(pid->error_i) = -*(pid->maxerror_i);
	    }
	}
    } else {
	/* not enabled, reset integrator */
	*(pid->error_i) = 0;
    }
    /* calculate derivative term */
    *(pid->error_d) = (tmp1 - pid->prev_error) * periodrecip;
    pid->prev_error = tmp1;
    /* apply derivative limits */
    if (*(pid->maxerror_d) != 0.0) {
	if (*(pid->error_d) > *(pid->maxerror_d)) {
	    *(pid->error_d) = *(pid->maxerror_d);
	} else if (*(pid->error_d) < -*(pid->maxerror_d)) {
	    *(pid->error_d) = -*(pid->maxerror_d);
	}
    }
    /* calculate derivative of command */
    /* save old value for 2nd derivative calc later */
    tmp2 = *(pid->cmd_d);
    if(!(pid->prev_ie && !*(pid->index_enable))) {
        // not falling edge of index_enable: the normal case
        *(pid->cmd_d) = (*(pid->command) - pid->prev_cmd) * periodrecip;
    }
    // else: leave cmd_d alone and use last period's.  prev_cmd
    // shouldn't be trusted because index homing has caused us to have
    // a step in position.  Using the previous period's derivative is
    // probably a decent approximation since index search is usually a
    // slow steady speed.

    // save ie for next time
    pid->prev_ie = *(pid->index_enable);

    pid->prev_cmd = *(pid->command);
    /* apply derivative limits */
    if (*(pid->maxcmd_d) != 0.0) {
	if (*(pid->cmd_d) > *(pid->maxcmd_d)) {
	    *(pid->cmd_d) = *(pid->maxcmd_d);
	} else if (*(pid->cmd_d) < -*(pid->maxcmd_d)) {
	    *(pid->cmd_d) = -*(pid->maxcmd_d);
	}
    }
    /* calculate 2nd derivative of command */
    *(pid->cmd_dd) = (*(pid->cmd_d) - tmp2) * periodrecip;
    /* apply 2nd derivative limits */
    if (*(pid->maxcmd_dd) != 0.0) {
	if (*(pid->cmd_dd) > *(pid->maxcmd_dd)) {
	    *(pid->cmd_dd) = *(pid->maxcmd_dd);
	} else if (*(pid->cmd_dd) < -*(pid->maxcmd_dd)) {
	    *(pid->cmd_dd) = -*(pid->maxcmd_dd);
	}
    }
    /* do output calcs only if enabled */
    if (enable != 0) {
	/* calculate the output value */
	tmp1 =
	    *(pid->bias) + *(pid->pgain) * tmp1 + *(pid->igain) * *(pid->error_i) +
	    *(pid->dgain) * *(pid->error_d);
	tmp1 += *(pid->command) * *(pid->ff0gain) + *(pid->cmd_d) * *(pid->ff1gain) +
	    *(pid->cmd_dd) * *(pid->ff2gain);
	/* apply output limits */
	if (*(pid->maxoutput) != 0.0) {
	    if (tmp1 > *(pid->maxoutput)) {
		tmp1 = *(pid->maxoutput);
		pid->limit_state = 1.0;
	    } else if (tmp1 < -*(pid->maxoutput)) {
		tmp1 = -*(pid->maxoutput);
		pid->limit_state = -1.0;
	    } else {
		pid->limit_state = 0.0;
	    }
	}
    } else {
	/* not enabled, force output to zero */
	tmp1 = 0.0;
	pid->limit_state = 0.0;
    }
    /* write final output value to output pin */
    *(pid->output) = tmp1;

    /* set 'saturated' outputs */
    if(pid->limit_state) { 
        *(pid->saturated) = 1;
        *(pid->saturated_s) += period * 1e-9;
        if(*(pid->saturated_count) != 2147483647)
            (*pid->saturated_count) ++;
    } else {
        *(pid->saturated) = 0;
        *(pid->saturated_s) = 0;
        *(pid->saturated_count) = 0;
    }
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pid(int num, hal_pid_t * addr)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 1];

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export pins */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->enable), comp_id,
			      "pid.%d.enable", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->command), comp_id,
				"pid.%d.command", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->feedback), comp_id,
				"pid.%d.feedback", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->error), comp_id,
				"pid.%d.error", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->output), comp_id,
				"pid.%d.output", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_OUT, &(addr->saturated), comp_id,
			      "pid.%d.saturated", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->saturated_s), comp_id,
				"pid.%d.saturated-s", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_s32_newf(HAL_OUT, &(addr->saturated_count), comp_id,
			      "pid.%d.saturated-count", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->pgain), comp_id,
				"pid.%d.Pgain", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->igain), comp_id,
				"pid.%d.Igain", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->dgain), comp_id,
				"pid.%d.Dgain", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->ff0gain), comp_id,
				"pid.%d.FF0", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->ff1gain), comp_id,
				"pid.%d.FF1", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->ff2gain), comp_id,
				"pid.%d.FF2", num);
    if (retval != 0) {
	return retval;
    }
    /* export pins (previously parameters) */
    retval = hal_pin_float_newf(HAL_IO, &(addr->deadband), comp_id,
				"pid.%d.deadband", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxerror), comp_id,
				"pid.%d.maxerror", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxerror_i), comp_id,
				"pid.%d.maxerrorI", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxerror_d), comp_id,
				"pid.%d.maxerrorD", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxcmd_d), comp_id,
				"pid.%d.maxcmdD", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxcmd_dd), comp_id,
				"pid.%d.maxcmdDD", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->bias), comp_id,
				"pid.%d.bias", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IO, &(addr->maxoutput), comp_id,
				"pid.%d.maxoutput", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->index_enable), comp_id,
			      "pid.%d.index-enable", num);
    if (retval != 0) {
	return retval;
    }
    /* export optional parameters */
    if (debug > 0) {
	retval = hal_pin_float_newf(HAL_OUT, &(addr->error_i), comp_id,
				    "pid.%d.errorI", num);
	if (retval != 0) {
	    return retval;
	}
	retval = hal_pin_float_newf(HAL_OUT, &(addr->error_d), comp_id,
				    "pid.%d.errorD", num);
	if (retval != 0) {
	    return retval;
	}
	retval = hal_pin_float_newf(HAL_OUT, &(addr->cmd_d), comp_id,
				    "pid.%d.commandD", num);
	if (retval != 0) {
	    return retval;
	}
	retval = hal_pin_float_newf(HAL_OUT, &(addr->cmd_dd), comp_id,
				    "pid.%d.commandDD", num);
	if (retval != 0) {
	    return retval;
	}
    } else {
	addr->error_i = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
	addr->error_d = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
	addr->cmd_d = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
	addr->cmd_dd = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
    }

    *(addr->error_i) = 0.0;
    *(addr->error_d) = 0.0;
    *(addr->cmd_d) = 0.0;
    *(addr->cmd_dd) = 0.0;
    /* init all structure members */
    *(addr->enable) = 0;
    *(addr->command) = 0;
    *(addr->feedback) = 0;
    *(addr->error) = 0;
    *(addr->output) = 0;
    *(addr->deadband) = 0.0;
    *(addr->maxerror) = 0.0;
    *(addr->maxerror_i) = 0.0;
    *(addr->maxerror_d) = 0.0;
    *(addr->maxcmd_d) = 0.0;
    *(addr->maxcmd_dd) = 0.0;
    addr->prev_error = 0.0;
    addr->prev_cmd = 0.0;
    addr->limit_state = 0.0;
    *(addr->bias) = 0.0;
    *(addr->pgain) = 1.0;
    *(addr->igain) = 0.0;
    *(addr->dgain) = 0.0;
    *(addr->ff0gain) = 0.0;
    *(addr->ff1gain) = 0.0;
    *(addr->ff2gain) = 0.0;
    *(addr->maxoutput) = 0.0;
    /* export function for this loop */
    rtapi_snprintf(buf, sizeof(buf), "pid.%d.do-pid-calcs", num);
    retval =
	hal_export_funct(buf, calc_pid, &(pid_array[num]), 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PID: ERROR: do_pid_calcs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
