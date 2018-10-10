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
#include "hal_priv.h"           // SHMPTR,SHMOFF
#include "hal_list.h"

#define VERBOSE_SETUP  // pin creation

/* module information */
MODULE_AUTHOR("John Kasunich, Michael Haberler");
MODULE_DESCRIPTION("Encoder Counter for EMC HAL");
MODULE_LICENSE("GPL");

RTAPI_TAG(HAL,HC_INSTANTIABLE);
RTAPI_TAG(HAL,HC_SMP_SAFE);

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
    hal_list_t  list;
    int inst_id;

    // fast, no fp
    struct update_state {
	bit_pin_ptr x4_mode;		/* u:r enables x4 counting (default) */
	bit_pin_ptr counter_mode;	/* u:r enables counter mode */
	bit_pin_ptr phaseA;		/* u:r quadrature input */
	bit_pin_ptr phaseB;		/* u:r quadrature input */
	bit_pin_ptr phaseZ;		/* u:r index pulse input */
	bit_pin_ptr latch_rising;    /* u:r latch on rising edge? */
	bit_pin_ptr latch_falling;   /* u:r latch on falling edge? */
	bit_pin_ptr latch_in;        /* u:r counter latch input */
	hal_bit_t old_latch;        /* value of latch on previous cycle */

	unsigned char state;	/* u:rw quad decode state machine state */
	unsigned char oldZ;		/* u:rw previous value of phase Z */
    } upd;

    // slow, fp
    struct capture_state {
	bit_pin_ptr index_ena;	/* c:rw index enable input */
	bit_pin_ptr reset;	/* c:r counter reset input */
	__s32 raw_count;		/* c:rw captured raw_count */
	__u32 timestamp;		/* c:rw captured timestamp */
	__s32 index_count;		/* c:rw captured index count */
	__s32 latch_count;		/* c:rw captured index count */
	s32_pin_ptr count;		/* c:w captured binary count value */
	s32_pin_ptr count_latch;     /* c:w captured binary count value */
	float_pin_ptr min_speed;     /* c:r minimum velocity to estimate nonzero */
	float_pin_ptr pos;		/* c:w scaled position (floating point) */
	float_pin_ptr pos_interp;	/* c:w scaled and interpolated position (float) */
	float_pin_ptr pos_latch;     /* c:w scaled latched position (floating point) */
	float_pin_ptr vel;		/* c:w scaled velocity (floating point) */
	float_pin_ptr pos_scale;	/* c:r pin: scaling factor for pos */
	double old_scale;		/* c:rw stored scale value */
	double scale;		/* c:rw reciprocal value used for scaling */
	int counts_since_timeout;	/* c:rw used for velocity calcs */
    } capt;

    struct shared_state {
	s32_pin_ptr raw_counts;	/* u:rw c:r raw count value */
	unsigned char Zmask;	/* u:rc c:s mask for oldZ, from index-ena */
	atomic buf[2];		/* u:w c:r double buffer for atomic data */
	atomic *bp;	        /* u:r c:w ptr to in-use buffer */
    } shared;
} counter_t;

static __u32 timebase;		/* u:rw c:r master timestamp for all counters */

// list of instance structures
static  hal_list_t head;

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

static const char *compname = "encoderv2";
static const char *prefix = "encoderv2";

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_encoder(const char *name, const int inst_id, counter_t *p);
static int update(void *arg, const hal_funct_args_t *fa);
static int capture(void *arg, const hal_funct_args_t *fa);
static int instantiate_encoder(const int argc, char* const *argv);
static int delete_encoder(const char *name, void *inst, const int inst_size);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/
int rtapi_app_main(void)
{
    int retval;
    dlist_init_entry(&head);

    if ((comp_id = hal_xinit(TYPE_RT, 0, 0,
			     instantiate_encoder,
			     delete_encoder,
			     compname)) < 0)
	return comp_id;

    hal_export_xfunct_args_t u = {
        .type = FS_XTHREADFUNC,
        .funct.x = capture,
        .arg = &head,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&u, "%s.capture-position",
				     prefix)) < 0)
	return retval;

    hal_export_xfunct_args_t mp = {
        .type = FS_XTHREADFUNC,
        .funct.x = update,
        .arg = &head,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&mp, "%s.update-counters",
				     prefix)) < 0)
	return retval;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static int instantiate_encoder(const int argc, char* const *argv)
{
    counter_t *p;
    int retval;
    int msg;
    const char* name;

    if(argc >= 2)
        name = argv[1];
    else
        HALFAIL_RC(EINVAL, "ERROR: insufficient args in argv");

    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later. */
    msg = rtapi_get_msg_level();

#ifndef  VERBOSE_SETUP
    rtapi_set_msg_level(RTAPI_MSG_WARN);
#endif

    if ((retval = hal_inst_create(name, comp_id, sizeof(counter_t), (void **)&p)) < 0)
	return retval;

    p->inst_id = retval;
    if ((retval = export_encoder(name, p->inst_id, p)) != 0)
	HALFAIL_RC(retval, "%s: ERROR: export(%s) failed", compname, name);

    // append to instance list
    dlist_init_entry(&p->list);
    dlist_add_after(&p->list, &head);

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int delete_encoder(const char *name, void *inst, const int inst_size)
{
    counter_t *p = inst;

    // delete from instance list
    dlist_remove_entry(&p->list);
    return 0;
}

/***********************************************************************
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/


static int update(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    counter_t *self;
    atomic *buf;
    unsigned char state;
    int latch, old_latch, rising, falling;

    // foreach encoder instance:
    dlist_for_each_entry(self, insts, list) {

	struct update_state *upd = &self->upd;
	struct shared_state *shared = &self->shared;

	// atomically fetch pointer to current buffer
	buf = (atomic *) rtapi_load_ptr((void **)&shared->bp);

	/* get state machine current state */
	state = upd->state;

	/* add input bits to state code */
	if (get_bit_pin(upd->phaseA)) {
	    state |= SM_PHASE_A_MASK;
	}
	if (get_bit_pin(upd->phaseB)) {
	    state |= SM_PHASE_B_MASK;
	}
	/* look up new state */
	if ( get_bit_pin(upd->counter_mode) ) {
	    state = lut_ctr[state & (SM_LOOKUP_MASK & ~SM_PHASE_B_MASK)];
	} else if ( get_bit_pin(upd->x4_mode) ) {
	    state = lut_x4[state & SM_LOOKUP_MASK];
	} else {
	    state = lut_x1[state & SM_LOOKUP_MASK];
	}
	/* should we count? */
	if (state & SM_CNT_UP_MASK) {
	    incr_s32_pin(shared->raw_counts, 1);
	    buf->raw_count = get_s32_pin(shared->raw_counts);
	    buf->timestamp = timebase;
	    buf->count_detected = 1;
	} else if (state & SM_CNT_DN_MASK) {
	    incr_s32_pin(shared->raw_counts, -1);
	    buf->raw_count = get_s32_pin(shared->raw_counts);
	    buf->timestamp = timebase;
	    buf->count_detected = 1;
	}
	/* save state machine state */
	upd->state = state;
	/* get old phase Z state, make room for new bit value */
	state = upd->oldZ << 1;
	/* add new value of phase Z */
	if (get_bit_pin(upd->phaseZ)) {
	    state |= 1;
	}
	upd->oldZ = state & 3;
	/* test for index enabled and rising edge on phase Z */
	if ((state & shared->Zmask) == 1) {
	    /* capture counts, reset Zmask */
	    buf->index_count = get_s32_pin(shared->raw_counts);
	    buf->index_detected = 1;
	    shared->Zmask = 0;
	}
        /* test for latch enabled and desired edge on latch-in */
        latch = get_bit_pin(upd->latch_in), old_latch = upd->old_latch;

        rising = latch && !old_latch;
        falling = !latch && old_latch;

        if((rising && get_bit_pin(upd->latch_rising))
                || (falling && get_bit_pin(upd->latch_falling))) {
            buf->latch_detected = 1;
            buf->latch_count = get_s32_pin(shared->raw_counts);
        }
        upd->old_latch = latch;
    }
    /* increment main timestamp counter */
    rtapi_add_u32(&timebase, fa_period(fa));
    return 0;
}

static int capture(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    counter_t *self;
    atomic *buf;
    __s32 delta_counts;
    __u32 delta_time;
    double vel, interp;
    hal_u32_t local_timebase;

    // foreach encoder instance:
    dlist_for_each_entry(self, insts, list) {

	struct capture_state *capt = &self->capt;
	struct shared_state *shared = &self->shared;

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
	    set_bit_pin(capt->index_ena, 0);
	}
        /* handle latch */
	if ( buf->latch_detected ) {
	    buf->latch_detected = 0;
	    capt->latch_count = buf->latch_count;
	}

	/* update Zmask based on index_ena */
	if (get_bit_pin(capt->index_ena)) {
	    shared->Zmask = 3;
	} else {
	    shared->Zmask = 0;
	}
	/* done interacting with update() */
	/* check for change in scale value */
	if ( get_float_pin(capt->pos_scale) != capt->old_scale ) {
	    /* save new scale to detect future changes */
	    capt->old_scale = get_float_pin(capt->pos_scale);
	    /* scale value has changed, test and update it */
	    if ((get_float_pin(capt->pos_scale) < 1e-20) && (get_float_pin(capt->pos_scale) > -1e-20)) {
		/* value too small, divide by zero is a bad thing */
		set_float_pin(capt->pos_scale, 1.0);
	    }
	    /* we actually want the reciprocal */
	    capt->scale = 1.0 / get_float_pin(capt->pos_scale);
	}
        /* check for valid min_speed */
        if ( get_float_pin(capt->min_speed) == 0.0 ) {
            set_float_pin(capt->min_speed,  1.0);
        }

	/* check reset input */
	if (get_bit_pin(capt->reset)) {
	    /* reset is active, reset the counter */
	    /* note: we NEVER reset raw_counts, that is always a
		running count of edges seen since startup.  The
		public "count" is the difference between raw_count
		and index_count, so it will become zero. */

	    capt->raw_count = get_s32_pin(shared->raw_counts);
	    capt->index_count = capt->raw_count;
	}

	// atomically read timestamp
	local_timebase = rtapi_load_u32(&timebase);

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
		set_float_pin(capt->vel, vel);
	    }
	} else {
	    /* no count */
	    if ( capt->counts_since_timeout ) {
		/* calc time since last count */
		delta_time = local_timebase - capt->timestamp;
		if ( delta_time < 1e9 / ( get_float_pin(capt->min_speed) * capt->scale )) {
		    /* not to long, estimate vel if a count arrived now */
		    vel = ( capt->scale ) / (delta_time * 1e-9);
		    /* make vel positive, even if scale is negative */
		    if ( vel < 0.0 ) vel = -vel;
		    /* use lesser of estimate and previous value */
		    /* use sign of previous value, magnitude of estimate */
		    if ( vel < get_float_pin(capt->vel) ) {
			set_float_pin(capt->vel, vel);
		    }
		    if ( -vel > get_float_pin(capt->vel) ) {
			set_float_pin(capt->vel, -vel);
		    }
		} else {
		    /* its been a long time, stop estimating */
		    capt->counts_since_timeout = 0;
		    set_float_pin(capt->vel, 0.0);
		}
	    } else {
		/* we already stopped estimating */
		set_float_pin(capt->vel, 0.0);
	    }
	}
	/* compute net counts */
	set_s32_pin(capt->count, capt->raw_count - capt->index_count);
        set_s32_pin(capt->count_latch, capt->latch_count - capt->index_count);

	/* scale count to make floating point position */
	set_float_pin(capt->pos,  get_s32_pin(capt->count) * capt->scale);
	set_float_pin(capt->pos_latch, get_s32_pin(capt->count_latch) * capt->scale);

	/* add interpolation value */
	delta_time = local_timebase - capt->timestamp;
	interp = get_float_pin(capt->vel) * (delta_time * 1e-9);
	set_float_pin(capt->pos_interp,  get_float_pin(capt->pos) + interp);

    }
    return 0;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
static int export_encoder(const char *name,
			  const int inst_id,
			  counter_t *_e)
{

    struct shared_state *shared = &_e->shared;
    struct update_state *upd = &_e->upd;
    struct capture_state *capt = &_e->capt;

#define FCHECK(rvalue) if (float_pin_null(rvalue)) return _halerrno;
#define BCHECK(rvalue) if (bit_pin_null(rvalue)) return _halerrno;
#define SCHECK(rvalue) if (s32_pin_null(rvalue)) return _halerrno;


    BCHECK(upd->counter_mode  = halxd_pin_bit_newf(HAL_IO, inst_id, 0, "%s.counter-mode", name));
    BCHECK(upd->x4_mode       = halxd_pin_bit_newf(HAL_IO, inst_id, 1, "%s.x4-mode", name));

    BCHECK(upd->phaseA        = halxd_pin_bit_newf(HAL_IN, inst_id, 0, "%s.phase-A", name));
    BCHECK(upd->phaseB        = halxd_pin_bit_newf(HAL_IN, inst_id, 0, "%s.phase-B", name));
    BCHECK(upd->phaseZ        = halxd_pin_bit_newf(HAL_IN, inst_id, 0, "%s.phase-Z", name));

    BCHECK(upd->latch_rising  = halxd_pin_bit_newf(HAL_IN, inst_id, 1, "%s.latch-rising", name));
    BCHECK(upd->latch_falling = halxd_pin_bit_newf(HAL_IN, inst_id, 1, "%s.latch-falling", name));
    BCHECK(upd->latch_in      = halxd_pin_bit_newf(HAL_IN, inst_id, 0, "%s.latch-input", name));


    BCHECK(capt->index_ena    = halxd_pin_bit_newf(HAL_IO, inst_id, 0, "%s.index-enable", name));
    BCHECK(capt->reset        = halxd_pin_bit_newf(HAL_IN, inst_id, 0, "%s.reset", name));

    SCHECK(capt->count        = halxd_pin_s32_newf(HAL_OUT,   inst_id, 0.0, "%s.counts", name));
    SCHECK(capt->count_latch  = halxd_pin_s32_newf(HAL_OUT,   inst_id, 0.0, "%s.counts-latched", name));

    FCHECK(capt->min_speed    = halxd_pin_float_newf(HAL_IN,  inst_id, 1.0, "%s.min-speed-estimate", name));
    FCHECK(capt->pos          = halxd_pin_float_newf(HAL_OUT, inst_id, 0.0, "%s.position", name));
    FCHECK(capt->pos_interp   = halxd_pin_float_newf(HAL_OUT, inst_id, 0.0, "%s.position-interpolated", name));
    FCHECK(capt->pos_latch    = halxd_pin_float_newf(HAL_OUT, inst_id, 0.0, "%s.position-latched", name));
    FCHECK(capt->vel          = halxd_pin_float_newf(HAL_OUT, inst_id, 0.0, "%s.velocity", name));
    FCHECK(capt->pos_scale    = halxd_pin_float_newf(HAL_IO,  inst_id, 1.0, "%s.position-scale", name));

    SCHECK(shared->raw_counts = halxd_pin_s32_newf(HAL_OUT, inst_id, 0, "%s.rawcounts", name));

    shared->Zmask = 0;
    shared->buf[0].count_detected = 0;
    shared->buf[1].count_detected = 0;
    shared->buf[0].index_detected = 0;
    shared->buf[1].index_detected = 0;
    shared->buf[0].latch_detected = 0;
    shared->buf[1].latch_detected = 0;
    shared->bp = &(shared->buf[0]);

    upd->state = 0;
    upd->oldZ = 0;

    capt->raw_count = 0;
    capt->timestamp = 0;
    capt->index_count = 0;
    capt->latch_count = 0;

    capt->old_scale = 1.0;
    capt->scale = 1.0;
    capt->counts_since_timeout = 0;

    return 0;
}
