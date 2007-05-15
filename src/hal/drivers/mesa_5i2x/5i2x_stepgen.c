/******************************************************************************

Copyright (C) 2007 John Kasunich <jmkasunich AT sourceforge DOT net>

$RCSfile$
$Author$
$Locker$
$Revision$
$State$
$Date$

This is the driver for hardware step generation on the Mesa
Electronics 5i20 board.  It works pretty much like the software
based stepgen, only faster.

**********************************************************************

This program is free software; you can redistribute it and/or
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

**********************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <asm/io.h>
#include "rtapi.h"			// RTAPI realtime OS API.
#include "hal.h"			// HAL public API decls.
#include "hal_5i2x.h"			// Hardware dependent defines.

/***************************************************************************
                          typedefs and defines
***************************************************************************/

/* register widths - these need to match the VHDL */
#define ACC_SIZE		48
#define POS_SIZE		32
#define RATE_SIZE		24
#define TIMER_SIZE		14

/* Step generator ports */
/* offsets from base address */

#define RATE_REG_ADDR		0x00
#define RATE_REG_LEN		RATE_SIZE
#define RATE_REG_SHIFT		0

#define LEN_MODE_ADDR		0x04
#define STEP_LEN_BITS		TIMER_SIZE
#define STEP_LEN_SHIFT		0
#define MODE_BITS		2
#define MODE_SHIFT		16
#define ENABLE_SHIFT		18

#define HOLD_SETUP_ADDR		0x08
#define DIR_HOLD_BITS		TIMER_SIZE
#define DIR_HOLD_SHIFT		0
#define DIR_SETUP_BITS		TIMER_SIZE
#define DIR_SETUP_SHIFT		16

#define POS_ADDR		0x00
/* number of fractional bits in position (accumulator readback) register */
#define POS_SHIFT		(POS_SIZE-(ACC_SIZE-(RATE_SIZE+1)))

/* timing stuff */
#define STEPGEN_MASTER_CLOCK 33000000
#define COUNTS_PER_HZ ((double)(1ll << (RATE_SIZE+1))/(double)STEPGEN_MASTER_CLOCK)
#define STEPGEN_MASTER_PERIOD (1000000000/STEPGEN_MASTER_CLOCK)


/***************************************************************************
                         Data Structures
***************************************************************************/

/* one stepgen */
typedef struct stepgen_t {
    struct stepgen_t *next;
    hal_s32_t *counts;
    hal_float_t *pos_fb;
    hal_bit_t *enable;
    hal_float_t *vel_cmd;
    hal_u32_t step_type;
    hal_float_t frequency;
    hal_float_t maxaccel;
    hal_float_t maxvel;
    hal_float_t scale;
    hal_u32_t steplen;
    hal_u32_t stepspace;
    hal_u32_t dirsetup;
    hal_u32_t dirhold;
    void __iomem *addr;
    hal_bit_t old_enable;
    hal_u32_t old_step_type;
    hal_float_t old_maxaccel;
    hal_float_t old_maxvel;
    hal_float_t old_scale;
    u32 old_steplen;
    u32 old_stepspace;
    u32 old_dirsetup;
    u32 old_dirhold;
    u32 hold_setup_value;
    u32 len_mode_value;
    double internal_maxvel;
    double max_deltav;
    double current_vel;
    int update_max;
    s32 old_accum;
    long long int counts_hires;
} stepgen_t;

/******************************************************************************
                                 Realtime Code
 ******************************************************************************/

static void read_stepgen(void *arg, long period)
{
    stepgen_t *s;
    s32 accum, delta;

    s = arg;
    if ( s->scale != s->old_scale ) {
	s->old_scale = s->scale;
	/* validate the new scale value */
	if ((s->scale < 1e-20) && (s->scale > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    s->scale = 1.0;
	}
	/* flag the change for write_stepgen() */
	s->update_max = 1;
    }
    /* read the position accumulator - this is fixed point,
	with POS_SHIFT fractional bits */
    accum = ioread32(s->addr+POS_ADDR);
    /* compute delta */
    delta = accum - s->old_accum;
    s->old_accum = accum;
    /* update integer and high resolution counts */
    s->counts_hires += delta;
    *(s->counts) = s->counts_hires >> POS_SHIFT;
    /* convert high res counts to position */
    *(s->pos_fb) = (((double)s->counts_hires) / s->scale) *
			( 1.0 / ( 1 << POS_SHIFT ));
}

static void write_stepgen(void *arg, long period)
{
    stepgen_t *s;
    u32 clocks, max;
    s32 addval;
    u32 min_period_ns;
    double max_freq, vel_cmd, vel_diff;
    int write;

    s = arg;
    /* lots of parameter processing, do only if something has changed */
    write = 0;
    if ( s->dirhold != s->old_dirhold ) {
	/* convert dirhold in ns to clock periods */
	clocks = s->dirhold * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	max = ((1<<DIR_HOLD_BITS)-1);
	if ( clocks > max ) { clocks = max; }
	/* set parameter to actual (post rounding & limiting) value */
	s->old_dirhold = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->dirhold = s->old_dirhold;
	/* merge new hold with previous setup */
	s->hold_setup_value &= ~(max << DIR_HOLD_SHIFT);
	s->hold_setup_value |= clocks << DIR_HOLD_SHIFT;
	/* force write */
	write = 1;
    }
    if ( s->dirsetup != s->old_dirsetup ) {
	clocks = s->dirsetup * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	max = ((1<<DIR_SETUP_BITS)-1);
	if ( clocks > max ) { clocks = max; }
	s->old_dirsetup = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->dirsetup = s->old_dirsetup;
	/* merge new setup with previous hold */
	s->hold_setup_value &= ~(max << DIR_SETUP_SHIFT);
	s->hold_setup_value |= clocks << DIR_SETUP_SHIFT;
	/* force write */
	write = 1;
    }
    if ( write ) {
	/* write setup and hold to hardware */
	iowrite32(s->hold_setup_value, s->addr + HOLD_SETUP_ADDR);
	write = 0;
    }
    if ( s->steplen != s->old_steplen ) {
	clocks = s->steplen * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	max = ((1<<STEP_LEN_BITS)-1);
	if ( clocks > max ) { clocks = max; }
	s->old_steplen = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->steplen = s->old_steplen;
	/* merge new step len with previous mode and enable */
	s->len_mode_value &= ~(max << STEP_LEN_SHIFT);
	s->len_mode_value |= clocks << STEP_LEN_SHIFT;
	/* force write */
	write = 1;
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->step_type != s->old_step_type ) {
	if ( s->step_type > 2 ) {
	    s->step_type = 0;
	}
	s->old_step_type = s->step_type;
	/* merge new mode with previous step len and enable */
	max = ((1<<MODE_BITS)-1);
	s->len_mode_value &= ~(max << MODE_SHIFT);
	/* add one - mode 00 is off, mode 01 is step type 0, etc. */
	s->len_mode_value |= (s->step_type + 1) << MODE_SHIFT;
	/* force write */
	write = 1;
    }
    if ( *(s->enable) != s->old_enable ) {
	s->old_enable = *(s->enable);
	/* merge new enable with previous step len and mode */
	if ( *(s->enable) ) {
	    s->len_mode_value |= 1 << ENABLE_SHIFT;
	} else {
	    s->len_mode_value &= ~(1 << ENABLE_SHIFT);
	}
	/* force write */
	write = 1;
    }
    if ( write ) {
	/* write mode, enable, and step length */
	iowrite32(s->len_mode_value, s->addr + LEN_MODE_ADDR);
	write = 0;
    }
    if ( s->stepspace != s->old_stepspace ) {
	/* convert stepspace in ns to clock periods */
	clocks = s->stepspace * (STEPGEN_MASTER_CLOCK / 1000000000.0);
	if ( clocks == 0 ) { clocks = 1; }
	/* set parameter to actual (post rounding) value */
	s->old_stepspace = clocks * (1000000000.0 / STEPGEN_MASTER_CLOCK);
	s->stepspace = s->old_stepspace;
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->scale != s->old_scale ) {
	s->old_scale = s->scale;
	/* validate the new scale value */
	if ((s->scale < 1e-20) && (s->scale > -1e-20)) {
	    /* value too small, divide by zero is a bad thing */
	    s->scale = 1.0;
	}
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->maxvel != s->old_maxvel ) {
	if ( s->maxvel < 0.0 ) {
	    s->maxvel = -s->maxvel;
	}
	s->old_maxvel = s->maxvel;
	/* force recalc of max frequency */
	s->update_max = 1;
    }
    if ( s->maxaccel != s->old_maxaccel ) {
	if ( s->maxaccel < 0.0 ) {
	    s->maxaccel = -s->maxaccel;
	}
	s->old_maxaccel = s->maxaccel;
	s->max_deltav = s->maxaccel * period * 0.000000001;
    }
    if ( s->update_max ) {
	/* either maxvel, scale, steplen, or stepspace changed */
	min_period_ns = s->steplen + s->stepspace;
	max_freq = 1000000000.0 / min_period_ns;
	s->internal_maxvel = max_freq / s->scale;
	if ( s->maxvel > 0.0 ) {
	    if ( s->maxvel < s->internal_maxvel ) {
		s->internal_maxvel = s->maxvel;
	    } else {
		s->maxvel = s->internal_maxvel;
		s->old_maxvel = s->maxvel;
	    }
	}
    }
    /* apply velocity limits */
    vel_cmd = *(s->vel_cmd);
    if ( vel_cmd > s->internal_maxvel ) {
	vel_cmd = s->internal_maxvel;
	*(s->vel_cmd) = vel_cmd;
    } else if ( vel_cmd < -s->internal_maxvel ) {
	vel_cmd = -s->internal_maxvel;
	*(s->vel_cmd) = vel_cmd;
    }
    /* apply ramping */
    if ( s->max_deltav != 0.0 ) {
	vel_diff = vel_cmd - s->current_vel;
	if ( vel_diff > s->max_deltav ) {
	    s->current_vel += s->max_deltav;
	} else if ( vel_diff < -s->max_deltav ) {
	    s->current_vel -= s->max_deltav;
	} else {
	    s->current_vel = vel_cmd;
	}
    }
    /* convert vel to freq */
    s->frequency = s->current_vel * s->scale;
    /* convert frequency to adder value and write to hardware */
    addval = s->frequency * COUNTS_PER_HZ;
    iowrite32(addval, s->addr + RATE_REG_ADDR);
}



/******************************************************************************
                                HAL export code
 ******************************************************************************/

int export_stepgen(__u8 **ppcfg, board_data_t *board)
{
    __u8 *data;
    int retval;
    int gennum, boardnum;
    stepgen_t *stepgen, **p;
    int code, addr, pin0, pin1;
    char name[HAL_NAME_LEN + 2];

    /* read and validate config data */
    data = *ppcfg;
    code = data[0];
    addr = (data[1] << 8) + data[2];
    pin0 = data[3];
    pin1 = data[4];
    /* return ptr to next block */
    *ppcfg = &(data[5]);
    /* Allocate HAL memory for the step generator */
    stepgen = (stepgen_t *)(hal_malloc(sizeof(stepgen_t)));
    if ( stepgen == NULL ) {
	rtapi_print_msg(RTAPI_MSG_ERR, "5i2x: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* find end of linked list */
    boardnum = board->num;
    gennum = 0;
    p = &(board->stepgen);
    while ( *p != NULL ) {
	p = &((*p)->next);
	gennum++;
    }
    /* add to end of list */
    *p = stepgen;
    stepgen->next = NULL;
    rtapi_print_msg(RTAPI_MSG_ERR,
	"board %d stepgen %d at %x, using pins %d and %d\n",
	boardnum, gennum, addr, pin0, pin1 );
    /* export output HAL pins for feedbacks */
    retval = hal_pin_s32_newf(HAL_OUT, &(stepgen->counts),
	comp_id, "5i20.%d.stepgen.%d.counts", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_pin_float_newf(HAL_OUT, &(stepgen->pos_fb),
	comp_id, "5i20.%d.stepgen.%d.pos-fb", boardnum, gennum);
    if (retval != 0) return retval;
    /* export HAL input pins for control */
    retval = hal_pin_bit_newf(HAL_IN, &(stepgen->enable),
	comp_id, "5i20.%d.stepgen.%d.enable", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_pin_float_newf(HAL_IN, &(stepgen->vel_cmd),
	comp_id, "5i20.%d.stepgen.%d.vel-cmd", boardnum, gennum);
    if (retval != 0) return retval;
    /* now the parameters */
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->step_type),
	comp_id, "5i20.%d.stepgen.%d.step-type", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RO, &(stepgen->frequency),
	comp_id, "5i20.%d.stepgen.%d.frequency", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->maxaccel),
	comp_id, "5i20.%d.stepgen.%d.maxaccel", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->maxvel),
	comp_id, "5i20.%d.stepgen.%d.maxvel", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_float_newf(HAL_RW, &(stepgen->scale),
	comp_id, "5i20.%d.stepgen.%d.scale", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->steplen),
	comp_id, "5i20.%d.stepgen.%d.steplen", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->stepspace),
	comp_id, "5i20.%d.stepgen.%d.stepspace", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->dirsetup),
	comp_id, "5i20.%d.stepgen.%d.dirsetup", boardnum, gennum);
    if (retval != 0) return retval;
    retval = hal_param_u32_newf(HAL_RW, &(stepgen->dirhold),
	comp_id, "5i20.%d.stepgen.%d.dirhold", boardnum, gennum);
    if (retval != 0) return retval;
    /* set initial value for pin and params */
    *(stepgen->counts) = 0;
    *(stepgen->pos_fb) = 0.0;
    *(stepgen->enable) = 0;
    *(stepgen->vel_cmd) = 0.0;
    stepgen->step_type = 0;
    stepgen->frequency = 0.0;
    stepgen->maxaccel = 0.0;
    stepgen->maxvel = 0.0;
    stepgen->scale = 200.0;
    stepgen->steplen = 100;
    stepgen->stepspace = 100;
    stepgen->dirsetup = 100;
    stepgen->dirhold = 100;
    /* init other stuff */
    stepgen->addr = board->base + addr;
    /* setting these so they don't match the actual values forces
	an immediate update of the hw once the functions run */
    stepgen->old_enable = 1;
    stepgen->old_step_type = 100;
    stepgen->old_maxaccel = -1.0;
    stepgen->old_maxvel = -1.0;
    stepgen->old_scale = 0.0;
    stepgen->counts_hires = 0;
    stepgen->old_steplen = 0;
    stepgen->old_stepspace = 0;
    stepgen->old_dirsetup = 0;
    stepgen->old_dirhold = 0;
    stepgen->internal_maxvel = 0;
    stepgen->max_deltav = 0;
    stepgen->current_vel = 0;
    stepgen->update_max = 1;
    stepgen->old_accum = 0;
    stepgen->counts_hires = 0;
    stepgen->len_mode_value = 0x00000001;
    stepgen->hold_setup_value = 0x00010001;
    /* export functions - one funct serves all generators */
    if ( gennum > 0 ) {
	/* already exported */
	return 0;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.stepgen.read", boardnum);
    retval = hal_export_funct(name, read_stepgen, stepgen, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d stepgen read funct export failed\n",
	    boardnum);
	return -1;
    }
    rtapi_snprintf(name, HAL_NAME_LEN, "5i20.%d.stepgen.write", boardnum);
    retval = hal_export_funct(name, write_stepgen, stepgen, 1, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "5i20: ERROR: board %d stepgen read funct export failed\n",
	    boardnum);
	return -1;
    }
    return 0;
}

