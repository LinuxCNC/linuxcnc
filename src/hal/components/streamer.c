/********************************************************************
* Description:  streamer.c
*               A HAL component that can be used to stream data
*               from a file onto HAL pins at a specific realtime
*               sample rate.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
/** This file, 'streamer.c', is the realtime part of a HAL component
    that allows numbers stored in a file to be "streamed" onto HAL
    pins at a uniform realtime sample rate.  When the realtime module
    is loaded, it creates a fifo in shared memory.  Then, the user
    space program 'hal_streamer' is invoked.  'hal_streamer' takes
    input from stdin and writes it to the fifo, and this component
    transfers the data from the fifo to HAL pins.

    Loading:

    loadrt streamer depth=100 cfg=uffb


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


/* Notes:
 * streamer.N.cur-depth, streamer.N.empty and streamer.N.underruns are
 * updated even if streamer.N.enabled is set to false.
 *
 * clock and clock_mode pins are provided to enable clocking.
 * The clock input pin actions are controlled by the clock_mode pin value:
 *   0: freerun at every loop (default)
 *   1: clock by falling edge
 *   2: clock by rising edge
 *   3: clock by any edge
 */
#include <rtapi.h>              /* RTAPI realtime OS API */
#include <rtapi_app.h>          /* RTAPI realtime module decls */
#include <hal.h>                /* HAL public API decls */
#include "streamer.h"		/* decls and such for fifos */
#include <rtapi_errno.h>
#include <rtapi_string.h>

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Realtime File Streamer HAL");
MODULE_LICENSE("GPL");
static char *cfg[MAX_STREAMERS];	/* config string, no default */
RTAPI_MP_ARRAY_STRING(cfg,MAX_STREAMERS,"config string");
static int depth[MAX_STREAMERS];	/* depth of fifo, default 0 */
RTAPI_MP_ARRAY_INT(depth,MAX_STREAMERS,"fifo depth");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the HAL shared memory data for one streamer */

typedef struct {
    hal_stream_t fifo;		/* pointer to user/RT fifo */
    hal_sint_t curr_depth;	/* pin: current fifo depth */
    hal_bool_t empty;		/* pin: underrun flag */
    hal_bool_t enable;		/* pin: enable streaming */
    hal_sint_t underruns;	/* pin: number of underruns */
    hal_bool_t clock;		/* pin: clock input */
    hal_sint_t clock_mode;	/* pin: clock mode */
    hal_refs_u pins[HAL_STREAM_MAX_PINS];
    int myclockedge;	        /* clock edge detector */
} streamer_t;

/* other globals */
static int comp_id;		/* component ID */
static int nstreamers;
static streamer_t *streams;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int init_streamer(int num, streamer_t *stream);
static void update(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;

    comp_id = hal_init("streamer");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: hal_init() failed\n");
	return -EINVAL;
    }

    streams = hal_malloc(MAX_STREAMERS * sizeof(streamer_t));

    /* validate config info */
    for ( n = 0 ; n < MAX_STREAMERS ; n++ ) {
	if (( cfg[n] == NULL ) || ( *(cfg[n]) == '\0' ) || ( depth[n] <= 0 )) {
	    break;
	}
	retval = hal_stream_create(&streams[n].fifo, comp_id, STREAMER_SHMEM_KEY+n, depth[n], cfg[n]);
	if(retval < 0) {
	    goto fail;
	}
	nstreamers++;
	retval = init_streamer(n, &streams[n]);
    }
    if ( n == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STREAMER: ERROR: no channels specified\n");
	retval = -EINVAL;
	goto fail;
    }

    hal_ready(comp_id);
    return 0;
fail:
    for(n=0; n<nstreamers; n++) hal_stream_destroy(&streams[n].fifo);
    hal_exit(comp_id);
    return retval;
}

void rtapi_app_exit(void)
{
    int i;
    for(i=0; i<nstreamers; i++) hal_stream_destroy(&streams[i].fifo);
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void update(void *arg, long period)
{
    (void)period;
    streamer_t *str = (streamer_t *)arg; /* point at streamer struct in HAL shmem */
    int doclk = 0;

    /* keep last two clock states to get all possible clock edges */
    int myclockedge = str->myclockedge=((str->myclockedge<<1) | (hal_get_bool(str->clock) & 1)) & 3;
    /* are we enabled? - generate doclock if enabled and right mode  */
    if (hal_get_bool(str->enable)) {
        doclk = 1;
        switch (hal_get_si32(str->clock_mode)) {
        /* clock-mode 0 means do clock if enabled */
        case 0:
            break;
        /* clock-mode 1 means enabled & falling edge */
        case 1:
            if (2 != myclockedge) {
                doclk = 0;
            }
            break;
        /* clock-mode 2 means enabled & rising edge */
        case 2:
            if (1 != myclockedge) {
                doclk = 0;
            }
            break;
        /* clock-mode 3 means enabled & both edges */
        case 3:
            if (0 == myclockedge || 3 == myclockedge) {
                doclk = 0;
            }
            break;
        default:
            break;
        }
    }
    /* point at user/RT fifo in other shmem */
    int depth = hal_stream_depth(&str->fifo);
    hal_set_si32(str->curr_depth, depth);
    hal_set_bool(str->empty, depth == 0);
    if(!doclk)
        return; /* done - output pins retain current values */
    if(depth == 0) {
        /* increase underrun only for valid clock*/
        hal_set_si32(str->underruns, hal_get_si32(str->underruns) + 1);
        return;
    }
    union hal_stream_data data[HAL_STREAM_MAX_PINS];
    if(hal_stream_read(&str->fifo, data, NULL) < 0) {
        /* should not happen (single reader invariant) */
        hal_set_si32(str->underruns, hal_get_si32(str->underruns) + 1);
        return;
    }
    int num_pins = hal_stream_element_count(&str->fifo);
    /* copy data from fifo to HAL pins */
    for (int n = 0; n < num_pins; n++) {
        switch ( hal_stream_element_type(&str->fifo, n) ) {
        case HAL_REAL: hal_set_real(str->pins[n].r, data[n].f); break;
        case HAL_S32:  hal_set_si32(str->pins[n].s, data[n].s); break;
        case HAL_U32:  hal_set_ui32(str->pins[n].u, data[n].u); break;
        case HAL_SINT: hal_set_sint(str->pins[n].s, data[n].l); break;
        case HAL_UINT: hal_set_uint(str->pins[n].u, data[n].k); break;
        case HAL_BOOL: hal_set_bool(str->pins[n].b, data[n].b); break;
        default:
            break;
        }
    }
}

static int init_streamer(int num, streamer_t *str)
{
    int retval;
    static const char strbase[] = "streamer.";
    /* export "standard" pins and params */
    retval = hal_pin_new_bool(comp_id, HAL_OUT, &str->empty, 1, "%s%d.empty", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'empty' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_bool(comp_id, HAL_IN, &str->enable, 1, "%s%d.enable", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'enable' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_OUT, &str->curr_depth, 0, "%s%d.curr-depth", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'curr_depth' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_IO, &str->underruns, 0, "%s%d.underruns", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'underruns' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_bool(comp_id, HAL_IN, &str->clock, 0, "%s%d.clock", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'clock' pin export failed\n");
        return -EIO;
    }
    retval = hal_pin_new_si32(comp_id, HAL_IN, &str->clock_mode, 0, "%s%d.clock-mode", strbase, num);
    if (retval != 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: 'clock_mode' pin export failed\n");
        return -EIO;
    }

    /* export user specified pins (the ones that stream data) */
    for (int n = 0; n < hal_stream_element_count(&str->fifo); n++) {
        static const char pstr[] = "streamer.%d.pin.%d";
        int type;
        switch(type = hal_stream_element_type(&str->fifo, n)) {
        case HAL_REAL: retval = hal_pin_new_real(comp_id, HAL_OUT, &str->pins[n].r, 0.0, pstr, num, n); break;
        case HAL_S32:  retval = hal_pin_new_si32(comp_id, HAL_OUT, &str->pins[n].s, 0,   pstr, num, n); break;
        case HAL_U32:  retval = hal_pin_new_ui32(comp_id, HAL_OUT, &str->pins[n].u, 0,   pstr, num, n); break;
        case HAL_SINT: retval = hal_pin_new_sint(comp_id, HAL_OUT, &str->pins[n].s, 0,   pstr, num, n); break;
        case HAL_UINT: retval = hal_pin_new_uint(comp_id, HAL_OUT, &str->pins[n].u, 0,   pstr, num, n); break;
        case HAL_BOOL: retval = hal_pin_new_bool(comp_id, HAL_OUT, &str->pins[n].b, 0,   pstr, num, n); break;
        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: stream type for pin %d has bad type %d\n", n, type);
            return -EIO;
        }
        if (retval != 0 ) {
            rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: pin '%d' export failed\n", n);
            return -EIO;
        }
    }
    /* export update function */
    retval = hal_export_functf(update, str, 1, 0, comp_id, "%s%d", strbase, num);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "STREAMER: ERROR: function export failed\n");
        return retval;
    }

    return 0;
}
