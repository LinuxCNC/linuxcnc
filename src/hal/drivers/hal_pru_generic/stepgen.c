//----------------------------------------------------------------------//
// Description: stepgen.c                                               //
// Code to interface to a PRU driven step generator                     //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2013-May    Charles Steinkuehler                                     //
//             Split into several files                                 //
//             Added support for PRU task list                          //
//             Refactored code to more closely match mesa-hostmot2      //
// 2012-Dec-30 Charles Steinkuehler                                     //
//             Initial version, based in part on:                       //
//               hal_pru.c      Micheal Halberler                       //
//               supply.c       Matt Shaver                             //
//               stepgen.c      John Kasunich                           //
//               hostmot2 code  Sebastian Kuzminsky                     //
//----------------------------------------------------------------------//
// This file is part of LinuxCNC HAL                                    //
//                                                                      //
// Copyright (C) 2012  Charles Steinkuehler                             //
//                     <charles AT steinkuehler DOT net>                //
//                                                                      //
// This program is free software; you can redistribute it and/or        //
// modify it under the terms of the GNU General Public License          //
// as published by the Free Software Foundation; either version 2       //
// of the License, or (at your option) any later version.               //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program; if not, write to the Free Software          //
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA        //
// 02110-1301, USA.                                                     //
//                                                                      //
// THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR       //
// ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE   //
// TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of      //
// harming persons must have provisions for completely removing power   //
// from all motors, etc, before persons enter any danger area.  All     //
// machinery must be designed to comply with local and national safety  //
// codes, and the authors of this software can not, and do not, take    //
// any responsibility for such compliance.                              //
//                                                                      //
// This code was written as part of the LinuxCNC project.  For more     //
// information, go to www.linuxcnc.org.                                 //
//----------------------------------------------------------------------//

// Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H
#include "config_module.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

// #include RTAPI_INC_LIST_H
// #include "rtapi.h"          /* RTAPI realtime OS API */
// #include "rtapi_app.h"      /* RTAPI realtime module decls */
// #include "rtapi_math.h"
// #include "hal.h"            /* HAL public API decls */
// #include <pthread.h>
// 
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <sys/types.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/hal_pru_generic/hal_pru_generic.h"


#define f_period_s ((double)(l_period_ns * 1e-9))


// Start out with default pulse length/width and setup/hold delays of 1 mS (1000000 nS) 
#define DEFAULT_DELAY 1000000


/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/
// 
// read accumulator to figure out where the stepper has gotten to
// 

void hpg_stepgen_read(hal_pru_generic_t *hpg, long l_period_ns) {
    // Read data from the PRU here...
    int i;

    for (i = 0; i < hpg->stepgen.num_instances; i ++) {
        s64 x, y;
        u32 acc;
        s64 acc_delta;

        // "atomic" read of accumulator and position register from PRU
        y = * (s64 *) ((u32) hpg->pru_data + hpg->stepgen.instance[i].task.addr + (u32) offsetof(PRU_task_stepdir_t, accum));
        do {
            x = y;
            y = * (s64 *) ((u32) hpg->pru_data + hpg->stepgen.instance[i].task.addr + (u32) offsetof(PRU_task_stepdir_t, accum));
        } while ( x != y );

        // Update internal state
        hpg->stepgen.instance[i].pru.accum = x & 0xFFFFFFFF;
        hpg->stepgen.instance[i].pru.pos   = x >> 32;

        *(hpg->stepgen.instance[i].hal.pin.test1) = hpg->stepgen.instance[i].pru.accum;
        *(hpg->stepgen.instance[i].hal.pin.test2) = hpg->stepgen.instance[i].pru.pos;

        // Mangle 32-bit step count and 27 bit accumulator (with 5 bits of status)
        // into a 16.16 value to match the hostmot2 stepgen logic and generally make
        // things less confusing
        acc  = (hpg->stepgen.instance[i].pru.accum >> 11) & 0x0000FFFF;
        acc |= (hpg->stepgen.instance[i].pru.pos << 16);

        *(hpg->stepgen.instance[i].hal.pin.test3) = acc;

        // those tricky users are always trying to get us to divide by zero
        if (rtapi_fabs(*(hpg->stepgen.instance[i].hal.pin.position_scale)) < 1e-6) {
            if (*(hpg->stepgen.instance[i].hal.pin.position_scale) >= 0.0) {
                *(hpg->stepgen.instance[i].hal.pin.position_scale) = 1.0;
                HPG_ERR("stepgen %d position_scale is too close to 0, resetting to 1.0\n", i);
            } else {
                *(hpg->stepgen.instance[i].hal.pin.position_scale) = -1.0;
                HPG_ERR("stepgen %d position_scale is too close to 0, resetting to -1.0\n", i);
            }
        }

        // The HM2 Accumulator Register is a 16.16 bit fixed-point
        // representation of the current stepper position.
        // The fractional part gives accurate velocity at low speeds, and
        // sub-step position feedback (like sw stepgen).
        acc_delta = (s64)acc - (s64)hpg->stepgen.instance[i].prev_accumulator;
        if (acc_delta > INT32_MAX) {
            acc_delta -= UINT32_MAX;
        } else if (acc_delta < INT32_MIN) {
            acc_delta += UINT32_MAX;
        }

        hpg->stepgen.instance[i].subcounts += acc_delta;

        *(hpg->stepgen.instance[i].hal.pin.counts) = hpg->stepgen.instance[i].subcounts >> 16;

        // note that it's important to use "subcounts/65536.0" instead of just
        // "counts" when computing position_fb, because position_fb needs sub-count
        // precision
        *(hpg->stepgen.instance[i].hal.pin.position_fb) = ((double)hpg->stepgen.instance[i].subcounts / 65536.0) / *(hpg->stepgen.instance[i].hal.pin.position_scale);

        hpg->stepgen.instance[i].prev_accumulator = acc;

    }
}

//
// Here's the stepgen position controller.  It uses first-order
// feedforward and proportional error feedback.  This code is based
// on John Kasunich's software stepgen code.
//

static void hpg_stepgen_instance_position_control(hal_pru_generic_t *hpg, long l_period_ns, int i, double *new_vel) {
    double ff_vel;
    double velocity_error;
    double match_accel;
    double seconds_to_vel_match;
    double position_at_match;
    double position_cmd_at_match;
    double error_at_match;
    double velocity_cmd;


    hpg_stepgen_instance_t *s = &hpg->stepgen.instance[i];

    *(s->hal.pin.dbg_pos_minus_prev_cmd) = *(s->hal.pin.position_fb) - s->old_position_cmd;

    // calculate feed-forward velocity in machine units per second
    ff_vel = (*(s->hal.pin.position_cmd) - s->old_position_cmd) / f_period_s;
    *(s->hal.pin.dbg_ff_vel) = ff_vel;

    s->old_position_cmd = *(s->hal.pin.position_cmd);

    velocity_error = *(s->hal.pin.velocity_fb) - ff_vel;
    *(s->hal.pin.dbg_vel_error) = velocity_error;

    // Do we need to change speed to match the speed of position-cmd?
    // If maxaccel is 0, there's no accel limit: fix this velocity error
    // by the next servo period!  This leaves acceleration control up to
    // the trajectory planner.
    // If maxaccel is not zero, the user has specified a maxaccel and we
    // adhere to that.
    if (velocity_error > 0.0) {
        if (*(s->hal.pin.maxaccel) == 0) {
            match_accel = -velocity_error / f_period_s;
        } else {
            match_accel = -*(s->hal.pin.maxaccel);
        }
    } else if (velocity_error < 0.0) {
        if (*(s->hal.pin.maxaccel) == 0) {
            match_accel = velocity_error / f_period_s;
        } else {
            match_accel = *(s->hal.pin.maxaccel);
        }
    } else {
        match_accel = 0;
    }

    if (match_accel == 0) {
        // vel is just right, dont need to accelerate
        seconds_to_vel_match = 0.0;
    } else {
        seconds_to_vel_match = -velocity_error / match_accel;
    }
    *(s->hal.pin.dbg_s_to_match) = seconds_to_vel_match;

    // compute expected position at the time of velocity match
    // Note: this is "feedback position at the beginning of the servo period after we attain velocity match"
    {
        double avg_v;
        avg_v = (ff_vel + *s->hal.pin.velocity_fb) * 0.5;
        position_at_match = *s->hal.pin.position_fb + (avg_v * (seconds_to_vel_match + f_period_s));
    }

    // Note: this assumes that position-cmd keeps the current velocity
    position_cmd_at_match = *s->hal.pin.position_cmd + (ff_vel * seconds_to_vel_match);
    error_at_match = position_at_match - position_cmd_at_match;

    *s->hal.pin.dbg_err_at_match = error_at_match;

    if (seconds_to_vel_match < f_period_s) {
        // we can match velocity in one period
        // try to correct whatever position error we have
        velocity_cmd = ff_vel - (0.5 * error_at_match / f_period_s);

        // apply accel limits?
        if (*(s->hal.pin.maxaccel) > 0) {
            if (velocity_cmd > (*(s->hal.pin.velocity_fb) + (*(s->hal.pin.maxaccel) * f_period_s))) {
                velocity_cmd =  *(s->hal.pin.velocity_fb) + (*(s->hal.pin.maxaccel) * f_period_s);
            } else if (velocity_cmd < (*(s->hal.pin.velocity_fb) - (*(s->hal.pin.maxaccel) * f_period_s))) {
                velocity_cmd = *s->hal.pin.velocity_fb - (*(s->hal.pin.maxaccel) * f_period_s);
            }
        }

    } else {
        // we're going to have to work for more than one period to match velocity

        double dv;  // delta V, change in velocity
        double dp;  // delta P, change in position

        /* calculate change in final position if we ramp in the opposite direction for one period */
        dv = -2.0 * match_accel * f_period_s;   // Change in velocity if we apply match_accel the opposite direction
        dp = dv * seconds_to_vel_match;         // Resulting position change if we invert match_accel

        /* decide which way to ramp */
        if (rtapi_fabs(error_at_match + (dp * 2.0)) < rtapi_fabs(error_at_match)) {
            match_accel = -match_accel;
        }

        /* and do it */
        velocity_cmd = *s->hal.pin.velocity_fb + (match_accel * f_period_s);
    }

    *new_vel = velocity_cmd;
}


// This function was invented by Jeff Epler.
// It forces a floating-point variable to be degraded from native register
// size (80 bits on x86) to C double size (64 bits).
static double force_precision(double d) __attribute__((__noinline__));
static double force_precision(double d) {
    return d;
}

static void update_stepgen(hal_pru_generic_t *hpg, long l_period_ns, int i) {
    double new_vel;

    double physical_maxvel;  // max vel supported by current step timings & position-scale
    double maxvel;           // actual max vel to use this time

    double steps_per_sec_cmd;

    hpg_stepgen_instance_t *s = &(hpg->stepgen.instance[i]);


    //
    // first sanity-check our maxaccel and maxvel params
    //

    // maxvel must be >= 0.0, and may not be faster than 1 step per (steplen+stepspace) seconds
    {
        double min_ns_per_step = (s->pru.steplen + s->pru.stepspace) * hpg->config.pru_period;
        double max_steps_per_s = 1.0e9 / min_ns_per_step;


        physical_maxvel = max_steps_per_s / rtapi_fabs(*(s->hal.pin.position_scale));
        physical_maxvel = force_precision(physical_maxvel);

        if (*(s->hal.pin.maxvel) < 0.0) {
            HPG_ERR("stepgen.%02d.maxvel < 0, setting to its absolute value\n", i);
            *(s->hal.pin.maxvel) = rtapi_fabs(*(s->hal.pin.maxvel));
        }

        if (*(s->hal.pin.maxvel) > physical_maxvel) {
            HPG_ERR("stepgen.%02d.maxvel is too big for current step timings & position-scale, clipping to max possible\n", i);
            *(s->hal.pin.maxvel) = physical_maxvel;
        }

        if (*(s->hal.pin.maxvel) == 0.0) {
            maxvel = physical_maxvel;
        } else {
            maxvel = *(s->hal.pin.maxvel);
        }
    }

    // maxaccel may not be negative
    if (*(s->hal.pin.maxaccel) < 0.0) {
        HPG_ERR("stepgen.%02d.maxaccel < 0, setting to its absolute value\n", i);
        *(s->hal.pin.maxaccel) = rtapi_fabs(*(s->hal.pin.maxaccel));
    }


    // select the new velocity we want
    if (*(s->hal.pin.control_type) == 0) {
        hpg_stepgen_instance_position_control(hpg, l_period_ns, i, &new_vel);
    } else {
        // velocity-mode control is easy
        new_vel = *s->hal.pin.velocity_cmd;
        if (*(s->hal.pin.maxaccel) > 0.0) {
            if (((new_vel - *s->hal.pin.velocity_fb) / f_period_s) > *(s->hal.pin.maxaccel)) {
                new_vel = *(s->hal.pin.velocity_fb) + (*(s->hal.pin.maxaccel) * f_period_s);
            } else if (((new_vel - *s->hal.pin.velocity_fb) / f_period_s) < -*(s->hal.pin.maxaccel)) {
                new_vel = *(s->hal.pin.velocity_fb) - (*(s->hal.pin.maxaccel) * f_period_s);
            }
        }
    }

    // clip velocity to maxvel
    if (new_vel > maxvel) {
        new_vel = maxvel;
    } else if (new_vel < -maxvel) {
        new_vel = -maxvel;
    }

    *s->hal.pin.velocity_fb = (hal_float_t)new_vel;

    steps_per_sec_cmd = new_vel * *(s->hal.pin.position_scale);
    s->pru.rate = steps_per_sec_cmd * (double)0x08000000 * (double) hpg->config.pru_period * 1e-9;
    
    // clip rate just to be safe...should be limited by code above
    if ((s->pru.rate < 0x80000000) && (s->pru.rate > 0x03FFFFFF)) {
        s->pru.rate = 0x03FFFFFF;
    } else if ((s->pru.rate >= 0x80000000) && (s->pru.rate < 0xFC000001)) {
        s->pru.rate = 0xFC000001;
    }

    *s->hal.pin.dbg_step_rate = s->pru.rate;
}

int export_stepgen(hal_pru_generic_t *hpg, int i)
{
    int r;

    // Pins
    r = hal_pin_float_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.position_cmd), hpg->config.comp_id, "%s.stepgen.%02d.position-cmd", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'position-cmd', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.velocity_cmd), hpg->config.comp_id, "%s.stepgen.%02d.velocity-cmd", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'velocity-cmd', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.velocity_fb), hpg->config.comp_id, "%s.stepgen.%02d.velocity-fb", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'velocity-fb', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.position_fb), hpg->config.comp_id, "%s.stepgen.%02d.position-fb", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'position-fb', aborting\n", i);
        return r;
    }

    r = hal_pin_s32_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.counts), hpg->config.comp_id, "%s.stepgen.%02d.counts", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'counts', aborting\n", i);
        return r;
    }

    r = hal_pin_bit_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.enable), hpg->config.comp_id, "%s.stepgen.%02d.enable", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'enable', aborting\n", i);
        return r;
    }

    r = hal_pin_bit_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.control_type), hpg->config.comp_id, "%s.stepgen.%02d.control-type", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'control-type', aborting\n", i);
        return r;
    }

    // debug pins

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_pos_minus_prev_cmd), hpg->config.comp_id, "%s.stepgen.%02d.dbg_pos_minus_prev_cmd", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_pos_minus_prev_cmd', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_ff_vel), hpg->config.comp_id, "%s.stepgen.%02d.dbg_ff_vel", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_ff_vel', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_s_to_match), hpg->config.comp_id, "%s.stepgen.%02d.dbg_s_to_match", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_s_to_match', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_vel_error), hpg->config.comp_id, "%s.stepgen.%02d.dbg_vel_error", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_vel_error', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_err_at_match), hpg->config.comp_id, "%s.stepgen.%02d.dbg_err_at_match", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_err_at_match', aborting\n", i);
        return r;
    }

    r = hal_pin_s32_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.dbg_step_rate), hpg->config.comp_id, "%s.stepgen.%02d.dbg_step_rate", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dbg_step_rate', aborting\n", i);
        return r;
    }

    r = hal_pin_s32_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.test1), hpg->config.comp_id, "%s.stepgen.%02d.test1", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'test1', aborting\n", i);
        return r;
    }

    r = hal_pin_s32_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.test2), hpg->config.comp_id, "%s.stepgen.%02d.test2", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'test2', aborting\n", i);
        return r;
    }

    r = hal_pin_s32_newf(HAL_OUT, &(hpg->stepgen.instance[i].hal.pin.test3), hpg->config.comp_id, "%s.stepgen.%02d.test3", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'test3', aborting\n", i);
        return r;
    }

    // param pins

    r = hal_pin_float_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.position_scale), hpg->config.comp_id, "%s.stepgen.%02d.position-scale", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'position-scale', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.maxvel), hpg->config.comp_id, "%s.stepgen.%02d.maxvel", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'maxvel', aborting\n", i);
        return r;
    }

    r = hal_pin_float_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.maxaccel), hpg->config.comp_id, "%s.stepgen.%02d.maxaccel", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'maxaccel', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.steplen), hpg->config.comp_id, "%s.stepgen.%02d.steplen", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'steplen', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.stepspace), hpg->config.comp_id, "%s.stepgen.%02d.stepspace", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'stepspace', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.dirsetup), hpg->config.comp_id, "%s.stepgen.%02d.dirsetup", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dirsetup', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.dirhold), hpg->config.comp_id, "%s.stepgen.%02d.dirhold", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dirhold', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.steppin), hpg->config.comp_id, "%s.stepgen.%02d.steppin", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'steppin', aborting\n", i);
        return r;
    }

    r = hal_pin_u32_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.dirpin), hpg->config.comp_id, "%s.stepgen.%02d.dirpin", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'dirpin', aborting\n", i);
        return r;
    }

    r = hal_pin_bit_newf(HAL_IN, &(hpg->stepgen.instance[i].hal.pin.stepinv), hpg->config.comp_id, "%s.stepgen.%02d.stepinvert", hpg->config.halname, i);
    if (r < 0) {
        HPG_ERR("stepgen %02d: Error adding pin 'stepinvert', aborting\n", i);
        return r;
    }

    // init
    *(hpg->stepgen.instance[i].hal.pin.position_cmd) = 0.0;
    *(hpg->stepgen.instance[i].hal.pin.counts) = 0;
    *(hpg->stepgen.instance[i].hal.pin.position_fb) = 0.0;
    *(hpg->stepgen.instance[i].hal.pin.velocity_fb) = 0.0;
    *(hpg->stepgen.instance[i].hal.pin.enable) = 0;
    *(hpg->stepgen.instance[i].hal.pin.control_type) = 0;

    *(hpg->stepgen.instance[i].hal.pin.position_scale) = 1.0;
    *(hpg->stepgen.instance[i].hal.pin.maxvel) = 0.0;
    *(hpg->stepgen.instance[i].hal.pin.maxaccel) = 1.0;

    hpg->stepgen.instance[i].subcounts = 0;

    *(hpg->stepgen.instance[i].hal.pin.steplen)   = rtapi_ceil((double)DEFAULT_DELAY / (double)hpg->config.pru_period);
    *(hpg->stepgen.instance[i].hal.pin.stepspace) = rtapi_ceil((double)DEFAULT_DELAY / (double)hpg->config.pru_period);
    *(hpg->stepgen.instance[i].hal.pin.dirsetup)  = rtapi_ceil((double)DEFAULT_DELAY / (double)hpg->config.pru_period);
    *(hpg->stepgen.instance[i].hal.pin.dirhold)   = rtapi_ceil((double)DEFAULT_DELAY / (double)hpg->config.pru_period);

    hpg->stepgen.instance[i].written_steplen = 0;
    hpg->stepgen.instance[i].written_stepspace = 0;
    hpg->stepgen.instance[i].written_dirsetup = 0;
    hpg->stepgen.instance[i].written_dirhold = 0;
    hpg->stepgen.instance[i].written_task = 0;

    // Start with 1/2 step offset in accumulator
    //hpg->stepgen.instance[i].PRU.accum = 1 << 26;
    hpg->stepgen.instance[i].pru.accum = 0;
    hpg->stepgen.instance[i].prev_accumulator = 0;
    hpg->stepgen.instance[i].old_position_cmd = *(hpg->stepgen.instance[i].hal.pin.position_cmd);

    *( hpg->stepgen.instance[i].hal.pin.steppin) = PRU_DEFAULT_PIN;
    *(hpg->stepgen.instance[i].hal.pin.dirpin)  = PRU_DEFAULT_PIN;

    *(hpg->stepgen.instance[i].hal.pin.stepinv) = 0;

    return 0;
}

int hpg_stepgen_init(hal_pru_generic_t *hpg){
    int r,i;

    if (hpg->config.num_stepgens <= 0)
        return 0;

rtapi_print("hpg_stepgen_init\n");

    hpg->stepgen.num_instances = hpg->config.num_stepgens;

    // Allocate HAL shared memory for state data
    hpg->stepgen.instance = (hpg_stepgen_instance_t *) hal_malloc(sizeof(hpg_stepgen_instance_t) * hpg->stepgen.num_instances);
    if (hpg->stepgen.instance == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: hal_malloc() failed\n", hpg->config.name);
        hal_exit(hpg->config.comp_id);
        return -1;
    }

    // Clear memory
    memset(hpg->stepgen.instance, 0, (sizeof(hpg_stepgen_instance_t) * hpg->stepgen.num_instances) );

    for (i=0; i < hpg->config.num_stepgens; i++) {
        hpg->stepgen.instance[i].task.addr = pru_malloc(hpg, sizeof(hpg->stepgen.instance[i].pru));
        hpg->stepgen.instance[i].pru.task.hdr.mode = eMODE_STEP_DIR;
        pru_task_add(hpg, &(hpg->stepgen.instance[i].task));

        if ((r = export_stepgen(hpg,i)) != 0){ 
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: failed to export stepgen %i: %i\n", hpg->config.name,i,r);
            return -1;
        }
    }

    return 0;
}

void hpg_stepgen_update(hal_pru_generic_t *hpg, long l_period_ns) {
    int i;

    for (i = 0; i < hpg->stepgen.num_instances; i ++) {
        // Update shadow of PRU control registers
        if ((*(hpg->stepgen.instance[i].hal.pin.steppin) != hpg->stepgen.instance[i].written_steppin) ||
            (*(hpg->stepgen.instance[i].hal.pin.dirpin)  != hpg->stepgen.instance[i].written_dirpin)  )
        {
            hpg->stepgen.instance[i].pru.task.hdr.dataX = fixup_pin(*(hpg->stepgen.instance[i].hal.pin.steppin));
            hpg->stepgen.instance[i].pru.task.hdr.dataY = fixup_pin(*(hpg->stepgen.instance[i].hal.pin.dirpin));
            hpg->stepgen.instance[i].written_steppin    = *(hpg->stepgen.instance[i].hal.pin.steppin);
            hpg->stepgen.instance[i].written_dirpin     = *(hpg->stepgen.instance[i].hal.pin.dirpin);
        }

        if (*(hpg->stepgen.instance[i].hal.pin.enable) == 0) {
            hpg->stepgen.instance[i].pru.rate = 0;
            hpg->stepgen.instance[i].old_position_cmd = *(hpg->stepgen.instance[i].hal.pin.position_cmd);
            *(hpg->stepgen.instance[i].hal.pin.velocity_fb) = 0;
        } else {
            // call update function
            update_stepgen(hpg, l_period_ns, i);
        }

        PRU_task_stepdir_t *pru = (PRU_task_stepdir_t *) ((u32) hpg->pru_data + (u32) hpg->stepgen.instance[i].task.addr);

        // Update timing parameters if changed
        if ((*(hpg->stepgen.instance[i].hal.pin.dirsetup)  != hpg->stepgen.instance[i].written_dirsetup ) ||
            (*(hpg->stepgen.instance[i].hal.pin.dirhold)   != hpg->stepgen.instance[i].written_dirhold  ) ||
            (*(hpg->stepgen.instance[i].hal.pin.steplen)   != hpg->stepgen.instance[i].written_steplen  ) ||
            (*(hpg->stepgen.instance[i].hal.pin.stepspace) != hpg->stepgen.instance[i].written_stepspace) ||
            (*(hpg->stepgen.instance[i].hal.pin.stepinv)   != hpg->stepgen.instance[i].written_stepinv  ) )
        {
            hpg->stepgen.instance[i].pru.dirsetup   = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.dirsetup));
            hpg->stepgen.instance[i].pru.dirhold    = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.dirhold));
            hpg->stepgen.instance[i].pru.steplen    = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.steplen));
            hpg->stepgen.instance[i].pru.stepspace  = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.stepspace));
            hpg->stepgen.instance[i].pru.stepinv    =                 *(hpg->stepgen.instance[i].hal.pin.stepinv);

            // Send new value(s) to the PRU
            pru->dirsetup   = hpg->stepgen.instance[i].pru.dirsetup;
            pru->dirhold    = hpg->stepgen.instance[i].pru.dirhold;
            pru->steplen    = hpg->stepgen.instance[i].pru.steplen;
            pru->stepspace  = hpg->stepgen.instance[i].pru.stepspace;
            pru->stepinv    = hpg->stepgen.instance[i].pru.stepinv;

            // Stash values written
            hpg->stepgen.instance[i].written_dirsetup  = *(hpg->stepgen.instance[i].hal.pin.dirsetup);
            hpg->stepgen.instance[i].written_dirhold   = *(hpg->stepgen.instance[i].hal.pin.dirhold);
            hpg->stepgen.instance[i].written_steplen   = *(hpg->stepgen.instance[i].hal.pin.steplen);
            hpg->stepgen.instance[i].written_stepspace = *(hpg->stepgen.instance[i].hal.pin.stepspace);
            hpg->stepgen.instance[i].written_stepinv   = *(hpg->stepgen.instance[i].hal.pin.stepinv);
        }

        // Update control word if changed
        if (hpg->stepgen.instance[i].pru.task.raw.dword[0] != hpg->stepgen.instance[i].written_task) {
            pru->task.raw.dword[0]                = hpg->stepgen.instance[i].pru.task.raw.dword[0];
            hpg->stepgen.instance[i].written_task = hpg->stepgen.instance[i].pru.task.raw.dword[0];
        }

        // Send rate update to the PRU
        pru->rate = hpg->stepgen.instance[i].pru.rate;

    }
}

void hpg_stepgen_force_write(hal_pru_generic_t *hpg) {
    int i;

    if (hpg->stepgen.num_instances <= 0) return;

    for (i = 0; i < hpg->stepgen.num_instances; i ++) {

        hpg->stepgen.instance[i].pru.task.hdr.mode  = eMODE_STEP_DIR;
        hpg->stepgen.instance[i].pru.task.hdr.len   = 0;
        hpg->stepgen.instance[i].pru.task.hdr.dataX = fixup_pin(*(hpg->stepgen.instance[i].hal.pin.steppin));
        hpg->stepgen.instance[i].pru.task.hdr.dataY = fixup_pin(*(hpg->stepgen.instance[i].hal.pin.dirpin));
        hpg->stepgen.instance[i].pru.task.hdr.addr  = hpg->stepgen.instance[i].task.next;
        hpg->stepgen.instance[i].pru.rate           = 0;
        hpg->stepgen.instance[i].pru.steplen        = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.steplen));
        hpg->stepgen.instance[i].pru.dirhold        = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.dirhold));
        hpg->stepgen.instance[i].pru.stepspace      = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.stepspace));
        hpg->stepgen.instance[i].pru.dirsetup       = ns2periods(hpg, *(hpg->stepgen.instance[i].hal.pin.dirsetup));
        hpg->stepgen.instance[i].pru.accum          = 0;
        hpg->stepgen.instance[i].pru.pos            = 0;
        hpg->stepgen.instance[i].pru.reserved[0]    = 0;
        hpg->stepgen.instance[i].pru.reserved[1]    = 0;

        PRU_task_stepdir_t *pru = (PRU_task_stepdir_t *) ((u32) hpg->pru_data + (u32) hpg->stepgen.instance[i].task.addr);
        *pru = hpg->stepgen.instance[i].pru;
    }
}
