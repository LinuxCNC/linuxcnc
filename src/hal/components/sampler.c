/********************************************************************
* Description:  sampler.c
*               A HAL component that can be used to capture data
*               from HAL pins at a specific realtime sample rate,
*		and allows the data to be written to stdout.
*
* Author: John Kasunich <jmkasunich at sourceforge dot net>
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/
/** This file, 'sampler.c', is the realtime part of a HAL component
    that allows data from HAL pins to be sampled at a uniform realtime
    sample rate and then be transferred to a file.  When this realtime
    module is loaded, it creates a fifo in shared memory and begins 
    capturing data from HAL pins to the fifo.  Then, the user space 
    program 'halsampler' is invoked, which reads the fifo and writes
    the data to stdout.

    Loading:

    loadrt sampler depth=100 cfg=uffb


*/

/** Copyright (C) 2006 John Kasunich
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

#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "hal.h"                /* HAL public API decls */
#include "streamer.h"		/* decls and such for fifos */
#include "rtapi_errno.h"
#include "rtapi_string.h"

/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Realtime HAL Sampler");
MODULE_LICENSE("GPL");
static char *cfg[MAX_SAMPLERS];	/* config string, no default */
RTAPI_MP_ARRAY_STRING(cfg,MAX_SAMPLERS,"config string");
static int depth[MAX_SAMPLERS];	/* depth of fifo, default 0 */
RTAPI_MP_ARRAY_INT(depth,MAX_SAMPLERS,"fifo depth");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the HAL shared memory data for one sampler */

typedef struct {
    fifo_t *fifo;		/* pointer to user/RT fifo */
    hal_s32_t *curr_depth;	/* pin: current fifo depth */
    hal_bit_t *full;		/* pin: overrun flag */
    hal_bit_t *enable;		/* pin: enable sampling */
    hal_s32_t *overruns;	/* pin: number of overruns */
    hal_s32_t *sample_num;	/* pin: sample ID / timestamp */
} sampler_t;

/* other globals */
static int comp_id;		/* component ID */
static int shmem_id[MAX_SAMPLERS];

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int parse_types(fifo_t *f, char *cfg);
static int init_sampler(int num, fifo_t *tmp_fifo);
static void sample(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int n, numchan, max_depth, retval;
    fifo_t tmp_fifo[MAX_SAMPLERS];

    /* validate config info */
    for ( n = 0 ; n < MAX_SAMPLERS ; n++ ) {
	if (( cfg[n] == NULL ) || ( *cfg == '\0' ) || ( depth[n] <= 0 )) {
	    break;
	}
	tmp_fifo[n].num_pins = parse_types(&(tmp_fifo[n]), cfg[n]);
	if ( tmp_fifo[n].num_pins == 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SAMPLER: ERROR: bad config string '%s'\n", cfg[n]);
	    return -EINVAL;
	}
	/* allow one extra "slot" for the sample number */
	max_depth = MAX_SHMEM / (sizeof(shmem_data_t) * (tmp_fifo[n].num_pins + 1));
	if ( depth[n] > max_depth ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SAMPLER: ERROR: depth too large, max is %d\n", max_depth);
	    return -ENOMEM;
	}
	tmp_fifo[n].depth = depth[n];
    }
    if ( n == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: no channels specified\n");
	return -EINVAL;
    }
    numchan = n;
    /* clear shmem IDs */
    for ( n = 0 ; n < MAX_SAMPLERS ; n++ ) {
	shmem_id[n] = -1;
    }

    /* have good config info, connect to the HAL */
    comp_id = hal_init("sampler");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SAMPLER: ERROR: hal_init() failed\n");
	return -EINVAL;
    }

    /* create the samplers - allocate memory, export pins, etc. */
    for (n = 0; n < numchan; n++) {
	retval = init_sampler(n, &(tmp_fifo[n]));
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SAMPLER: ERROR: sampler %d init failed\n", n);
	    hal_exit(comp_id);
	    return retval;
	}
    }
    rtapi_print_msg(RTAPI_MSG_INFO,
	"SAMPLER: installed %d data samplers\n", numchan);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    int n;

    /* free any shmem blocks */
    for ( n = 0 ; n < MAX_SAMPLERS ; n++ ) {
	if ( shmem_id[n] > 0 ) {
	    rtapi_shmem_delete(shmem_id[n], comp_id);
	}
    }
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
************************************************************************/

static void sample(void *arg, long period)
{
    sampler_t *samp;
    fifo_t *fifo;
    pin_data_t *pptr;
    shmem_data_t *dptr;
    int tmpin, newin, tmpout, n;

    /* point at sampler struct in HAL shmem */
    samp = arg;
    /* are we enabled? */
    if ( ! *(samp->enable) ) {
	/* no, done */
	return;
    }
    /* HAL pins are right after the sampler_t struct in HAL shmem */
    pptr = (pin_data_t *)(samp+1);
    /* point at user/RT fifo in other shmem */
    fifo = samp->fifo;
    /* fifo data area is right after the fifo_t struct in shmem */
    dptr = (shmem_data_t *)(fifo+1);
    /* calculate _next_ value for in */
    tmpin = fifo->in;
    newin = tmpin + 1;
    if ( newin >= fifo->depth ) {
	newin = 0;
    }
    tmpout = fifo->out;
    if ( newin == tmpout ) {
	/* fifo is full, need to overwrite the oldest data */
	tmpout++;
	if ( tmpout >= fifo->depth ) {
	    tmpout = 0;
	}
	fifo->out = tmpout;
        /* log the overrun */
	(*samp->overruns)++;
	*(samp->full) = 1;
    } else {
	*(samp->full) = 0;
    }
    /* make pointer to fifo entry */
    dptr += tmpin * (fifo->num_pins+1);
    /* copy data from HAL pins to fifo */
    for ( n = 0 ; n < fifo->num_pins ; n++ ) {
	switch ( fifo->type[n] ) {
	case HAL_FLOAT:
	    dptr->f = *(pptr->hfloat);
	    break;
	case HAL_BIT:
	    if ( *(pptr->hbit) ) {
		dptr->b = 1;
	    } else {
		dptr->b = 0;
	    }
	    break;
	case HAL_U32:
	    dptr->u = *(pptr->hu32);
	    break;
	case HAL_S32:
	    dptr->s = *(pptr->hs32);
	    break;
	default:
	    break;
	}
	dptr++;
	pptr++;
    }
    /* store sample number at the end of the fifo record */
    dptr->u = (*samp->sample_num)++;
    /* update fifo pointer */
    fifo->in = newin;
    /* calculate current depth */
    if ( newin < tmpout ) {
	newin += fifo->depth;
    }
    *(samp->curr_depth) = newin - tmpout;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int parse_types(fifo_t *f, char *cfg)
{
    char *c;
    int n;

    c = cfg;
    n = 0;
    while (( n < MAX_PINS ) && ( *c != '\0' )) {
	switch (*c) {
	case 'f':
	case 'F':
	    f->type[n++] = HAL_FLOAT;
	    c++;
	    break;
	case 'b':
	case 'B':
	    f->type[n++] = HAL_BIT;
	    c++;
	    break;
	case 'u':
	case 'U':
	    f->type[n++] = HAL_U32;
	    c ++;
	    break;
	case 's':
	case 'S':
	    f->type[n++] = HAL_S32;
	    c++;
	    break;
	default:
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SAMPLER: ERROR: unknown type '%c', must be F, B, U, or S\n", *c);
	    return 0;
	}
    }
    if ( *c != '\0' ) {
	/* didn't reach end of cfg string */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: more than %d items\n", MAX_PINS);
	return 0;
    }
    return n;
}

static int init_sampler(int num, fifo_t *tmp_fifo)
{
    int size, retval, n, usefp;
    void *shmem_ptr;
    sampler_t *str;
    pin_data_t *pptr;
    fifo_t *fifo;
    char buf[HAL_NAME_LEN + 1];

    /* alloc shmem for base sampler data and user specified pins */
    size = sizeof(sampler_t) + tmp_fifo->num_pins * sizeof(pin_data_t);
    str = hal_malloc(size);

    if (str == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: couldn't allocate HAL shared memory\n");
	return -ENOMEM;
    }
    /* export "standard" pins and params */
    retval = hal_pin_bit_newf(HAL_OUT, &(str->full), comp_id,
	"sampler.%d.full", num);
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: 'full' pin export failed\n");
	return -EIO;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(str->enable), comp_id,
	"sampler.%d.enable", num);
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: 'enable' pin export failed\n");
	return -EIO;
    }
    retval = hal_pin_s32_newf(HAL_OUT, &(str->curr_depth), comp_id,
	"sampler.%d.curr-depth", num);
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLEr: ERROR: 'curr_depth' pin export failed\n");
	return -EIO;
    }
    retval = hal_pin_s32_newf(HAL_IO, &(str->overruns), comp_id,
	"sampler.%d.overruns", num);
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: 'overruns' parameter export failed\n");
	return -EIO;
    }
    retval = hal_pin_s32_newf(HAL_IO, &(str->sample_num), comp_id,
	"sampler.%d.sample-num", num);
    if (retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: 'sample-num' parameter export failed\n");
	return -EIO;
    }
    /* init the standard pins and params */
    *(str->full) = 0;
    *(str->enable) = 1;
    *(str->curr_depth) = 0;
    *(str->overruns) = 0;
    *(str->sample_num) = 0;
    /* HAL pins are right after the sampler_t struct in HAL shmem */
    pptr = (pin_data_t *)(str+1);
    usefp = 0;
    /* export user specified pins (the ones that sample data) */
    for ( n = 0 ; n < tmp_fifo->num_pins ; n++ ) {
	rtapi_snprintf(buf, sizeof(buf), "sampler.%d.pin.%d", num, n);
	retval = hal_pin_new(buf, tmp_fifo->type[n], HAL_IN, (void **)pptr, comp_id );
	if (retval != 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SAMPLER: ERROR: pin '%s' export failed\n", buf);
	    return -EIO;
	}
	/* init the pin value */
	switch ( tmp_fifo->type[n] ) {
	case HAL_FLOAT:
	    *(pptr->hfloat) = 0.0;
	    usefp = 1;
	    break;
	case HAL_BIT:
	    *(pptr->hbit) = 0;
	    break;
	case HAL_U32:
	    *(pptr->hu32) = 0;
	    break;
	case HAL_S32:
	    *(pptr->hs32) = 0;
	    break;
	default:
	    break;
	}
	pptr++;
    }
    /* export update function */
    rtapi_snprintf(buf, sizeof(buf), "sampler.%d", num);
    retval = hal_export_funct(buf, sample, str, usefp, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: function export failed\n");
	return retval;
    }

    /* alloc shmem for user/RT comms (fifo) */
    size = sizeof(fifo_t) + (tmp_fifo->num_pins + 1) * tmp_fifo->depth * sizeof(shmem_data_t);
    shmem_id[num] = rtapi_shmem_new(SAMPLER_SHMEM_KEY+num, comp_id, size);
    if ( shmem_id[num] < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLEr: ERROR: couldn't allocate user/RT shared memory\n");
	return -ENOMEM;
    }
    retval = rtapi_shmem_getptr(shmem_id[num], &shmem_ptr, 0);
    if ( retval < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SAMPLER: ERROR: couldn't map user/RT shared memory\n");
	return -ENOMEM;
    }
    fifo = shmem_ptr;
    str->fifo = fifo;
    /* copy data from temp_fifo */
    *fifo = *tmp_fifo;
    /* init fields */
    fifo->in = 0;
    fifo->out = 0;
    fifo->last_sample = 0;
    fifo->last_sample--;

    /* mark it inited for user program */
    fifo->magic = FIFO_MAGIC_NUM;
    return 0;
}

