/** This file, 'stepgen.c', is a HAL component that provides software
    based step pulse generation.  The maximum step rate will depend
    on the speed of the PC, but is expected to exceed 1KHz for even
    the slowest computers, and may reach 10KHz on fast ones.  It is
    a realtime component.

    It supports up to 8 pulse generators.  Each generator can produce
    several types of outputs in addition to step/dir, including
    quadrature, half- and full-step unipolar and bipolar, three phase,
    and five phase.  A 32 bit feedback value is provided indicating
    the current position of the motor (assuming no lost steps).
    The number of counters and type of outputs is determined by a
    config string provided on the insmod command line.  The string
    consists of a series of decimal numbers.  Each number is the
    stepping type for one channel.  So a command line like this:
          insmod stepgen cfg="0 0 1 2"
    will install four step generators, two using stepping type 0,
    one using type 1, and one using type 2.

    The driver exports three functions.  'stepgen.make_pulses', is
    responsible for actually generating the step pulses.  It must
    be executed in a fast thread to reduce pulse jitter.  The other
    two functions are normally called from a much slower thread.
    'stepgen.update_freq' reads the frequency command and sets
    internal variables used by 'stepgen.make_pulses'.
    'stepgen.capture_position' captures and scales the current
    values of the position feedback counters.  Both 'update_freq' and
    'capture_position' use floating point, 'make_pulses' does not.

    The component can optionally create two threads, one that
    supports floating point and one that does not.  The 'period'
    parameter (at insmod time) determines the period of the non-FP
    thread, which is called 'stepgen.thread'.  The 'fp_period'
    parameter sets the period of the floating point thread, which
    is called 'stepgen.threadFP'

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
    by HAL parameters.  The parameters are (1): 'stepgen.n.dirsetup'
    minimum delay from a change on the DIR line to the beginning of
    a step pulse, (2): 'stepgen.n.steplen' length of the step pulse,
    (3): 'stepgen.n.stepspace', space between step pulses, and
    (4): 'stepgen.n.dirhold', minimum delay after step pulse before
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
    modify it under the terms of version 2.1 of the GNU General
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

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/ctype.h>	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#include <linux/types.h>
 /* FIX ME - Need an ugly hack here to get rid of compile warnings. */
#ifdef __attribute_used__
#undef __attribute_used__
#endif
#ifdef __attribute_pure__
#undef __attribute_pure__
#endif
#include <sys/cdefs.h>
#include <float.h>
#include <math.h>

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Step Pulse Generator for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
static char *cfg = "0 0 0";	/* config string, default = 3 x step/dir */
MODULE_PARM(cfg, "s");
MODULE_PARM_DESC(cfg, "config string");
static long period = 0;		/* non-FP thread period, default = none */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "non-FP thread period (nsecs)");
static long fp_period = 0;	/* FP thread period, default = none */
MODULE_PARM(fp_period, "l");
MODULE_PARM_DESC(fp_period, "floating point thread period (nsecs)");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** These structures contains the runtime data for a single generator.
    The 'st0_t' struct has data needed for stepping type 0 only, and
    'st2_t' has data needed for stepping types 2 and higher only.  A
    union is used so the two structs can share space in the main
    'stepgen_t' structure.  This keeps the frequently accessed parts
    of the main structure smaller, allowing them to occupy fewer cache
    lines.  This improves speed as well as conserving shared memory.
    Data is arranged in the structs in the order in which it will be
    accessed, so fetching one item will load the next item(s) into cache.
*/

typedef struct {
    unsigned char step_type;	/* stepping type - see list above */
    unsigned char need_step;	/* non-zero if we need to step */
    unsigned char setup_timer;	/* timer for dir setup time */
    unsigned char hold_timer;	/* timer for dir hold time */
    unsigned char space_timer;	/* timer for pulse spacing */
    unsigned char len_timer;	/* timer for pulse length */
    hal_u8_t dir_setup;		/* parameter: direction setup time */
    hal_u8_t dir_hold;		/* parameter: direction hold time */
    hal_u8_t step_len;		/* parameter: step pulse length */
    hal_u8_t step_space;	/* parameter: min step pulse spacing */
} st0_t;

typedef struct {
    unsigned char step_type;	/* stepping type - see list above */
    unsigned char state;	/* current position in state table */
    unsigned char cycle_max;	/* cycle length for step types 2 and up */
    unsigned char num_phases;	/* number of phases for types 2 and up */
    unsigned char *lut;		/* pointer to lookup table */
} st2_t;

typedef struct {
    signed long deltalim;	/* max allowed change per period */
    signed long newaddval;	/* desired freq generator add value */
    signed long addval;		/* actual frequency generator add value */
    unsigned long accum;	/* frequency generator accumulator */
    double old_pos_cmd;		/* previous position command (counts) */
    union {
	st0_t st0;		/* working data for step type 0 */
	st2_t st2;		/* working data for step types 2 and up */
    } wd;
    hal_bit_t *phase[5];	/* pins for output signals */
    hal_s32_t rawcount;		/* param: position feedback in counts */
    hal_s32_t *count;		/* pin: captured feedback in counts */
    hal_float_t pos_scale;	/* param: steps per position unit */
    float old_scale;		/* stored scale value */
    double scale_recip;		/* reciprocal value used for scaling */
    hal_float_t *pos_cmd;	/* pin: position command (position units) */
    hal_float_t *pos_fb;	/* pin: position feedback (position units) */
    hal_float_t pos_err;	/* param: position error, (steps) */
    hal_float_t vel_err;	/* param: velocity error (steps/sec) */
    hal_float_t freq;		/* param: frequency command */
    hal_float_t maxvel;		/* param: max velocity, (pos units/sec) */
    hal_float_t maxaccel;	/* param: max accel (pos units/sec^2) */
} stepgen_t;

/* ptr to array of stepgen_t structs in shared memory, 1 per channel */
static stepgen_t *stepgen_array;

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

#define MAX_STEP_TYPE 14

#define STEP_PIN	0	/* output phase used for STEP signal */
#define DIR_PIN		1	/* output phase used for DIR signal */
#define UP_PIN		0	/* output phase used for UP signal */
#define DOWN_PIN	1	/* output phase used for DOWN signal */

/* other globals */
static int comp_id;		/* component ID */
static int num_chan;		/* number of step generators configured */
static long periodns;		/* makepulses function period in nanosec */
static long old_periodns;	/* used to detect changes in periodns */
static float periodfp;		/* makepulses function period in seconds */
static float maxf;		/* maximum frequency, step types 1 & up */
static float freqscale;		/* conv. factor from Hz to addval counts */
static float accelscale;	/* conv. Hz/sec to addval cnts/period */
static long old_dtns;		/* update_freq funct period in nsec */
static float dt;		/* update_freq period in seconds */
static float recip_dt;		/* recprocal of period, avoids divides */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int parse_step_type(char *cp);
static int export_stepgen(int num, stepgen_t * addr, int steptype);
static void make_pulses(void *arg, long period);
static void update_freq(void *arg, long period);
static void update_pos(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_CHAN 8
#define MAX_TOK (MAX_CHAN)

int rtapi_app_main(void)
{
    char *cp;
    char *tokens[MAX_TOK];
    int step_type[MAX_CHAN];
    int n, retval;

    /* test for config string */
    if (cfg == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STEPGEN: ERROR: no config string\n");
	return -1;
    }
    /* point to config string */
    cp = cfg;
    /* break it into tokens */
    for (n = 0; n < MAX_TOK; n++) {
	/* strip leading whitespace */
	while ((*cp != '\0') && (isspace(*cp)))
	    cp++;
	/* mark beginning of token */
	tokens[n] = cp;
	/* find end of token */
	while ((*cp != '\0') && (!isspace(*cp)))
	    cp++;
	/* mark end of this token, prepare to search for next one */
	if (*cp != '\0') {
	    *cp = '\0';
	    cp++;
	}
    }
    /* mark all channels unused */
    for (n = 0; n < MAX_CHAN; n++) {
	step_type[n] = -1;
    }
    /* parse config string, results in step_type[] array */
    num_chan = 0;
    n = 0;
    while ((num_chan < MAX_CHAN) && (n < MAX_TOK)) {
	if (tokens[n][0] != '\0') {
	    /* something here, is it a valid stepping type? */
	    step_type[num_chan] = parse_step_type(tokens[n]);
	    if ((step_type[num_chan] < 0)
		|| (step_type[num_chan] > MAX_STEP_TYPE)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "STEPGEN: ERROR: bad stepping type '%s'\n", tokens[n]);
		return -1;
	    }
	    num_chan++;
	}
	n++;
    }
    /* OK, now we've parsed everything */
    if (num_chan == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STEPGEN: ERROR: no channels configured\n");
	return -1;
    }
    /* test for 'period' */
    if (period != 0) {
	if ((period < 1000) || (period > 500000)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STEPGEN: ERROR: invalid period: %d\n", period);
	    return -1;
	}
    }
    /* test for 'fp_period' */
    if (fp_period != 0) {
	if ((fp_period < (2 * period)) || (period > 50000000)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STEPGEN: ERROR: invalid fp_period: %d\n", period);
	    return -1;
	}
    }
    /* periodns will be set to the proper value when 'make_pulses()' runs for 
       the first time.  We load a default value here to avoid glitches at
       startup, but all these 'constants' are recomputed inside
       'update_freq()' using the real period. */
    old_periodns = periodns = 50000;
    old_dtns = 1000000;
    /* precompute some constants */
    periodfp = periodns * 0.000000001;
    maxf = 1.0 / periodfp;
    freqscale = ((1L << 30) * 2.0) / maxf;
    accelscale = freqscale * periodfp;
    dt = old_dtns * 0.000000001;
    recip_dt = 1.0 / dt;
    /* have good config info, connect to the HAL */
    comp_id = hal_init("stepgen");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "STEPGEN: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    stepgen_array = hal_malloc(num_chan * sizeof(stepgen_t));
    if (stepgen_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STEPGEN: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* export all the variables for each pulse generator */
    for (n = 0; n < num_chan; n++) {
	/* export all vars */
	retval = export_stepgen(n, &(stepgen_array[n]), step_type[n]);
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STEPGEN: ERROR: stepgen %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}
    }
    /* export functions */
    retval = hal_export_funct("stepgen.make_pulses", make_pulses,
	stepgen_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STEPGEN: ERROR: makepulses funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("stepgen.update_freq", update_freq,
	stepgen_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STEPGEN: ERROR: freq update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("stepgen.capture_position", update_pos,
	stepgen_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "STEPGEN: ERROR: pos update funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"STEPGEN: installed %d step pulse generators\n", num_chan);
    if (period > 0) {
	/* create a thread */
	retval = hal_create_thread("stepgen.thread", period, 0);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STEPGEN: ERROR: could not create non-FP thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"STEPGEN: created %d uS non-FP thread\n", period / 1000);
	}
    }
    if (fp_period > 0) {
	/* create a floating point thread */
	retval = hal_create_thread("stepgen.threadFP", fp_period, 1);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"STEPGEN: ERROR: could not create FP thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"STEPGEN: created %d uS FP thread\n", fp_period / 1000);
	}
    }
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
    at bit 30, not bit 31.  This means that with add_val at it's max
    (or min) value, an overflow (or underflow) occurs on every cycle.
    NOTE:  step/dir outputs cannot generate a step every cycle.  The
    maximum steprate is determined by the timing parameters.  The
    time between pulses must be at least (StepLen + StepSpace) cycles,
    and the time between direction changes must be at least (DirSetup +
    StepLen + DirHold) cycles.

*/

static void make_pulses(void *arg, long period)
{
    stepgen_t *stepgen;
    int n, p;
    unsigned char tmp_step, tmp_dir, state;

    /* store period so scaling constants can be (re)calculated */
    periodns = period;
    /* point to stepgen data structures */
    stepgen = arg;
    for (n = 0; n < num_chan; n++) {
	if (stepgen->deltalim != 0) {
	    /* implement accel/decel limit */
	    if (stepgen->newaddval > (stepgen->addval + stepgen->deltalim)) {
		/* new value is too high, increase addval as far as possible */
		stepgen->addval += stepgen->deltalim;
	    } else if (stepgen->newaddval <
		(stepgen->addval - stepgen->deltalim)) {
		/* new value is too low, decrease addval as far as possible */
		stepgen->addval -= stepgen->deltalim;
	    } else {
		/* new value can be reached in one step - do it */
		stepgen->addval = stepgen->newaddval;
	    }
	} else {
	    /* go to new freq without any ramping */
	    stepgen->addval = stepgen->newaddval;
	}
	/* get current value of bit 31 */
	tmp_step = stepgen->accum >> 31;
	/* update the accumulator */
	stepgen->accum += stepgen->addval;
	/* test for overflow/underflow (change in bit 31) */
	tmp_step ^= stepgen->accum >> 31;
	/* get direction bit, 1 if negative, 0 if positive */
	tmp_dir = stepgen->addval >> 31;

	/* generate output, based on stepping type */
	if (stepgen->wd.st0.step_type == 0) {
	    /* step/dir output, using working data in stepgen->wd.st0.xxx */
	    stepgen->wd.st0.need_step |= tmp_step;
	    /* update timers */
	    if (stepgen->wd.st0.setup_timer) {
		stepgen->wd.st0.setup_timer--;
	    }
	    if (stepgen->wd.st0.hold_timer) {
		stepgen->wd.st0.hold_timer--;
	    }
	    if (stepgen->wd.st0.space_timer) {
		stepgen->wd.st0.space_timer--;
	    }
	    if (stepgen->wd.st0.len_timer) {
		if (--stepgen->wd.st0.len_timer == 0) {
		    /* terminate step pulse */
		    *(stepgen->phase[STEP_PIN]) = 0;
		    /* begin timing pulse space */
		    stepgen->wd.st0.space_timer = stepgen->wd.st0.step_space;
		}
	    }
	    /* is direction OK? */
	    if (tmp_dir != *(stepgen->phase[DIR_PIN])) {
		/* need to change direction bit, can we? */
		if ((stepgen->wd.st0.hold_timer == 0) &&
		    (stepgen->wd.st0.setup_timer == 0)) {
		    /* set direction output */
		    *(stepgen->phase[DIR_PIN]) = tmp_dir;
		    /* start setup timer */
		    stepgen->wd.st0.setup_timer = stepgen->wd.st0.dir_setup;
		}
	    }
	    /* do we need to step? */
	    if (stepgen->wd.st0.need_step) {
		/* yes, can we? */
		if ((stepgen->wd.st0.space_timer == 0) &&
		    (stepgen->wd.st0.setup_timer == 0) &&
		    (*(stepgen->phase[DIR_PIN]) == tmp_dir)) {
		    /* yes, start step pulse */
		    *(stepgen->phase[STEP_PIN]) = 1;
		    /* start pulse length and dir hold timers */
		    stepgen->wd.st0.len_timer = stepgen->wd.st0.step_len;
		    stepgen->wd.st0.hold_timer =
			stepgen->wd.st0.step_len + stepgen->wd.st0.dir_hold;
		    /* don't need a step any more */
		    stepgen->wd.st0.need_step = 0;
		    /* count the step */
		    if (tmp_dir) {
			stepgen->rawcount--;
		    } else {
			stepgen->rawcount++;
		    }
		}
	    }
	} else if (stepgen->wd.st0.step_type == 1) {
	    /* pesudo-PWM */
	    *(stepgen->phase[UP_PIN]) = tmp_step & ~tmp_dir;
	    *(stepgen->phase[DOWN_PIN]) = tmp_step & tmp_dir;
	    /* count the step for feedback */
	    if (tmp_step) {
		if (tmp_dir) {
		    stepgen->rawcount--;
		} else {
		    stepgen->rawcount++;
		}
	    }
	} else {
	    /* step type 2 or greater */
	    /* update step cycle counter */
	    if (tmp_step) {
		if (tmp_dir) {
		    if (stepgen->wd.st2.state-- == 0) {
			stepgen->wd.st2.state = stepgen->wd.st2.cycle_max;
		    }
		    /* count the step for feedback */
		    stepgen->rawcount--;
		} else {
		    if (++stepgen->wd.st2.state > stepgen->wd.st2.cycle_max) {
			stepgen->wd.st2.state = 0;
		    }
		    /* count the step for feedback */
		    stepgen->rawcount++;
		}
		/* look up correct output pattern */
		state = (stepgen->wd.st2.lut)[stepgen->wd.st2.state];
		/* now output the phase bits */
		for (p = 0; p < stepgen->wd.st2.num_phases; p++) {
		    /* output one phase */
		    *(stepgen->phase[p]) = state & 1;
		    /* move to the next phase */
		    state >>= 1;
		}
	    }
	}
	/* move on to next step generator */
	stepgen++;
    }
    /* done */
}

static void update_freq(void *arg, long period)
{
    stepgen_t *stepgen;
    int n;
    double pos_cmd, vel_cmd, curr_pos, curr_vel, avg_v, max_freq, max_ac;
    double match_ac, match_time, est_out, est_cmd, est_err, dp, dv, new_vel;

    /* FIXME - while this code works just fine, there are a bunch of
       internal variables, many of which hold intermediate results that
       don't really need their own variables.  They are used either for
       clarity, or because that's how the code evolved.  This algorithm
       could use some cleanup and optimization. */
    /* this periodns stuff is a little convoluted because we need to
       calculate some constants here in this relatively slow thread but the
       constants are based on the period of the much faster 'make_pulses()'
       thread. */
    /* only recalc constants if period changes */
    if (periodns != old_periodns) {
	/* get ready to detect future period changes */
	old_periodns = periodns;
	/* recompute various constants that depend on periodns */
	periodfp = periodns * 0.000000001;
	maxf = 1.0 / periodfp;
	freqscale = ((1L << 30) * 2.0) / maxf;
	accelscale = freqscale * periodfp;
    }
    /* now recalc constants related to the period of this funct */
    /* only recalc constants if period changes */
    if (period != old_dtns) {
	/* get ready to detect future period changes */
	old_dtns = period;
	/* dT is the period of this thread, used for the position loop */
	dt = period * 0.000000001;
	/* calc the reciprocal once here, to avoid multiple divides later */
	recip_dt = 1.0 / dt;
    }
    /* point at stepgen data */
    stepgen = arg;
    /* loop thru generators */
    for (n = 0; n < num_chan; n++) {
	/* check for scale change */
	if ( stepgen->pos_scale != stepgen->old_scale ) {
	    /* get ready to detect future scale changes */
	    stepgen->old_scale = stepgen->pos_scale;
	    /* validate the new scale value */
	    if ((stepgen->pos_scale < 1e-20) && (stepgen->pos_scale > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		stepgen->pos_scale = 1.0;
	    }
	    /* we will need the reciprocal */
	    stepgen->scale_recip = 1.0 / stepgen->pos_scale;
	}
	/* calculate frequency limit */
	if (stepgen->wd.st0.step_type == 0) {
	    /* stepping type 0 limit depends on timing params */
	    max_freq =
		maxf / (stepgen->wd.st0.step_len +
		stepgen->wd.st0.step_space);
	} else if (stepgen->wd.st0.step_type == 1) {
	    /* stepping type 1 limit is thread freq / 2 */
	    max_freq = maxf * 0.5;
	} else {
	    /* all other step types can step at the thread frequency */
	    max_freq = maxf;
	}
	/* check for user specified frequency limit parameter */
	if (stepgen->maxvel <= 0.0) {
	    /* set to zero if negative */
	    stepgen->maxvel = 0.0;
	} else {
	    /* parameter is non-zero, compare to max_freq */
	    if ((stepgen->maxvel * stepgen->pos_scale) > max_freq) {
		/* parameter is too high, lower it */
		stepgen->maxvel = max_freq * stepgen->scale_recip;
	    } else {
		/* lower max_freq to match parameter */
		max_freq = stepgen->maxvel * stepgen->pos_scale;
	    }
	}
	/* set internal accel limit to its absolute max, which is
	   zero to full speed in one thread period */
	max_ac = max_freq * recip_dt;
	/* check for user specified accel limit parameter */
	if (stepgen->maxaccel <= 0.0) {
	    /* set to zero if negative */
	    stepgen->maxaccel = 0.0;
	} else {
	    /* parameter is non-zero, compare to max_ac */
	    if ((stepgen->maxaccel * stepgen->pos_scale) > max_ac) {
		/* parameter is too high, lower it */
		stepgen->maxaccel = max_ac * stepgen->scale_recip;
	    } else {
		/* lower limit to match parameter */
		max_ac = stepgen->maxaccel * stepgen->pos_scale;
	    }
	}
	/* calculate position command in counts */
	pos_cmd = *stepgen->pos_cmd * stepgen->pos_scale;
	/* calculate velocity command in counts/sec */
	vel_cmd = ( pos_cmd - stepgen->old_pos_cmd ) * recip_dt;
	stepgen->old_pos_cmd = pos_cmd;
	/* get current position and velocity in counts and counts/sec */
	curr_pos = stepgen->rawcount;
	curr_vel = stepgen->freq;
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
	    if (fabs(est_err) < 1.0) {
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
	    if ( fabs(est_err+dp*2.0) < fabs(est_err) ) {
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

	stepgen->freq = new_vel;
	/* calculate new addval */
	stepgen->newaddval = stepgen->freq * freqscale;
	/* calculate new deltalim */
	stepgen->deltalim = max_ac * accelscale;
	/* move on to next channel */
	stepgen++;
    }
    /* done */
}

static void update_pos(void *arg, long period)
{
    stepgen_t *stepgen;
    int n;

    stepgen = arg;
    for (n = 0; n < num_chan; n++) {
	/* capture raw counts to latches */
	*(stepgen->count) = stepgen->rawcount;
	/* check for change in scale value */
	if ( stepgen->pos_scale != stepgen->old_scale ) {
	    /* get ready to detect future scale changes */
	    stepgen->old_scale = stepgen->pos_scale;
	    /* validate the new scale value */
	    if ((stepgen->pos_scale < 1e-20) && (stepgen->pos_scale > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		stepgen->pos_scale = 1.0;
	    }
	    /* we will need the reciprocal */
	    stepgen->scale_recip = 1.0 / stepgen->pos_scale;
	}
	/* scale count to make floating point position */
	*(stepgen->pos_fb) = *(stepgen->count) * stepgen->scale_recip;
	/* move on to next channel */
	stepgen++;
    }
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int parse_step_type(char *cp)
{
    int result;

    if (*cp == '\0') {
	return -1;
    }
    /* initial value */
    result = 0;
    /* parse digits */
    while (*cp != '\0') {
	/* if char is a decimal digit, add it to result */
	if ((*cp >= '0') && (*cp <= '9')) {
	    result *= 10;
	    result += *cp - '0';
	} else {
	    /* not a valid digit */
	    return -1;
	}
	/* next char */
	cp++;
    }
    return result;
}

static int export_stepgen(int num, stepgen_t * addr, int step_type)
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
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.rawcounts", num);
    retval = hal_param_s32_new(buf, HAL_RD, &(addr->rawcount), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_WR, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for position scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.position-scale", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for position command command */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.position-cmd", num);
    retval = hal_pin_float_new(buf, HAL_RD, &(addr->pos_cmd), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.position-fb", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->pos_fb), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for position error */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.pos-err", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->pos_err), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for velocity error */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.vel-err", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->vel_err), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export param for scaled velocity (frequency in Hz) */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.frequency", num);
    retval = hal_param_float_new(buf, HAL_RD, &(addr->freq), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for max frequency */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.maxvel", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxvel), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for max accel/decel */
    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.maxaccel", num);
    retval = hal_param_float_new(buf, HAL_WR, &(addr->maxaccel), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* set default parameter values */
    addr->pos_scale = 1.0;
    addr->old_scale = 1.0;
    addr->scale_recip = 1.0;
    addr->freq = 0.0;
    addr->pos_err = 0.0;
    addr->vel_err = 0.0;
    addr->old_pos_cmd = 0.0;
    addr->maxvel = 0.0;
    addr->maxaccel = 0.0;
    addr->wd.st0.step_type = step_type;
    /* init the step generator core to zero output */
    addr->accum = 0;
    addr->addval = 0;
    addr->newaddval = 0;
    addr->deltalim = 0;
    addr->rawcount = 0;
    /* init the position controller */

    if (step_type == 0) {
	/* setup for stepping type 0 - step/dir */
	addr->wd.st0.need_step = 0;
	addr->wd.st0.setup_timer = 0;
	addr->wd.st0.hold_timer = 0;
	addr->wd.st0.space_timer = 0;
	addr->wd.st0.len_timer = 0;
	/* export parameters for step/dir pulse timing */
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.dirsetup", num);
	retval =
	    hal_param_u8_new(buf, HAL_WR, &(addr->wd.st0.dir_setup), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.dirhold", num);
	retval =
	    hal_param_u8_new(buf, HAL_WR, &(addr->wd.st0.dir_hold), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.steplen", num);
	retval =
	    hal_param_u8_new(buf, HAL_WR, &(addr->wd.st0.step_len), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.stepspace", num);
	retval =
	    hal_param_u8_new(buf, HAL_WR, &(addr->wd.st0.step_space),
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
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.step", num);
	retval =
	    hal_pin_bit_new(buf, HAL_WR, &(addr->phase[STEP_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[STEP_PIN]) = 0;
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.dir", num);
	retval =
	    hal_pin_bit_new(buf, HAL_WR, &(addr->phase[DIR_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[DIR_PIN]) = 0;
    } else if (step_type == 1) {
	/* setup for stepping type 1 - pseudo-PWM */
	/* export pins for up and down */
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.up", num);
	retval =
	    hal_pin_bit_new(buf, HAL_WR, &(addr->phase[UP_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[UP_PIN]) = 0;
	rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.down", num);
	retval =
	    hal_pin_bit_new(buf, HAL_WR, &(addr->phase[DOWN_PIN]), comp_id);
	if (retval != 0) {
	    return retval;
	}
	*(addr->phase[DOWN_PIN]) = 0;
    } else {
	/* setup for stepping types 2 and higher */
	addr->wd.st2.state = 0;
	addr->wd.st2.cycle_max = cycle_len_lut[step_type - 2] - 1;
	addr->wd.st2.num_phases = num_phases_lut[step_type - 2];
	addr->wd.st2.lut = (char *) (&(master_lut[step_type - 2][0]));
	/* export pins for output phases */
	for (n = 0; n < addr->wd.st2.num_phases; n++) {
	    rtapi_snprintf(buf, HAL_NAME_LEN, "stepgen.%d.phase-%c", num,
		n + 'A');
	    retval = hal_pin_bit_new(buf, HAL_WR, &(addr->phase[n]), comp_id);
	    if (retval != 0) {
		return retval;
	    }
	    *(addr->phase[n]) = 0;
	}
    }
    /* set initial pin values */
    *(addr->count) = 0;
    *(addr->pos_fb) = 0.0;
    *(addr->pos_cmd) = 0.0;
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
