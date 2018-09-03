/*--------------------------------------------------------------------------
// Description: hal_arm335xQEP.c
// HAL module to implement quadrature decoding using the ARM335x eQEP
// Module
//
// Author(s): Russell Gower
// License: GNU GPL Version 2.0 or (at your option) any later version.
//
// Major Changes:
// 2014-Nov    Russell Gower
//             Initial implementation, based on encoderc.c by John Kasunich
//--------------------------------------------------------------------------
// This file is part of LinuxCNC HAL
//
// Copyright (C) 2014  Russell Gower
//                     <russell AT thegowers DOT me DOT uk>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//
// THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR
// ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
// TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
// harming persons must have provisions for completely removing power
// from all motors, etc, before persons enter any danger area.  All
// machinery must be designed to comply with local and national safety
// codes, and the authors of this software can not, and do not, take
// any responsibility for such compliance.
//
// This code was written as part of the LinuxCNC project.  For more
// information, go to www.linuxcnc.org.
//------------------------------------------------------------------------*/

/* Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H */
#include "config_module.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "hal.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hal_arm335xQEP.h"


/* this probably should be an ARM335x define */
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

/* Module information */
#define MODNAME "hal_arm335xQEP"
MODULE_AUTHOR("Russell Gower");
MODULE_DESCRIPTION("eQEP EMC HAL driver for ARM335x");
MODULE_LICENSE("GPL");

#define MAX_ENC 3
char *encoders[MAX_ENC] = {0,};
RTAPI_MP_ARRAY_STRING(encoders, MAX_ENC, "name of encoders");

/* Globals */
const devices_t devices[] = {
    {"eQEP0", 0x48300000},
    {"eQEP1", 0x48302000},
    {"eQEP2", 0x48304000},
    {NULL,-1}
};

static const char *modname = MODNAME;
static int comp_id;

static eqep_t *eqep_array; /* pointer to array of eqep_t structs in
                                    shmem, 1 per encoder */
static int howmany;
static hal_u32_t timebase;
/*---------------------
 Function prototypes
---------------------*/
static int export_encoder(eqep_t *addr);
static int setup_eQEP(eqep_t *eqep);
static void update(void *arg, long period);

/*---------------------
 INIT and EXIT CODE
---------------------*/

int rtapi_app_main(void)
{
    int n, retval, i;
    eqep_t *eqep;
    /* test for number of channels */
    for (howmany=0; howmany < MAX_ENC && encoders[howmany]; howmany++) ;

    if(howmany <= 0)  {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: invalid number of encoders: %d\n", modname, howmany);
        return -1;
    }

    /* have good config info, connect to the HAL */
    comp_id = hal_init(modname);
    if(comp_id < 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n",modname);
        return -1;
    }

    /* allocate shared memory for eqep data */
    eqep_array = hal_malloc(howmany * sizeof(eqep_t));
    if (eqep_array ==  0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: hal_malloc() failed\n",modname);
        hal_exit(comp_id);
        return -1;
    }

    timebase = 0;

    /* setup and export all the variables for each eQEP device */
    for (n = 0; n < howmany; n++) {
        eqep = &(eqep_array[n]);
        /* make sure it's a valid device */
        for(i=0; devices[i].name; i++) {
            retval = strcmp(encoders[n], devices[i].name);
            if (retval == 0 ) {
                eqep->name = devices[i].name;
                int fd = open("/dev/mem", O_RDWR);

                eqep->pwm_reg = mmap(0, IOMEMLEN, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, devices[i].addr);

                eqep->eqep_reg = (void*) eqep->pwm_reg + 0x180;

                close(fd);

                if(eqep->pwm_reg == MAP_FAILED) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: mmap failed %s\n", modname, eqep->name);
                    return -1;
                }
                rtapi_print("memmapped %s to %p and %p\n",eqep->name,eqep->pwm_reg,eqep->eqep_reg);
                setup_eQEP(eqep);
                break;
            }
        }

        if(retval != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unknown device %s\n",
                modname, encoders[n]);
            return -1;
        }
    }

    /* export functions */
    retval = hal_export_funct("eqep.update", update,
        eqep_array, 0, 0, comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: function export failed\n",modname);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
        "%s: installed %d encoder counters\n", modname, howmany);
    retval = hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/*---------------------
 Realtime functions
---------------------*/

static void update(void *arg, long period)
{
    hal_s32_t     i;
    hal_s32_t   delta_counts;
    hal_u32_t   delta_time;
    hal_u32_t   iflg;
    hal_float_t  vel,interp;
    eqep_t *eqep;

    eqep = arg;

    for(i = 0; i < howmany; i++){
        /* Read the hardware  */
        eqep->raw_count = eqep->eqep_reg->QPOSCNT;
        iflg = eqep->eqep_reg->QFLG & EQEP_INTERRUPT_MASK;

        /* check if an index event has occured */
        if( *(eqep->index_ena) && (iflg & IEL)) {
            eqep->index_count = eqep->eqep_reg->QPOSILAT;
            *(eqep->index_ena) = 0;
        }

        /* check for phase errors */
        if( iflg & PHE ) {
            *(eqep->phase_error_count)++;
        }
        /* clear interrupt flags */
        eqep->eqep_reg->QCLR = iflg;

        /* check for changes in scale */
        if ( *(eqep->pos_scale) != eqep->old_scale ){
            eqep->old_scale = *(eqep->pos_scale);
            /* sanity check new value */
            if ((*(eqep->pos_scale) < 1e-20) && (*(eqep->pos_scale) > -1e-20)) {
                /* value is too small */
                *(eqep->pos_scale) = 1.0;
            }
            /* we want the reciprocal */
            eqep->scale = 1.0 / *(eqep->pos_scale);
        }

        /* has counter_mode been changed? */
        if ( *(eqep->counter_mode) != eqep->old_counter_mode ) {
            eqep->eqep_reg->QDECCTL ^= QSRC0;
            eqep->old_counter_mode = *(eqep->counter_mode);
        }

        /* has x2_mode been changed? */
        if ( *(eqep->x2_mode) != eqep->old_x2_mode ) {
            eqep->eqep_reg->QDECCTL ^= XCR;
            eqep->old_x2_mode = *(eqep->x2_mode);
        }

        /* has invert_A been changed? */
        if ( *(eqep->invertA) != eqep->old_invertA ) {
            eqep->eqep_reg->QDECCTL ^= QAP;
            eqep->old_invertA = *(eqep->invertA);
        }

        /* has invert_B been changed? */
        if ( *(eqep->invertB) != eqep->old_invertB ) {
            eqep->eqep_reg->QDECCTL ^= QBP;
            eqep->old_invertB = *(eqep->invertB);
        }

        /* has invert_Z been changed? */
        if ( *(eqep->invertZ) != eqep->old_invertZ ) {
            eqep->eqep_reg->QDECCTL ^= QIP;
            eqep->old_invertZ = *(eqep->invertZ);
        }

        /* has the capture prescaler been changed? */
        if ( *(eqep->capture_pre) != eqep->old_capture_pre ) {
            hal_u32_t active_pre;

            eqep->eqep_reg->QCAPCTL &= ~(CEN); // disable to prevent prescaler problems
            eqep->eqep_reg->QCAPCTL &= ~(CCPS0 | CCPS1 | CCPS2); // clear prescaler
            if (*(eqep->capture_pre) < 8u) {
                active_pre = *(eqep->capture_pre);
            }
            else {
                active_pre = 7u;  // clamp prescaler
            }
            eqep->eqep_reg->QCAPCTL |= active_pre << CCPS;
            eqep->eqep_reg->QCAPCTL |= CEN; // enable eQEP capture timer

            eqep->old_capture_pre = *(eqep->capture_pre);
            // prescale the capture tick, bit shift = division with base 2
            eqep->capture_freq = SYSCLKOUT / (hal_float_t)(1 << active_pre);
        }

        /* check for valid min_speed */
        if ( *(eqep->min_speed) <= 0.0 ) {
            *(eqep->min_speed) = 1.0;
        }

        /* check reset input */
        if (*(eqep->reset)) {
            /* reset is active, reset the counter */
            /* note: we NEVER reset raw_counts, that is always a
            running count of the edges seen since startup. The
            public "count" is the difference between raw_count
            and index_count, so it will become zero. */
            eqep->index_count = eqep->raw_count;
        }

        /* check for movement */
        if ( eqep->raw_count != eqep->old_raw_count ) {
            *(eqep->raw_counts) = eqep->raw_count;

            delta_counts = eqep->raw_count - eqep->old_raw_count;
            if (delta_counts < 0) {
                eqep->reverse = true;
                delta_counts = -delta_counts;
            }
            else {
                eqep->reverse = false;
            }
            delta_time = timebase - eqep->timestamp;
            eqep->old_raw_count = eqep->raw_count;
            eqep->timestamp = timebase;

        } else { /* no counts */
            delta_counts = 0;
        }

        /* decide which velocity estimation method to use */
        if (delta_counts >= *(eqep->capture_threshold)) {
            vel = ((hal_float_t)delta_counts * eqep->scale) / ((hal_float_t)delta_time * 1e-9);
        }
        else { /* use capture timer for velocity */
            if (eqep->eqep_reg->QEPSTS & UPEVNT) { /* we had an up event */
                hal_s32_t period_count;
                eqep->eqep_reg->QEPSTS = UPEVNT;  /* clear UPEVNT */
                period_count = eqep->eqep_reg->QCPRDLAT;
                *(eqep->capture_period) = period_count;
                vel = eqep->capture_freq / ((hal_float_t)period_count * *(eqep->pos_scale));
            }
            else { /* guess the velocity */
                hal_s32_t timer_count;
                timer_count = eqep->eqep_reg->QCTMRLAT;
                vel =  eqep->capture_freq / ((hal_float_t)timer_count * *(eqep->pos_scale));
                if (vel < *(eqep->min_speed)) {  /* no guessing beyond min speed */
                    vel = 0.0;
                    *(eqep->capture_period) = 0.0;
                }
                else if (vel > eqep->old_vel) {  /* use only if slower */
                    vel = eqep->old_vel;
                }
                else {
                    *(eqep->capture_period) = timer_count;
                }
            }
            /* apply direction to velocity */
            if (eqep->eqep_reg->QEPSTS & COEF) { /* overflow event */
                eqep->eqep_reg->QEPSTS = COEF;
                *(eqep->capture_overflow_count) += 1;
                *(eqep->capture_period) = 0;
                vel = 0.0;
            }
            if (eqep->eqep_reg->QEPSTS & CDEF) {  /* dir change event */
                eqep->eqep_reg->QEPSTS = CDEF;
                *(eqep->capture_dir_change_count) += 1;
                *(eqep->capture_period) = 0;
                vel = 0.0;
            }
        }

        /* store and apply velocity */
        eqep->old_vel = vel;
        if (eqep->reverse) {
            vel = -vel;
        }
        *(eqep->vel) = vel;

        /* compute net counts */
        *(eqep->count) = eqep->raw_count - eqep->index_count;

        *(eqep->pos) = *(eqep->count) * eqep->scale;

        /* add interpolation value */
        delta_time = timebase - eqep->timestamp;
        interp = *(eqep->vel) * (delta_time * 1e-9);
        *(eqep->pos_interp) = *(eqep->pos) + interp;


        /* move on to the next channel */
        eqep++;
    }
    timebase += (hal_u32_t)period;
}


/*---------------------
 Local functions
---------------------*/
static int setup_eQEP(eqep_t *eqep)
{
    export_encoder(eqep);
    *(eqep->raw_counts) = 0;
    *(eqep->count) = 0;
    *(eqep->min_speed) = 1.0;
    *(eqep->pos) = 0.0;
    *(eqep->pos_scale) = 1.0;
    *(eqep->vel) = 0.0;
    *(eqep->phase_error_count) = 0;
    *(eqep->counter_mode) = false;
    *(eqep->x2_mode) = false;
    *(eqep->invertA) = false;
    *(eqep->invertB) = false;
    *(eqep->invertZ) = false;
    *(eqep->capture_period) = 0;
    *(eqep->capture_threshold) = 100;
    *(eqep->capture_overflow_count) = 0;
    *(eqep->capture_dir_change_count) = 0;
    eqep->old_raw_count = 0;
    eqep->old_scale = 1.0;
    eqep->raw_count = 0;
    eqep->timestamp = 0;
    eqep->index_count = 0;
    eqep->counts_since_timeout = 0;
    eqep->scale = 1.0 / *(eqep->pos_scale);
    eqep->reverse = false;
    eqep->old_vel = 0.0;
    eqep->old_counter_mode = false;
    eqep->old_x2_mode = false;
    eqep->old_invertA = false;
    eqep->old_invertB = false;
    eqep->old_invertZ = false;
    eqep->old_capture_pre = 255u;
    eqep->capture_freq = 0.0;

    eqep->eqep_reg->QDECCTL = XCR; /* start in x1 resolution */
    eqep->eqep_reg->QPOSINIT = 0;
    eqep->eqep_reg->QPOSMAX = -1;
    eqep->eqep_reg->QEINT |= (IEL | PHE);
    eqep->eqep_reg->QEPCTL = PHEN | IEL0 | IEL1 | SWI |PCRM0;

    eqep->eqep_reg->QCAPCTL = 0u; // reset to prevent prescaler problems
    eqep->eqep_reg->QCAPCTL |= CEN; // enable eQEP capture

    rtapi_print("%s: REVID = %#x\n",modname, eqep->eqep_reg->QREVID);
    return 0;
}

static int export_encoder(eqep_t *eqep)
{
    if (hal_pin_bit_newf(HAL_IO, &(eqep->index_ena), comp_id, "%s.index-enable", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting index-enable\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->reset), comp_id, "%s.reset", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting reset\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_IO, &(eqep->count), comp_id, "%s.counts", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting counts\n");
        return -1;
    }
    if (hal_pin_float_newf(HAL_IO, &(eqep->pos_scale), comp_id, "%s.position-scale", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting position-scale\n");
        return -1;
    }
    if (hal_pin_float_newf(HAL_IN, &(eqep->min_speed), comp_id, "%s.min-speed-estimate", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting min-speed-estimate\n");
        return -1;
    }
    if (hal_pin_float_newf(HAL_OUT, &(eqep->pos_interp), comp_id, "%s.position-interpolated", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting position-interpolated\n");
        return -1;
    }
    if (hal_pin_float_newf(HAL_OUT, &(eqep->vel), comp_id, "%s.velocity", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting velocity\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_OUT, &(eqep->phase_error_count), comp_id, "%s.phase-errors", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting phase_errors\n");
        return -1;
    }
    if (hal_pin_float_newf(HAL_OUT, &(eqep->pos), comp_id, "%s.position", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting position\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_OUT, &(eqep->raw_counts), comp_id, "%s.rawcounts", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting rawcounts\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->counter_mode), comp_id, "%s.counter-mode", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting counter mode\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->x2_mode), comp_id, "%s.x2-mode", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting x2 mode\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->invertA), comp_id, "%s.invert-A", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting invert_A mode\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->invertB), comp_id, "%s.invert-B", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting invert_B mode\n");
        return -1;
    }
    if (hal_pin_bit_newf(HAL_IO, &(eqep->invertZ), comp_id, "%s.invert-Z", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting invert_Z mode\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_OUT, &(eqep->capture_period), comp_id, "%s.capture-period", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting capture-period\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_OUT, &(eqep->capture_overflow_count), comp_id, "%s.capture-overflows", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting capture-overflows\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_OUT, &(eqep->capture_dir_change_count), comp_id, "%s.capture-dir-changes", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting capture-dir-changes\n");
        return -1;
    }
    if (hal_pin_s32_newf(HAL_IN, &(eqep->capture_threshold), comp_id, "%s.capture-threshold", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting capture-threshold\n");
        return -1;
    }
    if (hal_pin_u32_newf(HAL_IN, &(eqep->capture_pre), comp_id, "%s.capture-prescaler", eqep->name)) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Error exporting capture-prescaler\n");
        return -1;
    }

    return 0;
}

