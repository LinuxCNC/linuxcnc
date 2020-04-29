/********************************************************************
* Description:  siggen.c
*               This file, 'siggen.c', is a HAL component that 
*               generates square, triangle, sine, cosine, and 
*               sawtooth waves plus a clock signal.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change:  17Nov2010 - Matt Shaver added the "clock" output pin.
********************************************************************/
/** This file, 'siggen.c', is a HAL component that generates square,
    triangle, sine, cosine, and sawtooth waves.  I expect that it 
    will mostly be used for testing.  It is a realtime component.

    It supports any number of signal generators, as set by the
    insmod parameter 'num_chan'.  Alternatively,use the names= specifier
    and a list of unique names separated by commas.  The names= and
    num_chan= specifiers are mututally exclusive.

    Each generator has a number of pins and parameters, whose
    names begin with 'siggen.x.', where 'x' is the generator number.
    Generator numbers start at zero.

    Each generator is controlled by three pins.  'frequency' sets
    the frequency in Hertz.  'amplitude' sets the peak amplitude,
    and 'offset' sets the DC offset.  For example, if 'amplitude'
    is 1.0 and 'offset' is 0.0, the outputs will swing from -1.0
    to +1.0.  If 'amplitude' is 2.5 and 'offset' is 10.0, then
    the outputs will swing from 7.5 to 12.5.

    There are six output pins: 'square', 'triangle', 'sine', 'cosine',
    'clock', and 'sawtooth'.  All six run at the same frequency,
    amplitude, and offset.

    This component exports one function per signal generator,
    called 'siggen.x.update'.  It is a floating point function.

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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
#include <float.h>
#include <rtapi_math.h>
#include <rtapi_string.h>

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Signal Generator Component for EMC HAL");
MODULE_LICENSE("GPL");
static int num_chan;	/* number of channels */
static int default_num_chan = 1;
static int howmany;
RTAPI_MP_INT(num_chan, "number of channels");

#define MAX_CHAN 16
static char *names[MAX_CHAN] = {0,};
RTAPI_MP_ARRAY_STRING(names, MAX_CHAN, "names of siggen");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** This structure contains the runtime data for a single siggen.
*/

typedef struct {
    hal_float_t *square;	/* pin: output */
    hal_float_t *sawtooth;	/* pin: output */
    hal_float_t *triangle;	/* pin: output */
    hal_float_t *sine;		/* pin: output */
    hal_float_t *cosine;	/* pin: output */
    hal_bit_t *clock;		/* pin: output */
    hal_float_t *frequency;	/* pin: frequency */
    hal_float_t *amplitude;	/* pin: amplitude */
    hal_float_t *offset;	/* pin: offset */
    hal_bit_t *reset;		/* pin: reset */
    double index;		/* position within output cycle */
} hal_siggen_t;

/* pointer to array of siggen_t structs in shared memory, 1 per gen */
static hal_siggen_t *siggen_array;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_siggen(int num, hal_siggen_t * addr,char* prefix);
static void calc_siggen(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/


int rtapi_app_main(void)
{
    int n, retval, i;

    if(num_chan && names[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR,"num_chan= and names= are mutually exclusive\n");
        return -EINVAL;
    }
    if(!num_chan && !names[0]) num_chan = default_num_chan;

    if(num_chan) {
        howmany = num_chan;
    } else {
        howmany = 0;
        for (i = 0; i < MAX_CHAN; i++) {
            if ( (names[i] == NULL) || (*names[i] == 0) ){
                break;
            }
            howmany = i + 1;
        }
    }

    /* test for number of channels */
    if ((howmany <= 0) || (howmany > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SIGGEN: ERROR: invalid number of channels: %d\n", howmany);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("siggen");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SIGGEN: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for siggen data */
    siggen_array = hal_malloc(howmany * sizeof(hal_siggen_t));
    if (siggen_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SIGGEN: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export variables and functions for each siggen */
    i = 0; // for names= items
    for (n = 0; n < howmany; n++) {
	/* export everything for this loop */
        if(num_chan) {
            char buf[HAL_NAME_LEN + 1];
            rtapi_snprintf(buf, sizeof(buf), "siggen.%d", n);
	    retval = export_siggen(n, &(siggen_array[n]),buf);
        } else {
	    retval = export_siggen(n, &(siggen_array[n]),names[i++]);
        }

	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SIGGEN: ERROR: siggen %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"SIGGEN: installed %d signal generators\n", howmany);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME LOOP CALCULATIONS                     *
************************************************************************/

static void calc_siggen(void *arg, long period)
{
    hal_siggen_t *siggen;
    double tmp1, tmp2;

    /* point to the data for this signal generator */
    siggen = arg;
    /* calculate the time since last execution */
    tmp1 = period * 0.000000001;

    /* calculate how much of an output cycle that has passed */
    tmp2 = *(siggen->frequency) * tmp1;
    /* limit frequency to comply with Nyquist limit */
    if ( tmp2 > 0.5 ) {
	*(siggen->frequency) = 0.5 / tmp1;
	tmp2 = 0.5;
    }
    /* index ramps from 0.0 to 0.99999 for each output cycle */
    if ( *(siggen->reset) ) {
	siggen->index  = 0.5;
    } else {
	siggen->index += tmp2;
    }
    /* wrap index if it is >= 1.0 */
    if ( siggen->index >= 1.0 ) {
	siggen->index -= 1.0;
    }

    /* generate the square wave and clock output */
    /* tmp1 steps from -1.0 to +1.0 when index passes 0.5 */
    if ( siggen->index > 0.5 ) {
	tmp1 = 1.0;
	*(siggen->clock) = 1;
    } else {
	tmp1 = -1.0;
	*(siggen->clock) = 0;
    }
    /* apply scaling and offset, and write to output */
    *(siggen->square) = (tmp1 * *(siggen->amplitude)) + *(siggen->offset);

    /* generate the sawtooth wave output */
    /* tmp2 ramps from -1.0 to +1.0 as index goes from 0 to 1 */
    tmp2 = (siggen->index * 2.0) - 1.0;
    /* apply scaling and offset, and write to output */
    *(siggen->sawtooth) = (tmp2 * *(siggen->amplitude)) + *(siggen->offset);

    /* generate the triangle wave output */
    /* tmp2 ramps from -2.0 to +2.0 as index goes from 0 to 1 */
    tmp2 *= 2.0;
    /* flip first half of ramp, now goes from +1 to -1 to +1 */
    tmp2 = (tmp2 * tmp1) - 1.0;
    /* apply scaling and offset, and write to output */
    *(siggen->triangle) = (tmp2 * *(siggen->amplitude)) + *(siggen->offset);

    /* generate the sine wave output */
    /* tmp1 is angle in radians */
    tmp1 = siggen->index * (2.0 * 3.1415927);
    /* get sine, apply scaling and offset, and write to output */
    *(siggen->sine) = (sin(tmp1) * *(siggen->amplitude)) + *(siggen->offset);

    /* generate the cosine wave output */
    /* get cosine, apply scaling and offset, and write to output */
    *(siggen->cosine) = (cos(tmp1) * *(siggen->amplitude)) + *(siggen->offset);
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_siggen(int num, hal_siggen_t * addr,char* prefix)
{
    int retval;
    char buf[HAL_NAME_LEN + 1];

    /* export pins */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->square), comp_id,
				"%s.square", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->sawtooth), comp_id,
				"%s.sawtooth", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->triangle), comp_id,
				"%s.triangle", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->sine), comp_id,
				"%s.sine", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(addr->cosine), comp_id,
				"%s.cosine", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_OUT, &(addr->clock), comp_id,
				"%s.clock", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->frequency), comp_id,
				"%s.frequency", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->amplitude), comp_id,
				"%s.amplitude", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(addr->offset), comp_id,
				"%s.offset", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->reset), comp_id,
				"%s.reset", prefix);
    if (retval != 0) {
	return retval;
    }
    /* init all structure members */
    *(addr->square) = 0.0;
    *(addr->sawtooth) = 0.0;
    *(addr->triangle) = 0.0;
    *(addr->sine) = 0.0;
    *(addr->cosine) = 0.0;
    *(addr->clock) = 0;
    *(addr->frequency) = 1.0;
    *(addr->amplitude) = 1.0;
    *(addr->offset) = 0.0;
    addr->index = 0.0;
    /* export function for this loop */
    rtapi_snprintf(buf, sizeof(buf), "%s.update", prefix);
    retval =
	hal_export_funct(buf, calc_siggen, &(siggen_array[num]), 1, 0,
	comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SIGGEN: ERROR: update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}
