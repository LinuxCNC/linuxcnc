/********************************************************************
* Description: blocks.c
*   Assorted simple functional blocks for HAL.
*
*   See the "Users Manual" at emc2/docs/Hal_Introduction.pdf
*
*   This HAL component contains an assortment of simple blocks.
*   Each individual type of block is invoked by a parameter on
*   the insmdo command line.  Each parameter is of the form:
*   <blockname>=<number_of_blocks>
*
*   For example, wcomp=2 installs two window comparators.
*
*   List of blocks currently implemented:
*     constant = pin controlled by parameter
*     wcomp = window comparator - out is true if min < in < max
*     comp = 2 input comparator - out is true if in1 > in0
*     sum2 = 2 input summer - out = in1 * gain1 + in2 * gain2
*     mux2 = two input analog mux - out = in1 if sel is true, else in0
*     integ = integrator, out = integral of in
*     ddt = differentiator, out = derivative of in
*
*********************************************************************
*
* Author: John Kasunich (jmkasunich AT att DOT net)
* License: GPL Version 2
* Created on: 2004/06/14
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
********************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/ctype.h>	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Functional blocks for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */


/*
TODO: undo this hack once hal_malloc is fully converted:

*/

#define hal_malloc new_hal_malloc

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** These structures contain the runtime data for a single block. */

typedef struct {
    hal_float_t *out;		/* pin: output */
    hal_float_t value;		/* parameter: value of constant */
} constant_t;

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_bit_t *out;		/* pin: output */
    hal_float_t min;		/* parameter: low threashold */
    hal_float_t max;		/* parameter: high threashold */
} wcomp_t;

typedef struct {
    hal_float_t *in0;		/* pin: input0 (inverting) */
    hal_float_t *in1;		/* pin: input1 (non-inverting) */
    hal_bit_t *out;		/* pin: output */
    hal_float_t hyst;		/* parameter: hysteresis */
} comp_t;

typedef struct {
    hal_float_t *in0;		/* pin: input used when sel = 0 */
    hal_float_t *in1;		/* pin: input used when sel != 0 */
    hal_float_t *out;		/* pin: output */
    hal_bit_t *sel;		/* pin: select input */
} mux2_t;

typedef struct {
    hal_float_t *in0;		/* pin: input 0 */
    hal_float_t *in1;		/* pin: input 1 */
    hal_float_t *out;		/* pin: output */
    hal_float_t gain0;		/* param: gain for input 0 */
    hal_float_t gain1;		/* param: gain for input 1 */
} sum2_t;

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_float_t *out;		/* pin: output */
} integ_t;

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_float_t *out;		/* pin: output */
    float old;			/* internal state */
} ddt_t;

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int create_constant(int new_block_id, int block_type);
static int create_wcomp(int new_block_id, int block_type);
static int create_comp(int new_block_id, int block_type);
static int create_mux2(int new_block_id, int block_type);
static int create_sum2(int new_block_id, int block_type);
static int create_integ(int new_block_id, int block_type);
static int create_ddt(int new_block_id, int block_type);

static void constant_funct(void *arg, long period);
static void wcomp_funct(void *arg, long period);
static void comp_funct(void *arg, long period);
static void mux2_funct(void *arg, long period);
static void sum2_funct(void *arg, long period);
static void integ_funct(void *arg, long period);
static void ddt_funct(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

hal_module_info blocks_module_info={
	module_name: "blocks",
	author: "John Kasunich (jmkasunich AT att DOT net)",
	short_description: "An assortment of simple blocks.",
	info_link: "Contact John Kasunich (jmkasunich AT att DOT net)"
};

enum {
	BLOCKS_TYPE_CONSTANT=0,
	BLOCKS_TYPE_WCOMP,
	BLOCKS_TYPE_COMP,
	BLOCKS_TYPE_SUM2,
	BLOCKS_TYPE_MUX2,
	BLOCKS_TYPE_INTEG,
	BLOCKS_TYPE_DDT};


hal_block_type_info blocks_block_types[]=
{
	{
	block_type_id		:BLOCKS_TYPE_CONSTANT,
	type_name		:"constant",
        short_description	:"pin controlled by parameter",
	create			:create_constant
	},
	{
	block_type_id		:BLOCKS_TYPE_WCOMP,
	type_name		:"wcomp",
        short_description	:"window comparator - out is true if min < in < max",
	create			:create_wcomp
	},
	{
	block_type_id		:BLOCKS_TYPE_COMP,
	type_name		:"comp",
        short_description	:"2 input comparator - out is true if in1 > in0",
	create			:create_comp
	},
	{
	block_type_id		:BLOCKS_TYPE_SUM2,
	type_name		:"sum2",
        short_description	:"2 input summer - out = in1 * gain1 + in2 * gain2",
	create			:create_sum2
	},
	{
	block_type_id		:BLOCKS_TYPE_MUX2,
	type_name		:"mux2",
        short_description	:"two input analog mux - out = in1 if sel is true, else in0",
	create			:create_mux2
	},
	{
	block_type_id		:BLOCKS_TYPE_INTEG,
	type_name		:"integ",
        short_description	:"integrator, out = integral of in",
	create			:create_integ
	},
	{
	block_type_id		:BLOCKS_TYPE_DDT,
	type_name		:"ddt",
        short_description	:"differentiator, out = derivative of in",
	create			:create_ddt
	}
};



int rtapi_app_main(void)
{
    int n;
    int result;

    /* connect to the HAL */


//    comp_id = hal_init("blocks");
    comp_id = hal_register_module(&blocks_module_info);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "BLOCKS: ERROR: hal_register_module() failed\n");
	return -1;
    }

    /* Register our ability to create blocks... */

    for (n=0; 
	n < (sizeof( blocks_block_types ) / sizeof(hal_block_type_info)); 
	n++)

	{
	result=hal_register_block_type(comp_id, &blocks_block_types[n]);
	if (HAL_SUCCESS != result)
		{
		rtapi_print_msg(RTAPI_MSG_ERR, "BLOCKS: ERROR: failed to register ability to create blocks of type '%s'\n", blocks_block_types[n].type_name);
		hal_exit(comp_id);
		}
	}

    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*                     REALTIME BLOCK FUNCTIONS                         *
************************************************************************/

static void constant_funct(void *arg, long period)
{
    constant_t *constant;

    /* point to block data */
    constant = (constant_t *) arg;
    /* calculate output */
    *(constant->out) = constant->value;
}

static void wcomp_funct(void *arg, long period)
{
    wcomp_t *wcomp;
    float tmp;

    /* point to block data */
    wcomp = (wcomp_t *) arg;
    /* calculate output */
    tmp = *(wcomp->in);
    if ((wcomp->min < tmp) && (tmp < wcomp->max)) {
	*(wcomp->out) = 1;
    } else {
	*(wcomp->out) = 0;
    }
}

static void comp_funct(void *arg, long period)
{
    comp_t *comp;
    float tmp;

    /* point to block data */
    comp = (comp_t *) arg;
    /* calculate output */
    tmp = *(comp->in1) - *(comp->in0);
    if (*(comp->out)) {
	if (tmp < -comp->hyst) {
	    *(comp->out) = 0;
	}
    } else {
	if (tmp > comp->hyst) {
	    *(comp->out) = 1;
	}
    }
}

static void mux2_funct(void *arg, long period)
{
    mux2_t *mux2;

    /* point to block data */
    mux2 = (mux2_t *) arg;
    /* calculate output */
    if (*(mux2->sel)) {
	*(mux2->out) = *(mux2->in1);
    } else {
	*(mux2->out) = *(mux2->in0);
    }
}

static void sum2_funct(void *arg, long period)
{
    sum2_t *sum2;

    /* point to block data */
    sum2 = (sum2_t *) arg;
    /* calculate output */
    *(sum2->out) = *(sum2->in0) * sum2->gain0 + *(sum2->in1) * sum2->gain1;
}

static void integ_funct(void *arg, long period)
{
    integ_t *integ;

    /* point to block data */
    integ = (integ_t *) arg;
    /* calculate output */
    *(integ->out) += *(integ->in) * (period * 0.000000001);
}

static void ddt_funct(void *arg, long period)
{
    ddt_t *ddt;
    float tmp;

    /* point to block data */
    ddt = (ddt_t *) arg;
    /* calculate output */
    tmp = *(ddt->in);
    *(ddt->out) = (tmp - ddt->old) / (period * 0.000000001);
    ddt->old = tmp;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int create_constant(int new_block_id, int block_type)
{
    int retval, msg;
    constant_t *constant;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for window comparator */
    constant = hal_malloc(new_block_id, sizeof(constant_t));
    if (constant == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for output */
    retval = hal_pin_float_new("out", HAL_WR, &(constant->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export param for value */
    retval = hal_param_float_new("value", HAL_WR, &(constant->value), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'value' param export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("constant", constant_funct, constant, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'constant' funct export failed\n");
	return -1;
    }
    /* set default parameter values */
    constant->value = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_wcomp(int new_block_id, int block_type)
{
    int retval, msg;
    wcomp_t *wcomp;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for window comparator */
    wcomp = hal_malloc(new_block_id, sizeof(wcomp_t));
    if (wcomp == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for input */
    retval = hal_pin_float_new("in", HAL_RD, &(wcomp->in), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_bit_new("out", HAL_WR, &(wcomp->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export params for min and max */
    retval = hal_param_float_new("min", HAL_WR, &(wcomp->min), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'min' param export failed\n");
	return retval;
    }
    retval = hal_param_float_new("max", HAL_WR, &(wcomp->max), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'max' param export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("wcomp", wcomp_funct, wcomp, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'wcomp' funct export failed\n");
	return -1;
    }
    /* set default parameter values */
    wcomp->min = -1.0;
    wcomp->max = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_comp(int new_block_id, int block_type)
{
    int retval, msg;
    comp_t *comp;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2-input comparator */
    comp = hal_malloc(new_block_id, sizeof(comp_t));
    if (comp == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    retval = hal_pin_float_new("in0", HAL_RD, &(comp->in0), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in0' pin export failed\n");
	return retval;
    }
    retval = hal_pin_float_new("in1", HAL_RD, &(comp->in1), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in1' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_bit_new("out", HAL_WR, &(comp->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export params for hystersis */
    retval = hal_param_float_new("hyst", HAL_WR, &(comp->hyst), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'hyst' param export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("comp", comp_funct, comp, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'comp' funct export failed\n");
	return -1;
    }
    /* set default parameter values */
    comp->hyst = 0.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_mux2(int new_block_id, int block_type)
{
    int retval, msg;
    mux2_t *mux2;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2 input multiplexor */
    mux2 = hal_malloc(new_block_id, sizeof(mux2_t));
    if (mux2 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    retval = hal_pin_float_new("in0", HAL_RD, &(mux2->in0), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in0' pin export failed\n");
	return retval;
    }
    retval = hal_pin_float_new("in1", HAL_RD, &(mux2->in1), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in1' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_float_new("out", HAL_WR, &(mux2->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export pin for select input */
    retval = hal_pin_bit_new("sel", HAL_RD, &(mux2->sel), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'sel' pin export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("mux2", mux2_funct, mux2, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'mux2' funct export failed\n");
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_sum2(int new_block_id, int block_type)
{
    int retval, msg;
    sum2_t *sum2;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2-input summer */
    sum2 = hal_malloc(new_block_id, sizeof(sum2_t));
    if (sum2 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    retval = hal_pin_float_new("in0", HAL_RD, &(sum2->in0), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in0' pin export failed\n");
	return retval;
    }
    retval = hal_pin_float_new("in1", HAL_RD, &(sum2->in1), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in1' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_float_new("out", HAL_RD, &(sum2->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export params for gains */
    retval = hal_param_float_new("gain0", HAL_WR, &(sum2->gain0), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'gain0' param export failed\n");
	return retval;
    }
    retval = hal_param_float_new("gain1", HAL_WR, &(sum2->gain1), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'gain1' param export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("sum2", sum2_funct, sum2, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'sum2' funct export failed\n");
	return -1;
    }
    /* set default parameter values */
    sum2->gain0 = 1.0;
    sum2->gain1 = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_integ(int new_block_id, int block_type)
{
    int retval, msg;
    integ_t *integ;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for integrator */
    integ = hal_malloc(new_block_id, sizeof(integ_t));
    if (integ == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for input */
    retval = hal_pin_float_new("in", HAL_RD, &(integ->in), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_float_new("out", HAL_WR, &(integ->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("integ", integ_funct, integ, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'integ' funct export failed\n");
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int create_ddt(int new_block_id, int block_type)
{
    int retval, msg;
    ddt_t *ddt;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for differentiator */
    ddt = hal_malloc(new_block_id, sizeof(ddt_t));
    if (ddt == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for input */
    retval = hal_pin_float_new("in", HAL_RD, &(ddt->in), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'in' pin export failed\n");
	return retval;
    }
    /* export pin for output */
    retval = hal_pin_float_new("out", HAL_WR, &(ddt->out), new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'out' pin export failed\n");
	return retval;
    }
    /* export function */
    retval = hal_export_funct("ddt", ddt_funct, ddt, 1, 0, new_block_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: 'ddt' funct export failed\n");
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
