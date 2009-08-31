/********************************************************************
* Description:  freqgen.c
*               A HAL component that generates step pulses at a 
*               specific frequency in software.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/
/** This file, 'freqgen.c', is a HAL component that generates step
    pulses at a specific frequency in software.  The maximum step
    rate will depend on the speed of the PC, but is expected to
    exceed 1KHz for even the slowest computers, and may reach
    10KHz on fast ones.  It is a realtime component.

    Note that this  is _not_ the preferred step pulse generator
    for EMC2.  This module simply generates a frequency (velocity).
    A separate PID or other loop is needed to turn EMC's position
    commands into velocity commands, and the PID loop needs to
    be tuned, which adds unneccessary complexity to the machine
    setup.  For EMC and other applications that use position
    instead of velocity commands, the component "stepgen" is
    preferred.  It contains a built in, pre-tuned position to
    velocity converter.  This module is provided only for special
    cases where it is more appropriate.

    It supports up to 8 pulse generators.  Each generator can produce
    several types of outputs in addition to step/dir, including
    quadrature, half- and full-step unipolar and bipolar, three phase,
    and five phase.  A 32 bit feedback value is provided indicating
    the current position of the motor (assuming no lost steps).
    The number of step generators and type of outputs is determined
    by the insmod command line parameter 'step_type'.  It accepts
    a comma separated (no spaces) list of up to 8 stepping types
    to configure up to 8 channels.  So a command line like this:
          insmod freqgen step_type=0,0,1,2
    will install four step generators, two using stepping type 0,
    one using type 1, and one using type 2.

    The driver exports three functions.  'freqgen.make-pulses', is
    responsible for actually generating the step pulses.  It must
    be executed in a fast thread to reduce pulse jitter.  The other
    two functions are normally called from a much slower thread.
    'freqgen.update-freq' reads the frequency command and sets
    internal variables used by 'freqgen.make-pulses'.
    'freqgen.capture-position' captures and scales the current
    values of the position feedback counters.  Both 'update-freq' and
    'capture-position' use floating point, 'make-pulses' does not.

    Polarity:

    All signals from this module have fixed polarity (active high
    in most cases).  If the driver needs the opposite polarity,
    the signals can be inverted using parameters exported by the
    hardware driver(s) such as ParPort.

    Stepping Types:

    This module supports a number of stepping types, as follows:

    Type 0:  Step and Direction
                       _____         _____              _____
    STEP  ____________/     \_______/     \____________/     \___
                      |     |       |     |
              |--(1)--|-(2)-|--(3)--|     |--(4)--|
          ____|___________________________________|_____________
    DIR   ____X___________________________________X_____________

    There are two output pins, STEP and DIR.  Timing is controlled
    by HAL parameters.  The parameters are (1): 'freqgen.n.dirsetup'
    minimum delay from a change on the DIR line to the beginning of
    a step pulse, (2): 'freqgen.n.steplen' length of the step pulse,
    (3): 'freqgen.n.stepspace', space between step pulses, and
    (4): 'freqgen.n.dirhold', minimum delay after step pulse before
    DIR may change.  The default values for all four parameters are
    1, which means 1 period of the thread.  A positive frequency
    command results in DIR low, negative frequency command means
    DIR high.  The minimum time between step pulses is 'steplen' +
    'stepspace' periods, and the frequency command is clamped to
    avoid exceeding these limits.

    Type 1:  Up/Down (aka Pseudo-PWM)

    There are two output pins, UP and DOWN.  Whenever a step is
    required, either UP or DOWN is asserted for a single period.
    The frequency command is not clamped, so a step may be
    required every period, and the UP or DOWN line may be asserted
    for several periods in a row.  (At the maximum freqency
    command, UP or DOWN will be constantly asserted.)  This type
    of signal may be usefull with some stepper drives, but is
    primarily intended as a simple and cheap DAC.  A filter
    and differential amp connected between UP and DOWN can
    produce a +/-10V signal, with bandwidth and resolution
    determined by the filter (in general, faster bandwidth
    gives lower resolution, and vice-versa.)

    All the remaining stepping types are simply different patterns
    of output states.  For all of these types, a step can occur
    in every period.  When a step occurs, the output state changes
    to the next (or previous) state in the state listings that
    follow.  There are from two to five output pins, called
    'PhaseA' thru 'PhaseE'.

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
#include "hal.h"		/* HAL public API decls */

#define MAX_CHAN 8
#define STRINGIZE(x) #x
#define MAX_CHAN_STR STRINGIZE(MAX_CHAN)
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Frequency Generator for EMC HAL");
MODULE_LICENSE("GPL");
int step_type[MAX_CHAN] = { -1, -1, -1, -1, -1, -1, -1, -1 };
RTAPI_MP_ARRAY_INT(step_type, MAX_CHAN, "stepping types for up to "MAX_CHAN_STR" channels");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** These structures contains the runtime data for a single generator.
    The 'st0_t' struct has data needed for stepping type 0 only, and
    'st2_t' has data needed for stepping types 2 and higher only.  A
    union is used so the two structs can share space in the main
    'freqgen_t' structure.  This keeps the frequently accessed parts
    of the main structure smaller, allowing them to occupy fewer cache
    lines.  This improves speed as well as conserving shared memory.
    Data is arranged in the structs in the order in which it will be
    accessed, so fetching one item will load the next item(s) into cache.
*/

typedef struct {
    unsigned char step_type;	/* stepping type - see list above */
    unsigned char need_step;	/* non-zero if we need to step */
    unsigned int setup_timer;	/* timer for dir setup time */
    unsigned int hold_timer;	/* timer for dir hold time */
    unsigned int space_timer;	/* timer for pulse spacing */
    unsigned int len_timer;	/* timer for pulse length */
    hal_u32_t dir_setup;	/* parameter: direction setup time */
    hal_u32_t dir_hold;		/* parameter: direction hold time */
    hal_u32_t step_len;		/* parameter: step pulse length */
    hal_u32_t step_space;	/* parameter: min step pulse spacing */
} st0_t;

typedef struct {
    unsigned char step_type;	/* stepping type - see list above */
    unsigned char state;	/* current position in state table */
    unsigned char cycle_max;	/* cycle length for step types 2 and up */
    unsigned char num_phases;	/* number of phases for types 2 and up */
    const unsigned char *lut;		/* pointer to lookup table */
} st2_t;

typedef struct {
    signed long deltalim;	/* max allowed change per period */
    signed long newaddval;	/* desired freq generator add value */
    signed long addval;		/* actual frequency generator add value */
    unsigned long accum;	/* frequency generator accumulator */
    union {
	st0_t st0;		/* working data for step type 0 */
	st2_t st2;		/* working data for step types 2 and up */
    } wd;
    hal_bit_t *phase[5];	/* pins for output signals */
    hal_s32_t rawcount;		/* param: current position (feedback) */
    hal_s32_t *count;		/* captured binary count value */
    hal_float_t pos_scale;	/* parameter: scaling factor for pos */
    double old_scale;		/* stored scale value */
    double scale_recip;		/* reciprocal value used for scaling */
    hal_float_t *pos;		/* scaled position (floating point) */
    hal_float_t *vel;		/* velocity command */
    hal_float_t vel_scale;	/* parameter: scaling for vel to Hz */
    hal_float_t maxfreq;	/* parameter: max frequency in Hz */
    hal_float_t freq;		/* parameter: velocity cmd scaled to Hz */
    hal_float_t maxaccel;	/* parameter: max accel rate in Hz/sec */
    int printed_error;          /* Has the error message been logged? */
} freqgen_t;

/* ptr to array of freqgen_t structs in shared memory, 1 per channel */
static freqgen_t *freqgen_array;

/* lookup tables for stepping types 2 and higher - phase A is the LSB */

static const unsigned char master_lut[][10] = {
    {1, 3, 2, 0, 0, 0, 0, 0, 0, 0},	/* Quadrature */
    {1, 2, 4, 0, 0, 0, 0, 0, 0, 0},	/* Three Wire */
    {1, 3, 2, 6, 4, 5, 0, 0, 0, 0},	/* Three Wire Half Step */
    {1, 2, 4, 8, 0, 0, 0, 0, 0, 0},	/* Unipolar Full Step 1 */
    {3, 6, 12, 9, 0, 0, 0, 0, 0, 0},	/* Unipoler Full Step 2 */
    {1, 7, 14, 8, 0, 0, 0, 0, 0, 0},	/* Bipolar Full Step 1 */
    {5, 6, 10, 9, 0, 0, 0, 0, 0, 0},	/* Bipoler Full Step 2 */
    {1, 3, 2, 6, 4, 12, 8, 9, 0, 0},	/* Unipolar Half Step */
    {1, 5, 7, 6, 14, 10, 8, 9, 0, 0},	/* Bipolar Half Step */
    {1, 2, 4, 8, 16, 0, 0, 0, 0, 0},	/* Five Wire Unipolar */
    {3, 6, 12, 24, 17, 0, 0, 0, 0, 0},	/* Five Wire Wave */
    {1, 3, 2, 6, 4, 12, 8, 24, 16, 17},	/* Five Wire Uni Half */
    {3, 7, 6, 14, 12, 28, 24, 25, 17, 19}	/* Five Wire Wave Half */
};

static const unsigned char cycle_len_lut[] =
    { 4, 3, 6, 4, 4, 4, 4, 8, 8, 5, 5, 10, 10 };

static const unsigned char num_phases_lut[] =
    { 2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, };

#define MAX_STEP_TYPE 12

#define STEP_PIN	0	/* output phase used for STEP signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */
#define COUNT_PIN	2	/* output phase used for COUNT signal */

/* other globals */
static int comp_id;		/* component ID */
static int num_chan;		/* number of step generators configured */
static long periodns;		/* makepulses function period in nanosec */
static long old_periodns;	/* used to detect changes in periodns */
static double periodfp;		/* makepulses function period in seconds */
static double maxf;		/* maximum frequency, step types 1 & up */
static double freqscale;	/* conv. factor from Hz to addval counts */
static double accelscale;	/* conv. Hz/sec to addval cnts/period */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_freqgen(int num, freqgen_t * addr, int steptype);
static void make_pulses(void *arg, long period);
static void update_freq(void *arg, long period);
static void update_pos(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval;
    rtapi_print_msg(RTAPI_MSG_ERR,
        "FREQGEN: freqgen is deprecated and will be removed in emc2.4.  "
        "Use stepgen with ctrl_type=v instead\n");
     for (n = 0; n < MAX_CHAN && step_type[n] != -1 ; n++) {
	if ((step_type[n] > MAX_STEP_TYPE) || (step_type[n] < 0)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "FREQGEN: ERROR: bad stepping type '%i', axis %i\n",
			    step_type[n], n);
	    return -1;
	} else {
	    num_chan++;
	}
    }
    if (num_chan == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "FREQGEN: ERROR: no channels configured\n");
	return -1;
    }
    /* periodns will be set to the proper value when 'make_pulses()' runs for
       the first time.  We load a default value here to avoid glitches at
       startup, but all these 'constants' are recomputed inside
       'update_freq()' using the real period. */
    periodns = 50000;
    /* precompute some constants */
    periodfp = periodns * 0.000000001;
    maxf = 1.0 / periodfp;
    freqscale = ((1L << 30) * 2.0) / maxf;
    accelscale = freqscale * periodfp;
    old_periodns = periodns;
    /* have good config info, connect to the HAL */
    comp_id = hal_init("freqgen");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "FREQGEN: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    freqgen_array = hal_malloc(num_chan * sizeof(freqgen_t));
    if (freqgen_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "FREQGEN: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each pulse generator */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_freqgen(n, &(freqgen_array[n]), step_type[n]);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"FREQGEN: ERROR: freqgen %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions */
    retval = hal_export_funct("freqgen.make-pulses", make_pulses,
	freqgen_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "FREQGEN: ERROR: makepulses funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("freqgen.update-freq", update_freq,
	freqgen_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "FREQGEN: ERROR: freq update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("freqgen.capture-position", update_pos,
	freqgen_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "FREQGEN: ERROR: pos update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"FREQGEN: installed %d step pulse generators\n", num_chan);
    hal_ready(comp_id);
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
    to frequency to an accumulator.  When the accumulator overflows (or
    underflows), it is time to generate an up (or down) output pulse.
    The add value is limited to +/-2^30, and overflows are detected
    at bit 30, not bit 31.  This means that with add_val at its max
    (or min) value, and overflow (or underflow) occurs on every cycle.
    NOTE:  step/dir outputs cannot generate a step every cycle.  The
    maximum steprate is determined by the timing parameters.  The
    time between pulses must be at least (StepLen + StepSpace) cycles,
    and the time between direction changes must be at least (DirSetup +
    StepLen + DirHold) cycles.

*/

static void make_pulses(void *arg, long period)
{
    freqgen_t *freqgen;
    int n, p;
    unsigned char tmp_step, tmp_dir, state;

    /* store period so scaling constants can be (re)calculated */
    periodns = period;
    /* point to freqgen data structures */
    freqgen = arg;
    for (n = 0; n < num_chan; n++) {
	if (freqgen->deltalim != 0) {
	    /* implement accel/decel limit */
	    if (freqgen->newaddval > (freqgen->addval + freqgen->deltalim)) {
		/* new value is too high, increase addval as far as possible */
		freqgen->addval += freqgen->deltalim;
	    } else if (freqgen->newaddval <
		(freqgen->addval - freqgen->deltalim)) {
		/* new value is too low, decrease addval as far as possible */
		freqgen->addval -= freqgen->deltalim;
	    } else {
		/* new value can be reached in one step - do it */
		freqgen->addval = freqgen->newaddval;
	    }
	} else {
	    /* go to new freq without any ramping */
	    freqgen->addval = freqgen->newaddval;
	}
	/* get current value of bit 31 */
	tmp_step = freqgen->accum >> 31;
	/* update the accumulator */
	freqgen->accum += freqgen->addval;
	/* test for overflow/underflow (change in bit 31) */
	tmp_step ^= freqgen->accum >> 31;
	/* get direction bit, 1 if negative, 0 if positive */
	tmp_dir = freqgen->addval >> 31;

	/* generate output, based on stepping type */
	if (freqgen->wd.st0.step_type == 0) {
	    /* step/dir output, using working data in freqgen->wd.st0.xxx */
	    freqgen->wd.st0.need_step |= tmp_step;
	    /* update timers */
	    if (freqgen->wd.st0.setup_timer) {
		freqgen->wd.st0.setup_timer--;
	    }
	    if (freqgen->wd.st0.hold_timer) {
		freqgen->wd.st0.hold_timer--;
	    }
	    if (freqgen->wd.st0.space_timer) {
		freqgen->wd.st0.space_timer--;
	    }
	    if (freqgen->wd.st0.len_timer) {
		if (--freqgen->wd.st0.len_timer == 0) {
		    /* terminate step pulse */
		    *(freqgen->phase[STEP_PIN]) = 0;
		    /* begin timing pulse space */
		    freqgen->wd.st0.space_timer = freqgen->wd.st0.step_space;
		}
	    }
	    /* is direction OK? */
	    if (tmp_dir != *(freqgen->phase[DIR_PIN])) {
		/* need to change direction bit, can we? */
		if ((freqgen->wd.st0.hold_timer == 0) &&
		    (freqgen->wd.st0.setup_timer == 0)) {
		    /* set direction output */
		    *(freqgen->phase[DIR_PIN]) = tmp_dir;
		    /* start setup timer */
		    freqgen->wd.st0.setup_timer = freqgen->wd.st0.dir_setup;
		}
	    }
	    /* do we need to step? */
	    if (freqgen->wd.st0.need_step) {
		/* yes, can we? */
		if ((freqgen->wd.st0.space_timer == 0) &&
		    (freqgen->wd.st0.setup_timer == 0) &&
		    (*(freqgen->phase[DIR_PIN]) == tmp_dir)) {
		    /* yes, start step pulse */
		    *(freqgen->phase[STEP_PIN]) = 1;
		    /* start pulse length and dir hold timers */
		    freqgen->wd.st0.len_timer = freqgen->wd.st0.step_len;
		    freqgen->wd.st0.hold_timer =
			freqgen->wd.st0.step_len + freqgen->wd.st0.dir_hold;
		    /* don't need a step any more */
		    freqgen->wd.st0.need_step = 0;
		    /* count the step */
		    if (tmp_dir) {
			freqgen->rawcount--;
		    } else {
			freqgen->rawcount++;
		    }
		}
	    }
	} else if (freqgen->wd.st0.step_type == 1) {
	    /* pesudo-PWM */
	    *(freqgen->phase[UP_PIN]) = tmp_step & ~tmp_dir;
	    *(freqgen->phase[DOWN_PIN]) = tmp_step & tmp_dir;
	    *(freqgen->phase[COUNT_PIN]) = tmp_step;
	    /* count the step for feedback */
	    if (tmp_step) {
		if (tmp_dir) {
		    freqgen->rawcount--;
		} else {
		    freqgen->rawcount++;
		}
	    }
	} else {
	    /* step type 2 or greater */
	    /* update step cycle counter */
	    if (tmp_step) {
		if (tmp_dir) {
		    if (freqgen->wd.st2.state-- == 0) {
			freqgen->wd.st2.state = freqgen->wd.st2.cycle_max;
		    }
		    /* count the step for feedback */
		    freqgen->rawcount--;
		} else {
		    if (++freqgen->wd.st2.state > freqgen->wd.st2.cycle_max) {
			freqgen->wd.st2.state = 0;
		    }
		    /* count the step for feedback */
		    freqgen->rawcount++;
		}
		/* look up correct output pattern */
		state = (freqgen->wd.st2.lut)[freqgen->wd.st2.state];
		/* now output the phase bits */
		for (p = 0; p < freqgen->wd.st2.num_phases; p++) {
		    /* output one phase */
		    *(freqgen->phase[p]) = state & 1;
		    /* move to the next phase */
		    state >>= 1;
		}
	    }
	}
	/* move on to next step generator */
	freqgen++;
    }
    /* done */
}

static void update_freq(void *arg, long period)
{
    freqgen_t *freqgen;
    int n;
    double tmpf, limf;

    /* this periodns stuff is a little convoluted because we need to
       calculate some constants here in this relatively slow thread but the
       constants are based on the period of the much faster 'make_pulses()'
       thread. */
    if (periodns != old_periodns) {
	/* recompute various constants that depend on periodns */
	periodfp = periodns * 0.000000001;
	maxf = 1.0 / periodfp;
	freqscale = ((1L << 30) * 2.0) / maxf;
	accelscale = freqscale * periodfp;
	old_periodns = periodns;
    }
    /* update the step generators */
    freqgen = arg;
    for (n = 0; n < num_chan; n++) {
	/* calculate frequency limit */
	if (freqgen->wd.st0.step_type == 0) {
	    /* stepping type 0 limit depends on timing params that may change 
	     */
	    limf =
		maxf / (freqgen->wd.st0.step_len +
		freqgen->wd.st0.step_space);
	} else {
	    limf = maxf;
	}
	/* check for user specified frequency limit parameter */
	if (freqgen->maxfreq <= 0.0) {
	    /* set to zero if negative */
	    freqgen->maxfreq = 0.0;
	} else {
	    /* parameter is non-zero, compare to limf */
	    if (freqgen->maxfreq > limf) {
                 if(!freqgen->printed_error) {
                     rtapi_print_msg(RTAPI_MSG_ERR,
                         "FREQGEN: Channel %d: The requested maximum velocity of %d steps per second is not attainable.\n",
                         n, (int) freqgen->maxfreq);
                     rtapi_print_msg(RTAPI_MSG_ERR,
                         "FREQGEN: The maximum number of steps possible is %d per second\n",
                             (int) limf);
                     freqgen->printed_error = 1;
                 }
		/* parameter is too high, lower it */
		freqgen->maxfreq = limf;
	    } else {
		/* lower limit to match parameter */
		limf = freqgen->maxfreq;
	    }
	}
	/* convert velocity command to Hz */
	tmpf = *(freqgen->vel) * freqgen->vel_scale;
	/* limit the commanded frequency */
	if (tmpf > limf) {
	    tmpf = limf;
	} else if (tmpf < -limf) {
	    tmpf = -limf;
	}
	/* save limited frequency */
	freqgen->freq = tmpf;
	/* calculate new addval */
	freqgen->newaddval = tmpf * freqscale;
	/* check for illegal (negative) maxaccel parameter */
	if (freqgen->maxaccel <= 0.0) {
	    /* set to zero if negative */
	    freqgen->maxaccel = 0.0;
	}
	/* calculate new deltalim */
	freqgen->deltalim = freqgen->maxaccel * accelscale;
	/* move on to next channel */
	freqgen++;
    }
    /* done */
}

static void update_pos(void *arg, long period)
{
    freqgen_t *freqgen;
    int n;

    freqgen = arg;
    for (n = 0; n < num_chan; n++) {
	/* capture raw counts to latches */
	*(freqgen->count) = freqgen->rawcount;
	/* check for change in scale value */
	if ( freqgen->pos_scale != freqgen->old_scale ) {
	    /* get ready to detect future scale changes */
	    freqgen->old_scale = freqgen->pos_scale;
	    /* validate the new scale value */
	    if ((freqgen->pos_scale < 1e-20) && (freqgen->pos_scale > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		freqgen->pos_scale = 1.0;
	    }
	    /* we will need the reciprocal */
	    freqgen->scale_recip = 1.0 / freqgen->pos_scale;
	}
	/* scale count to make floating point position */
	*(freqgen->pos) = *(freqgen->count) * freqgen->scale_recip;
	/* move on to next channel */
	freqgen++;
    }
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_freqgen(int num, freqgen_t * addr, int step_type)
{
    int n, retval, msg;
    char buf[HAL_NAME_LEN + 2];

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export param variable for raw counts */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.rawcounts", num);
    retval = hal_param_s32_new(buf, HAL_RO, &(addr->rawcount), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_OUT, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.position-fb", num);
    retval = hal_pin_float_new(buf, HAL_OUT, &(addr->pos), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for position scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for frequency command */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.velocity", num);
    retval = hal_pin_float_new(buf, HAL_IN, &(addr->vel), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for frequency scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.velocity-scale", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->vel_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for max frequency */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.maxfreq", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->maxfreq), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export param for scaled velocity (frequency in Hz) */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.frequency", num);
    retval = hal_param_float_new(buf, HAL_RO, &(addr->freq), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for max accel/decel */
    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.maxaccel", num);
    retval = hal_param_float_new(buf, HAL_RW, &(addr->maxaccel), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* set default parameter values */
    addr->pos_scale = 1.0;
    addr->vel_scale = 1.0;
    /* set maxfreq very high - let update_freq() fix it */
    addr->maxfreq = 1e15;
    addr->maxaccel = 0.0;
    addr->wd.st0.step_type = step_type;
    /* init the step generator core to zero output */
    addr->accum = 0;
    addr->addval = 0;
    addr->newaddval = 0;
    addr->deltalim = 0;
    addr->rawcount = 0;
    addr->printed_error = 0;
    if (step_type == 0) {
	/* setup for stepping type 0 - step/dir */
	addr->wd.st0.need_step = 0;
	addr->wd.st0.setup_timer = 0;
	addr->wd.st0.hold_timer = 0;
	addr->wd.st0.space_timer = 0;
	addr->wd.st0.len_timer = 0;
	/* export parameters for step/dir pulse timing */
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.dirsetup", num);
	retval =
	    hal_param_u32_new(buf, HAL_RW, &(addr->wd.st0.dir_setup), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.dirhold", num);
	retval =
	    hal_param_u32_new(buf, HAL_RW, &(addr->wd.st0.dir_hold), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.steplen", num);
	retval =
	    hal_param_u32_new(buf, HAL_RW, &(addr->wd.st0.step_len), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.stepspace", num);
	retval =
	    hal_param_u32_new(buf, HAL_RW, &(addr->wd.st0.step_space),
	    comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* init the parameters */
	addr->wd.st0.dir_setup = 1;
	addr->wd.st0.dir_hold = 1;
	addr->wd.st0.step_len = 1;
	addr->wd.st0.step_space = 1;
	/* export pins for step and direction */
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.step", num);
	retval =
	    hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[STEP_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[STEP_PIN]) = 0;
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.dir", num);
	retval =
	    hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[DIR_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[DIR_PIN]) = 0;
    } else if (step_type == 1) {
	/* setup for stepping type 1 - pseudo-PWM */
	/* export pins for up and down */
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.up", num);
	retval =
	    hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[UP_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[UP_PIN]) = 0;
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.down", num);
	retval =
	    hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[DOWN_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[DOWN_PIN]) = 0;
	rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.count", num);
	retval =
	    hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[COUNT_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[COUNT_PIN]) = 0;
    } else {
	/* setup for stepping types 2 and higher */
	addr->wd.st2.state = 0;
	addr->wd.st2.cycle_max = cycle_len_lut[step_type - 2] - 1;
	addr->wd.st2.num_phases = num_phases_lut[step_type - 2];
	addr->wd.st2.lut = &(master_lut[step_type - 2][0]);
	/* export pins for output phases */
	for (n = 0; n < addr->wd.st2.num_phases; n++) {
	    rtapi_snprintf(buf, HAL_NAME_LEN, "freqgen.%d.phase-%c", num,
		n + 'A');
	    retval = hal_pin_bit_new(buf, HAL_OUT, &(addr->phase[n]), comp_id);
	    if (retval != 0) {
		return retval;
	    }
	    *(addr->phase[n]) = 0;
	}
    }
    /* set initial pin values */
    *(addr->count) = 0;
    *(addr->pos) = 0.0;
    *(addr->vel) = 0.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
