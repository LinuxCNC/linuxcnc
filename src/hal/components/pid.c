/** This file, 'pid.c', is a HAL component that provides Proportional/
    Integeral/Derivative control loops.  It is a realtime component.

    It supports a maximum of 16 PID loops, as set by the insmod parameter
    'num_chan'.

    In this documentation, it is assumed that we are discussing position
    loops.  However this component can be used to implement other loops
    such as speed loops, torch height control, and others.

    Each loop has a number of pins and parameters, whose names begin
    with 'pid.x.', where 'x' is the channel number.

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
    deadband	Amount of error that will be ignored
    maxerror	Limit on error
    maxerrorI	Limit on error integrator
    maxerrorD	Limit on error differentiator
    maxcmdD	Limit on command differentiator
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

    The PID loop calculations are as follows (see the code for
    all the nitty gritty details):

    error = command - feedback
    if ( abs(error) < deadband ) then error = 0
    limit error to +/- maxerror
    errorI += error * period
    limit errorI to +/- maxerrorI
    errorD = (error - previouserror) / period
    limit errorD to +/- paxerrorD
    commandD = (command - previouscommand) / period
    limit commandD to +/- maxcmdD
    output = bias + error * Pgain + errorI * Igain +
             errorD * Dgain + command * FF0 + commandD + FF1
    limit output to +/- maxoutput

    This component exports one function called 'pid.x.do_pid_calcs'
    for each PID loop.  This allows loops to be included in different
    threads and execute at different rates.
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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("PID Loop Component for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static int num_chan = 3;	/* number of channels - default = 3 */
MODULE_PARM(num_chan, "i");
MODULE_PARM_DESC(chan, "number of channels");
static int debug = 0;		/* flag to export optional params */
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "enables optional params");
static long fp_period = 0;	/* float pt thread period, default = none */
MODULE_PARM(fp_period, "l");
MODULE_PARM_DESC(fp_period, "floating point thread period (nsecs)");
#endif /* MODULE */

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
    hal_float_t deadband;	/* param: deadband */
    hal_float_t maxerror;	/* param: limit for error */
    hal_float_t maxerror_i;	/* param: limit for integrated error */
    hal_float_t maxerror_d;	/* param: limit for differentiated error */
    hal_float_t maxcmd_d;	/* param: limit for differentiated cmd */
    hal_float_t error_i;	/* opt. param: integrated error */
    float prev_error;		/* previous error for differentiator */
    hal_float_t error_d;	/* opt. param: differentiated error */
    float prev_cmd;		/* previous command for differentiator */
    hal_float_t cmd_d;		/* opt. param: differentiated command */
    hal_float_t bias;		/* param: steady state offset */
    hal_float_t pgain;		/* param: proportional gain */
    hal_float_t igain;		/* param: integral gain */
    hal_float_t dgain;		/* param: derivative gain */
    hal_float_t ff0gain;	/* param: feedforward proportional */
    hal_float_t ff1gain;	/* param: feedforward derivative */
    hal_float_t maxoutput;	/* param: limit for PID output */
    hal_float_t *output;	/* pin: the output value */
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
    comp_id = hal_init("pid_loop");
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
	retval = export_pid(n + 1, &(pid_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PID: ERROR: loop %d var export failed\n", n + 1);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "PID: installed %d PID loops\n",
	num_chan);
    if (fp_period > 0) {
	/* create a floating point thread */
	retval = hal_create_thread("pid.threadFP", fp_period, 1);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PID: ERROR: could not create FP thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "PID: created %d uS FP thread\n",
		fp_period / 1000);
	}
    }
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
    float tmp1;
    int enable;
    float periodfp, periodrecip;

    /* point to the data for this PID loop */
    pid = arg;
    /* precalculate some timing constants */
    periodfp = period * 0.000000001;
    periodrecip = 1.0 / periodfp;
    /* get the enable bit */
    enable = *(pid->enable);
    /* calculate the error */
    tmp1 = *(pid->command) - *(pid->feedback);
    /* apply the deadband */
    if ((tmp1 < pid->deadband) && (tmp1 > -pid->deadband)) {
	tmp1 = 0;
    }
    /* apply error limits */
    if (pid->maxerror != 0.0) {
	if (tmp1 > pid->maxerror) {
	    tmp1 = pid->maxerror;
	} else if (tmp1 < -pid->maxerror) {
	    tmp1 = -pid->maxerror;
	}
    }
    /* store error to error pin */
    *(pid->error) = tmp1;
    /* do integrator calcs only if enabled */
    if (enable != 0) {
	/* compute integral term */
	pid->error_i += tmp1 * periodfp;
	/* apply integrator limits */
	if (pid->maxerror_i != 0.0) {
	    if (pid->error_i > pid->maxerror_i) {
		pid->error_i = pid->maxerror_i;
	    } else if (pid->error_i < -pid->maxerror_i) {
		pid->error_i = -pid->maxerror_i;
	    }
	}
    } else {
	/* not enabled, reset integrator */
	pid->error_i = 0;
    }
    /* calculate derivative term */
    pid->error_d = (tmp1 - pid->prev_error) * periodrecip;
    pid->prev_error = tmp1;
    /* apply derivative limits */
    if (pid->maxerror_d != 0.0) {
	if (pid->error_d > pid->maxerror_d) {
	    pid->error_d = pid->maxerror_d;
	} else if (pid->error_d < -pid->maxerror_d) {
	    pid->error_d = -pid->maxerror_d;
	}
    }
    /* calculate derivative of command */
    pid->cmd_d = (*(pid->command) - pid->prev_cmd) * periodrecip;
    pid->prev_cmd = *(pid->command);
    /* apply derivative limits */
    if (pid->maxcmd_d != 0.0) {
	if (pid->cmd_d > pid->maxcmd_d) {
	    pid->cmd_d = pid->maxcmd_d;
	} else if (pid->cmd_d < -pid->maxcmd_d) {
	    pid->cmd_d = -pid->maxcmd_d;
	}
    }
    /* do output calcs only if enabled */
    if (enable != 0) {
	/* calculate the output value */
	tmp1 =
	    pid->bias + pid->pgain * tmp1 + pid->igain * pid->error_i +
	    pid->dgain * pid->error_d;
	tmp1 += *(pid->command) * pid->ff0gain + pid->cmd_d * pid->ff1gain;
	/* apply output limits */
	if (pid->maxoutput != 0.0) {
	    if (tmp1 > pid->maxoutput) {
		tmp1 = pid->maxoutput;
	    } else if (tmp1 < -pid->maxoutput) {
		tmp1 = -pid->maxoutput;
	    }
	}
    } else {
	/* not enabled, force output to zero */
	tmp1 = 0.0;
    }
    /* write final output value to output pin */
    *(pid->output) = tmp1;
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_pid(int num, hal_pid_t * addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 2];

    /* export pins */
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.enable", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->enable), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.command", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->command), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.feedback", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->feedback), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.error", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->error), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.output", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->output), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameters */
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.deadband", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->deadband), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.maxerror", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxerror), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.maxerrorI", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxerror_i), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.maxerrorD", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxerror_d), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.maxcmdD", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxcmd_d), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.bias", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->bias), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.Pgain", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->pgain), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.Igain", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->igain), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.Dgain", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->dgain), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.FF0", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->ff0gain), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.FF1", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->ff1gain), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.maxoutput", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxoutput), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export optional parameters */
    if (debug > 0) {
	rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.errorI", num);
	retval = hal_param_float_new(buf, HAL_RD, &(addr->error_i), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.errorD", num);
	retval = hal_param_float_new(buf, HAL_RD, &(addr->error_d), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.commandD", num);
	retval = hal_param_float_new(buf, HAL_RD, &(addr->cmd_d), comp_id);
	if (retval != 0) {
	    return retval;
	}
    }
    /* init all structure members */
    *(addr->enable) = 0;
    *(addr->command) = 0;
    *(addr->feedback) = 0;
    *(addr->error) = 0;
    *(addr->output) = 0;
    addr->deadband = 0.0;
    addr->maxerror = 0.0;
    addr->maxerror_i = 0.0;
    addr->maxerror_d = 0.0;
    addr->maxcmd_d = 0.0;
    addr->error_i = 0.0;
    addr->prev_error = 0.0;
    addr->error_d = 0.0;
    addr->prev_cmd = 0.0;
    addr->cmd_d = 0.0;
    addr->bias = 0.0;
    addr->pgain = 1.0;
    addr->igain = 0.0;
    addr->dgain = 0.0;
    addr->ff0gain = 0.0;
    addr->ff1gain = 0.0;
    addr->maxoutput = 0.0;
    /* export function for this loop */
    rtapi_snprintf(buf, HAL_NAME_LEN, "pid.%d.do_pid_calcs", num);
    retval =
	hal_export_funct(buf, calc_pid, &(pid_array[num - 1]), 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PID: ERROR: do_pid_calcs funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}
