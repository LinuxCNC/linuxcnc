/********************************************************************
* Description:  supply.c
*               This file, 'supply.c', is a HAL component supplying 
*               HAL pins preset to useful values like TRUE and 1.0.
*
* Author: Matt Shaver
* License: GPL Version 2
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
/** This file, 'supply.c', is a HAL component supplying HAL pins preset
    to useful values like TRUE and 1.0.  I expect that it will mostly
    be used for testing.  It is a realtime component.
*/

/** Copyright (C) 2004 Matt Shaver
                       <mshaver AT users DOT sourceforge DOT net>
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

#include <rtapi.h>		/* RTAPI realtime OS API */
#include <rtapi_app.h>		/* RTAPI realtime module decls */
#include <hal.h>		/* HAL public API decls */

/* module information */
MODULE_AUTHOR("Matt Shaver");
MODULE_DESCRIPTION("Supply Component for EMC HAL");
MODULE_LICENSE("GPL");
static int num_chan = 1;	/* number of channels - default = 1 */
RTAPI_MP_INT(num_chan, "number of channels");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** This structure contains the runtime data.
*/

typedef struct {
    hal_bool_t q;		/* pin: q output of simulated flip-flop */
    hal_bool_t _q;		/* pin: /q output of simulated flip-flop */
    hal_real_t variable;	/* pin: output set by param "value" */
    hal_real_t _variable;	/* pin: output set by param "value" * -1.0 */
    hal_bool_t d;		/* pin: d input to simulated flip-flop */
    hal_real_t value;		/* pin: value of float pin "variable" */
} hal_supply_t;

/* pointer to supply_t struct */
static hal_supply_t *supply_array;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_supply(int num, hal_supply_t * addr);
static void update_supply(void *arg, long l);

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
	    "SUPPLY: ERROR: invalid num_chan: %d\n", num_chan);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("supply");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SUPPLY: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for supply data */
    supply_array = hal_malloc(num_chan * sizeof(hal_supply_t));
    if (supply_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SUPPLY: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export variables and functions for each supply */
    for (n = 0; n < num_chan; n++) {
	retval = export_supply(n, &(supply_array[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SUPPLY: ERROR: var export failed\n");
	    hal_exit(comp_id);
	    return -1;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "SUPPLY:installed %d supplies\n",
	num_chan);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/

static void update_supply(void *arg, long l)
{
    (void)l;
    /* point to the data */
    hal_supply_t *supply = arg;

    /* set pin = param */
    rtapi_bool d = hal_get_bool(supply->d);
    hal_set_bool(supply->q, d);
    hal_set_bool(supply->_q, !d);
    rtapi_real value = hal_get_real(supply->value);
    hal_set_real(supply->variable, value);
    hal_set_real(supply->_variable, value * -1.0);
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_supply(int num, hal_supply_t * addr)
{
    int retval;

    /* export pins */
    retval = hal_pin_new_bool(comp_id, HAL_OUT, &(addr->q), 0, "supply.%d.q", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_new_bool(comp_id, HAL_OUT, &(addr->_q), 1, "supply.%d._q", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_new_real(comp_id, HAL_OUT, &(addr->variable), 0.0,"supply.%d.variable", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_new_real(comp_id, HAL_OUT, &(addr->_variable), 0.0, "supply.%d._variable", num);
    if (retval != 0) {
	return retval;
    }
    /* export parameters */
    retval = hal_pin_new_bool(comp_id, HAL_IO, &(addr->d), 0, "supply.%d.d", num);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_new_real(comp_id, HAL_IO, &(addr->value), 0.0, "supply.%d.value", num);
    if (retval != 0) {
	return retval;
    }
    /* export function for this loop */
    retval =
	hal_export_functf(update_supply, &(supply_array[num]), 1, 0,
	comp_id, "supply.%d.update", num);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SUPPLY: ERROR: update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}
