/********************************************************************
* Description:  encoder.c
*               This file, 'encoder.c', is a HAL component that
*               provides software based counting of quadrature
*               encoder signals.
*
* Author: John Kasunich
* License: GPL Version 2
*
* Copyright (c) 2003 All rights reserved.
*
* Last change:
********************************************************************/
/** This file, 'encoder.c', is a HAL component that provides software
    based counting of quadrature encoder signals.  The maximum count
    rate will depend on the speed of the PC, but is expected to exceed
    1KHz for even the slowest computers, and may reach 10KHz on fast
    ones.  It is a realtime component.

    It supports up to eight counters, with optional index pulses.
    The number of counters is set by the module parameter 'num_chan='
    when the component is insmod'ed.  Alternatively, use the
    names= specifier and a list of unique names separated by commas.
    The names= and num_chan= specifiers are mutually exclusive.

    The driver exports variables for each counters inputs and output.
    It also exports two functions.  "encoder.update-counters" must be
    called in a high speed thread, at least twice the maximum desired
    count rate.  "encoder.capture-position" can be called at a much
    slower rate, and updates the output variables.
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
#include "rtapi_atomics.h"
#include "rtapi_string.h"
#include "hal.h"		/* HAL public API decls */

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Encoder Counter for EMC HAL");
MODULE_LICENSE("GPL");

static int num_chan;
static int default_num_chan=3;
static int howmany;
RTAPI_MP_INT(num_chan, "number of encoder channels");

#define MAX_CHAN 8
char *names[MAX_CHAN] = {0,};
RTAPI_MP_ARRAY_STRING(names, MAX_CHAN, "names of encoder");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* data that is atomically passed from fast function to slow one */

typedef struct {
    // event flags: set by update, cleared by capture
    char count_detected;  // u:s c:rc
    char index_detected;  // u:s c:rc
    char latch_detected;  // u:s c:rc

    // event params: set by update, read by capture
    __s32 raw_count;      // u:s c:r
    __u32 timestamp;      // u:s c:r, applies only to count_detected/raw_count
    __s32 index_count;    // u:s c:r
    __s32 latch_count;    // u:s c:r
} atomic;

/* this structure contains the runtime data for a single counter
   u:rw means update() reads and writes the
   c:w  means capture() writes the field
   c:s u:rc means capture() sets (to 1), update() reads and clears
*/

typedef struct {

    // fast, no fp
    struct update_state {
	hal_bit_t *x4_mode;		/* u:r enables x4 counting (default) */
	hal_bit_t *counter_mode;	/* u:r enables counter mode */
	hal_bit_t *phaseA;		/* u:r quadrature input */
	hal_bit_t *phaseB;		/* u:r quadrature input */
	hal_bit_t *phaseZ;		/* u:r index pulse input */
	hal_bit_t *latch_rising;    /* u:r latch on rising edge? */
	hal_bit_t *latch_falling;   /* u:r latch on falling edge? */
	hal_bit_t *latch_in;        /* u:r counter latch input */
	hal_bit_t old_latch;        /* value of latch on previous cycle */

	unsigned char state;	/* u:rw quad decode state machine state */
	unsigned char oldZ;		/* u:rw previous value of phase Z */
    } upd;

    // slow, fp
    struct capture_state {
	hal_bit_t *index_ena;	/* c:rw index enable input */
	hal_bit_t *reset;	/* c:r counter reset input */
	__s32 raw_count;		/* c:rw captured raw_count */
	__u32 timestamp;		/* c:rw captured timestamp */
	__s32 index_count;		/* c:rw captured index count */
	__s32 latch_count;		/* c:rw captured index count */
	hal_s32_t *count;		/* c:w captured binary count value */
	hal_s32_t *count_latch;     /* c:w captured binary count value */
	hal_float_t *min_speed;     /* c:r minimum velocity to estimate nonzero */
	hal_float_t *pos;		/* c:w scaled position (floating point) */
	hal_float_t *pos_interp;	/* c:w scaled and interpolated position (float) */
	hal_float_t *pos_latch;     /* c:w scaled latched position (floating point) */
	hal_float_t *vel;		/* c:w scaled velocity (floating point) */
	hal_float_t *pos_scale;	/* c:r pin: scaling factor for pos */
	double old_scale;		/* c:rw stored scale value */
	double scale;		/* c:rw reciprocal value used for scaling */
	int counts_since_timeout;	/* c:rw used for velocity calcs */
    } capt;

    struct shared_state {
	hal_s32_t *raw_counts;	/* u:rw c:r raw count value */
	unsigned char Zmask;	/* u:rc c:s mask for oldZ, from index-ena */
	atomic buf[2];		/* u:w c:r double buffer for atomic data */
	atomic *bp;	        /* u:r c:w ptr to in-use buffer */
    } shared;
} counter_t;

static __u32 timebase;		/* master timestamp for all counters */

/* pointer to array of counter_t structs in shmem, 1 per counter */
static counter_t *counter_array;

/* bitmasks for quadrature decode state machine */
#define SM_PHASE_A_MASK 0x01
#define SM_PHASE_B_MASK 0x02
#define SM_LOOKUP_MASK  0x0F
#define SM_CNT_UP_MASK  0x40
#define SM_CNT_DN_MASK  0x80

/* Lookup table for quadrature decode state machine.  This machine
   will reject glitches on either input (will count up 1 on glitch,
   down 1 after glitch), and on both inputs simultaneously (no count
   at all)  In theory, it can count once per cycle, in practice the
   maximum count rate should be at _least_ 10% below the sample rate,
   and preferrable around half the sample rate.  It counts every
   edge of the quadrature waveform, 4 counts per complete cycle.
*/
static const unsigned char lut_x4[16] = {
    0x00, 0x44, 0x88, 0x0C, 0x80, 0x04, 0x08, 0x4C,
    0x40, 0x04, 0x08, 0x8C, 0x00, 0x84, 0x48, 0x0C
};

/* same thing, but counts only once per complete cycle */

static const unsigned char lut_x1[16] = {
    0x00, 0x44, 0x08, 0x0C, 0x80, 0x04, 0x08, 0x0C,
    0x00, 0x04, 0x08, 0x0C, 0x00, 0x04, 0x08, 0x0C
};

/* look-up table for a one-wire counter */

static const unsigned char lut_ctr[16] = {
   0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_encoder(counter_t * addr,char * prefix);
static void update(void *arg, long period);
static void capture(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, retval, i;
    counter_t *cntr;

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
            if (names[i] == NULL) {
                break;
            }
            howmany = i + 1;
        }
    }

    /* test for number of channels */
    if ((howmany <= 0) || (howmany > MAX_CHAN)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: invalid number of channels: %d\n", howmany);
	return -1;
    }
    /* have good config info, connect to the HAL */
    comp_id = hal_init("encoder");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "ENCODER: ERROR: hal_init() failed\n");
	return -1;
    }
    /* allocate shared memory for counter data */
    counter_array = hal_malloc(howmany * sizeof(counter_t));
    if (counter_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }
    /* init master timestamp counter */
    timebase = 0;
    /* export all the variables for each counter */
    i = 0; // for names= items
    for (n = 0; n < howmany; n++) {
	/* point to struct */
	cntr = &(counter_array[n]);
	/* export all vars */
        if(num_chan) {
            char buf[HAL_NAME_LEN + 1];
            rtapi_snprintf(buf, sizeof(buf), "encoder.%d", n);
	    retval = export_encoder(cntr,buf);
        } else {
	    retval = export_encoder(cntr,names[i++]);
        }
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"ENCODER: ERROR: counter %d var export failed\n", n);
	    hal_exit(comp_id);
	    return -1;
	}

	struct shared_state *shared = &cntr->shared;
	shared->Zmask = 0;
	shared->buf[0].count_detected = 0;
	shared->buf[1].count_detected = 0;
	shared->buf[0].index_detected = 0;
	shared->buf[1].index_detected = 0;
	shared->bp = &(shared->buf[0]);
	*(shared->raw_counts) = 0;

	struct update_state *upd = &cntr->upd;
	upd->state = 0;
	upd->oldZ = 0;
	*(upd->x4_mode) = 1;
	*(upd->counter_mode) = 0;
	*(upd->latch_rising) = 1;
	*(upd->latch_falling) = 1;

	struct capture_state *capt = &cntr->capt;
	capt->raw_count = 0;
	capt->timestamp = 0;
	capt->index_count = 0;
	capt->latch_count = 0;
	*(capt->count) = 0;
	*(capt->min_speed) = 1.0;
	*(capt->pos) = 0.0;
	*(capt->pos_interp) = 0.0;  // was missing
	*(capt->pos_latch) = 0.0;
	*(capt->vel) = 0.0;
	*(capt->pos_scale) = 1.0;
	capt->old_scale = 1.0;
	capt->scale = 1.0;
	capt->counts_since_timeout = 0;
    }
    /* export functions */
    retval = hal_export_funct("encoder.update-counters", update,
	counter_array, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: count funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = hal_export_funct("encoder.capture-position", capture,
	counter_array, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "ENCODER: ERROR: capture funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"ENCODER: installed %d encoder counters\n", howmany);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

// FAST NOFP
static void update(void *arg, long period)
{
    counter_t *cntr;
    atomic *buf;
    int n;
    unsigned char state;
    int latch, old_latch, rising, falling;

    cntr = arg;
    for (n = 0; n < howmany; n++) {

	struct update_state *upd = &cntr->upd;
	struct shared_state *shared = &cntr->shared;

	// atomically fetch pointer to current buffer
	buf = (atomic *) rtapi_load_ptr((void **)&shared->bp);

	/* get state machine current state */
	state = upd->state;

	/* add input bits to state code */
	if (*(upd->phaseA)) {
	    state |= SM_PHASE_A_MASK;
	}
	if (*(upd->phaseB)) {
	    state |= SM_PHASE_B_MASK;
	}
	/* look up new state */
	if ( *(upd->counter_mode) ) {
	    state = lut_ctr[state & (SM_LOOKUP_MASK & ~SM_PHASE_B_MASK)];
	} else if ( *(upd->x4_mode) ) {
	    state = lut_x4[state & SM_LOOKUP_MASK];
	} else {
	    state = lut_x1[state & SM_LOOKUP_MASK];
	}
	/* should we count? */
	if (state & SM_CNT_UP_MASK) {
	    (*shared->raw_counts)++;
	    buf->raw_count = *(shared->raw_counts);
	    buf->timestamp = timebase;
	    buf->count_detected = 1;
	} else if (state & SM_CNT_DN_MASK) {
	    (*shared->raw_counts)--;
	    buf->raw_count = *(shared->raw_counts);
	    buf->timestamp = timebase;
	    buf->count_detected = 1;
	}
	/* save state machine state */
	upd->state = state;
	/* get old phase Z state, make room for new bit value */
	state = upd->oldZ << 1;
	/* add new value of phase Z */
	if (*(upd->phaseZ)) {
	    state |= 1;
	}
	upd->oldZ = state & 3;
	/* test for index enabled and rising edge on phase Z */
	if ((state & shared->Zmask) == 1) {
	    /* capture counts, reset Zmask */
	    buf->index_count = *(shared->raw_counts);
	    buf->index_detected = 1;
	    shared->Zmask = 0;
	}
        /* test for latch enabled and desired edge on latch-in */
        latch = *(upd->latch_in), old_latch = upd->old_latch;

        rising = latch && !old_latch;
        falling = !latch && old_latch;

        if((rising && *(upd->latch_rising))
                || (falling && *(upd->latch_falling))) {
            buf->latch_detected = 1;
            buf->latch_count = *(shared->raw_counts);
        }
        upd->old_latch = latch;

	/* move on to next channel */
	cntr++;
    }
    /* increment main timestamp counter */
    timebase += period;
    /* done */
}

// SLOW FP
static void capture(void *arg, long period)
{
    counter_t *cntr;
    atomic *buf;
    int n;
    __s32 delta_counts;
    __u32 delta_time;
    double vel, interp;

    cntr = arg;
    for (n = 0; n < howmany; n++) {

	struct capture_state *capt = &cntr->capt;
	struct shared_state *shared = &cntr->shared;

	/* point to active buffer */
	buf = (atomic *) shared->bp;
	/* tell update() to use the other buffer */
	if ( buf == &(shared->buf[0]) ) {
	    rtapi_store_ptr((void **)&shared->bp, &(shared->buf[1]));
	} else {
	    rtapi_store_ptr((void **)&shared->bp, &(shared->buf[0]));
	}

	// force visibility of the cntr->bp change and any changes to its contents
	// from the previous invocation onto update()
	rtapi_smp_wmb();

	/* handle index */
	if ( buf->index_detected ) {
	    buf->index_detected = 0;
	    capt->index_count = buf->index_count;
	    *(capt->index_ena) = 0;
	}
        /* handle latch */
	if ( buf->latch_detected ) {
	    buf->latch_detected = 0;
	    capt->latch_count = buf->latch_count;
	}

	/* update Zmask based on index_ena */
	if (*(capt->index_ena)) {
	    shared->Zmask = 3;
	} else {
	    shared->Zmask = 0;
	}
	/* done interacting with update() */
	/* check for change in scale value */
	if ( *(capt->pos_scale) != capt->old_scale ) {
	    /* save new scale to detect future changes */
	    capt->old_scale = *(capt->pos_scale);
	    /* scale value has changed, test and update it */
	    if ((*(capt->pos_scale) < 1e-20) && (*(capt->pos_scale) > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		*(capt->pos_scale) = 1.0;
	    }
	    /* we actually want the reciprocal */
	    capt->scale = 1.0 / *(capt->pos_scale);
	}
        /* check for valid min_speed */
        if ( *(capt->min_speed) == 0 ) {
            *(capt->min_speed) = 1;
        }

	/* check reset input */
	if (*(capt->reset)) {
	    /* reset is active, reset the counter */
	    /* note: we NEVER reset raw_counts, that is always a
		running count of edges seen since startup.  The
		public "count" is the difference between raw_count
		and index_count, so it will become zero. */

	    capt->raw_count = *(shared->raw_counts);
	    capt->index_count = capt->raw_count;
	}
	/* process data from update() */
	if ( buf->count_detected ) {
	    /* one or more counts in the last period */
	    buf->count_detected = 0;
	    delta_counts = buf->raw_count - capt->raw_count;
	    delta_time = buf->timestamp - capt->timestamp;
	    capt->raw_count = buf->raw_count;
	    capt->timestamp = buf->timestamp;
	    if ( capt->counts_since_timeout < 2 ) {
		capt->counts_since_timeout++;
	    } else {
		vel = (delta_counts * capt->scale ) / (delta_time * 1e-9);
		*(capt->vel) = vel;
	    }
	} else {
	    /* no count */
	    if ( capt->counts_since_timeout ) {
		/* calc time since last count */
		delta_time = timebase - capt->timestamp;
		if ( delta_time < 1e9 / ( *(capt->min_speed) * capt->scale )) {
		    /* not to long, estimate vel if a count arrived now */
		    vel = ( capt->scale ) / (delta_time * 1e-9);
		    /* make vel positive, even if scale is negative */
		    if ( vel < 0.0 ) vel = -vel;
		    /* use lesser of estimate and previous value */
		    /* use sign of previous value, magnitude of estimate */
		    if ( vel < *(capt->vel) ) {
			*(capt->vel) = vel;
		    }
		    if ( -vel > *(capt->vel) ) {
			*(capt->vel) = -vel;
		    }
		} else {
		    /* its been a long time, stop estimating */
		    capt->counts_since_timeout = 0;
		    *(capt->vel) = 0;
		}
	    } else {
		/* we already stopped estimating */
		*(capt->vel) = 0;
	    }
	}
	/* compute net counts */
	*(capt->count) = capt->raw_count - capt->index_count;
        *(capt->count_latch) = capt->latch_count - capt->index_count;

	/* scale count to make floating point position */
	*(capt->pos) = *(capt->count) * capt->scale;
	*(capt->pos_latch) = *(capt->count_latch) * capt->scale;

	/* add interpolation value */
	delta_time = timebase - capt->timestamp;
	interp = *(capt->vel) * (delta_time * 1e-9);
	*(capt->pos_interp) = *(capt->pos) + interp;
	/* move on to next channel */
	cntr++;
    }
    /* done */
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_encoder(counter_t * addr,char * prefix)
{
    int retval, msg;

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* export pins for the quadrature inputs */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.phaseA), comp_id,
            "%s.phase-A", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.phaseB), comp_id,
            "%s.phase-B", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index input */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.phaseZ), comp_id,
            "%s.phase-Z", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index enable input */
    retval = hal_pin_bit_newf(HAL_IO, &(addr->capt.index_ena), comp_id,
            "%s.index-enable", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the reset input */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->capt.reset), comp_id,
            "%s.reset", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pins for position latching */
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.latch_in), comp_id,
            "%s.latch-input", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.latch_rising), comp_id,
            "%s.latch-rising", prefix);
    if (retval != 0) {
	return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(addr->upd.latch_falling), comp_id,
            "%s.latch-falling", prefix);
    if (retval != 0) {
	return retval;
    }

    /* export parameter for raw counts */
    retval = hal_pin_s32_newf(HAL_OUT, &(addr->shared.raw_counts), comp_id,
            "%s.rawcounts", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by capture() */
    retval = hal_pin_s32_newf(HAL_OUT, &(addr->capt.count), comp_id,
            "%s.counts", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts latched by capture() */
    retval = hal_pin_s32_newf(HAL_OUT, &(addr->capt.count_latch), comp_id,
            "%s.counts-latched", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for minimum speed estimated by capture() */
    retval = hal_pin_float_newf(HAL_IN, &(addr->capt.min_speed), comp_id,
            "%s.min-speed-estimate", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by capture() */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->capt.pos), comp_id,
            "%s.position", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled and interpolated position captured by capture() */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->capt.pos_interp), comp_id,
            "%s.position-interpolated", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for latched position captured by capture() */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->capt.pos_latch), comp_id,
            "%s.position-latched", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled velocity captured by capture() */
    retval = hal_pin_float_newf(HAL_OUT, &(addr->capt.vel), comp_id,
            "%s.velocity", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaling */
    retval = hal_pin_float_newf(HAL_IO, &(addr->capt.pos_scale), comp_id,
            "%s.position-scale", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for x4 mode */
    retval = hal_pin_bit_newf(HAL_IO, &(addr->upd.x4_mode), comp_id,
            "%s.x4-mode", prefix);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counter mode */
    retval = hal_pin_bit_newf(HAL_IO, &(addr->upd.counter_mode), comp_id,
            "%s.counter-mode", prefix);
    if (retval != 0) {
	return retval;
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}
