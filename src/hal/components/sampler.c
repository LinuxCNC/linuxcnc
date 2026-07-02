/********************************************************************
* Description:  sampler.c
*               A HAL component that can be used to capture data
*               from HAL pins at a specific realtime sample rate,
*		and allows the data to be written to stdout.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
/** This file, 'sampler.c', is the realtime part of a HAL component
    that allows data from HAL pins to be sampled at a uniform realtime
    sample rate and then be transferred to a file.  When this realtime
    module is loaded, it creates a fifo in shared memory and begins 
    capturing data from HAL pins to the fifo.  Then, the user space 
    program 'halsampler' is invoked, which reads the fifo and writes
    the data to stdout.

    Loading:

    loadrt sampler depth=100 cfg=uffb


*/

/** Copyright (C) 2006 John Kasunich
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

#include <rtapi.h>              /* RTAPI realtime OS API */
#include <rtapi_app.h>          /* RTAPI realtime module decls */
#include <hal.h>                /* HAL public API decls */
#include "streamer.h"		/* decls and such for fifos */
#include <rtapi_errno.h>
#include <rtapi_string.h>

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Realtime HAL Sampler");
MODULE_LICENSE("GPL");
static char *cfg[MAX_SAMPLERS];	/* config string, no default */
RTAPI_MP_ARRAY_STRING(cfg,MAX_SAMPLERS,"config string");
static int depth[MAX_SAMPLERS];	/* depth of fifo, default 0 */
RTAPI_MP_ARRAY_INT(depth,MAX_SAMPLERS,"fifo depth");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the HAL shared memory data for one sampler */

typedef struct {
    hal_stream_t fifo;		/* pointer to user/RT fifo */
    hal_sint_t curr_depth;	/* pin: current fifo depth */
    hal_bool_t full;		/* pin: overrun flag */
    hal_bool_t enable;		/* pin: enable sampling */
    hal_sint_t overruns;	/* pin: number of overruns */
    hal_sint_t sample_num;	/* pin: sample ID / timestamp */
    hal_refs_u pins[HAL_STREAM_MAX_PINS];
    int num_pins;
} sampler_t;

/* other globals */
static int comp_id;		/* component ID */
static int nsamplers;
static sampler_t *samplers;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int init_sampler(int num, sampler_t *tmp_fifo);
static void sample(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;

    comp_id = hal_init("sampler");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: hal_init() failed\n");
	return -EINVAL;
    }

    samplers = hal_malloc(MAX_SAMPLERS * sizeof(sampler_t));
    /* validate config info */
    for ( n = 0 ; n < MAX_SAMPLERS ; n++ ) {
	if (( cfg[n] == NULL ) || ( *(cfg[n]) == '\0' ) || ( depth[n] <= 0 )) {
	    break;
	}
	retval = hal_stream_create(&samplers[n].fifo, comp_id, SAMPLER_SHMEM_KEY+n, depth[n], cfg[n]);
	if(retval < 0) {
	    goto fail;
	}
	nsamplers++;
	retval = init_sampler(n, &samplers[n]);
    }
    if ( n == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: no channels specified\n");
	return -EINVAL;
    }

    hal_ready(comp_id);
    return 0;
fail:
    for(n=0; n<nsamplers; n++) hal_stream_detach(&samplers[n].fifo);
    hal_exit(comp_id);
    return retval;
}

void rtapi_app_exit(void)
{
    int i;
    for(i=0; i<nsamplers; i++) hal_stream_detach(&samplers[i].fifo);
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void sample(void *arg, long period)
{
    (void)period;
    sampler_t *samp = (sampler_t *)arg; /* point at sampler struct in HAL shmem */

    /* are we enabled? */
    if (!hal_get_bool(samp->enable)) {
        hal_set_si32(samp->curr_depth, hal_stream_depth(&samp->fifo));
        hal_set_bool(samp->full, !hal_stream_writable(&samp->fifo));
        return;
    }

    /* copy data from HAL pins to fifo */
    union hal_stream_data data[HAL_STREAM_MAX_PINS];
    int num_pins = hal_stream_element_count(&samp->fifo);
    for (int n = 0; n < num_pins; n++) {
        switch (hal_stream_element_type(&samp->fifo, n)) {
        case HAL_FLOAT: data[n].f = hal_get_real(samp->pins[n].r); break;
        case HAL_S32:   data[n].s = hal_get_si32(samp->pins[n].s); break;
        case HAL_U32:   data[n].u = hal_get_ui32(samp->pins[n].u); break;
        case HAL_S64:   data[n].l = hal_get_sint(samp->pins[n].s); break;
        case HAL_U64:   data[n].k = hal_get_uint(samp->pins[n].u); break;
        case HAL_BIT:   data[n].b = hal_get_bool(samp->pins[n].b); break;
        default:
            break;
        }
    }
    if ( hal_stream_write(&samp->fifo, data) < 0) {
        /* fifo is full, data is lost */
        /* log the overrun */
        hal_set_si32(samp->overruns, hal_get_si32(samp->overruns) + 1);
        hal_set_bool(samp->full, 1);
        hal_set_si32(samp->curr_depth, hal_stream_maxdepth(&samp->fifo));
    } else {
        hal_set_bool(samp->full, 0);
        hal_set_si32(samp->curr_depth, hal_stream_depth(&samp->fifo));
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int init_sampler(int num, sampler_t *str)
{
    int retval;
    static const char strbase[] = "sampler.";

    /* export "standard" pins and params */
    retval = hal_pin_new_bool(comp_id, HAL_OUT, &str->full, 0, "%s%d.full", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'full' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_bool(comp_id, HAL_IN, &str->enable, 1, "%s%d.enable", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'enable' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_OUT, &str->curr_depth, 0, "%s%d.curr-depth", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'curr_depth' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_IO, &str->overruns, 0, "%s%d.overruns", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'overruns' parameter export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_IO, &str->sample_num, 0, "%s%d.sample-num", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: 'sample-num' parameter export failed\n");
        return -EIO;
    }

    /* export user specified pins (the ones that sample data) */
    for (int n = 0; n < hal_stream_element_count(&str->fifo); n++) {
        static const char pstr[] = "sampler.%d.pin.%d";
        int type;
        switch(type = hal_stream_element_type(&str->fifo, n)) {
        case HAL_FLOAT: retval = hal_pin_new_real(comp_id, HAL_IN, &str->pins[n].r, 0.0, pstr, num, n); break;
        case HAL_S32:   retval = hal_pin_new_si32(comp_id, HAL_IN, &str->pins[n].s, 0,   pstr, num, n); break;
        case HAL_U32:   retval = hal_pin_new_ui32(comp_id, HAL_IN, &str->pins[n].u, 0,   pstr, num, n); break;
        case HAL_S64:   retval = hal_pin_new_sint(comp_id, HAL_IN, &str->pins[n].s, 0,   pstr, num, n); break;
        case HAL_U64:   retval = hal_pin_new_uint(comp_id, HAL_IN, &str->pins[n].u, 0,   pstr, num, n); break;
        case HAL_BIT:   retval = hal_pin_new_bool(comp_id, HAL_IN, &str->pins[n].b, 0,   pstr, num, n); break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: stream type for pin %d has bad type %d\n", n, type);
            return -EIO;
        }
        if (retval != 0 ) {
            rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: pin '%d' export failed\n", n);
            return -EIO;
        }
    }
    /* export update function */
    retval = hal_export_functf(sample, str, 1, 0, comp_id, "%s%d", strbase, num);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: function export failed\n");
        return retval;
    }

    return 0;
}
