/********************************************************************
* Description: timedelay.c
*   Time delay relay for HAL
*
*   See the "Users Manual" at emc2/docs/Hal_Introduction.pdf
*
* This component is the equivalent of a time delay relay.  There are
* separate parameters for "on delay" and "off delay".  The delays
* are specified in milliseconds.
*
* There are two pins, an input bit and an output bit.  The output is
* changed only if the input remains at the same state for the duration
* specified in the corresponding delay parameter.
*
* You can create several time delays at the same time, by supplying a
* command-line parameter when the component is loaded.  The number of
* delays is limited to 8.
*
* If you need that many delays though, you should consider using
* ClassicLadder instead of this component.  By default, one delay
* component will be created.
*
* The default on-delay and off-delay are both 500 ms.
*
*********************************************************************
*
* Author: Stephen Wille Padnos (swpadnos AT sourceforge DOT net)
*       Based on a work by John Kasunich
* License: GPL Version 2
* Created on: Dec. 16, 2005
* System: Linux
*
* Copyright (c) 2005 All rights reserved.
*
********************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("Stephen Wille Padnos");
MODULE_DESCRIPTION("Time Delay for EMC HAL");
MODULE_LICENSE("GPL");
static int num_delays = 1;		/* number of delays to create, default = 1 */
MODULE_PARM(num_delays, "i");
MODULE_PARM_DESC(num_delays, "Number of delays");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

#define MAX_DELAYS 8
#define DEFAULT_DELAY 0.500

/* Runtime data for a single delay. */
typedef struct {
    hal_bit_t *in;		/* pin: input */
    hal_bit_t *out;		/* pin: output */
    hal_float_t off_delay;	/* parameter: turn-off delay */
    hal_float_t on_delay;	/* parameter: turn-on delay */
    hal_float_t elapsed;	/* parameter: export the elapsed time */
    double timer;		/* internal: delay timer */
} bit_delay_t;

/* pointer to array of delay structs in shmem */
static bit_delay_t *delay_array;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_delay(int num, bit_delay_t * addr);
static void process_delays(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;

    /* test for too many delays asked for */
    if ((num_delays <= 0) || (num_delays > MAX_DELAYS)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "TIMEDELAY: ERROR: Invalid number of bit delays\n");
	return -1;
    }
    
    /* have good config info, connect to the HAL */
    comp_id = hal_init("timedelay");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for delay array */
    delay_array = hal_malloc(num_delays * sizeof(bit_delay_t));
    if (delay_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export pins/params for all delays */
    for (n = 0; n < num_delays; n++) {
	/* export all vars */
	retval = export_delay(n, &(delay_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"TIMEDELAY: ERROR: group %d export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }

    /* export update function */
    retval = hal_export_funct("process_delays", process_delays, delay_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: process_delays funct export failed\n");
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"TIMEDELAY: installed %d time delays\n", num_delays);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                     REALTIME DELAY FUNCTION                          *
************************************************************************/

/*  The delay works by checking the input and output states, and either
    incrementing a counter, or zeroing it.
    if (input == output), zero the counter
    else increment counter, and change the output if counter > delay time
*/
static void process_delays(void *arg, long period)
{
    bit_delay_t *delays, *thisdelay;
    int n;
    double dperiod;

    /* point to filter group */
    delays = (bit_delay_t *) arg;

    /* convert period from a long in nanoseconds to a double in seconds */
    dperiod = (double)1.0e-9 * period;

    for (n=0;n<num_delays; n++) {
	thisdelay = &(delays[n]);
	if (*(thisdelay->in) != *(thisdelay->out)) {
	    /* need to process the delay */
	    thisdelay->timer += dperiod;
	    if (*(thisdelay->in) == 0) {
		/* off delay */
		if (thisdelay->timer >= thisdelay->off_delay) {
		    *(thisdelay->out) = 0;
		    thisdelay->timer = 0.0;
		}
	    } else {
		/* on delay */
		if (thisdelay->timer >= thisdelay->on_delay) {
		    *(thisdelay->out) = 1;
		    thisdelay->timer = 0.0;
		}
	    }
	} else {
	    /* input == output, so reset the delay timer */
	    thisdelay->timer = 0.0;
	}
	/* update the exported timer param */
	thisdelay->elapsed = thisdelay->timer;
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_delay(int num, bit_delay_t * addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 2];

    /* export pin for input bit */
    rtapi_snprintf(buf, HAL_NAME_LEN, "delay.%d.in", num);
    retval = hal_pin_bit_new(buf, HAL_IN, &(addr->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output bit */
    rtapi_snprintf(buf, HAL_NAME_LEN, "delay.%d.out", num);
    retval = hal_pin_bit_new(buf, HAL_OUT, &(addr->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    
    /* export off delay parameter */
    rtapi_snprintf(buf, HAL_NAME_LEN, "delay.%d.off_delay", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->off_delay), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: '%s' parameter export failed\n", buf);
	return retval;
    }
    /* export on delay parameter */
    rtapi_snprintf(buf, HAL_NAME_LEN, "delay.%d.on_delay", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->on_delay), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: '%s' parameter export failed\n", buf);
	return retval;
    }
    /* export elapsed time parameter */
    rtapi_snprintf(buf, HAL_NAME_LEN, "delay.%d.elapsed", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->elapsed), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "TIMEDELAY: ERROR: '%s' parameter export failed\n", buf);
	return retval;
    }

    /* set initial parameter and pin values */
    *(addr->in) = 0;
    *(addr->out) = 0;
    addr->timer = 0.0;
    addr->off_delay = DEFAULT_DELAY;
    addr->on_delay = DEFAULT_DELAY;
    addr->elapsed = 0.0;
    return 0;
}
