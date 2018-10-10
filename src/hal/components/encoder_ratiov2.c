/********************************************************************
* Description:  encoder_ratio.c
*               A HAL component that can be used to synchronize two
*               axes (like an "electronic gear").
*
* Author: John Kasunich
* License: GPL Version 2
*
* Copyright (c) 2004 All rights reserved.
*
* Last change: Michael Haberler 9/2015
*              make SMP-safe, instantiable, use v2 pins
*
********************************************************************/
/** This file, 'encoder_ratio.c', is a HAL component that can be used
    to synchronize two axes (like an "electronic gear").  It counts
    encoder pulses from both axes in software, and produces an error
    value that can be used with a PID loop to make the slave encoder
    track the master encoder with a specific ratio. The maximum count
    rate will depend on the speed of the PC, but is expected to exceed
    5KHz for even the slowest computers, and may reach 10-15KHz on fast
    ones.  It is a realtime component.

    This module supports up to eight axis pairs.  The number of pairs
    is set by the module parameter 'num_chan' when the component is
    insmod'ed.  Alternatively, use the names= specifier and a list of
    unique names separated by commas.
    The names= and num_chan= specifiers are mutually exclusive.

    The module exports pins and parameters for each axis pair as follows:

    Input Pins:

    encoder-ratio.N.master-A   (bit) Phase A of master axis encoder
    encoder-ratio.N.master-B   (bit) Phase B of master axis encoder
    encoder-ratio.N.slave-A    (bit) Phase A of slave axis encoder
    encoder-ratio.N.slave-B    (bit) Phase B of slave axis encoder
    encoder-ratio.N.enable     (bit) Enables master-slave tracking

    Output Pins:

    encoder-ratio.N.error      (float) Position error of slave (in revs)

    Parameters:

    encoder-ratio.N.master-ppr     (u32) Master axis PPR
    encoder-ratio.N.slave-ppr      (u32) Slave axis PPR
    encoder-ratio.N.master-teeth   (u32) "teeth" on master "gear"
    encoder-ratio.N.slave-teeth    (u32) "teeth" on slave "gear"

    The module also exports two functions.  "encoder-ratio.sample"
    must be called in a high speed thread, at least twice the maximum
    desired count rate.  "encoder-ratio.update" can be called at a
    much slower rate, and updates the output pin(s).

    When the enable pin is FALSE, the error pin simply reports the
    slave axis position, in revolutions.  As such, it would normally
    be connected to the feedback pin of a PID block for closed loop
    control of the slave axis.  Normally the command input of the
    PID block is left unconnected (zero), so the slave axis simply
    sits still.  However when the enable input goes TRUE, the error
    pin becomes the slave position minus the scaled master position.
    The scale factor is the ratio of master teeth to slave teeth.
    As the master moves, error becomes non-zero, and the PID loop
    will drive the slave axis to track the master.
*/

/** Copyright (C) 2004 John Kasunich
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
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"           // SHMPTR,SHMOFF
#include "hal_list.h"

#define VERBOSE_SETUP  // pin creation

MODULE_AUTHOR("John Kasunich, Michael Haberler");
MODULE_DESCRIPTION("Encoder Ratio Module for HAL");
MODULE_LICENSE("GPL");

RTAPI_TAG(HAL,HC_INSTANTIABLE);
RTAPI_TAG(HAL,HC_SMP_SAFE);

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

// passed atomically from update to sample
typedef union  {
    hal_u64_t u64;
    struct {
	int master_increment;
	int slave_increment;
    } i;
} increments_t;

// since rtapi_load_u64/rtapi_store_u64 is used for increments_t, better be sure:
rtapi_ct_assert(sizeof(increments_t) == sizeof(hal_u64_t), "BUG: assertion violated");

/* this structure contains the runtime data for a single counter */
typedef struct {
    hal_list_t  list;
    int inst_id;

    struct to_sample {
	increments_t incr;  // s:r u:w
    } sample_reads;

    struct sample_state {
	bit_pin_ptr master_A;	/* quadrature input */
	bit_pin_ptr master_B;	/* quadrature input */
	bit_pin_ptr slave_A;	/* quadrature input */
	bit_pin_ptr slave_B;	/* quadrature input */
	bit_pin_ptr enable;	/* enable input */

	unsigned char master_state;	/* quad decode state machine state */
	unsigned char slave_state;	/* quad decode state machine state */
    } smpl;

    struct upd_state {
	double output_scale;

	u32_pin_ptr master_ppr;	  /* parameter: master encoder PPR */
	u32_pin_ptr slave_ppr;	  /* parameter: slave encoder PPR */
	u32_pin_ptr master_teeth; /* parameter: master "gear" tooth count */
	u32_pin_ptr slave_teeth;  /* parameter: slave "gear" tooth count */
	float_pin_ptr error;	  /* error output */
    } upd;

    // passed atomically from sample to update update
    struct to_update {
	hal_s32_t raw_error;	    // s:w u:r
    } update_reads;

} encoder_pair_t;

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
static const unsigned char lut[16] = {
    0x00, 0x44, 0x88, 0x0C, 0x80, 0x04, 0x08, 0x4C,
    0x40, 0x04, 0x08, 0x8C, 0x00, 0x84, 0x48, 0x0C
};

/* other globals */
static int comp_id;		/* component ID */

static const char *compname = "encoder_ratiov2";
static const char *prefix = "encoder-ratiov2";

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static int export_encoder_pair(const char *name, const int inst_id,
			       encoder_pair_t * addr);
static int sample(void *arg, const hal_funct_args_t *fa);
static int update(void *arg, const hal_funct_args_t *fa);
static int instantiate_encoder_pair(const int argc, char* const *argv);
static int delete_encoder_pair(const char *name, void *inst, const int inst_size);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int retval;
    dlist_init_entry(&head);

    if ((comp_id = hal_xinit(TYPE_RT, 0, 0,
			     instantiate_encoder_pair,
			     delete_encoder_pair,
			     compname)) < 0)
	return comp_id;

    hal_export_xfunct_args_t u = {
        .type = FS_XTHREADFUNC,
        .funct.x = update,
        .arg = &head,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&u, "%s.update",
				     prefix)) < 0)
	return retval;

    hal_export_xfunct_args_t mp = {
        .type = FS_XTHREADFUNC,
        .funct.x = sample,
        .arg = &head,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = comp_id
    };
    if ((retval = hal_export_xfunctf(&mp, "%s.sample",
				     prefix)) < 0)
	return retval;
    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static int instantiate_encoder_pair(const int argc, char* const *argv)
{
    encoder_pair_t *p;
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

    if ((retval = hal_inst_create(name, comp_id, sizeof(encoder_pair_t), (void **)&p)) < 0)
	return retval;

    p->inst_id = retval;
    if ((retval = export_encoder_pair(name, p->inst_id, p)) != 0)
	HALFAIL_RC(retval, "%s: ERROR: export(%s) failed", compname, name);

    // append to instance list
    dlist_init_entry(&p->list);
    dlist_add_after(&p->list, &head);

    /* restore saved message level */
    rtapi_set_msg_level(msg);
    return 0;
}

static int delete_encoder_pair(const char *name, void *inst, const int inst_size)
{
    encoder_pair_t *p = inst;

    // delete from instance list
    dlist_remove_entry(&p->list);
    return 0;
}

/***********************************************************************
*            REALTIME ENCODER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static int sample(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    encoder_pair_t *self;

    // foreach encoder_pair instance: sample inputs
    dlist_for_each_entry(self, insts, list) {

	// private state
	struct sample_state *smpl = &self->smpl;

	// shared state per direction:
	struct to_update *update_reads = &self->update_reads;

	// current value of raw_error, udpdated eventually
	hal_s32_t raw_error =  rtapi_load_s32(&update_reads->raw_error);

	// r/o atomic snapshot of master_increment and slave_increment
	const increments_t incr = { .u64 = rtapi_load_u64(&self->sample_reads.incr.u64) };

	/* detect transitions on master encoder */
	/* get state machine current state */
	unsigned char state = smpl->master_state;

	/* add input bits to state code */
	if (get_bit_pin(smpl->master_A)) {
	    state |= SM_PHASE_A_MASK;
	}
	if (get_bit_pin(smpl->master_B)) {
	    state |= SM_PHASE_B_MASK;
	}
	/* look up new state */
	state = lut[state & SM_LOOKUP_MASK];
	/* are we enabled? */
	if ( get_bit_pin(smpl->enable) != 0 ) {
	    /* has an edge been detected? */
	    if (state & SM_CNT_UP_MASK) {
		raw_error -= incr.i.master_increment;
	    } else if (state & SM_CNT_DN_MASK) {
		raw_error += incr.i.master_increment;
	    }
	}
	/* save state machine state */
	smpl->master_state = state;
	/* detect transitions on slave encoder */
	/* get state machine current state */
	state = smpl->slave_state;
	/* add input bits to state code */
	if (get_bit_pin(smpl->slave_A)) {
	    state |= SM_PHASE_A_MASK;
	}
	if (get_bit_pin(smpl->slave_B)) {
	    state |= SM_PHASE_B_MASK;
	}
	/* look up new state */
	state = lut[state & SM_LOOKUP_MASK];
	/* has an edge been detected? */
	if (state & SM_CNT_UP_MASK) {
	    raw_error += incr.i.slave_increment;
	} else if (state & SM_CNT_DN_MASK) {
	    raw_error -= incr.i.slave_increment;
	}

	// atomically update the raw error
	rtapi_store_s32(&update_reads->raw_error, raw_error);

	/* save state machine state */
	smpl->slave_state = state;
    }
    return 0;
}

static int update(void *arg, const hal_funct_args_t *fa)
{
    hal_list_t *insts = arg;
    encoder_pair_t *self;

    // foreach encoder_pair instance: update parameters, error pin
    dlist_for_each_entry(self, insts, list) {

	// update private state
	struct upd_state *up = &self->upd;

	// atomically fetch current raw_error (R/O in this funct)
	const hal_s32_t raw_error =  rtapi_load_s32( &self->update_reads.raw_error);

	increments_t incr;

	/* scale raw error to output pin */
	if ( up->output_scale > 0 ) {
	    set_float_pin(up->error, raw_error / up->output_scale);
	}
	/* update scale factors (only needed if params change, but
	   it's faster to do it every time than to detect changes.) */
	incr.i.master_increment = get_u32_pin(up->master_teeth) * get_u32_pin(up->slave_ppr);
	incr.i.slave_increment = get_u32_pin(up->slave_teeth) * get_u32_pin(up->master_ppr);
	up->output_scale = get_u32_pin(up->master_ppr) *
	    get_u32_pin(up->slave_ppr) *
	    get_u32_pin(up->slave_teeth);

	// atomically update master_increment and slave_increment
	rtapi_store_u64(&self->sample_reads.incr.u64,incr.u64);
    }
    return 0;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
static int export_encoder_pair(const char *name,
			       const int inst_id,
			       encoder_pair_t *e)
{
#define FCHECK(rvalue) if (float_pin_null(rvalue)) return _halerrno;
#define BCHECK(rvalue) if (bit_pin_null(rvalue)) return _halerrno;
#define UCHECK(rvalue) if (u32_pin_null(rvalue)) return _halerrno;

    BCHECK(e->smpl.master_A  = halxd_pin_bit_newf(HAL_IN, inst_id, 0,   "%s.master-A", name));
    BCHECK(e->smpl.master_B  = halxd_pin_bit_newf(HAL_IN, inst_id, 0,   "%s.master-B", name));
    BCHECK(e->smpl.slave_A   = halxd_pin_bit_newf(HAL_IN, inst_id, 0,   "%s.slave-A", name));
    BCHECK(e->smpl.slave_B   = halxd_pin_bit_newf(HAL_IN, inst_id, 0,   "%s.slave-B", name));
    BCHECK(e->smpl.enable    = halxd_pin_bit_newf(HAL_IN, inst_id, 0,   "%s.enable", name));

    FCHECK(e->upd.error   = halxd_pin_float_newf(HAL_OUT, inst_id, 0.0, "%s.error", name));

    UCHECK(e->upd.master_ppr   = halxd_pin_u32_newf(HAL_IO, inst_id, 0.0, "%s.master-ppr", name));
    UCHECK(e->upd.slave_ppr    = halxd_pin_u32_newf(HAL_IO, inst_id, 0.0, "%s.slave-ppr", name));
    UCHECK(e->upd.master_teeth = halxd_pin_u32_newf(HAL_IO, inst_id, 0.0, "%s.master-teeth", name));
    UCHECK(e->upd.slave_teeth  = halxd_pin_u32_newf(HAL_IO, inst_id, 0.0, "%s.slave-teeth", name));

    // sample_state
    e->smpl.master_state = 0;
    e->smpl.slave_state = 0;

    // shared_state
    e->sample_reads.incr.i.master_increment = 0;
    e->sample_reads.incr.i.slave_increment = 0;
    e->update_reads.raw_error = 0;

    // update_state
    e->upd.output_scale = 1.0;

    return 0;
}
