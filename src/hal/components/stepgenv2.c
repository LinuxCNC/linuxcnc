/********************************************************************
* Description:  stepgen.c
*               This file, 'stepgen.c', is a HAL component that
*               provides software based step pulse generation.
*
* Author: John Kasunich
* License: GPL Version 2
*
* Copyright (c) 2003-2007 All rights reserved.
*
* Last change: Michael Haberler 9/2015
*   make SMP-safe
*   instantiable
********************************************************************/
/** This file, 'stepgen.c', is a HAL component that provides software
    based step pulse generation.  The maximum step rate will depend
    on the speed of the PC, but is expected to exceed 5KHz for even
    the slowest computers, and may reach 25KHz on fast ones.  It is
    a realtime component.

    It supports up to 16 pulse generators.  Each generator can produce
    several types of outputs in addition to step/dir, including
    quadrature, half- and full-step unipolar and bipolar, three phase,
    and five phase.  A 32 bit feedback value is provided indicating
    the current position of the motor in counts (assuming no lost
    steps), and a floating point feedback in user specified position
    units is also provided.

    The number of step generators and type of outputs is determined
    by the insmod command line parameter 'step_type'.  It accepts
    a comma separated (no spaces) list of up to 16 stepping types
    to configure up to 16 channels.  A second command line parameter
    "ctrl_type", selects between position and velocity control modes
    for each step generator.  (ctrl_type is optional, the default
    control type is position.)

    So a command line like this:

	insmod stepgen step_type=0,0,1,2  ctrl_type=p,p,v,p

    will install four step generators, two using stepping type 0,
    one using type 1, and one using type 2.  The first two and
    the last one will be running in position mode, and the third
    one will be running in velocity mode.

    The driver exports three functions.  'stepgen.make-pulses', is
    responsible for actually generating the step pulses.  It must
    be executed in a fast thread to reduce pulse jitter.  The other
    two functions are normally called from a much slower thread.
    'stepgen.update-freq' reads the position or frequency command
    and sets internal variables used by 'stepgen.make-pulses'.
    'stepgen.capture-position' captures and scales the current
    values of the position feedback counters.  Both 'update-freq' and
    'capture-position' use floating point, 'make-pulses' does not.

    Polarity:

    All signals from this module have fixed polarity (active high
    in most cases).  If the driver needs the opposite polarity,
    the signals can be inverted using parameters exported by the
    hardware driver(s) such as ParPort.

    Timing parameters:

    There are five timing parameters which control the output waveform.
    No step type uses all five, and only those which will be used are
    exported to HAL.  The values of these parameters are in nano-seconds,
    so no recalculation is needed when changing thread periods.  In
    the timing diagrams that follow, they are identfied by the
    following numbers:

    (1): 'stepgen.n.steplen' = length of the step pulse
    (2): 'stepgen.n.stepspace' = minimum space between step pulses
	  (actual space depends on frequency command, and is infinite
	  if the frequency command is zero)
    (3): 'stepgen.n.dirhold' = minimum delay after a step pulse before
	  a direction change - may be longer
    (4): 'stepgen.n.dirsetup' = minimum delay after a direction change
	  and before the next step - may be longer
    (5): 'stepgen.n.dirdelay' = minimum delay after a step before a
	 step in the opposite direction - may be longer

    Stepping Types:

    This module supports a number of stepping types, as follows:

    Type 0:  Step and Direction
               _____         _____               _____
    STEP  ____/     \_______/     \_____________/     \______
              |     |       |     |             |     |
    Time      |-(1)-|--(2)--|-(1)-|--(3)--|-(4)-|-(1)-|
                                          |__________________
    DIR   ________________________________/

    There are two output pins, STEP and DIR.  Step pulses appear on
    STEP.  A positive frequency command results in DIR low, negative
    frequency command means DIR high.  The minimum period of the
    step pulses is 'steplen' + 'stepspace', and the frequency
    command is clamped to avoid exceeding these limits.  'steplen'
    and 'stepspace' must both be non-zero.  'dirsetup' or 'dirhold'
    may be zero, but their sum must be non-zero, to ensure non-zero
    low time between the last up step and the first down step.

    Type 1:  Up/Down
             _____       _____
    UP    __/     \_____/     \________________________________
            |     |     |     |         |
    Time    |-(1)-|-(2)-|-(1)-|---(5)---|-(1)-|-(2)-|-(1)-|
                                        |_____|     |_____|
    DOWN  ______________________________/     \_____/     \____


    There are two output pins, UP and DOWN.  A positive frequency
    command results in pulses on UP, negative frequency command
    results in pulses on DOWN.  The minimum period of the step
    pulses is 'steplen' + 'stepspace', and the frequency command
    is clamped to avoid exceeding these limits.  'steplen',
    'stepspace', and 'dirdelay' must all be non-zero.

    Types 2 and higher:  State Patterns

    STATE   |---1---|---2---|---3---|----4----|---3---|---2---|
            |       |       |       |         |       |       |
    Time    |--(1)--|--(1)--|--(1)--|--(1+5)--|--(1)--|--(1)--|

    All the remaining stepping types are simply different repeating
    patterns on two to five output pins.  When a step occurs, the
    output pins change to the next (or previous) pattern in the
    state listings that follow.  The output pins are called 'PhaseA'
    thru 'PhaseE'.  Timing constraints are obeyed as indicated
    in the drawing above.  'steplen' must be non-zero.  'dirdelay'
    may be zero.  Because stepspace is not used, state based
    stepping types can run faster than types 0 and 1.

    Type 2:  Quadrature (aka Gray/Grey code)

    State   Phase A   Phase B
      0        1        0
      1        1        1
      2        0        1
      3        0        0
      0        1        0

    Type 3:  Three Wire

    State   Phase A   Phase B   Phase C
      0        1        0         0
      1        0        1         0
      2        0        0         1
      0        1        0         0

    Type 4:  Three Wire HalfStep

    State   Phase A   Phase B   Phase C
      0        1        0         0
      1        1        1         0
      2        0        1         0
      3        0        1         1
      4        0        0         1
      5        1        0         1
      0        1        0         0

    Type 5:  Unipolar Full Step (one winding on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        0        1         0         0
      2        0        0         1         0
      3        0        0         0         1
      0        1        0         0         0

    Type 6:  Unipolar Full Step (two windings on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        1         0         0
      1        0        1         1         0
      2        0        0         1         1
      3        1        0         0         1
      0        1        1         0         0

    Type 7:  Bipolar Full Step (one winding on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        1         1         0
      2        0        1         1         1
      3        0        0         0         1
      0        1        0         0         0

    Type 8:  Bipolar Full Step (two windings on)

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         1         0
      1        0        1         1         0
      2        0        1         0         1
      3        1        0         0         1
      0        1        0         1         0

    Type 9:  Unipolar Half Step

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        1         0         0
      2        0        1         0         0
      3        0        1         1         0
      4        0        0         1         0
      5        0        0         1         1
      6        0        0         0         1
      7        1        0         0         1
      0        1        0         0         0

    Type 10:  Bipolar Half Step

    State   Phase A   Phase B   Phase C   Phase D
      0        1        0         0         0
      1        1        0         1         0
      2        1        1         1         0
      3        0        1         1         0
      4        0        1         1         1
      5        0        1         0         1
      6        0        0         0         1
      7        1        0         0         1
      0        1        0         0         0

    Type 11:  Five Wire Unipolar

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        0         0         0        0
      1        0        1         0         0        0
      2        0        0         1         0        0
      3        0        0         0         1        0
      4        0        0         0         0        1
      0        1        0         0         0        0

    Type 12:  Five Wire Wave

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        1         0         0        0
      1        0        1         1         0        0
      2        0        0         1         1        0
      3        0        0         0         1        1
      4        1        0         0         0        1
      0        1        1         0         0        0

    Type 13:  Five Wire Unipolar HalfStep

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        0         0         0        0
      1        1        1         0         0        0
      2        0        1         0         0        0
      3        0        1         1         0        0
      4        0        0         1         0        0
      5        0        0         1         1        0
      6        0        0         0         1        0
      7        0        0         0         1        1
      8        0        0         0         0        1
      9        1        0         0         0        1
      0        1        0         0         0        0

    Type 14:  Five Wire Wave HalfStep

    State   Phase A   Phase B   Phase C   Phase D  Phase E
      0        1        1         0         0        0
      1        1        1         1         0        0
      2        0        1         1         0        0
      3        0        1         1         1        0
      4        0        0         1         1        0
      5        0        0         1         1        1
      6        0        0         0         1        1
      7        1        0         0         1        1
      8        1        0         0         0        1
      9        1        1         0         0        1
      0        1        1         0         0        0

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
#include "triple-buffer.h"
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"           // SHMPTR,SHMOFF
#include "hal_list.h"
#include "hal_logging.h"

#include <float.h>
#include "rtapi_math.h"

// debugging
// #define TRACE_TB
// #define VERBOSE_SETUP


#define MAX_CYCLE 18
#define USER_STEP_TYPE 13

/* module information */
MODULE_AUTHOR("John Kasunich, Michael Haberler");
MODULE_DESCRIPTION("Step Pulse Generator for EMC HAL");
MODULE_LICENSE("GPL");

int step_type =  -1;
RTAPI_IP_INT(step_type, "stepping types for this instance");

char *ctrl_type = "p";
RTAPI_IP_STRING(ctrl_type,"control type (pos or vel) for this instance");

int user_step_type[] = { [0 ... MAX_CYCLE-1] = -1 };
RTAPI_IP_ARRAY_INT(user_step_type, MAX_CYCLE,
		   "lookup table for user-defined step type for this instance");

RTAPI_TAG(HAL,HC_INSTANTIABLE);
RTAPI_TAG(HAL,HC_SMP_SAFE);

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/
// parameter set for make_pulses()
struct mp_params {
    hal_s32_t target_addval;	/* uf:w mp:r         desired freq generator add value */
    hal_s32_t deltalim;		/* uf:w mp:r         max allowed change per period */

    // uf:rw mp:r - parameters, r/o by make_pulses()
    hal_u32_t step_len;		/* uf:w mp:r step pulse length */
    hal_u32_t dir_hold_dly;     /* uf:w mp:r direction hold time or delay */
    hal_u32_t dir_setup;        /* uf:w mp:r direction setup time */

    char __mp_params_pad[RTAPI_CACHELINE -  5 * sizeof(hal_s32_t)];
};

/** This structure contains the runtime data for a single generator. */

/* structure members are ordered to optimize caching for makepulses,
   which runs in the fastest thread */

typedef struct {
    hal_list_t  list;                  // list of instances
    int inst_id;
    char *iname;

    // shared state between make_pulses(),  update_pos() and update_freq()
    struct shared {
	// passed as atomics
	hal_s64_t accum;	        /* uf:r up:r mp:rw   frequency generator accumulator */
	char __accum_pad[RTAPI_CACHELINE - sizeof(hal_s64_t)];

	hal_s32_t addval;		/* uf:w mp:rw        actual frequency generator add value */
	char __addval_pad[RTAPI_CACHELINE - sizeof(hal_s32_t)];

	// triple buffer for param messages update_freq() -> make_pulses()
	TB_FLAG_FAST(tb);
	struct mp_params tb_state[3];

    } shared;

    // make_pulses() private state
    struct make_pulses {
	struct mp_params *mpp;  // current make_pulses params
	unsigned int timer1;	/* times out when step pulse should end */
	unsigned int timer2;	/* times out when safe to change dir */
	unsigned int timer3;	/* times out when safe to step in new dir */
	int hold_dds;		/* prevents accumulator from updating */
	hal_s32_t *rawcount;	/* pin: position feedback in counts */
	hal_bit_t *jitter_correct;  /* mp:r use actual instead of assumed time */

	int curr_dir;		/* current direction */
	int state;		/* current position in state table */
	hal_bit_t *phase[5];	/* pins for output signals */
	int cycle_max;		/* cycle length for step types 2 and up */
	const unsigned char *lut;	/* pointer to state lookup table */
	int num_phases;		/* number of phases for types 2 and up */
    } mp;

    // updated in both update_pos() and update_freq() (change detection)
    // not an issue as long as both are on the same (slow) thread any order
    // not read by make_pulses
    struct ufp_shared {
	hal_float_t *pos_scale;	        /* pin: steps per position unit */
	double old_scale;		/* stored scale value */
	double scale_recip;		/* reciprocal value used for scaling */
    } ufp;

    // update_pos() private state
    struct update_pos {
	hal_s32_t *count;	/* pin: captured feedback in counts */
	hal_float_t *pos_fb;	/* pin: position feedback (position units) */
    } upos;

    // update_freq() private state
    struct update_freq {
	int pos_mode;		/* 1 = position mode, 0 = velocity mode */
	hal_float_t *pos_cmd;	/* pin: position command (position units) */
	double old_pos_cmd;		/* previous position command (counts) */
	hal_float_t *vel_cmd;	/* pin: velocity command (pos units/sec) */

	hal_float_t *freq;	/* pin: frequency command */
	hal_float_t *maxvel;	/* pin: max velocity, (pos units/sec) */
	hal_float_t *maxaccel;	/* pin: max accel (pos units/sec^2) */
	hal_u32_t *step_len;	/* pin: step pulse length */
	hal_u32_t *step_space;	/* pin: min step pulse spacing */
	hal_u32_t *dir_hold_dly; /* pin: direction hold time or delay */
	hal_u32_t *dir_setup;   /* pin: direction setup time */

	hal_u32_t old_step_len;	/* used to detect parameter changes */
	hal_u32_t old_step_space;
	hal_u32_t old_dir_hold_dly;
	hal_u32_t old_dir_setup;
    } upfrq;

    // setup-time constants
    struct readonly {            // driving pins, never modified
	hal_bit_t *enable;	 /* mp:r uf:r pin for enable stepgen */
	int step_type;		 /* mp:r uf:r stepping type - see list above */
    } ro;

    int printed_error;		/* flag to avoid repeated printing */
} stepgen_t;

/* lookup tables for stepping types 2 and higher - phase A is the LSB */

static unsigned char master_lut[][MAX_CYCLE] = {
    {1, 3, 2, 0, 0, 0, 0, 0, 0, 0},	/* type 2: Quadrature */
    {1, 2, 4, 0, 0, 0, 0, 0, 0, 0},	/* type 3: Three Wire */
    {1, 3, 2, 6, 4, 5, 0, 0, 0, 0},	/* type 4: Three Wire Half Step */
    {1, 2, 4, 8, 0, 0, 0, 0, 0, 0},	/* 5: Unipolar Full Step 1 */
    {3, 6, 12, 9, 0, 0, 0, 0, 0, 0},	/* 6: Unipoler Full Step 2 */
    {1, 7, 14, 8, 0, 0, 0, 0, 0, 0},	/* 7: Bipolar Full Step 1 */
    {5, 6, 10, 9, 0, 0, 0, 0, 0, 0},	/* 8: Bipoler Full Step 2 */
    {1, 3, 2, 6, 4, 12, 8, 9, 0, 0},	/* 9: Unipolar Half Step */
    {1, 5, 7, 6, 14, 10, 8, 9, 0, 0},	/* 10: Bipolar Half Step */
    {1, 2, 4, 8, 16, 0, 0, 0, 0, 0},	/* 11: Five Wire Unipolar */
    {3, 6, 12, 24, 17, 0, 0, 0, 0, 0},	/* 12: Five Wire Wave */
    {1, 3, 2, 6, 4, 12, 8, 24, 16, 17},	/* 13: Five Wire Uni Half */
    {3, 7, 6, 14, 12, 28, 24, 25, 17, 19},	/* 14: Five Wire Wave Half */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0} /* 15: User-defined */
};

static unsigned char cycle_len_lut[] =
    { 4, 3, 6, 4, 4, 4, 4, 8, 8, 5, 5, 10, 10, 0 };

static unsigned char num_phases_lut[] =
    { 2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 0, };

#define MAX_STEP_TYPE 15

#define STEP_PIN	0	/* output phase used for STEP signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */

#define PICKOFF		28	/* bit location in DDS accum */



/* other globals */
static int comp_id;		/* component ID */
static  hal_list_t head;

static long periodns;		/* makepulses function period in nanosec */
static long old_periodns;	/* used to detect changes in periodns */
static double periodfp;		/* makepulses function period in seconds */
static double freqscale;	/* conv. factor from Hz to addval counts */
static double accelscale;	/* conv. Hz/sec to addval cnts/period */
static long old_dtns;		/* update_freq funct period in nsec */
static double dt;		/* update_freq period in seconds */
static double recip_dt;		/* recprocal of period, avoids divides */

static const char *compname = "stepgenv2";
static const char *prefix = "stepgenv2";

typedef enum CONTROL { POSITION, VELOCITY, INVALID } CONTROL;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_stepgen(const char *name,  stepgen_t *addr,
			  const int step_type, const int pos_mode);
static int make_pulses(void *arg, const hal_funct_args_t *fa);
static int update_freq(void *arg, const hal_funct_args_t *fa);
static int update_pos(void *arg,  const hal_funct_args_t *fa);
static int setup_user_step_type(void);
static CONTROL parse_ctrl_type(const char *ctrl);
static int instantiate_stepgen( const int argc, char* const *argv);
static int delete_stepgen(const char *name, void *inst, const int inst_size);


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/
int rtapi_app_main(void)
{
    int retval = setup_user_step_type();
    if(retval < 0) {
        return retval;
    }

    dlist_init_entry(&head);

    if ((comp_id = hal_xinit(TYPE_RT, 0, 0,
			     instantiate_stepgen,
			     delete_stepgen,
			     compname)) < 0)
	return comp_id;

    hal_export_xfunct_args_t uf = {
        .type = FS_XTHREADFUNC,
        .funct.x = update_freq,
        .arg = &head,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&uf, "%s.update-freq",
				     prefix)) < 0)
	return retval;

    hal_export_xfunct_args_t up = {
        .type = FS_XTHREADFUNC,
        .funct.x = update_pos,
        .arg = &head,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&up, "%s.capture-position",
				     prefix)) < 0)
	return retval;
    hal_export_xfunct_args_t mp = {
        .type = FS_XTHREADFUNC,
        .funct.x = make_pulses,
        .arg = &head,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&mp, "%s.make-pulses",
				     prefix)) < 0)
	return retval;

    /* periodns will be set to the proper value when 'make_pulses()' runs for
       the first time.  We load a default value here to avoid glitches at
       startup, but all these 'constants' are recomputed inside
       'update_freq()' using the real period. */
    old_periodns = periodns = 50000;
    old_dtns = 1000000;

    /* precompute some constants */
    periodfp = periodns * 0.000000001;
    freqscale = (1L << PICKOFF) * periodfp;
    accelscale = freqscale * periodfp;
    dt = old_dtns * 0.000000001;
    recip_dt = 1.0 / dt;

    hal_ready(comp_id);
    return 0;
}

static int instantiate_stepgen(const int argc, char* const *argv)
{
    int retval;
    const char* name;
    
    if(argc >= 2)
        name = argv[1];
    else
        HALFAIL_RC(EINVAL, "ERROR: insufficient args in argv");

    // validate instance parameters
    if ((step_type > MAX_STEP_TYPE) || (step_type < 0)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"STEPGEN instance %s: ERROR: bad stepping type '%d'\n",
			name, step_type);
	return -1;
    }
    int ctype = parse_ctrl_type(ctrl_type);
    if (ctype == INVALID) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"STEPGEN instance %s: ERROR: bad control type '%s'"
			" (must be 'p' or 'v')\n", name, ctrl_type);
	return -1;
    }

    stepgen_t *p;
    if ((retval = hal_inst_create(name, comp_id, sizeof(stepgen_t), (void **)&p)) < 0)
	return retval;

    p->inst_id = retval;
    if ((retval = export_stepgen(name, p, step_type, ctype == POSITION)) != 0)
	HALFAIL_RC(retval, "STEPGEN: ERROR: export_stepgen(%s, %s) failed", compname, name);

    p->iname = halg_strdup(1, name);

    // append to instance list
    dlist_init_entry(&p->list);
    dlist_add_after(&p->list, &head);
    return 0;
}

static int delete_stepgen(const char *name, void *inst, const int inst_size)
{
    stepgen_t *p = inst;
    halg_free_str(&p->iname);

    // if controlled turnoff of some pin(s) is to be done on shutdown,
    // this is the place:

    // delete from instance list
    dlist_remove_entry(&p->list);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*              REALTIME STEP PULSE GENERATION FUNCTIONS                *
************************************************************************/

/** The frequency generator works by adding a signed value proportional
    to frequency to an accumulator.  When bit PICKOFF of the accumulator
    toggles, a step is generated.
*/

static int make_pulses(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    stepgen_t *self;
    long old_addval, target_addval, new_addval, step_now;
    int p;
    unsigned char outbits;

    // foreach stepgen instance:
    dlist_for_each_entry(self, insts, list) {

	struct make_pulses *mp = &self->mp;
	struct shared *shared = &self->shared;
	const struct readonly *ro = &self->ro;

	if (rtapi_tb_snapshot(&shared->tb)) {
	    // new parameter set available, fetch it
	    mp->mpp = &self->shared.tb_state[rtapi_tb_snap_idx(&shared->tb)];

#ifdef TRACE_TB
	    HALDBG("SNAP inst=%d target_addval=%d deltalim=%d "
		   "step_len=%d dir_hold_dly=%d dir_setup=%d",
		   self->inst_id,
		   mp->mpp->target_addval,
		   mp->mpp->deltalim,
		   mp->mpp->step_len,
		   mp->mpp->dir_hold_dly,
		   mp->mpp->dir_setup);
#endif

	    // not needed on every cycle, really just once
	    /* store period so scaling constants can be (re)calculated */
	    periodns = fa_period(fa);
	}

	long delay = *(mp->jitter_correct) ? fa_current_period(fa) : periodns;

	// currently valid parameter set
	struct mp_params *mpp = mp->mpp;

	// work from local copies
	hal_s32_t addval = rtapi_load_s32(&shared->addval);
	hal_s64_t accum =  rtapi_load_s64(&shared->accum);

	/* decrement "timing constraint" timers */
	if ( mp->timer1 > 0 ) {
	    if ( mp->timer1 > delay ) {
		mp->timer1 -= delay;
	    } else {
		mp->timer1 = 0;
	    }
	}
	if ( mp->timer2 > 0 ) {
	    if ( mp->timer2 > delay ) {
		mp->timer2 -= delay;
	    } else {
		mp->timer2 = 0;
	    }
	}
	if ( mp->timer3 > 0 ) {
	    if ( mp->timer3 > delay ) {
		mp->timer3 -= delay;
	    } else {
		mp->timer3 = 0;
		/* last timer timed out, cancel hold */
		mp->hold_dds = 0;
	    }
	}
	if ( !mp->hold_dds && *(ro->enable) ) {
	    /* update addval (ramping) */
	    old_addval = addval;
	    target_addval = mp->mpp->target_addval;
	    if (mp->mpp->deltalim != 0) {
		/* implement accel/decel limit */
		if (target_addval > (old_addval + mp->mpp->deltalim)) {
		    /* new value is too high, increase addval as far as possible */
		    new_addval = old_addval + mp->mpp->deltalim;
		} else if (target_addval < (old_addval - mp->mpp->deltalim)) {
		    /* new value is too low, decrease addval as far as possible */
		    new_addval = old_addval - mp->mpp->deltalim;
		} else {
		    /* new value can be reached in one step - do it */
		    new_addval = target_addval;
		}
	    } else {
		/* go to new freq without any ramping */
		new_addval = target_addval;
	    }
	    /* save result */
	    addval = new_addval;
	    /* check for direction reversal */
	    if (((new_addval >= 0) && (old_addval < 0)) ||
		((new_addval < 0) && (old_addval >= 0))) {
		/* reversal required, can we do so now? */
		if ( mp->timer3 != 0 ) {
		    /* no - hold everything until delays time out */
		    mp->hold_dds = 1;
		}
	    }
	}
	/* update DDS */
	if ( !mp->hold_dds && *(ro->enable) ) {
	    /* save current value of low half of accum */
	    step_now = accum;
	    /* update the accumulator */
	    accum += addval;
	    /* test for changes in low half of accum */
	    step_now ^= accum;
	    /* we only care about the pickoff bit */
	    step_now &= (1L << PICKOFF);
	    /* update rawcounts parameter */
	    *(mp->rawcount) = accum >> PICKOFF;

	   // atomic update
	    rtapi_store_s64(&shared->accum, accum);
	} else {
	    /* DDS is in hold, no steps */
	    step_now = 0;
	}
	if ( mp->timer2 == 0 ) {
	    /* update direction - do not change if addval = 0 */
	    if ( addval > 0 ) {
		mp->curr_dir = 1;
	    } else if ( shared->addval < 0 ) {
		mp->curr_dir = -1;
	    }
	}
	// save updated addval (might be zeroed by update_freq())
	rtapi_store_s32(&shared->addval, addval);

	if ( step_now ) {
	    /* (re)start various timers */

	    /* timer 1 = time till end of step pulse */
	    mp->timer1 = mpp->step_len;

	    /* timer 2 = time till allowed to change dir pin */
	    mp->timer2 = mp->timer1 + mpp->dir_hold_dly;

	    /* timer 3 = time till allowed to step the other way */
	    mp->timer3 = mp->timer2 + mpp->dir_setup;

	    if ( ro->step_type >= 2 ) {
		/* update state */
		mp->state += mp->curr_dir;
		if ( mp->state < 0 ) {
		    mp->state = mp->cycle_max;
		} else if ( mp->state > mp->cycle_max ) {
		    mp->state = 0;
		}
	    }
	}
	/* generate output, based on stepping type */
	if (ro->step_type == 0) {
	    /* step/dir output */
	    if ( mp->timer1 != 0 ) {
		 *(mp->phase[STEP_PIN]) = 1;
	    } else {
		 *(mp->phase[STEP_PIN]) = 0;
	    }
	    if ( mp->curr_dir < 0 ) {
		 *(mp->phase[DIR_PIN]) = 1;
	    } else {
		 *(mp->phase[DIR_PIN]) = 0;
	    }
	} else if (ro->step_type == 1) {
	    /* up/down */
	    if ( mp->timer1 != 0 ) {
		if ( mp->curr_dir < 0 ) {
		    *(mp->phase[UP_PIN]) = 0;
		    *(mp->phase[DOWN_PIN]) = 1;
		} else {
		    *(mp->phase[UP_PIN]) = 1;
		    *(mp->phase[DOWN_PIN]) = 0;
		}
	    } else {
		*(mp->phase[UP_PIN]) = 0;
		*(mp->phase[DOWN_PIN]) = 0;
	    }
	} else {
	    /* step type 2 or greater */
	    /* look up correct output pattern */
	    outbits = (mp->lut)[mp->state];
	    /* now output the phase bits */
	    for (p = 0; p < mp->num_phases; p++) {
		/* output one phase */
		*(mp->phase[p]) = outbits & 1;
		/* move to the next phase */
		outbits >>= 1;
	    }
	}
    }
    return 0;
}

static int update_pos(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    stepgen_t *self;
    hal_s64_t accum;

    // foreach stepgen instance:
    dlist_for_each_entry(self, insts, list) {

	struct update_pos *upos = &self->upos;
	struct ufp_shared *ufp_shared = &self->ufp;
	struct shared *shared = &self->shared;

	accum = rtapi_load_s64(&shared->accum);

	/* compute integer counts */
	*(upos->count) = accum >> PICKOFF;

	// duplicated in update_freq() -mah

	/* check for change in scale value */
	if (*(ufp_shared->pos_scale) != ufp_shared->old_scale) {
	    /* get ready to detect future scale changes */
	    ufp_shared->old_scale = *(ufp_shared->pos_scale);
	    /* validate the new scale value */
	    if ((*(ufp_shared->pos_scale) < 1e-20)
		&& (*(ufp_shared->pos_scale) > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		*(ufp_shared->pos_scale) = 1.0;
	    }
	    /* we will need the reciprocal, and the accum is fixed point with
	       fractional bits, so we precalc some stuff */
	    ufp_shared->scale_recip = (1.0 / (1L << PICKOFF)) / *(ufp_shared->pos_scale);
	}

	/* scale accumulator to make floating point position, after
	   removing the one-half count offset */
	*(upos->pos_fb) = (double)(accum-(1<< (PICKOFF-1))) * ufp_shared->scale_recip;
    }
    return 0;
}

/* helper function - computes integeral multiple of increment that is greater
   or equal to value */
static unsigned long ulceil(unsigned long value, unsigned long increment)
{
    if ( value == 0 ) {
	return 0;
    }
    return increment*(((value-1)/increment)+1);
}

static int update_freq(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    stepgen_t *self;

    int newperiod;
    long min_step_period;
    hal_s64_t accum;
    double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
    double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;
    double desired_freq;

    /*! \todo FIXME - while this code works just fine, there are a bunch of
       internal variables, many of which hold intermediate results that
       don't really need their own variables.  They are used either for
       clarity, or because that's how the code evolved.  This algorithm
       could use some cleanup and optimization. */
    /* this periodns stuff is a little convoluted because we need to
       calculate some constants here in this relatively slow thread but the
       constants are based on the period of the much faster 'make_pulses()'
       thread. */
    /* only recalc constants if period changes */
    newperiod = 0;
    if (periodns != old_periodns) {
	/* get ready to detect future period changes */
	old_periodns = periodns;
	/* recompute various constants that depend on periodns */
	periodfp = periodns * 0.000000001;
	freqscale = (1L << PICKOFF) * periodfp;
	accelscale = freqscale * periodfp;
	/* force re-evaluation of the timing parameters */
	newperiod = 1;
    }
    /* now recalc constants related to the period of this funct */
    /* only recalc constants if period changes */
    if (fa_period(fa) != old_dtns) {
	/* get ready to detect future period changes */
	old_dtns = fa_period(fa);
	/* dT is the period of this thread, used for the position loop */
	dt = fa_period(fa) * 0.000000001;
	/* calc the reciprocal once here, to avoid multiple divides later */
	recip_dt = 1.0 / dt;
    }

    // foreach stepgen instance:
    dlist_for_each_entry(self, insts, list) {

	struct update_freq *upfreq = &self->upfrq;
	struct shared *shared = &self->shared;
	const struct readonly *ro = &self->ro;
	struct ufp_shared *ufp_shared = &self->ufp;

	// get a handle on the currently write buffer
	struct mp_params *mpp =  &shared->tb_state[rtapi_tb_write_idx(&shared->tb)];

	/* check for scale change */
	if (*(ufp_shared->pos_scale) != ufp_shared->old_scale) {
	    /* get ready to detect future scale changes */
	    ufp_shared->old_scale = *(ufp_shared->pos_scale);
	    /* validate the new scale value */
	   if ((*(ufp_shared->pos_scale) < 1e-20)
		 && (*(ufp_shared->pos_scale) > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		     *(ufp_shared->pos_scale) = 1.0;
	   }
	    /* we will need the reciprocal, and the accum is fixed point with
	       fractional bits, so we precalc some stuff */
	   ufp_shared->scale_recip = (1.0 / (1L << PICKOFF)) / *(ufp_shared->pos_scale);
	}
	if ( newperiod ) {
	    /* period changed, force recalc of timing parameters */
	    upfreq->old_step_len = ~0;
	    upfreq->old_step_space = ~0;
	    upfreq->old_dir_hold_dly = ~0;
	    upfreq->old_dir_setup = ~0;
	}
	/* process timing parameters */
	if ( *(upfreq->step_len) != upfreq->old_step_len ) {
	    /* must be non-zero */
	    if ( *(upfreq->step_len) == 0 ) {
		*(upfreq->step_len) = 1;
	    }
	    /* make integer multiple of periodns */
	    upfreq->old_step_len = ulceil(*(upfreq->step_len), periodns);
	    *(upfreq->step_len) = upfreq->old_step_len;
	}
	if ( *(upfreq->step_space) != upfreq->old_step_space ) {
	    /* make integer multiple of periodns */
	    upfreq->old_step_space = ulceil(*(upfreq->step_space), periodns);
	    *(upfreq->step_space) = upfreq->old_step_space;
	}
	if ( *(upfreq->dir_setup) != upfreq->old_dir_setup ) {
	    /* make integer multiple of periodns */
	    upfreq->old_dir_setup = ulceil(*(upfreq->dir_setup), periodns);
	    *(upfreq->dir_setup) = upfreq->old_dir_setup;
	}
	if ( *(upfreq->dir_hold_dly) != upfreq->old_dir_hold_dly ) {
	    if ( (*(upfreq->dir_hold_dly) + *(upfreq->dir_setup)) == 0 ) {
		/* dirdelay must be non-zero step types 0 and 1 */
		if ( ro->step_type < 2 ) {
		    *(upfreq->dir_hold_dly) = 1;
		}
	    }
	    upfreq->old_dir_hold_dly = ulceil(*(upfreq->dir_hold_dly), periodns);
	    *(upfreq->dir_hold_dly) = upfreq->old_dir_hold_dly;
	}
	/* test for disabled stepgen */
	if (*ro->enable == 0) {
	    /* disabled: keep updating old_pos_cmd (if in pos ctrl mode) */
	    if ( upfreq->pos_mode ) {
		upfreq->old_pos_cmd = *(upfreq->pos_cmd) * *(ufp_shared->pos_scale);
	    }
	    /* set velocity to zero */
	    upfreq->freq = 0;

	    // atomically clear
	    rtapi_store_s32(&shared->addval, 0);

	    mpp->target_addval = 0;
	    mpp->deltalim = 0;  // ?
	    mpp->step_len =     *(upfreq->step_len);
	    mpp->dir_hold_dly = *(upfreq->dir_hold_dly);
	    mpp->dir_setup =    *(upfreq->dir_setup);

	    // write memory barrier
	    rtapi_smp_wmb();
	    // commit
	    rtapi_tb_flip(&shared->tb);

	    /* and skip to next one */
	    continue;
	}
	/* calculate frequency limit */
	min_step_period = *(upfreq->step_len) + *(upfreq->step_space);
	max_freq = 1.0 / (min_step_period * 0.000000001);
	/* check for user specified frequency limit parameter */
	if (*(upfreq->maxvel) <= 0.0) {
	    /* set to zero if negative */
	    *(upfreq->maxvel) = 0.0;
	} else {
	    /* parameter is non-zero, compare to max_freq */
	    desired_freq = *(upfreq->maxvel) * rtapi_fabs(*(ufp_shared->pos_scale));
	    if (desired_freq > max_freq) {
		/* parameter is too high, complain about it */
		if(!self->printed_error) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
			"STEPGEN: %s: The requested maximum velocity of %d steps/sec is too high.\n",
			self->iname, (int)desired_freq);
		    rtapi_print_msg(RTAPI_MSG_ERR,
			"STEPGEN: The maximum possible frequency is %d steps/second\n",
			(int)max_freq);
		    self->printed_error = 1;
		}
		/* parameter is too high, limit it */
		*(upfreq->maxvel) = max_freq / rtapi_fabs(*(ufp_shared->pos_scale));
	    } else {
		/* lower max_freq to match parameter */
		max_freq = *(upfreq->maxvel) * rtapi_fabs(*(ufp_shared->pos_scale));
	    }
	}
	/* set internal accel limit to its absolute max, which is
	   zero to full speed in one thread period */
	max_ac = max_freq * recip_dt;
	/* check for user specified accel limit parameter */
	if (*(upfreq->maxaccel) <= 0.0) {
	    /* set to zero if negative */
	    *(upfreq->maxaccel) = 0.0;
	} else {
	    /* parameter is non-zero, compare to max_ac */
	    if (*(upfreq->maxaccel) * rtapi_fabs(*(ufp_shared->pos_scale)) > max_ac) {
		/* parameter is too high, lower it */
		*(upfreq->maxaccel) = max_ac / rtapi_fabs(*(ufp_shared->pos_scale));
	    } else {
		/* lower limit to match parameter */
		max_ac = *(upfreq->maxaccel) * rtapi_fabs(*(ufp_shared->pos_scale));
	    }
	}
	/* at this point, all scaling, limits, and other parameter
	   changes have been handled - time for the main control */
	if ( upfreq->pos_mode ) {
	    /* calculate position command in counts */
	    pos_cmd = *(upfreq->pos_cmd) * *(ufp_shared->pos_scale);
	    /* calculate velocity command in counts/sec */
	    vel_cmd = (pos_cmd - upfreq->old_pos_cmd) * recip_dt;
	    upfreq->old_pos_cmd = pos_cmd;

	    // atomically load accum
	    accum = rtapi_load_s64(&shared->accum);

	    /* convert from fixed point to double, after subtracting
	       the one-half step offset */
	    curr_pos = (accum-(1<< (PICKOFF-1))) * (1.0 / (1L << PICKOFF));
	    /* get velocity in counts/sec */
	    curr_vel = *(upfreq->freq);
	    /* At this point we have good values for pos_cmd, curr_pos,
	       vel_cmd, curr_vel, max_freq and max_ac, all in counts,
	       counts/sec, or counts/sec^2.  Now we just have to do
	       something useful with them. */
	    /* determine which way we need to ramp to match velocity */
	    if (vel_cmd > curr_vel) {
		match_ac = max_ac;
	    } else {
		match_ac = -max_ac;
	    }
	    /* determine how long the match would take */
	    match_time = (vel_cmd - curr_vel) / match_ac;
	    /* calc output position at the end of the match */
	    avg_v = (vel_cmd + curr_vel) * 0.5;
	    est_out = curr_pos + avg_v * match_time;
	    /* calculate the expected command position at that time */
	    est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * dt);
	    /* calculate error at that time */
	    est_err = est_out - est_cmd;
	    if (match_time < dt) {
		/* we can match velocity in one period */
		if (rtapi_fabs(est_err) < 0.0001) {
		    /* after match the position error will be acceptable */
		    /* so we just do the velocity match */
		    new_vel = vel_cmd;
		} else {
		    /* try to correct position error */
		    new_vel = vel_cmd - 0.5 * est_err * recip_dt;
		    /* apply accel limits */
		    if (new_vel > (curr_vel + max_ac * dt)) {
			new_vel = curr_vel + max_ac * dt;
		    } else if (new_vel < (curr_vel - max_ac * dt)) {
			new_vel = curr_vel - max_ac * dt;
		    }
		}
	    } else {
		/* calculate change in final position if we ramp in the
		   opposite direction for one period */
		dv = -2.0 * match_ac * dt;
		dp = dv * match_time;
		/* decide which way to ramp */
		if (rtapi_fabs(est_err + dp * 2.0) < rtapi_fabs(est_err)) {
		    match_ac = -match_ac;
		}
		/* and do it */
		new_vel = curr_vel + match_ac * dt;
	    }
	    /* apply frequency limit */
	    if (new_vel > max_freq) {
		new_vel = max_freq;
	    } else if (new_vel < -max_freq) {
		new_vel = -max_freq;
	    }
	    /* end of position mode */
	} else {
	    /* velocity mode is simpler */
	    /* calculate velocity command in counts/sec */
	    vel_cmd = *(upfreq->vel_cmd) * *(ufp_shared->pos_scale);
	    /* apply frequency limit */
	    if (vel_cmd > max_freq) {
		vel_cmd = max_freq;
	    } else if (vel_cmd < -max_freq) {
		vel_cmd = -max_freq;
	    }
	    /* calc max change in frequency in one period */
	    dv = max_ac * dt;
	    /* apply accel limit */
	    if ( vel_cmd > (*(upfreq->freq) + dv) ) {
		new_vel = *(upfreq->freq) + dv;
	    } else if ( vel_cmd < (*(upfreq->freq) - dv) ) {
		new_vel = *(upfreq->freq) - dv;
	    } else {
		new_vel = vel_cmd;
	    }
	    /* end of velocity mode */
	}
	*(upfreq->freq) = new_vel;

	/* calculate new addval */
	mpp->target_addval = *(upfreq->freq) * freqscale;
	/* calculate new deltalim */
	mpp->deltalim = max_ac * accelscale;

	mpp->step_len =     *(upfreq->step_len);
	mpp->dir_hold_dly = *(upfreq->dir_hold_dly);
	mpp->dir_setup =    *(upfreq->dir_setup);

	// write memory barrier
	rtapi_smp_wmb();
	// commit
	rtapi_tb_flip(&shared->tb);
    }
    return 0;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
static int export_stepgen(const char *name,  stepgen_t *self,
			  const int step_type, const int pos_mode)
{
    int n, retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
#ifndef VERBOSE_SETUP
    rtapi_set_msg_level(RTAPI_MSG_WARN);
#endif

    /* export param variable for raw counts */
    retval = hal_pin_s32_newf(HAL_OUT, &(self->mp.rawcount), self->inst_id,
			      "%s.rawcounts", name);
    if (retval != 0) { return retval; }
    retval = hal_pin_bit_newf(HAL_IN, &(self->mp.jitter_correct), self->inst_id,
			      "%s.jitter-correct", name);
    if (retval != 0) { return retval; }

    /* export pin for counts captured by update() */
    retval = hal_pin_s32_newf(HAL_OUT, &(self->upos.count), self->inst_id,
			      "%s.counts", name);
    if (retval != 0) { return retval; }
    /* export parameter for position scaling */
    retval = hal_pin_float_newf(HAL_IO, &(self->ufp.pos_scale), self->inst_id,
				"%s.position-scale", name);
    if (retval != 0) { return retval; }
    /* export pin for command */
    if ( pos_mode ) {
	retval = hal_pin_float_newf(HAL_IN, &(self->upfrq.pos_cmd), self->inst_id,
				    "%s.position-cmd", name);
    } else {
	retval = hal_pin_float_newf(HAL_IN, &(self->upfrq.vel_cmd), self->inst_id,
				    "%s.velocity-cmd", name);
    }
    if (retval != 0) { return retval; }
    /* export pin for enable command */
    retval = hal_pin_bit_newf(HAL_IN, &(self->ro.enable), self->inst_id,
			      "%s.enable", name);
    if (retval != 0) { return retval; }
    /* export pin for scaled position captured by update() */
    retval = hal_pin_float_newf(HAL_OUT, &(self->upos.pos_fb), self->inst_id,
				"%s.position-fb", name);
    if (retval != 0) { return retval; }
    /* export param for scaled velocity (frequency in Hz) */
    retval = hal_pin_float_newf(HAL_OUT, &(self->upfrq.freq), self->inst_id,
				"%s.frequency", name);
    if (retval != 0) { return retval; }
    /* export parameter for max frequency */
    retval = hal_pin_float_newf(HAL_IO, &(self->upfrq.maxvel), self->inst_id,
				"%s.maxvel", name);
    if (retval != 0) { return retval; }
    /* export parameter for max accel/decel */
    retval = hal_pin_float_newf(HAL_IO, &(self->upfrq.maxaccel), self->inst_id,
				"%s.maxaccel", name);
    if (retval != 0) { return retval; }
    /* every step type uses steplen */
    retval = hal_pin_u32_newf(HAL_IO, &(self->upfrq.step_len), self->inst_id,
			      "%s.steplen", name);
    if (retval != 0) { return retval; }
    /* step/dir and up/down use 'stepspace' */
    retval = hal_pin_u32_newf(HAL_IO, &(self->upfrq.step_space),
			      self->inst_id, "%s.stepspace", name);
    if (retval != 0) { return retval; }
    /* step/dir is the only one that uses dirsetup and dirhold */
    retval = hal_pin_u32_newf(HAL_IO, &(self->upfrq.dir_setup),
			      self->inst_id, "%s.dirsetup", name);
    if (retval != 0) { return retval; }
    if ( step_type == 0 ) {
	retval = hal_pin_u32_newf(HAL_IO, &(self->upfrq.dir_hold_dly),
				  self->inst_id, "%s.dirhold", name);
	if (retval != 0) { return retval; }
    } else {
	/* the others use dirdelay */
	retval = hal_pin_u32_newf(HAL_IO, &(self->upfrq.dir_hold_dly),
				  self->inst_id, "%s.dirdelay", name);
	if (retval != 0) { return retval; }
    }
    /* export output pins */
    if ( step_type == 0 ) {
	/* step and direction */
	retval = hal_pin_bit_newf(HAL_OUT, &(self->mp.phase[STEP_PIN]),
				  self->inst_id, "%s.step", name);
	if (retval != 0) { return retval; }
	*(self->mp.phase[STEP_PIN]) = 0;
	retval = hal_pin_bit_newf(HAL_OUT, &(self->mp.phase[DIR_PIN]),
				  self->inst_id, "%s.dir", name);
	if (retval != 0) { return retval; }
	*(self->mp.phase[DIR_PIN]) = 0;
    } else if (step_type == 1) {
	/* up and down */
	retval = hal_pin_bit_newf(HAL_OUT, &(self->mp.phase[UP_PIN]),
				  self->inst_id, "%s.up", name);
	if (retval != 0) { return retval; }
	*(self->mp.phase[UP_PIN]) = 0;
	retval = hal_pin_bit_newf(HAL_OUT, &(self->mp.phase[DOWN_PIN]),
				  self->inst_id, "%s.down", name);
	if (retval != 0) { return retval; }
	*(self->mp.phase[DOWN_PIN]) = 0;
    } else {
	/* stepping types 2 and higher use a varying nameber of phase pins */
	self->mp.num_phases = num_phases_lut[step_type - 2];
	for (n = 0; n < self->mp.num_phases; n++) {
	    retval = hal_pin_bit_newf(HAL_OUT, &(self->mp.phase[n]),
				      self->inst_id, "%s.phase-%c", name, n + 'A');
	    if (retval != 0) { return retval; }
	    *(self->mp.phase[n]) = 0;
	}
    }


    // --- update_freq() private state and pins ---
    self->upfrq.pos_mode = pos_mode;
    self->upfrq.old_pos_cmd = 0.0;
    if ( pos_mode ) {
	*(self->upfrq.pos_cmd) = 0.0;
    } else {
	*(self->upfrq.vel_cmd) = 0.0;
    }
    *(self->upfrq.freq) = 0.0;
    *(self->upfrq.maxvel) = 0.0;
    *(self->upfrq.maxaccel) = 0.0;
    *(self->upfrq.step_len) = 1;

    /* timing parameter defaults depend on step type */
    if ( step_type < 2 ) {
	*(self->upfrq.step_space) = 1;
    } else {
	*(self->upfrq.step_space) = 0;
    }
    if ( step_type == 0 ) {
	*(self->upfrq.dir_hold_dly) = 1;
	*(self->upfrq.dir_setup) = 1;
    } else {
	*(self->upfrq.dir_hold_dly) = 1;
	*(self->upfrq.dir_setup) = 0;
    }

    /* set 'old' values to make update_freq validate the timing params */
    self->upfrq.old_step_len = ~0;
    self->upfrq.old_step_space = ~0;
    self->upfrq.old_dir_hold_dly = ~0;
    self->upfrq.old_dir_setup = ~0;
    // --- update_freq() private state initialisation complete  ---

    // update_pos() state and pins
    *(self->upos.count) = 0;
    *(self->upos.pos_fb) = 0.0;

    // update_freq()/update_pos() shared
    *(self->ufp.pos_scale) = 1.0;
    self->ufp.old_scale = 0.0;
    self->ufp.scale_recip = 0.0;

    // readonly params
    self->ro.step_type = step_type;
    *(self->ro.enable) = 0;

    // ---  shared state ---
    /* accumulator gets a half step offset, so it will step half
       way between integer positions, not at the integer positions */
    self->shared.accum = 1 << (PICKOFF-1);
    self->shared.addval = 0;

    // set triple buffer initial state
    rtapi_tb_init(&self->shared.tb);

    // supply startup params to make_pulses
    // this guaranteeds the struct mp_params *mpp local in make_pulses()
    // will never be NULL
    struct mp_params *mpp =  &self->shared.tb_state[rtapi_tb_write_idx(&self->shared.tb)];
    mpp->target_addval = 0;
    mpp->deltalim = 0;
    mpp->step_len = *(self->upfrq.step_len);
    mpp->dir_hold_dly = *(self->upfrq.dir_hold_dly);
    mpp->dir_setup = *(self->upfrq.dir_setup);

    // --- make_pulses() private state and pins ---
    if ( step_type >= 2 ) {
	/* init output stuff */
	self->mp.cycle_max = cycle_len_lut[step_type - 2] - 1;
	self->mp.lut = &(master_lut[step_type - 2][0]);
    }
    /* init the step generator core to zero output */
    self->mp.timer1 = 0;
    self->mp.timer2 = 0;
    self->mp.timer3 = 0;
    self->mp.hold_dds = 0;
    *(self->mp.rawcount) = 0;
    self->mp.curr_dir = 0;
    self->mp.state = 0;
    // --- make_pulses() private state init complete ---


    // all init complete, only now do barrier and flip

    // write memory barrier
    rtapi_smp_wmb();
    // commit startup params to make_pulses
    rtapi_tb_flip(&self->shared.tb);

    // --- all shared state init complete ---

    self->printed_error = 0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int setup_user_step_type(void) {
    int used_phases = 0;
    int i = 0;
    for(i=0; i<10 && user_step_type[i] != -1; i++) {
        master_lut[USER_STEP_TYPE][i] = user_step_type[i];
	used_phases |= user_step_type[i];
    }
    cycle_len_lut[USER_STEP_TYPE] = i;
    if(used_phases & ~0x1f) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STEPGEN: ERROR: "
			"bad user step type uses more than 5 phases");
	return -EINVAL; // more than 5 phases is not allowed
    }

    if(used_phases & 0x10) num_phases_lut[USER_STEP_TYPE] = 5;
    else if(used_phases & 0x8) num_phases_lut[USER_STEP_TYPE] = 4;
    else if(used_phases & 0x4) num_phases_lut[USER_STEP_TYPE] = 3;
    else if(used_phases & 0x2) num_phases_lut[USER_STEP_TYPE] = 2;
    else if(used_phases & 0x1) num_phases_lut[USER_STEP_TYPE] = 1;

    if(used_phases)
	rtapi_print_msg(RTAPI_MSG_INFO,
			"User step type has %d phases and %d steps per cycle\n",
			num_phases_lut[USER_STEP_TYPE], i);
    return 0;
}

static CONTROL parse_ctrl_type(const char *ctrl)
{
    if(!ctrl || !*ctrl || *ctrl == 'p' || *ctrl == 'P') return POSITION;
    if(*ctrl == 'v' || *ctrl == 'V') return VELOCITY;
    return INVALID;
}
