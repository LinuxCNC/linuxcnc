/** This file, 'halscope_rt.c', is a HAL component that together with
    'halscope.c' provides an oscilliscope to view HAL pins, signals,
    and parameters
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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private API decls */
#include "halsc_rt.h"		/* scope related declarations */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("Oscilliscope for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */

static long period = 0;		/* thread period - default = no thread */
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
#endif /* MODULE */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

static int comp_id;		/* component ID */
static int shm_id;		/* shared memory ID */
static scope_rt_control_t ctrl_struct;	/* realtime control structure */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static void init_rt_control_struct(scope_rt_control_t * ctrl, void *shmem);
static void init_shared_control_struct(scope_shm_control_t * ctrl);

static void sample(void *arg, long period);
static int check_trigger(scope_rt_control_t * ctrl);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    scope_rt_control_t *ctrl;
    int retval;
    void *shm_base;

    /* connect to the HAL */
    comp_id = hal_init("scope_rt");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "SCOPE: ERROR: hal_init() failed\n");
	return -1;
    }
    /* connect to scope shared memory block */
    shm_id = rtapi_shmem_new(SCOPE_SHM_KEY, comp_id, SCOPE_SHM_SIZE);
    if (shm_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SCOPE: ERROR: failed to get shared memory\n");
	hal_exit(comp_id);
	return -1;
    }
    retval = rtapi_shmem_getptr(shm_id, &shm_base);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SCOPE: ERROR: failed to map shared memory\n");
	rtapi_shmem_delete(shm_id, comp_id);
	hal_exit(comp_id);
	return -1;
    }

    /* init control structure */
    ctrl = &ctrl_struct;
    init_rt_control_struct(ctrl, shm_base);

/* FIXME - resume cleanup here */

    /* export scope data sampling function */
    retval = hal_export_funct("scope.sample", sample, ctrl, 0, 0, comp_id);
    if (retval != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "SCOPE_RT: ERROR: sample funct export failed\n");
	hal_exit(comp_id);
	return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "SCOPE_RT: installed sample function\n");

    /* was 'period' specified in the insmod command? */
    if (period > 0) {
	/* yes, create a thread */
	retval = hal_create_thread("scope.thread", period, 0, comp_id);
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"SCOPE_RT: ERROR: could not create thread\n");
	    hal_exit(comp_id);
	    return -1;
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO,
		"SCOPE_RT: created %d uS thread\n", period / 1000);
	}
    }
    return 0;
}

void rtapi_app_exit(void)
{
    rtapi_shmem_delete(shm_id, comp_id);
    hal_exit(comp_id);
}

/***********************************************************************
*            REALTIME SCOPE DATA ACQUISITION FUNCTION                  *
************************************************************************/

static void sample(void *arg, long period)
{
    scope_rt_control_t *ctrl;
    scope_shm_control_t *share;

    ctrl = arg;
    share = ctrl->shared;
    share->watchdog = 0;
    share->mult_cntr++;
    if (share->mult_cntr < share->mult) {
	/* not time to do anything yet */
	return;
    }
    /* reset counter */
    share->mult_cntr = 0;
    /* run the sampling state machine */
    switch (share->state) {
    case IDLE:
	/* do nothing while waiting for INIT */
	break;
    case INIT:
	/* init start pointer, curr pointer, sample count */
	share->curr = 0;
	share->start = share->curr;
	share->samples = 0;
	share->force_trig = 0;
	/* set next state */
	share->state = PRE_TRIG;
	break;
    case PRE_TRIG:
	/* acquire a sample */
/* FIXME - uncomment this call later capture_sample(ctrl); */
	/* increment sample counter */
	share->samples++;
	/* check if all pre-trigger samples captured */
	if (share->samples >= share->pre_trig) {
	    /* yes - start waiting for trigger */
	    share->state = TRIG_WAIT;
	}
	break;
    case TRIG_WAIT:
	/* acquire a sample */
/* FIXME - uncomment this call later capture_sample(ctrl); */
	/* increment start pointer (discard oldest pre-trig sample) */
	share->start += share->sample_len;
	/* is there a valid sample here, or end of buffer? */
	if ((share->start + share->sample_len) > share->buf_len) {
	    /* end of buffer, wrap back to beginning */
	    share->start = 0;
	}
	/* check if trigger condition met */
	if (check_trigger(ctrl)) {
	    /* yes - start acquiring post trigger data */
	    share->state = POST_TRIG;
	}
	break;
    case POST_TRIG:
	/* acquire a sample */
/* FIXME - uncomment this call later capture_sample(ctrl); */
	/* increment sample counter */
	share->samples++;
	/* check if all post-trigger samples captured */
	if (share->samples >= share->rec_len) {
	    /* yes - stop sampling */
	    share->state = DONE;
	}
	break;
    case DONE:
	/* do nothing while GUI displays waveform */
	break;
    default:
	/* shouldn't get here - if we do, set a legal state */
	share->state = IDLE;
	break;
    }
    /* done */
}

#if 0
static void capture_sample(scope_rt_control_t * ctrl)
{
    scope_data_t buffer[];
    int n;
    scope_data_t *dest;

    buffer = (char *) (ctrl) + sizeof(scope_control_t);
    dest = &(buffer[ctrl->curr]);
    /* loop through all channels to acquire data */
    for (n = 0; n < NUM_CHAN; n++) {
	/* capture 1, 2, or 4 bytes, based on data size */
	switch (ctrl->size[n]) {
	case 1:
	    dest->d1 = *((unsigned char *) (ctrl->data[n]));
	    dest++;
	    break;
	case 2:
	    dest->d2 = *((unsigned short *) (ctrl->data[n]));
	    dest++;
	    break;
	case 4:
	    dest->d4 = *((unsigned long *) (ctrl->data[n]));
	    dest++;
	    break;
	case 0:
	    break;
	}
    }
    /* increment sample pointer */
    ctrl->curr += ctrl->active_chans;
    /* is there room in the buffer for another sample? */
    if ((ctrl->curr + ctrl->active_chans) > ctrl->buf_len) {
	/* no, wrap back to beginning of buffer */
	ctrl->curr = 0;
    }
}
#endif

static int check_trigger(scope_rt_control_t * ctrl)
{
    if (ctrl->shared->force_trig != 0) {
	return 1;
    }
    return 0;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static void init_rt_control_struct(scope_rt_control_t * ctrl, void *shmem)
{
    char *cp;
    int n, skip;
    hal_comp_t *comp;

    /* first clear entire struct to all zeros */
    cp = (char *) ctrl;
    for (n = 0; n < sizeof(scope_rt_control_t); n++) {
	cp[n] = 0;
    }
    /* save pointer to shared control structure */
    ctrl->shared = shmem;
    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & !3;
    /* the rest of the shared memory area is the data buffer */
    ctrl->buffer = (scope_data_t *) ((char *) (shmem)) + skip;
    /* is the user space component loaded already? */
    comp = halpr_find_comp_by_name("scope_gui");
    if (comp == NULL) {
	/* no, must init shared structure */
	init_shared_control_struct(ctrl->shared);
    }
    /* init remainder of local structure */

    /* done */
}

static void init_shared_control_struct(scope_shm_control_t * share)
{
    int skip;

    /* round size of shared struct up to a multiple of 4 for alignment */
    skip = (sizeof(scope_shm_control_t) + 3) & !3;
    /* remainder of shmem area is buffer */
    share->buf_len = (SCOPE_SHM_SIZE - skip) / sizeof(scope_data_t);
    share->watchdog = 0;
    share->mult = 1;
    share->mult_cntr = 0;
    share->rec_len = 0;
    share->sample_len = 0;
    share->pre_trig = 0;
    share->force_trig = 0;
    share->start = 0;
    share->curr = 0;
    share->samples = 0;
}

#if 0
static int export_counter(int num, counter_t * addr)
{
    int retval;
    char buf[HAL_NAME_LEN + 2];

    /* export pins for the quadrature inputs */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-A", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->phaseA), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-B", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->phaseB), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.phase-I", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->phaseZ), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the index enable input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.index-enable", num);
    retval = hal_pin_bit_new(buf, HAL_RD_WR, &(addr->index_ena), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for the reset input */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.reset", num);
    retval = hal_pin_bit_new(buf, HAL_RD, &(addr->reset), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for raw counts */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.rawcounts", num);
    retval = hal_param_s32_new(buf, &(addr->raw_count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for counts captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.counts", num);
    retval = hal_pin_s32_new(buf, HAL_WR, &(addr->count), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export pin for scaled position captured by update() */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.position", num);
    retval = hal_pin_float_new(buf, HAL_WR, &(addr->pos), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export parameter for scaling */
    rtapi_snprintf(buf, HAL_NAME_LEN, "encoder.%d.position-scale", num);
    retval = hal_param_float_new(buf, &(addr->pos_scale), comp_id);
    if (retval != 0) {
	return retval;
    }
    return 0;
}
#endif
