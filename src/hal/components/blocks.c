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
*     mux4 = four input analog mux - out = in<n> based on sel1,sel0
*     integ = integrator, out = integral of in
*     ddt = differentiator, out = derivative of in
*     limit1 = first order limiter (limits output)
*     limit2 = second order limiter (limits output and 1st derivative)
*     limit3 = third order limiter (limits output, 1st & 2nd derivative)
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

#include <math.h>

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
static int mux4 = 0;		/* number of 4-input muxes */
MODULE_PARM(mux4, "i");
MODULE_PARM_DESC(mux4, "4-input multiplexors");
static int integ = 0;		/* number of integerators */
MODULE_PARM(integ, "i");
MODULE_PARM_DESC(integ, "integrators");
static int ddt = 0;		/* number of differentiators */
MODULE_PARM(ddt, "i");
MODULE_PARM_DESC(ddt, "differentiators");
static int limit1 = 0;		/* number of limiters */
MODULE_PARM(limit1, "i");
MODULE_PARM_DESC(limit1, "first order limiters");
static int limit2 = 0;		/* number of limiters */
MODULE_PARM(limit2, "i");
MODULE_PARM_DESC(limit2, "second order limiters");
static int limit3 = 0;		/* number of limiters */
MODULE_PARM(limit3, "i");
MODULE_PARM_DESC(limit3, "third order limiters");
static int n_estop = 0;		/* number of estop blocks */
MODULE_PARM(n_estop, "i");
MODULE_PARM_DESC(n_estop, "estop latch blocks");
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
    hal_float_t *in0;		/* pin: input when sel1,0 = 0,0 */
    hal_float_t *in1;		/* pin: input when sel1,0 = 0,1 */
    hal_float_t *in2;		/* pin: input when sel1,0 = 1,0 */
    hal_float_t *in3;		/* pin: input when sel1,0 = 1,1 */
    hal_float_t *out;		/* pin: output */
    hal_bit_t *sel0;		/* pin: select input */
    hal_bit_t *sel1;		/* pin: select input */
} mux4_t;

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

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_float_t *out;		/* pin: output */
    hal_float_t max;		/* param: maximum value */
    hal_float_t min;		/* param: minimum value */
} limit1_t;

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_float_t *out;		/* pin: output */
    hal_float_t max;		/* param: maximum value */
    hal_float_t min;		/* param: minimum value */
    hal_float_t maxv;		/* param: 1st derivative limit */
    double old_out;		/* previous output */
} limit2_t;

typedef struct {
    hal_float_t *in;		/* pin: input */
    hal_float_t *out;		/* pin: output */
    hal_float_t min;		/* param: minimum value */
    hal_float_t max;		/* param: maximum value */
    hal_float_t maxv;		/* param: 1st derivative limit */
    hal_float_t maxa;		/* param: 2nd derivative limit */
    double old_in;		/* previous input */
    double old_out;		/* previous output */
    double old_v;		/* previous 1st derivative */
} limit3_t;


typedef struct {
    hal_bit_t *in;		/* pin: input from hardware chain */
    hal_bit_t *out;		/* pin: output */
    hal_bit_t *reset;		/* pin: estop reset (rising edge) */
    hal_bit_t *wd;		/* pin: watchdog/charge pump drive */
    hal_bit_t old_reset;	/* internal, for rising edge detect */
} estop_t;




/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_constant(int num);
static int export_wcomp(int num);
static int export_comp(int num);
static int export_mux2(int num);
static int export_mux4(int num);
static int export_sum2(int num);
static int export_integ(int num);
static int export_ddt(int num);
static int export_limit1(int num);
static int export_limit2(int num);
static int export_limit3(int num);
static int export_estop(int num);

static void constant_funct(void *arg, long period);
static void wcomp_funct(void *arg, long period);
static void comp_funct(void *arg, long period);
static void mux2_funct(void *arg, long period);
static void mux4_funct(void *arg, long period);
static void sum2_funct(void *arg, long period);
static void integ_funct(void *arg, long period);
static void ddt_funct(void *arg, long period);
static void limit1_funct(void *arg, long period);
static void limit2_funct(void *arg, long period);
static void limit3_funct(void *arg, long period);
static void estop_funct(void *arg, long period);

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
    /* allocate and export 4 input multiplexors */
    if (mux4 > 0) {
	for (n = 0; n < mux4; n++) {
	    if (export_mux4(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_mux4(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d 4-input muxes\n", mux4);
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
    /* allocate and export first order limiters */
    if (limit1 > 0) {
	for (n = 0; n < limit1; n++) {
	    if (export_limit1(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_limit1(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d first order limiters\n", limit1);
    }
    /* allocate and export second order limiters */
    if (limit2 > 0) {
	for (n = 0; n < limit2; n++) {
	    if (export_limit2(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_limit2(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d second order limiters\n", limit2);
    }
    /* allocate and export third order limiters */
    if (limit3 > 0) {
	for (n = 0; n < limit3; n++) {
	    if (export_limit3(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_limit3(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d third order limiters\n", limit3);
    }
    /* allocate and export estop latch blocks */
    if (n_estop > 0) {
	for (n = 0; n < n_estop; n++) {
	    if (export_estop(n) != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "BLOCKS: ERROR: export_estop(%d) failed\n", n);
		hal_exit(comp_id);
		return -1;
	    }
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "BLOCKS: installed %d estop latch blocks\n", n_estop);
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

static void mux4_funct(void *arg, long period)
{
    mux4_t *mux4;

    /* point to block data */
    mux4 = (mux4_t *) arg;
    /* calculate output */
    if (*(mux4->sel1)) {
	if (*(mux4->sel0)) {
	    *(mux4->out) = *(mux4->in3);
	} else {
	    *(mux4->out) = *(mux4->in2);
	}
    } else {
	if (*(mux4->sel0)) {
	    *(mux4->out) = *(mux4->in1);
	} else {
	    *(mux4->out) = *(mux4->in0);
	}
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

static void limit1_funct(void *arg, long period)
{
    limit1_t *limit1;
    double out;

    /* point to block data */
    limit1 = (limit1_t *) arg;
    /* apply first order limit */
    out = *(limit1->in);
    if ( out < limit1->min ) {
	out = limit1->min;
    }
    if ( out > limit1->max ) {
	out = limit1->max;
    }
    *(limit1->out) = out;
}

static void limit2_funct(void *arg, long period)
{
    limit2_t *limit2;
    double out, delta, tmp;

    /* point to block data */
    limit2 = (limit2_t *) arg;
    /* apply first order limit */
    out = *(limit2->in);
    if ( out < limit2->min ) {
	out = limit2->min;
    }
    if ( out > limit2->max ) {
	out = limit2->max;
    }
    /* apply second order limit */
    delta = limit2->maxv * (period * 0.000000001);
    tmp = limit2->old_out - delta;
    if ( out < tmp ) {
	out = tmp;
    }
    tmp = limit2->old_out + delta;
    if ( out > tmp ) {
	out = tmp;
    }
    limit2->old_out = out;
    *(limit2->out) = out;
}

static void limit3_funct(void *arg, long period)
{
    limit3_t *limit3;
    double in, out, dt, in_v, min_v, max_v, ramp_a, avg_v, err, dv, dp;
    double min_out, max_out, match_time, est_in, est_out;

    est_in = est_out = match_time = 0;
    
    /* point to block data */
    limit3 = (limit3_t *) arg;
    /* apply first order limit */
    in = *(limit3->in);
    if ( in < limit3->min ) {
	in = limit3->min;
    }
    if ( in > limit3->max ) {
	in = limit3->max;
    }
    /* calculate input derivative */
    dt = period * 0.000000001;
    in_v = (in - limit3->old_in) / dt;
    /* determine v and out that can be reached in one period */
    min_v = limit3->old_v - limit3->maxa * dt;
    if ( min_v < -limit3->maxv ) {
        min_v = -limit3->maxv;
    }
    max_v = limit3->old_v + limit3->maxa * dt;
    if ( max_v > limit3->maxv ) {
        max_v = limit3->maxv;
    }
    min_out = limit3->old_out + min_v * dt;
    max_out = limit3->old_out + max_v * dt;
    if ( ( in >= min_out ) && ( in <= max_out ) && ( in_v >= min_v ) && ( in_v <= max_v ) ) {
        /* we can follow the command without hitting a limit */
	out = in;
	limit3->old_v = ( out - limit3->old_out ) / dt;
    } else {
	/* can't follow commanded path while obeying limits */ 
	/* determine which way we need to ramp to match v */
	if ( in_v > limit3->old_v ) {
	    ramp_a = limit3->maxa;
	} else {
	    ramp_a = -limit3->maxa;
	}
	/* determine how long the match would take */
	match_time = ( in_v - limit3->old_v ) / ramp_a;
	/* where we will be at the end of the match */
	avg_v = ( in_v + limit3->old_v + ramp_a * dt ) * 0.5;
	est_out = limit3->old_out + avg_v * match_time;
	/* calculate the expected command position at that time */
	est_in = limit3->old_in + in_v * match_time;
	/* calculate position error at that time */
	err = est_out - est_in;
	/* calculate change in final position if we ramp in the
	   opposite direction for one period */
	dv = -2.0 * ramp_a * dt;
	dp = dv * match_time;
	
	/* decide what to do */
	if ( fabs(err+dp*2.0) < fabs(err) ) {
	    ramp_a = -ramp_a;
	}
		
		
	if ( ramp_a < 0.0 ) {
	    out = min_out;
	    limit3->old_v = min_v;
	} else {
	    out = max_out;
	    limit3->old_v = max_v;
	}
    }    
    limit3->old_out = out;
    limit3->old_in = in;
    *(limit3->out) = out;
}

/* this module sets 'out' true when 'in' is true _and_ it sees
   a rising edge on 'reset'.  If 'in' goes false, it sets 'out'
   false.  It also toggles 'watchdog' constantly, which should
   be used for a charge pump or other such circuit.  Note that
   this block should _not_ be relied on to turn off 'out', 
   instead, 'in' should be daisy-chained through 'out' in
   hardware, and this module is only used to prevent a restart
   if/when 'in' comes back on.
*/

static void estop_funct(void *arg, long period)
{
    estop_t *estop;

    /* point to block data */
    estop = (estop_t *) arg;

    /* check input (which should be from hardware chain) */
    if ( *(estop->in) != 0 ) {
	/* hardware estop chain is OK, check for reset edge */
	if (( *(estop->reset) != 0 ) && ( estop->old_reset == 0 )) {
	    /* got a rising edge, set output true */
	    *(estop->out) = 1;
	}
	/* toggle watchdog */
	if ( *(estop->wd) == 0 ) {
	    *(estop->wd) = 1;
	} else {
	    *(estop->wd) = 0;
	}
    } else {
	/* hardware chain is not OK, turn off output */
	*(estop->out) = 0;
    }
    /* store state of reset input for next pass (for edge detect) */
    estop->old_reset = *(estop->reset);
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

static int export_mux4(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    mux4_t *mux4;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for 4 input multiplexor */
    mux4 = hal_malloc(sizeof(mux4_t));
    if (mux4 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.in0", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux4->in0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.in1", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux4->in1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.in2", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux4->in2), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.in3", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(mux4->in3), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(mux4->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pins for select input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.sel0", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(mux4->sel0), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d.sel1", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(mux4->sel1), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "mux4.%d", num);
    retval = hal_export_funct(buf, mux4_funct, mux4, 1, 0, comp_id);
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

static int export_limit1(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    limit1_t *limit1;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for first order limiter */
    limit1 = hal_malloc(sizeof(limit1_t));
    if (limit1 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit1.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(limit1->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit1.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(limit1->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit1.%d.min", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit1->min), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit1.%d.max", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit1->max), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit1.%d", num);
    retval = hal_export_funct(buf, limit1_funct, limit1, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    limit1->min = -1e20;
    limit1->max =  1e20;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_limit2(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    limit2_t *limit2;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for second order limiter */
    limit2 = hal_malloc(sizeof(limit2_t));
    if (limit2 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(limit2->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(limit2->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d.min", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit2->min), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d.max", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit2->max), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d.maxv", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit2->maxv), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit2.%d", num);
    retval = hal_export_funct(buf, limit2_funct, limit2, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    limit2->min = -1e20;
    limit2->max =  1e20;
    limit2->maxv = 1e20;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_limit3(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    limit3_t *limit3;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for third order limiter */
    limit3 = hal_malloc(sizeof(limit3_t));
    if (limit3 == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pin for input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.in", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(limit3->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pin for output */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.out", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(limit3->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export params */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.min", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit3->min), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.max", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit3->max), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.maxv", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit3->maxv), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d.maxa", num);
    retval = hal_param_float_new(buf, HAL_WR, &(limit3->maxa), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' param export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "limit3.%d", num);
    retval = hal_export_funct(buf, limit3_funct, limit3, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* set default parameter values */
    limit3->min = -1e20;
    limit3->max =  1e20;
    limit3->maxv = 1e20;
    limit3->maxa = 1e20;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int export_estop(int num)
{
    int retval, msg;
    char buf[HAL_NAME_LEN + 2];
    estop_t *estop;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* allocate shared memory for an estop latch */
    estop = hal_malloc(sizeof(estop_t));
    if (estop == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export pins for inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "estop.%d.in", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(estop->in), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "estop.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(estop->reset), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export pins for outputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "estop.%d.out", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(estop->out), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "estop.%d.watchdog", num);
    retval = hal_pin_bit_new(buf, HAL_WR, &(estop->wd), comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' pin export failed\n", buf);
	return retval;
    }
    /* export function */
    rtapi_snprintf(buf, HAL_NAME_LEN, "estop.%d", num);
    retval = hal_export_funct(buf, estop_funct, estop, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "BLOCKS: ERROR: '%s' funct export failed\n", buf);
	return -1;
    }
    /* initialize internal vars */
    estop->old_reset = 104;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

