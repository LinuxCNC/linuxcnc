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
static int constant = 0;	/* number of constants */
MODULE_PARM(constant, "i");
MODULE_PARM_DESC(constant, "constants");
static int wcomp = 0;		/* number of window comps */
MODULE_PARM(wcomp, "i");
MODULE_PARM_DESC(wcomp, "window comparators");
static int comp = 0;		/* number of 2-input comps */
MODULE_PARM(comp, "i");
MODULE_PARM_DESC(comp, "2-input comparators");
static int sum2 = 0;		/* number of 2-input summers */
MODULE_PARM(sum2, "i");
MODULE_PARM_DESC(sum2, "2-input summers");
static int mux2 = 0;		/* number of 2-input muxes */
MODULE_PARM(mux2, "i");
MODULE_PARM_DESC(mux2, "2-input multiplexors");
static int integ = 0;		/* number of integerators */
MODULE_PARM(integ, "i");
MODULE_PARM_DESC(integ, "integrators");
static int ddt = 0;		/* number of differentiators */
MODULE_PARM(ddt, "i");
MODULE_PARM_DESC(ddt, "differentiators");
#endif /* MODULE */

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

static int export_constant(int num);
static int export_wcomp(int num);
static int export_comp(int num);
static int export_mux2(int num);
static int export_sum2(int num);
static int export_integ(int num);
static int export_ddt(int num);

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

int rtapi_app_main(void)
{
    int n;

    /* connect to the HAL */
    comp_id = hal_init("blocks");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "BLOCKS: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate and export constants */
    if (constant > 0) {
	for (n = 0; n < constant; n++) {
	    if (export_constant(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_constant(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d constants\n", constant);
    }
    /* allocate and export window comparators */
    if (wcomp > 0) {
	for (n = 0; n < wcomp; n++) {
	    if (export_wcomp(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_wcomp(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d window comparators\n", wcomp);
    }
    /* allocate and export 2-input comparators */
    if (comp > 0) {
	for (n = 0; n < comp; n++) {
	    if (export_comp(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_comp(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d 2-input comparators\n", comp);
    }
    /* allocate and export 2 input multiplexors */
    if (mux2 > 0) {
	for (n = 0; n < mux2; n++) {
	    if (export_mux2(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_mux2(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d 2-input muxes\n", mux2);
    }
    /* allocate and export 2 input summers */
    if (sum2 > 0) {
	for (n = 0; n < sum2; n++) {
	    if (export_sum2(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_sum2(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d 2-input summers\n", sum2);
    }
    /* allocate and export integrators */
    if (integ > 0) {
	for (n = 0; n < integ; n++) {
	    if (export_integ(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_integ(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d 2 integrators\n", integ);
    }
    /* allocate and export differentiators */
    if (ddt > 0) {
	for (n = 0; n < ddt; n++) {
	    if (export_ddt(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_ddt(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d differentiators\n", ddt);
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

static int export_constant(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    constant_t *constant;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for window comparator */
    constant = hal_malloc(sizeof(constant_t));
    if (constant == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "constant.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(constant->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export param for value */
    rtapi_snprintf(buf, HAL_NAME_LEN, "constant.%d.value", num);
    retval = hal_param_float_new(buf, HAL_WR, &(constant->value), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "constant.%d", num);
    retval = hal_export_funct(buf, constant_funct, constant, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    constant->value = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_wcomp(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    wcomp_t *wcomp;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for window comparator */
    wcomp = hal_malloc(sizeof(wcomp_t));
    if (wcomp == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "wcomp.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(wcomp->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "wcomp.%d.out", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(wcomp->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params for min and max */
    rtapi_snprintf(buf, HAL_NAME_LEN, "wcomp.%d.min", num);
    retval = hal_param_float_new(buf, HAL_WR, &(wcomp->min), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "wcomp.%d.max", num);
    retval = hal_param_float_new(buf, HAL_WR, &(wcomp->max), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "wcomp.%d", num);
    retval = hal_export_funct(buf, wcomp_funct, wcomp, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    wcomp->min = -1.0;
    wcomp->max = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_comp(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    comp_t *comp;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2-input comparator */
    comp = hal_malloc(sizeof(comp_t));
    if (comp == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "comp.%d.in0", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(comp->in0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "comp.%d.in1", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(comp->in1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "comp.%d.out", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(comp->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params for hystersis */
    rtapi_snprintf(buf, HAL_NAME_LEN, "comp.%d.hyst", num);
    retval = hal_param_float_new(buf, HAL_WR, &(comp->hyst), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "comp.%d", num);
    retval = hal_export_funct(buf, comp_funct, comp, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    comp->hyst = 0.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_mux2(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    mux2_t *mux2;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2 input multiplexor */
    mux2 = hal_malloc(sizeof(mux2_t));
    if (mux2 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux2.%d.in0", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux2->in0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux2.%d.in1", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux2->in1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux2.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(mux2->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for select input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux2.%d.sel", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(mux2->sel), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux2.%d", num);
    retval = hal_export_funct(buf, mux2_funct, mux2, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_sum2(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    sum2_t *sum2;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 2-input summer */
    sum2 = hal_malloc(sizeof(sum2_t));
    if (sum2 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d.in0", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(sum2->in0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d.in1", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(sum2->in1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(sum2->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params for gains */
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d.gain0", num);
    retval = hal_param_float_new(buf, HAL_WR, &(sum2->gain0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d.gain1", num);
    retval = hal_param_float_new(buf, HAL_WR, &(sum2->gain1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "sum2.%d", num);
    retval = hal_export_funct(buf, sum2_funct, sum2, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    sum2->gain0 = 1.0;
    sum2->gain1 = 1.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_integ(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    integ_t *integ;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for integrator */
    integ = hal_malloc(sizeof(integ_t));
    if (integ == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "integ.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(integ->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "integ.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(integ->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "integ.%d", num);
    retval = hal_export_funct(buf, integ_funct, integ, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_ddt(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    ddt_t *ddt;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for differentiator */
    ddt = hal_malloc(sizeof(ddt_t));
    if (ddt == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "ddt.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(ddt->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "ddt.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(ddt->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "ddt.%d", num);
    retval = hal_export_funct(buf, ddt_funct, ddt, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
