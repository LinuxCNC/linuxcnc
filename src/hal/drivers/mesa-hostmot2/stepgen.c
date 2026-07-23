
//
//    Copyright (C) 2007-2009 Sebastian Kuzminsky
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <rtapi_slab.h>

#include <rtapi.h>
#include <rtapi_string.h>
#include <rtapi_math.h>

#include <hal.h>

#include "hostmot2.h"


#define f_period_s ((double)(l_period_ns * 1e-9))




// 
// read accumulator to figure out where the stepper has gotten to
// 

void hm2_stepgen_process_tram_read(hostmot2_t *hm2, long l_period_ns) {
    (void)l_period_ns;
    int i;
    rtapi_u32 mode = 0;
    rtapi_u32 latch = 0;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_instance_t *inst = &hm2->stepgen.instance[i];
        rtapi_u32 acc = hm2->stepgen.accumulator_reg[i];
        if (hm2->stepgen.firmware_supports_index) {
            mode = hm2->stepgen.mode_reg[i] & HM2_STEPGEN_MODE_MASK ;
            latch = hm2->stepgen.mode_reg[i] & HM2_STEPGEN_LATCH_MASK;
        }
        rtapi_s64 acc_delta;
        rtapi_s64 latch_delta;

        // those tricky users are always trying to get us to divide by zero
        rtapi_real position_scale = hal_get_real(inst->hal.param.position_scale);
        if (fabs(position_scale) < 1e-6) {
            if (position_scale >= 0.0) {
                position_scale = hal_set_real(inst->hal.param.position_scale, 1.0);
                HM2_ERR("stepgen %d position_scale is too close to 0, resetting to 1.0\n", i);
            } else {
                position_scale = hal_set_real(inst->hal.param.position_scale, -1.0);
                HM2_ERR("stepgen %d position_scale is too close to 0, resetting to -1.0\n", i);
            }
        }

        // The HM2 Accumulator Register is a 16.16 bit fixed-point
        // representation of the current stepper position.
        // The fractional part gives accurate velocity at low speeds, and
        // sub-step position feedback (like sw stepgen).
        acc_delta = (rtapi_s64)acc - (rtapi_s64)inst->prev_accumulator;
        if (acc_delta > RTAPI_INT32_MAX) {
            acc_delta -= RTAPI_UINT32_MAX;
        } else if (acc_delta < RTAPI_INT32_MIN) {
            acc_delta += RTAPI_UINT32_MAX;
        }



        if (hm2->stepgen.firmware_supports_index) {
            if (inst->written_index_enable)  {     // searching for index
                if ((mode & HM2_STEPGEN_LATCH_ON_INDEX) == 0) {   // hardware index detected and position at index latched
                    latch_delta = (rtapi_s64)latch - (rtapi_s64)inst->prev_accumulator;
                    if (latch_delta > RTAPI_INT32_MAX) {
                        latch_delta -= RTAPI_UINT32_MAX;
                    } else if (latch_delta < RTAPI_INT32_MIN) {
                        latch_delta += RTAPI_UINT32_MAX;
                    }
                    hal_set_real(inst->hal.pin.position_latch, ((double)(inst->subcounts+latch_delta) / 65536.0) / position_scale);
                    acc_delta -= latch_delta;		
                    inst->subcounts = 0;
                    hal_set_bool(inst->hal.pin.index_enable, 0);
                    inst->written_index_enable  = 0;
                }
            }
            else if (inst->written_probe_enable) {
                if ((mode & HM2_STEPGEN_LATCH_ON_PROBE) == 0) {
                    latch_delta = (rtapi_s64)latch - (rtapi_s64)inst->prev_accumulator;
                    if (latch_delta > RTAPI_INT32_MAX) {
                        latch_delta -= RTAPI_UINT32_MAX;
                    } else if (latch_delta < RTAPI_INT32_MIN) {
                        latch_delta += RTAPI_UINT32_MAX;
                    }
                    hal_set_real(inst->hal.pin.position_latch, ((double)(inst->subcounts+latch_delta) / 65536.0) / position_scale);
                    hal_set_bool(inst->hal.pin.latch_enable, 0);
                    inst->written_probe_enable = 0;
                } 
            }
        }

        inst->subcounts += acc_delta;

        if (hal_get_bool(inst->hal.pin.position_reset)) {
            inst->subcounts = 0;
        }

        hal_set_si32(inst->hal.pin.counts, inst->subcounts >> 16);

        // note that it's important to use "subcounts/65536.0" instead of just
        // "counts" when computing position_fb, because position_fb needs sub-count
        // precision
        hal_set_real(inst->hal.pin.position_fb, ((double)inst->subcounts / 65536.0) / position_scale);
        inst->prev_accumulator = acc;
    }    
}




//
// Here's the stepgen position controller.  It uses first-order
// feedforward and proportional error feedback.  This code is based
// on John Kasunich's software stepgen code.
//

static void hm2_stepgen_instance_position_control(hostmot2_t *hm2, long l_period_ns, int i, double *new_vel) {
    double ff_vel;
    double velocity_error;
    double match_accel;
    double seconds_to_vel_match;
    double position_at_match;
    double position_cmd_at_match;
    double error_at_match;
    double velocity_cmd;

    hm2_stepgen_instance_t *s = &hm2->stepgen.instance[i];


    hal_set_real(s->hal.pin.dbg_pos_minus_prev_cmd, hal_get_real(s->hal.pin.position_fb) - s->old_position_cmd);

    // calculate feed-forward velocity in machine units per second
    rtapi_real position_cmd = hal_get_real(s->hal.pin.position_cmd);
    ff_vel = (position_cmd - s->old_position_cmd) / f_period_s;
    hal_set_real(s->hal.pin.dbg_ff_vel, ff_vel);

    s->old_position_cmd = position_cmd;

    rtapi_real velocity_fb = hal_get_real(s->hal.pin.velocity_fb);
    velocity_error = velocity_fb - ff_vel;
    hal_set_real(s->hal.pin.dbg_vel_error, velocity_error);

    // Do we need to change speed to match the speed of position-cmd?
    // If maxaccel is 0, there's no accel limit: fix this velocity error
    // by the next servo period!  This leaves acceleration control up to
    // the trajectory planner.
    // If maxaccel is not zero, the user has specified a maxaccel and we
    // adhere to that.
    if (velocity_error > 0.0) {
        rtapi_real maxaccel = hal_get_real(s->hal.param.maxaccel);
        if (maxaccel == 0) {
            match_accel = -velocity_error / f_period_s;
        } else {
            match_accel = -maxaccel;
        }
    } else if (velocity_error < 0.0) {
        rtapi_real maxaccel = hal_get_real(s->hal.param.maxaccel);
        if (maxaccel == 0) {
            match_accel = velocity_error / f_period_s;
        } else {
            match_accel = maxaccel;
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
    hal_set_real(s->hal.pin.dbg_s_to_match, seconds_to_vel_match);

    // compute expected position at the time of velocity match
    // Note: this is "feedback position at the beginning of the servo period after we attain velocity match"
    {
        double avg_v;
        avg_v = (ff_vel + velocity_fb) * 0.5;
        position_at_match = hal_get_real(s->hal.pin.position_fb) + (avg_v * (seconds_to_vel_match + f_period_s));
    }

    // Note: this assumes that position-cmd keeps the current velocity
    position_cmd_at_match = position_cmd + (ff_vel * seconds_to_vel_match);
    error_at_match = position_at_match - position_cmd_at_match;

    hal_set_real(s->hal.pin.dbg_err_at_match, error_at_match);

    if (seconds_to_vel_match < f_period_s) {
        // we can match velocity in one period
        // try to correct whatever position error we have
        velocity_cmd = ff_vel - (0.5 * error_at_match / f_period_s);

        // apply accel limits?
        rtapi_real maxaccel = hal_get_real(s->hal.param.maxaccel);
        if (maxaccel > 0) {
            if (velocity_cmd > (velocity_fb + (maxaccel * f_period_s))) {
                velocity_cmd = velocity_fb + (maxaccel * f_period_s);
            } else if (velocity_cmd < (velocity_fb - (maxaccel * f_period_s))) {
                velocity_cmd = velocity_fb - (maxaccel * f_period_s);
            }
        }

    } else {
        // we're going to have to work for more than one period to match velocity
        // FIXME: I dont really get this part yet

        double dv;
        double dp;

        /* calculate change in final position if we ramp in the opposite direction for one period */
        dv = -2.0 * match_accel * f_period_s;
        dp = dv * seconds_to_vel_match;

        /* decide which way to ramp */
        if (fabs(error_at_match + (dp * 2.0)) < fabs(error_at_match)) {
            match_accel = -match_accel;
        }

        /* and do it */
        velocity_cmd = velocity_fb + (match_accel * f_period_s);
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


static void hm2_stepgen_instance_prepare_tram_write(hostmot2_t *hm2, long l_period_ns, int i) {
    double new_vel;

    double physical_maxvel;  // max vel supported by current step timings & position-scale
    double maxvel;           // actual max vel to use this time

    double steps_per_sec_cmd;

    hm2_stepgen_instance_t *s = &hm2->stepgen.instance[i];


    //
    // first sanity-check our maxaccel and maxvel params
    //

    // maxvel must be >= 0.0, and may not be faster than 1 step per (steplen+stepspace) seconds
    {
        double min_ns_per_step = hal_get_ui32(s->hal.param.steplen) + hal_get_ui32(s->hal.param.stepspace);
        double max_steps_per_s = 1.0e9 / min_ns_per_step;

        physical_maxvel = max_steps_per_s / fabs(hal_get_real(s->hal.param.position_scale));
        physical_maxvel = force_precision(physical_maxvel);

        rtapi_real pmaxvel = hal_get_real(s->hal.param.maxvel);
        if (pmaxvel < 0.0) {
            HM2_ERR("stepgen.%02d.maxvel < 0, setting to its absolute value\n", i);
            pmaxvel = hal_set_real(s->hal.param.maxvel, fabs(pmaxvel));
        }

        if (pmaxvel > physical_maxvel) {
            HM2_ERR("stepgen.%02d.maxvel is too big for current step timings & position-scale, clipping to max possible\n", i);
            pmaxvel= hal_set_real(s->hal.param.maxvel, physical_maxvel);
        }

        if (pmaxvel == 0.0) {
            maxvel = physical_maxvel;
        } else {
            maxvel = pmaxvel;
        }
    }

    // maxaccel may not be negative
    rtapi_real maxaccel = hal_get_real(s->hal.param.maxaccel);
    if (maxaccel < 0.0) {
        HM2_ERR("stepgen.%02d.maxaccel < 0, setting to its absolute value\n", i);
        maxaccel = hal_set_real(s->hal.param.maxaccel, fabs(maxaccel));
    }


    // select the new velocity we want
    if (!hal_get_bool(s->hal.pin.control_type)) {
        hm2_stepgen_instance_position_control(hm2, l_period_ns, i, &new_vel);
    } else {
        // velocity-mode control is easy
        new_vel = hal_get_real(s->hal.pin.velocity_cmd);
        rtapi_real velocity_fb = hal_get_real(s->hal.pin.velocity_fb);
        if (maxaccel > 0.0) {
            if (((new_vel - velocity_fb) / f_period_s) > maxaccel) {
                new_vel = velocity_fb + (maxaccel * f_period_s);
            } else if (((new_vel - velocity_fb) / f_period_s) < -maxaccel) {
                new_vel = velocity_fb - (maxaccel * f_period_s);
            }
        }
    }

    // clip velocity to maxvel
    if (new_vel > maxvel) {
        new_vel = maxvel;
    } else if (new_vel < -maxvel) {
        new_vel = -maxvel;
    }


    hal_set_real(s->hal.pin.velocity_fb, (rtapi_real)new_vel);

    steps_per_sec_cmd = new_vel * hal_get_real(s->hal.param.position_scale);
    // the double cast here is intentional.  (uint32_t)(-1.0) is undefined in
    // C (and in practice it gives the undesired value 0 on arm systems), but
    // (uint32_t)(int32-t)(-1.0) is defined and gives the desired value on all
    // systems.
    hm2->stepgen.step_rate_reg[i] = (uint32_t)(int32_t)(steps_per_sec_cmd * (4294967296.0 / (double)hm2->stepgen.clock_frequency));

    hal_set_si32(s->hal.pin.dbg_step_rate, hm2->stepgen.step_rate_reg[i]);
}


void hm2_stepgen_prepare_tram_write(hostmot2_t *hm2, long l_period_ns) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        if (!hal_get_bool(hm2->stepgen.instance[i].hal.pin.enable)) {
            hm2->stepgen.step_rate_reg[i] = 0;
            hm2->stepgen.instance[i].old_position_cmd = hal_get_real(hm2->stepgen.instance[i].hal.pin.position_cmd);
            hal_set_real(hm2->stepgen.instance[i].hal.pin.velocity_fb, 0);
        } else {
            hm2_stepgen_instance_prepare_tram_write(hm2, l_period_ns, i);
        }
    }
}




static void hm2_stepgen_update_dir_setup_time(hostmot2_t *hm2, int i) {
    rtapi_u32 dirsetup = hal_get_ui32(hm2->stepgen.instance[i].hal.param.dirsetup);
    hm2->stepgen.dir_setup_time_reg[i] = (double)dirsetup * ((double)hm2->stepgen.clock_frequency / (double)1e9);
    if (hm2->stepgen.dir_setup_time_reg[i] > 0x3FFF) {
        HM2_ERR("stepgen %d has invalid dirsetup, resetting to max\n", i);
        hm2->stepgen.dir_setup_time_reg[i] = 0x3FFF;
        dirsetup = hal_set_ui32(hm2->stepgen.instance[i].hal.param.dirsetup, (double)hm2->stepgen.dir_setup_time_reg[i] * ((double)1e9 / (double)hm2->stepgen.clock_frequency));
    }
    hm2->stepgen.instance[i].written_dirsetup = dirsetup;
}


static void hm2_stepgen_update_dir_hold_time(hostmot2_t *hm2, int i) {
    rtapi_u32 dirhold = hal_get_ui32(hm2->stepgen.instance[i].hal.param.dirhold);
    hm2->stepgen.dir_hold_time_reg[i] = (double)dirhold * ((double)hm2->stepgen.clock_frequency / (double)1e9);
    if (hm2->stepgen.dir_hold_time_reg[i] > 0x3FFF) {
        HM2_ERR("stepgen %d has invalid dirhold, resetting to max\n", i);
        hm2->stepgen.dir_hold_time_reg[i] = 0x3FFF;
        dirhold = hal_set_ui32(hm2->stepgen.instance[i].hal.param.dirhold, (double)hm2->stepgen.dir_hold_time_reg[i] * ((double)1e9 / (double)hm2->stepgen.clock_frequency));
    }
    hm2->stepgen.instance[i].written_dirhold = dirhold;
}


static void hm2_stepgen_update_pulse_idle_width(hostmot2_t *hm2, int i) {
    rtapi_u32 stepspace = hal_get_ui32(hm2->stepgen.instance[i].hal.param.stepspace);
    hm2->stepgen.pulse_idle_width_reg[i] = (double)stepspace * ((double)hm2->stepgen.clock_frequency / (double)1e9);
    if (hm2->stepgen.pulse_idle_width_reg[i] > 0x3FFF) {
        HM2_ERR("stepgen %d has invalid stepspace, resetting to max\n", i);
        hm2->stepgen.pulse_idle_width_reg[i] = 0x3FFF;
        stepspace = hal_set_ui32(hm2->stepgen.instance[i].hal.param.stepspace, (double)hm2->stepgen.pulse_idle_width_reg[i] * ((double)1e9 / (double)hm2->stepgen.clock_frequency));
    }
    hm2->stepgen.instance[i].written_stepspace = stepspace;
}


static void hm2_stepgen_update_pulse_width(hostmot2_t *hm2, int i) {
    rtapi_u32 steplen = hal_get_ui32(hm2->stepgen.instance[i].hal.param.steplen);
    hm2->stepgen.pulse_width_reg[i] = (double)steplen * ((double)hm2->stepgen.clock_frequency / (double)1e9);
    if (hm2->stepgen.pulse_width_reg[i] > 0x3FFF) {
        HM2_ERR("stepgen %d has invalid steplen, resetting to max\n", i);
        hm2->stepgen.pulse_width_reg[i] = 0x3FFF;
        steplen = hal_set_ui32(hm2->stepgen.instance[i].hal.param.steplen, (double)hm2->stepgen.pulse_width_reg[i] * ((double)1e9 / (double)hm2->stepgen.clock_frequency));
    }
    hm2->stepgen.instance[i].written_steplen = steplen;
}


static void hm2_stepgen_update_mode(hostmot2_t *hm2, int i) {
    rtapi_u32 buff;
    rtapi_u32 modebuff;
    int j;
    hm2_stepgen_instance_t *inst =  &hm2->stepgen.instance[i];
    
    // No point coming back unless something changes
    rtapi_u32 step_type = hal_get_ui32(inst->hal.param.step_type);
    inst->written_step_type = step_type;
    if (hm2->stepgen.firmware_supports_swap) {
        inst->written_swap_step_dir  = hal_get_bool(inst->hal.param.swap_step_dir);
    }
    if (hm2->stepgen.firmware_supports_index) {
        inst->written_index_polarity  = hal_get_bool(inst->hal.pin.index_polarity);
        inst->written_probe_polarity  = hal_get_bool(inst->hal.pin.latch_polarity);
    } 
    inst->tablehash = (((hal_get_ui32(inst->hal.param.table[0])  ^ hal_get_ui32(inst->hal.param.table[1])) 
                       ^ hal_get_ui32(inst->hal.param.table[2])) ^ hal_get_ui32(inst->hal.param.table[3]));

    modebuff = step_type;  // start with the step mode


    if (hm2->stepgen.firmware_supports_index) {  // or in the index/probe control bits if needed
       if (hal_get_bool(inst->hal.pin.index_enable)) {
	    modebuff |= HM2_STEPGEN_LATCH_ON_INDEX;
        }
        if (hal_get_bool(inst->hal.pin.index_polarity)) {
	    modebuff |= HM2_STEPGEN_INDEX_POLARITY;
        }
        if (hal_get_bool(inst->hal.pin.latch_enable)) {
	    modebuff |= HM2_STEPGEN_LATCH_ON_PROBE;
        }
        if (hal_get_bool(inst->hal.pin.latch_polarity)) {
	    modebuff |= HM2_STEPGEN_PROBE_POLARITY;
        }
    }

                                                                             
    if (step_type <= 2) {
        
        if (hm2->stepgen.firmware_supports_swap) {
            if  (hal_get_bool(inst->hal.param.swap_step_dir)) {
                modebuff |= HM2_STEPGEN_SWAP_STEP_DIR;
            }
	}
        hm2->stepgen.mode_reg[i] = modebuff;        
        return;
    } 

    if (inst->table_width < step_type){
        HM2_ERR("the firmware only supports %i pins in the step pattern for "
                "stepgen instance %i, you asked for %i. Reverting to step type 0\n",
                inst->table_width, 
                i,
                step_type);
        hm2->stepgen.mode_reg[i] = 0;
    }
    if (step_type > 16){
        HM2_ERR("the firmware only supports tables up to a depth of 16, you"
                "requested %i. Reverting to step type 0\n",
                step_type);
        hm2->stepgen.mode_reg[i] = 0;
	// Assure that we do not index outside the param.table[] array in the
	// loop below. But the code may be off anyway and the result is
	// anybody's guess.
        step_type = 16;
    }
    // In that case, we can assume that we have been fed a step table and the
    // step_type is actually the table length. 

    for (j = step_type - 1; j >= 0 ; j--){
        buff = ((hal_get_ui32(inst->hal.param.table[j / 4]) >> ((j % 4) * 8)) & 0xFF);
        hm2->llio->write(hm2->llio, hm2->stepgen.table_sequence_data_setup_addr 
                + (i * sizeof(rtapi_u32)), &buff, sizeof(rtapi_u32));
    }
    
    hm2->stepgen.mode_reg[i] = 3 | modebuff; // force 2 LSbs to 1,1 to select table mode in stepgen hardware

    buff = step_type -1;	    
    
    hm2->llio->write(hm2->llio, hm2->stepgen.table_sequence_length_addr
            + (i * sizeof(rtapi_u32)), &buff, sizeof(rtapi_u32));
}


static void hm2_stepgen_set_dpll_timer(hostmot2_t *hm2) {
    rtapi_u32 data = 0;
    rtapi_s32 dpll_timer_num = hal_get_si32(hm2->stepgen.hal->pin.dpll_timer_num);
    if ((dpll_timer_num < -1) || (dpll_timer_num > 4)) {
        dpll_timer_num = hal_set_si32(hm2->stepgen.hal->pin.dpll_timer_num, 0);
    }
    if (dpll_timer_num > -1) {
        data = (dpll_timer_num << 12) | (1 << 15);
    }
    hm2->llio->write(hm2->llio, hm2->stepgen.dpll_timer_num_addr, &data, sizeof(rtapi_u32));
    hm2->stepgen.written_dpll_timer_num = dpll_timer_num;
}


void hm2_stepgen_write(hostmot2_t *hm2) {
    int i;
    int need_mode_update;

    // FIXME
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_instance_t *inst = &hm2->stepgen.instance[i];
        if (hal_get_ui32(inst->hal.param.dirsetup) != inst->written_dirsetup) {
            hm2_stepgen_update_dir_setup_time(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.dir_setup_time_addr + (i * sizeof(rtapi_u32)), &hm2->stepgen.dir_setup_time_reg[i], sizeof(rtapi_u32));
        }

        if (hal_get_ui32(inst->hal.param.dirhold) != inst->written_dirhold) {
            hm2_stepgen_update_dir_hold_time(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.dir_hold_time_addr + (i * sizeof(rtapi_u32)), &hm2->stepgen.dir_hold_time_reg[i], sizeof(rtapi_u32));
        }

        if (hal_get_ui32(inst->hal.param.steplen) != inst->written_steplen) {
            hm2_stepgen_update_pulse_width(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.pulse_width_addr + (i * sizeof(rtapi_u32)), &hm2->stepgen.pulse_width_reg[i], sizeof(rtapi_u32));
        }

        if (hal_get_ui32(inst->hal.param.stepspace) != inst->written_stepspace) {
            hm2_stepgen_update_pulse_idle_width(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.pulse_idle_width_addr + (i * sizeof(rtapi_u32)), &hm2->stepgen.pulse_idle_width_reg[i], sizeof(rtapi_u32));
        }

        need_mode_update=0;

	if ((hal_get_ui32(inst->hal.param.step_type) != inst->written_step_type) 
           || (((hal_get_ui32(inst->hal.param.table[0]) ^ hal_get_ui32(inst->hal.param.table[1])) 
               ^ hal_get_ui32(inst->hal.param.table[2])) ^ hal_get_ui32(inst->hal.param.table[3]))
           != inst->tablehash) {   
            need_mode_update = 1;
        }

        if (hm2->stepgen.firmware_supports_swap) {
            if (hal_get_bool(inst->hal.param.swap_step_dir) != inst->written_swap_step_dir) {
                need_mode_update |= 1;
            }
        }

        if (hm2->stepgen.firmware_supports_index) {
            if ((hal_get_bool(inst->hal.pin.index_enable) != inst->written_index_enable)
               || (hal_get_bool(inst->hal.pin.latch_enable) != inst->written_probe_enable)
               || (hal_get_bool(inst->hal.pin.index_polarity) != inst->written_index_polarity)
               || (hal_get_bool(inst->hal.pin.latch_polarity) != inst->written_probe_polarity)) {
                need_mode_update |= 1;
            }
        }
        if (need_mode_update) {
            hm2_stepgen_update_mode(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.mode_addr + (i * sizeof(rtapi_u32)), &hm2->stepgen.mode_reg[i], sizeof(rtapi_u32));
            if (hm2->stepgen.firmware_supports_index) {
                inst->written_index_enable  = hal_get_bool(inst->hal.pin.index_enable); // we need to update these only after the write has occurred
                inst->written_probe_enable  = hal_get_bool(inst->hal.pin.latch_enable); // to avoid a race condition (index detected before index enable has been set)
            }
        }
    }

    if (hm2->stepgen.num_instances > 0 && hm2->dpll_module_present) {
        if (hal_get_si32(hm2->stepgen.hal->pin.dpll_timer_num) != (int)hm2->stepgen.written_dpll_timer_num) {
            hm2_stepgen_set_dpll_timer(hm2);
        }
    }
}


//static void hm2_stepgen_force_write_mode(hostmot2_t *hm2) {
//    int i;
//    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
//        hm2_stepgen_update_mode(hm2, i);
//    }
//    hm2->llio->write(
//        hm2->llio,
//        hm2->stepgen.mode_addr,
//        hm2->stepgen.mode_reg,
//        (hm2->stepgen.num_instances * sizeof(rtapi_u32))
//    );
//}


static void hm2_stepgen_force_write_dir_setup_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_dir_setup_time(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.dir_setup_time_addr,
        hm2->stepgen.dir_setup_time_reg,
        (hm2->stepgen.num_instances * sizeof(rtapi_u32))
    );
    
}


static void hm2_stepgen_force_write_dir_hold_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_dir_hold_time(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.dir_hold_time_addr,
        hm2->stepgen.dir_hold_time_reg,
        (hm2->stepgen.num_instances * sizeof(rtapi_u32))
    );
}


static void hm2_stepgen_force_write_pulse_idle_width(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_pulse_idle_width(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.pulse_idle_width_addr,
        hm2->stepgen.pulse_idle_width_reg,
        (hm2->stepgen.num_instances * sizeof(rtapi_u32))
    );
}


static void hm2_stepgen_force_write_pulse_width_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_pulse_width(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.pulse_width_addr,
        hm2->stepgen.pulse_width_reg,
        (hm2->stepgen.num_instances * sizeof(rtapi_u32))
    );
}


static void hm2_stepgen_force_write_master_dds(hostmot2_t *hm2) {
    rtapi_u32 val = 0xffffffff;
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.master_dds_addr,
        &val,
        sizeof(rtapi_u32)
    );
}

static void hm2_stepgen_force_write_dpll_timer(hostmot2_t *hm2) {
    if (hm2->stepgen.num_instances > 0 && hm2->dpll_module_present) {
        hm2_stepgen_set_dpll_timer(hm2);
    }
}


void hm2_stepgen_force_write(hostmot2_t *hm2) {
    if (hm2->stepgen.num_instances == 0) return;
//    hm2_stepgen_force_write_mode(hm2);
    hm2_stepgen_force_write_dir_setup_time(hm2);
    hm2_stepgen_force_write_dir_hold_time(hm2);
    hm2_stepgen_force_write_pulse_width_time(hm2);
    hm2_stepgen_force_write_pulse_idle_width(hm2);
    hm2_stepgen_force_write_master_dds(hm2);
    hm2_stepgen_force_write_dpll_timer(hm2);
}




void hm2_stepgen_tram_init(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2->stepgen.instance[i].prev_accumulator = hm2->stepgen.accumulator_reg[i];
        hm2->stepgen.instance[i].old_position_cmd = hal_get_real(hm2->stepgen.instance[i].hal.pin.position_cmd);
    }
}




void hm2_stepgen_allocate_pins(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->num_pins; i ++) {
        if (
            (hm2->pin[i].sec_tag != HM2_GTAG_STEPGEN)
            || ((hm2->pin[i].sec_unit >= hm2->stepgen.num_instances) & (hm2->pin[i].sec_unit != 0x80))
            || ((hm2->pin[i].sec_pin & 0x7F) > hm2->config.stepgen_width) & (hm2->pin[i].sec_pin >= 0x80)) {
            continue;
        }

        hm2_set_pin_source(hm2, i, HM2_PIN_SOURCE_IS_SECONDARY);
        if (hm2->pin[i].sec_pin & 0x80){
            hm2_set_pin_direction_at_start(hm2, i, HM2_PIN_DIR_IS_OUTPUT);
        }
    }
}




int hm2_stepgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //
     hm2->stepgen.firmware_supports_swap = 0;
    hm2->stepgen.firmware_supports_index = 0;
    if (hm2_md_is_consistent(hm2, md_index, 0, 10, 4, 0x01FF)) {
        HM2_PRINT("WARNING: this firmware has stepgen v0!\n");
        HM2_PRINT("WARNING: high step rates require zero stepspace!\n");
        HM2_PRINT("WARNING: upgrade your firmware!\n");
    } else if (hm2_md_is_consistent(hm2, md_index, 1, 10, 4, 0x01FF)) {
        HM2_PRINT("WARNING: this firmware has stepgen v1!\n");
        HM2_PRINT("WARNING: this version is susceptible to waveform timing violations in certain configurations!\n");
        HM2_PRINT("WARNING: upgrade your firmware!\n");
    } else if (hm2_md_is_consistent(hm2, md_index, 2, 10, 4, 0x01FF)) {
        // this one is ok, as far as we know
    } else if (hm2_md_is_consistent(hm2, md_index, 0x42, 10, 4, 0x01FF)) {
        // This version supports swap
        hm2->stepgen.firmware_supports_swap = 1;
    } else if (hm2_md_is_consistent(hm2, md_index, 0xC2, 10, 4, 0x01FF)) {
        // This version supports swap and index/probe
        hm2->stepgen.firmware_supports_swap = 1;
        hm2->stepgen.firmware_supports_index = 1;
    } else {
        HM2_ERR("unknown stepgen MD:\n");
        HM2_ERR("    Version = %d, expected 0-2\n", md->version);
        HM2_ERR("    NumRegisters = %d, expected 10\n", md->num_registers);
        HM2_ERR("    InstanceStride = 0x%08X, expected 4\n", md->instance_stride);
        HM2_ERR("    MultipleRegisters = 0x%08X, expected 0x000001FF\n", md->multiple_registers);
        return -EINVAL;
    }

    if (hm2->stepgen.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_stepgens > md->instances) {
        HM2_ERR(
            "config.num_stepgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_stepgens,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_stepgens == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_stepgens == -1) {
        hm2->stepgen.num_instances = md->instances;
    } else {
        hm2->stepgen.num_instances = hm2->config.num_stepgens;
    }


    // allocate the module-global HAL shared memory
    hm2->stepgen.hal = hal_malloc(sizeof(*hm2->stepgen.hal));
    if (hm2->stepgen.hal == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->stepgen.instance = hal_malloc(hm2->stepgen.num_instances * sizeof(*hm2->stepgen.instance));
    if (hm2->stepgen.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->stepgen.clock_frequency = md->clock_freq;
    hm2->stepgen.version = md->version;

    hm2->stepgen.step_rate_addr = md->base_address + (0 * md->register_stride);
    hm2->stepgen.accumulator_addr = md->base_address + (1 * md->register_stride);
    hm2->stepgen.mode_addr = md->base_address + (2 * md->register_stride);
    hm2->stepgen.dir_setup_time_addr = md->base_address + (3 * md->register_stride);
    hm2->stepgen.dir_hold_time_addr = md->base_address + (4 * md->register_stride);
    hm2->stepgen.pulse_width_addr = md->base_address + (5 * md->register_stride);
    hm2->stepgen.pulse_idle_width_addr = md->base_address + (6 * md->register_stride);
    hm2->stepgen.table_sequence_data_setup_addr = md->base_address + (7 * md->register_stride);
    hm2->stepgen.table_sequence_length_addr = md->base_address + (8 * md->register_stride);
    hm2->stepgen.master_dds_addr = md->base_address + (9 * md->register_stride);
    hm2->stepgen.dpll_timer_num_addr = md->base_address + (10 * md->register_stride);

    r = hm2_register_tram_write_region(hm2, hm2->stepgen.step_rate_addr, (hm2->stepgen.num_instances * sizeof(rtapi_u32)), &hm2->stepgen.step_rate_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for StepGen Step Rate register (%d)\n", r);
        goto fail0;
    }
    
//    if (hm2->stepgen.firmware_supports_index) {
//        r = hm2_register_tram_write_region(hm2, hm2->stepgen.mode_addr, (hm2->stepgen.num_instances * sizeof(rtapi_u32)), &hm2->stepgen.mode_reg);
//        if (r < 0) {
//            HM2_ERR("error registering tram write region for StepGen mode register (%d)\n", r);
//            goto fail0;
//        }
//    }

    r = hm2_register_tram_read_region(hm2, hm2->stepgen.accumulator_addr, (hm2->stepgen.num_instances * sizeof(rtapi_u32)), &hm2->stepgen.accumulator_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for StepGen Accumulator register (%d)\n", r);
        goto fail0;
    }

    if (hm2->stepgen.firmware_supports_index) {
        r = hm2_register_tram_read_region(hm2, hm2->stepgen.mode_addr, (hm2->stepgen.num_instances * sizeof(rtapi_u32)), &hm2->stepgen.mode_reg);
        if (r < 0) {
            HM2_ERR("error registering tram read region for StepGen mode register (%d)\n", r);
            goto fail0;
        }
    }
 
    hm2->stepgen.mode_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->stepgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->stepgen.mode_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->stepgen.dir_setup_time_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->stepgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->stepgen.dir_setup_time_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }

    hm2->stepgen.dir_hold_time_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->stepgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->stepgen.dir_hold_time_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail2;
    }

    hm2->stepgen.pulse_width_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->stepgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->stepgen.pulse_width_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail3;
    }

    hm2->stepgen.pulse_idle_width_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->stepgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->stepgen.pulse_idle_width_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail4;
    }


    // export to HAL

    {
        if (hm2->dpll_module_present) {
            r = hal_pin_new_si32(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.hal->pin.dpll_timer_num),
                                 -1, "%s.stepgen.timer-number", hm2->llio->name);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.timer-number', aborting\n", r, hm2->llio->name);
                return -EINVAL;
            }
        }

        for (int i = 0; i < hm2->stepgen.num_instances; i ++) {
            
            // Work out if table setup registers are needed. 
            {
                hm2->stepgen.instance[i].table_width = 0;
                for (int j = 0; j < hm2->num_pins; j++){
                    if (hm2->pin[j].sec_tag == HM2_GTAG_STEPGEN && hm2->pin[j].sec_unit == i){
                        if (hm2->pin[j].sec_pin > hm2->stepgen.instance[i].table_width){
                            hm2->stepgen.instance[i].table_width =  hm2->pin[j].sec_pin;
                        }
                    }
                }
            }
            // pins
            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.position_cmd),
                                 0.0, "%s.stepgen.%02d.position-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.position-cmd', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.velocity_cmd),
                                 0.0, "%s.stepgen.%02d.velocity-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.velocity-cmd', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.velocity_fb),
                                 0.0, "%s.stepgen.%02d.velocity-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.velocity-fb', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.position_fb),
                                 0.0, "%s.stepgen.%02d.position-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.position-fb', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_si32(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.counts),
                                 0, "%s.stepgen.%02d.counts", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.counts', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.enable),
                                 0, "%s.stepgen.%02d.enable", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.enable', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.control_type),
                                 0, "%s.stepgen.%02d.control-type", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.control-type', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.position_reset),
                                 0, "%s.stepgen.%02d.position-reset", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.position-reset', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            if (hm2->stepgen.firmware_supports_index) {

                r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.position_latch),
                                     0.0, "%s.stepgen.%02d.position-latch", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding pin '%s.stepgen.%02d.position-latch', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }

                r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IO, &(hm2->stepgen.instance[i].hal.pin.index_enable),
                                     0, "%s.stepgen.%02d.index-enable", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding pin '%s.stepgen.%02d.index-enable', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
     
                r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IO, &(hm2->stepgen.instance[i].hal.pin.latch_enable),
                                     0, "%s.stepgen.%02d.probe-enable", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding pin '%s.stepgen.%02d.probe-enable', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }

                r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.index_polarity),
                                     0, "%s.stepgen.%02d.index-invert", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding pin '%s.stepgen.%02d.index-invert', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
 
                r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.latch_polarity),
                                     0, "%s.stepgen.%02d.probe-invert", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding pin '%s.stepgen.%02d.probe-invert', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
            }

            // debug pins

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_pos_minus_prev_cmd),
                                 0.0, "%s.stepgen.%02d.dbg_pos_minus_prev_cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_pos_minus_prev_cmd', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_ff_vel),
                                 0.0, "%s.stepgen.%02d.dbg_ff_vel", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_ff_vel', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_s_to_match),
                                 0.0, "%s.stepgen.%02d.dbg_s_to_match", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_s_to_match', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_vel_error),
                                 0.0, "%s.stepgen.%02d.dbg_vel_error", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_vel_error', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_err_at_match),
                                 0.0, "%s.stepgen.%02d.dbg_err_at_match", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_err_at_match', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_pin_new_si32(hm2->llio->comp_id, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.dbg_step_rate),
                                 0, "%s.stepgen.%02d.dbg_step_rate", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.stepgen.%02d.dbg_step_rate', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }


            // parameters
            r = hal_param_new_real(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.position_scale),
                                   1.0, "%s.stepgen.%02d.position-scale", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.position-scale', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_real(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.maxvel),
                                   0.0, "%s.stepgen.%02d.maxvel", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.maxvel', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_real(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.maxaccel),
                                   1.0, "%s.stepgen.%02d.maxaccel", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.maxaccel', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            // start out the slowest possible, let the user speed up if they want
            rtapi_u32 v = (double)0x3FFF * ((double)1e9 / (double)hm2->stepgen.clock_frequency);
            r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.steplen),
                                   v, "%s.stepgen.%02d.steplen", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.steplen', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.stepspace),
                                   v, "%s.stepgen.%02d.stepspace", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.stepspace', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.dirsetup),
                                   v, "%s.stepgen.%02d.dirsetup", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.dirsetup', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.dirhold),
                                   v, "%s.stepgen.%02d.dirhold", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.dirhold', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.step_type),
                                   0, "%s.stepgen.%02d.step_type", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding param '%s.stepgen.%02d.step_type', aborting\n", r, hm2->llio->name, i);
                r = -ENOMEM;
                goto fail5;
            }

            if (hm2->stepgen.firmware_supports_swap) {
                r = hal_param_new_bool(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.swap_step_dir),
                                       0, "%s.stepgen.%02d.swap_step_dir", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding param '%s.stepgen.%02d.swap_step_dir', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
            } else {
                r = hal_param_new_fake(hm2->llio->comp_id, (hal_refs_u *)&(hm2->stepgen.instance[i].hal.param.swap_step_dir));
                if (r < 0) {
                    HM2_ERR("error %d allocating fake param '%s.stepgen.%02d.swap_step_dir', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
            }       

            if (hm2->stepgen.instance[i].table_width > 2){
                r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.table[0]),
                                       0, "%s.stepgen.%02d.table-data-0", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding param '%s.stepgen.%02d.table-data-0', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
                r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.table[1]),
                                       0, "%s.stepgen.%02d.table-data-1", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding param '%s.stepgen.%02d.table-data-1', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
                r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.table[2]),
                                       0, "%s.stepgen.%02d.table-data-2", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding param '%s.stepgen.%02d.table-data-2', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
                r = hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &(hm2->stepgen.instance[i].hal.param.table[3]),
                                       0, "%s.stepgen.%02d.table-data-3", hm2->llio->name, i);
                if (r < 0) {
                    HM2_ERR("error %d adding param '%s.stepgen.%02d.table-data-3', aborting\n", r, hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
            } else {
                // Still allocate storage to prevent crashing when dereferencing the table
                r  = hal_param_new_fake(hm2->llio->comp_id, (hal_refs_u *)&(hm2->stepgen.instance[i].hal.param.table[0]));
                r += hal_param_new_fake(hm2->llio->comp_id, (hal_refs_u *)&(hm2->stepgen.instance[i].hal.param.table[1]));
                r += hal_param_new_fake(hm2->llio->comp_id, (hal_refs_u *)&(hm2->stepgen.instance[i].hal.param.table[2]));
                r += hal_param_new_fake(hm2->llio->comp_id, (hal_refs_u *)&(hm2->stepgen.instance[i].hal.param.table[3]));
                if (r < 0) {
                    HM2_ERR("error allocating fake param '%s.stepgen.%02d.table-data-N', aborting\n", hm2->llio->name, i);
                    r = -ENOMEM;
                    goto fail5;
                }
            }

            // init
            hm2->stepgen.instance[i].subcounts = 0;

            hm2->stepgen.instance[i].written_steplen = 0;
            hm2->stepgen.instance[i].written_stepspace = 0;
            hm2->stepgen.instance[i].written_dirsetup = 0;
            hm2->stepgen.instance[i].written_dirhold = 0;

            hm2->stepgen.instance[i].written_step_type = 0xffffffff;

            hm2->stepgen.instance[i].prev_accumulator = 0;
        }
    }

    return hm2->stepgen.num_instances;


fail5:
    rtapi_kfree(hm2->stepgen.pulse_idle_width_reg);

fail4:
    rtapi_kfree(hm2->stepgen.pulse_width_reg);

fail3:
    rtapi_kfree(hm2->stepgen.dir_hold_time_reg);

fail2:
    rtapi_kfree(hm2->stepgen.dir_setup_time_reg);

fail1:
    rtapi_kfree(hm2->stepgen.mode_reg);

fail0:
    hm2->stepgen.num_instances = 0;
    return r;
}




void hm2_stepgen_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->stepgen.num_instances <= 0) return;
    HM2_PRINT("StepGen: %d\n", hm2->stepgen.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->stepgen.clock_frequency, hm2_hz_to_mhz(hm2->stepgen.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->stepgen.version);
    HM2_PRINT("    step_rate_addr: 0x%04X\n", hm2->stepgen.step_rate_addr);
    HM2_PRINT("    accumulator_addr: 0x%04X\n", hm2->stepgen.accumulator_addr);
    HM2_PRINT("    mode_addr: 0x%04X\n", hm2->stepgen.mode_addr);
    HM2_PRINT("    dir_setup_time_addr: 0x%04X\n", hm2->stepgen.dir_setup_time_addr);
    HM2_PRINT("    dir_hold_time_addr: 0x%04X\n", hm2->stepgen.dir_hold_time_addr);
    HM2_PRINT("    pulse_width_addr: 0x%04X\n", hm2->stepgen.pulse_width_addr);
    HM2_PRINT("    pulse_idle_width_addr: 0x%04X\n", hm2->stepgen.pulse_idle_width_addr);
    HM2_PRINT("    table_sequence_data_setup_addr: 0x%04X\n", hm2->stepgen.table_sequence_data_setup_addr);
    HM2_PRINT("    table_sequence_length_addr: 0x%04X\n", hm2->stepgen.table_sequence_length_addr);
    HM2_PRINT("    master_dds_addr: 0x%04X\n", hm2->stepgen.master_dds_addr);
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        enable = %d\n", (int)hal_get_bool(hm2->stepgen.instance[i].hal.pin.enable));
        HM2_PRINT("        hw:\n");
        HM2_PRINT("            step_rate = 0x%08X\n", hm2->stepgen.step_rate_reg[i]);
        HM2_PRINT("            accumulator = 0x%08X\n", hm2->stepgen.accumulator_reg[i]);
        HM2_PRINT("            mode = 0x%08X\n", hm2->stepgen.mode_reg[i]);
        HM2_PRINT("            dir_setup_time = 0x%08X (%u ns)\n", hm2->stepgen.dir_setup_time_reg[i], hal_get_ui32(hm2->stepgen.instance[i].hal.param.dirsetup));
        HM2_PRINT("            dir_hold_time = 0x%08X (%u ns)\n", hm2->stepgen.dir_hold_time_reg[i], hal_get_ui32(hm2->stepgen.instance[i].hal.param.dirhold));
        HM2_PRINT("            pulse_width = 0x%08X (%u ns)\n", hm2->stepgen.pulse_width_reg[i], hal_get_ui32(hm2->stepgen.instance[i].hal.param.steplen));
        HM2_PRINT("            pulse_idle_width = 0x%08X (%u ns)\n", hm2->stepgen.pulse_idle_width_reg[i], hal_get_ui32(hm2->stepgen.instance[i].hal.param.stepspace));
    }
}

