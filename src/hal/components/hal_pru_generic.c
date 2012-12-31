//----------------------------------------------------------------------//
// Description: hal_pru_generic.c                                       //
// PRU code implementing step/dir generation and other functions of     //
// hopeful use to off-load timing critical code from LinuxCNC HAL       //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Last change:                                                         //
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

#include "config.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include "rtapi.h"          /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
#include "rtapi_math.h"
#include "hal.h"            /* HAL public API decls */
#include <pthread.h>

#include "prussdrv.h"           // UIO interface to uio_pruss
#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

MODULE_AUTHOR("Charles Steinkuehler");
MODULE_DESCRIPTION("AM335x PRU demo component");
MODULE_LICENSE("GPL");

/***********************************************************************
*                    MODULE PARAMETERS AND DEFINES                     *
************************************************************************/

// Maximum number of PRU "channels"
#define MAX_CHAN 7

// Default PRU code to load (prefixed by EMC_RTLIB_DIR)
// Fixme: This should probably be compiled in, via a header file generated
//        by pasm -PRUv2 -c myprucode.p
#define  DEFAULT_CODE  "stepgen.bin"

// The kernel module required to talk to the PRU
#define UIO_PRUSS  "uio_pruss"

// Default pin to use for PRU modules...use a pin that does not leave the PRU
#define PRU_DEFAULT_PIN 17

// PRU "thread" frequency in Hz
#define PRU_FREQUENCY 10000

// Start out with a default delays of 1 mS (1000000 nS) 
#define DEFAULT_DELAY 1000000

#define f_period_s ((double)(l_period_ns * 1e-9))

// This function was invented by Jeff Epler.
// It forces a floating-point variable to be degraded from native register
// size (80 bits on x86) to C double size (64 bits).
static double force_precision(double d) __attribute__((__noinline__));
static double force_precision(double d) {
    return d;
}

const char *chan_mode[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(chan_mode,MAX_CHAN,"operating mode for up to 7 channels");

static char *prucode = "";
RTAPI_MP_STRING(prucode, "filename of PRU code (.bin), default: stepgen.bin");

static int pru = 0;
RTAPI_MP_INT(pru, "PRU number to execute this code in, default 0; values 0 or 1");

static int disabled = 0;
RTAPI_MP_INT(disabled, "start the PRU in disabled state (for debugging); default: enabled");

static int event = -1;
RTAPI_MP_INT(event, "PRU event number to listen for (0..7, default: none)");

typedef enum MODE { 
    eMODE_INVALID   = -1,
    eMODE_NONE      = 0,
    eMODE_STEP_DIR  = 1,
    eMODE_UP_DOWN   = 2,    // Not implemented yet!
    eMODE_DELTA_SIG = 3,
    eMODE_PWM       = 4,
    eMODE_GPIO      = 5,    // Not implemented yet!
    eMODE_RESERVED6 = 6,    // Table-driven stepgen?
    eMODE_RESERVED7 = 7
} MODE;

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

union PRU_chan_state_raw
{
    u64     qword[4];
    u32     dword[8];
    u16     word[16];
    u8      byte[32];
};

struct PRU_chan_ctrl_reg
{
    u8      enable;
    u8      mode;
    u8      pin1;
    u8      pin2;
};

struct PRU_chan_state_generic
{
    struct PRU_chan_ctrl_reg ctrl;

    u32     reserved[7];
};

struct PRU_chan_state_stepdir
{
    // PRU control and state data
    struct PRU_chan_ctrl_reg ctrl;

    s32     rate;
    u16     steplen;
    u16     dirhold;
    u16     stepspace;
    u16     dirsetup;
    u32     accum;
    u32     pos;
    u32     reserved[2];
};

struct PRU_chan_state_delta
{
    // PRU control and state data
    struct PRU_chan_ctrl_reg ctrl;

    u16     value1;         // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    u16     value2;         // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    u32     reserved[6];
};

struct PRU_chan_state_pwm
{
    // PRU control and state data
    struct PRU_chan_ctrl_reg ctrl;

    u32     period;
    u32     high1;
    u32     high2;
    u32     reserved[4];
};

typedef union PRU_chan_state_type
{
    union   PRU_chan_state_raw      raw;
    struct  PRU_chan_state_generic  gen;
    struct  PRU_chan_state_stepdir  step;
    struct  PRU_chan_state_delta    delta;
    struct  PRU_chan_state_pwm      pwm;
} PRU_chan_state_t, *PRU_chan_state_ptr;

struct chan_state_raw
{
    union   PRU_chan_state_raw PRU;
};

struct chan_state_generic
{
    struct PRU_chan_state_generic PRU;
};

struct chan_state_stepdir
{
    struct PRU_chan_state_stepdir PRU;

    // HAL Pins
    hal_bit_t       *hal_enable;

    hal_float_t     *hal_command;
    hal_float_t     *hal_position;

    // HAL Parameters
    hal_u32_t       hal_steplen;
    hal_u32_t       hal_stepspace;
    hal_u32_t       hal_dirhold;
    hal_u32_t       hal_dirsetup;

    hal_s32_t       hal_rawcount;

    // Export pins (mostly) matching hostom2 stepgen instance to ease integration
    struct {

        struct {
            hal_float_t     *position_cmd;
            hal_float_t     *velocity_cmd;
            hal_s32_t       *counts;
            hal_float_t     *position_fb;
            hal_float_t     *velocity_fb;
            hal_bit_t       *enable;
            hal_bit_t       *control_type;              // 0="position control", 1="velocity control"

            // debug pins
            hal_float_t     *dbg_ff_vel;
            hal_float_t     *dbg_vel_error;
            hal_float_t     *dbg_s_to_match;
            hal_float_t     *dbg_err_at_match;
            hal_s32_t       *dbg_step_rate;
            hal_float_t     *dbg_pos_minus_prev_cmd;
        } pin;

        struct {
            hal_float_t     position_scale;
            hal_float_t     maxvel;
            hal_float_t     maxaccel;

            hal_u32_t       steplen;
            hal_u32_t       stepspace;
            hal_u32_t       dirsetup;
            hal_u32_t       dirhold;

            hal_u32_t       steppin;
            hal_u32_t       dirpin;
        } param;

    } hal;

    // this variable holds the previous position command, for
    // computing the feedforward velocity
    hal_float_t old_position_cmd;

    u32 prev_accumulator;

    // this is a 48.16 signed fixed-point representation of the current
    // stepgen position (16 bits of sub-step resolution)
    s64 subcounts;

    u32 written_steplen;
    u32 written_stepspace;
    u32 written_dirsetup;
    u32 written_dirhold;
    u32 written_ctrl;
};

struct chan_state_delta
{
    // PRU control and state data
    struct PRU_chan_state_delta PRU;

    // HAL Pins
    hal_bit_t       *hal_enable;

    hal_float_t     *hal_out1;
    hal_float_t     *hal_out2;

    // HAL Parameters
    hal_u32_t       hal_pin1;
    hal_u32_t       hal_pin2;

};

struct chan_state_pwm
{
    // PRU control and state data
    struct PRU_chan_state_pwm PRU;

    // HAL Pins
    hal_bit_t       *hal_enable;

    hal_u32_t       *hal_period;
    hal_u32_t       *hal_out1;
    hal_u32_t       *hal_out2;

    // HAL Parameters
    hal_u32_t       hal_pin1;
    hal_u32_t       hal_pin2;
};

typedef union chan_state_type
{
    struct  chan_state_raw      raw;
    struct  chan_state_generic  gen;
    struct  chan_state_stepdir  step;
    struct  chan_state_delta    delta;
    struct  chan_state_pwm      pwm;
} chan_state_t, *chan_state_ptr;

// Array of state data for all channels
//chan_state_ptr chan_state;

static tprussdrv *pruss;                // driver descriptor

/* other globals */
static int comp_id;     /* component ID */

static const char *modname = "hal_pru_generic";

// shared with PRU
static unsigned long *pru_data_ram;     // points to PRU data RAM
static tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static MODE parse_mode(const char *mode);
static int export_pru();
static void stepgen_position_control(chan_state_ptr chan_state, long l_period_ns, int i, double *new_vel);
static void update_pru(void *arg, long l);
static int setup_pru(int pru, char *filename, int disabled, chan_state_ptr chan_state);
static void pru_shutdown(int pru);
static void *pruevent_thread(void *arg);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    chan_state_ptr chan_state;
    int retval;
    int i;

    comp_id = hal_init(modname);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
        return -1;
    }

    // Allocate HAL shared memory for state data
    chan_state = (chan_state_ptr) hal_malloc(sizeof(chan_state_t) * MAX_CHAN);
    if (chan_state == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "%s: ERROR: hal_malloc() failed\n", modname);
	    hal_exit(comp_id);
	    return -1;
    }

    // Clear channel control state
    memset(chan_state, 0, sizeof(chan_state_t) * MAX_CHAN);

    for (i = 0; i < MAX_CHAN; i++) {
        chan_state[i].gen.PRU.ctrl.mode = parse_mode(chan_mode[i]);
        if(chan_state[i].gen.PRU.ctrl.mode == eMODE_INVALID) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: bad mode '%s' for channel %i\n",
                    modname, chan_mode[i], i);
            return -1;
        }
    }

    if ((retval = export_pru(chan_state))) {
    rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: var export failed: %d\n",
            modname, retval);
    hal_exit(comp_id);
    return -1;
    }
    if ((retval = setup_pru(pru, prucode, disabled, chan_state))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: ERROR: failed to initialize PRU\n", modname);
        hal_exit(comp_id);
        return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed\n", modname);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    pru_shutdown(pru);
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/
static void read_pru(void *arg, long period)
{
    // Read data from the PRU here...
    chan_state_ptr chan_state   = (chan_state_ptr) arg;
    PRU_chan_state_ptr pru      = (PRU_chan_state_ptr) pru_data_ram;
    int i;

    for (i = 0; i < MAX_CHAN; i++) {
        s64 x, y;
        u32 acc;
        s64 acc_delta;

        switch (chan_state[i].gen.PRU.ctrl.mode) {

        case eMODE_STEP_DIR :

            // "atomic" read of accumulator and position register from PRU
	        do {
	            x = pru[i].raw.qword[2];
	            y = pru[i].raw.qword[2];
	        } while ( x != y );

            // Update internal state and HAL outputs
            chan_state[i].raw.PRU.qword[2] = x;
            chan_state[i].step.hal_rawcount = chan_state[i].step.PRU.pos;

            // Mangle 32-bit step count and 27 bit accumulator (with 5 bits of status)
            // into a 16.16 value as expected by the hostmot2 stepgen logic
            y >>= 11;                       // Lower 32 bits contains 5 flag bits and a 27-bit accumulator...shift...
            y &= 0x00000000FFFFFFFF;        // ...and get rid of the flag bits
            x >>= 16;                       // Shift the full counts down...
            x &= 0x00000000FFFF0000;        // ...and mask all but the 16 bits we're interested in
            x |= y;                         // Combine the low & high words to make a 16.16 fixed-point value...
            x -= 0x0000000000008000;        // ...and subtract off our 1/2 step starting offset

            acc = x;

            // those tricky users are always trying to get us to divide by zero
            if (fabs(chan_state[i].step.hal.param.position_scale) < 1e-6) {
                if (chan_state[i].step.hal.param.position_scale >= 0.0) {
                    chan_state[i].step.hal.param.position_scale = 1.0;
                    rtapi_print_msg(RTAPI_MSG_ERR,
                            "%s: stepgen %d position_scale is too close to 0, resetting to 1.0\n", modname, i);
                } else {
                    chan_state[i].step.hal.param.position_scale = -1.0;
                    rtapi_print_msg(RTAPI_MSG_ERR,
                            "%s: stepgen %d position_scale is too close to 0, resetting to -1.0\n", modname, i);
                }
            }

            // The HM2 Accumulator Register is a 16.16 bit fixed-point
            // representation of the current stepper position.
            // The fractional part gives accurate velocity at low speeds, and
            // sub-step position feedback (like sw stepgen).
            acc_delta = (s64)acc - (s64)chan_state[i].step.prev_accumulator;
            if (acc_delta > INT32_MAX) {
                acc_delta -= UINT32_MAX;
            } else if (acc_delta < INT32_MIN) {
                acc_delta += UINT32_MAX;
            }

            chan_state[i].step.subcounts += acc_delta;

            *(chan_state[i].step.hal.pin.counts) = chan_state[i].step.subcounts >> 16;

            // note that it's important to use "subcounts/65536.0" instead of just
            // "counts" when computing position_fb, because position_fb needs sub-count
            // precision
            *(chan_state[i].step.hal.pin.position_fb) = ((double)chan_state[i].step.subcounts / 65536.0) / chan_state[i].step.hal.param.position_scale;

            chan_state[i].step.prev_accumulator = acc;

            break;

        default :
            // Nothing to export for other types
            break;
        }
    }

}

static void update_stepgen(chan_state_ptr chan_state, long l_period_ns, int i)
{
    double new_vel;

    double physical_maxvel;  // max vel supported by current step timings & position-scale
    double maxvel;           // actual max vel to use this time

    double steps_per_sec_cmd;

    struct chan_state_stepdir *s = &chan_state[i].step;

    //
    // first sanity-check our maxaccel and maxvel params
    //

    // maxvel must be >= 0.0, and may not be faster than 1 step per (steplen+stepspace) seconds
    {
        double min_ns_per_step = s->hal.param.steplen + s->hal.param.stepspace;
        double max_steps_per_s = 1.0e9 / min_ns_per_step;

        physical_maxvel = max_steps_per_s / fabs(s->hal.param.position_scale);
        physical_maxvel = force_precision(physical_maxvel);

        if (s->hal.param.maxvel < 0.0) {
            rtapi_print_msg(RTAPI_MSG_ERR,"%s: stepgen.%02d.maxvel < 0, setting to its absolute value\n", modname, i);
            s->hal.param.maxvel = fabs(s->hal.param.maxvel);
        }

        if (s->hal.param.maxvel > physical_maxvel) {
            rtapi_print_msg(RTAPI_MSG_ERR,"%s: stepgen.%02d.maxvel is too big for current step timings & position-scale, clipping to max possible\n", modname, i);
            s->hal.param.maxvel = physical_maxvel;
        }

        if (s->hal.param.maxvel == 0.0) {
            maxvel = physical_maxvel;
        } else {
            maxvel = s->hal.param.maxvel;
        }
    }

    // maxaccel may not be negative
    if (s->hal.param.maxaccel < 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: stepgen.%02d.maxaccel < 0, setting to its absolute value\n", modname, i);
        s->hal.param.maxaccel = fabs(s->hal.param.maxaccel);
    }

    // select the new velocity we want
    if (*s->hal.pin.control_type == 0) {
        stepgen_position_control(chan_state, l_period_ns, i, &new_vel);
    } else {
        // velocity-mode control is easy
        new_vel = *s->hal.pin.velocity_cmd;
        if (s->hal.param.maxaccel > 0.0) {
            if (((new_vel - *s->hal.pin.velocity_fb) / f_period_s) > s->hal.param.maxaccel) {
                new_vel = (*s->hal.pin.velocity_fb) + (s->hal.param.maxaccel * f_period_s);
            } else if (((new_vel - *s->hal.pin.velocity_fb) / f_period_s) < -s->hal.param.maxaccel) {
                new_vel = (*s->hal.pin.velocity_fb) - (s->hal.param.maxaccel * f_period_s);
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

    steps_per_sec_cmd = new_vel * s->hal.param.position_scale;
    s->PRU.rate = steps_per_sec_cmd * (134217728.0 / (double)PRU_FREQUENCY);
    
    // clip rate just to be safe...should be limited by code above
    if (s->PRU.rate > 0x7FFFFFF) {
        s->PRU.rate = 0x7FFFFFF;
    } else if (s->PRU.rate < -0x7FFFFFF) {
        s->PRU.rate = -0x7FFFFFF;
    }

    *s->hal.pin.dbg_step_rate = s->PRU.rate;
}

u16 ns2periods(hal_u32_t ns)
{
    u16 p = ceil((double)ns / ((double)1e9 / (double)PRU_FREQUENCY));
    return p;
}

static void update_pru(void *arg, long period)
{
    chan_state_ptr chan_state   = (chan_state_ptr) arg;
    PRU_chan_state_ptr pru      = (PRU_chan_state_ptr) pru_data_ram;
    int i, j;

    for (i = 0; i < MAX_CHAN; i++) {
        switch (chan_state[i].gen.PRU.ctrl.mode) {

        case eMODE_STEP_DIR :

            // Update shadow of PRU control registers
            chan_state[i].step.PRU.ctrl.enable  = *(chan_state[i].step.hal.pin.enable);
            chan_state[i].step.PRU.ctrl.pin1    = chan_state[i].step.hal.param.steppin;
            chan_state[i].step.PRU.ctrl.pin2    = chan_state[i].step.hal.param.dirpin;

            if (*(chan_state[i].step.hal.pin.enable) == 0) {
                chan_state[i].step.PRU.rate = 0;
                chan_state[i].step.old_position_cmd = *(chan_state[i].step.hal.pin.position_cmd);
                *(chan_state[i].step.hal.pin.velocity_fb) = 0;
            } else {
                // call update function
                update_stepgen(chan_state, period, i);
            }

            // Update timing parameters if changed
            if ((chan_state[i].step.hal.param.dirsetup  != chan_state[i].step.written_dirsetup ) ||
                (chan_state[i].step.hal.param.dirhold   != chan_state[i].step.written_dirhold  ) ||
                (chan_state[i].step.hal.param.steplen   != chan_state[i].step.written_steplen  ) ||
                (chan_state[i].step.hal.param.stepspace != chan_state[i].step.written_stepspace))
            {
                chan_state[i].step.PRU.dirsetup     = ns2periods(chan_state[i].step.hal.param.dirsetup);
                chan_state[i].step.PRU.dirhold      = ns2periods(chan_state[i].step.hal.param.dirhold);
                chan_state[i].step.PRU.steplen      = ns2periods(chan_state[i].step.hal.param.steplen);
                chan_state[i].step.PRU.stepspace    = ns2periods(chan_state[i].step.hal.param.stepspace);

                // Send new value(s) to the PRU
                pru[i].raw.dword[2] = chan_state[i].raw.PRU.dword[2];
                pru[i].raw.dword[3] = chan_state[i].raw.PRU.dword[3];

                // Stash values written
                chan_state[i].step.written_dirsetup  = chan_state[i].step.hal.param.dirsetup;
                chan_state[i].step.written_dirhold   = chan_state[i].step.hal.param.dirhold;
                chan_state[i].step.written_steplen   = chan_state[i].step.hal.param.steplen;
                chan_state[i].step.written_stepspace = chan_state[i].step.hal.param.stepspace;
            }

            // Update control word if changed
            if (chan_state[i].raw.PRU.dword[0] != chan_state[i].step.written_ctrl) {
                pru[i].raw.dword[0] = chan_state[i].raw.PRU.dword[0];
                chan_state[i].step.written_ctrl = chan_state[i].raw.PRU.dword[0];
            }

            // Send rate update to the PRU
            pru[i].step.rate = chan_state[i].step.PRU.rate;

            break;

        case eMODE_DELTA_SIG :

            // Update shadow of PRU control registers
            chan_state[i].delta.PRU.ctrl.enable  = *(chan_state[i].delta.hal_enable);

            if (*(chan_state[i].delta.hal_out1) >= 1.0) {
                chan_state[i].delta.PRU.value1 = 0x4000;
            } else if (*(chan_state[i].delta.hal_out1) <= 0.0) {
                chan_state[i].delta.PRU.value1 = 0x0000;
            } else {
                chan_state[i].delta.PRU.value1 = 
                    (u32) (*(chan_state[i].delta.hal_out1) * (1 << 14)) & 0x3FFF;
            }

            if (*(chan_state[i].delta.hal_out2) == 1.0) {
                chan_state[i].delta.PRU.value2 = 0x4000;
            } else if (*(chan_state[i].delta.hal_out2) <= 0.0) {
                chan_state[i].delta.PRU.value2 = 0x0000;
            } else {
                chan_state[i].delta.PRU.value2 =
                    (u32) (*(chan_state[i].delta.hal_out2) * (1 << 14)) & 0x3FFF;
            }

            chan_state[i].delta.PRU.ctrl.pin1   = chan_state[i].delta.hal_pin1;
            chan_state[i].delta.PRU.ctrl.pin2   = chan_state[i].delta.hal_pin2;

            // Send updates to PRU
            for (j = 0; j < 2; j++) {
                pru[i].raw.dword[j] = chan_state[i].raw.PRU.dword[j];
            }
            break;

        case eMODE_PWM :

            // Update shadow of PRU control registers
            chan_state[i].pwm.PRU.ctrl.enable   = *(chan_state[i].pwm.hal_enable);
            chan_state[i].pwm.PRU.period        = *(chan_state[i].pwm.hal_period);
            chan_state[i].pwm.PRU.high1         = *(chan_state[i].pwm.hal_out1);
            chan_state[i].pwm.PRU.high2         = *(chan_state[i].pwm.hal_out2);

            chan_state[i].pwm.PRU.ctrl.pin1     = chan_state[i].pwm.hal_pin1;
            chan_state[i].pwm.PRU.ctrl.pin2     = chan_state[i].pwm.hal_pin2;

            // Send updates to PRU
            for (j = 0; j < 4; j++) {
                pru[i].raw.dword[j] = chan_state[i].raw.PRU.dword[j];
            }
            break;

        default :
            // Nothing to export for other types
            break;
        }
    }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static MODE parse_mode(const char *mode)
{
    if (mode == NULL) return eMODE_NONE;
    if (*mode == '0' || *mode == 'n' || *mode == 'N') return eMODE_NONE;
    if (*mode == '1' || *mode == 's' || *mode == 'S') return eMODE_STEP_DIR;
    if (*mode == '2' || *mode == 'u' || *mode == 'U') return eMODE_UP_DOWN;
    if (*mode == '3' || *mode == 'd' || *mode == 'D') return eMODE_DELTA_SIG;
    if (*mode == '4' || *mode == 'p' || *mode == 'P') return eMODE_PWM;
    if (*mode == '5' || *mode == 'g' || *mode == 'G') return eMODE_GPIO;
    if (*mode == '6' || *mode == 'x' || *mode == 'X') return eMODE_RESERVED6;
    if (*mode == '7' || *mode == 'y' || *mode == 'Y') return eMODE_RESERVED7;

    return eMODE_INVALID;
}

//
// Here's the stepgen position controller.  It uses first-order
// feedforward and proportional error feedback.  This code is based
// on John Kasunich's software stepgen code.
//

static void stepgen_position_control(chan_state_ptr chan_state, long l_period_ns, int i, double *new_vel)
{
    double ff_vel;
    double velocity_error;
    double match_accel;
    double seconds_to_vel_match;
    double position_at_match;
    double position_cmd_at_match;
    double error_at_match;
    double velocity_cmd;

    struct chan_state_stepdir *s = &chan_state[i].step;

    (*s->hal.pin.dbg_pos_minus_prev_cmd) = (*s->hal.pin.position_fb) - s->old_position_cmd;

    // calculate feed-forward velocity in machine units per second
    ff_vel = ((*s->hal.pin.position_cmd) - s->old_position_cmd) / f_period_s;
    (*s->hal.pin.dbg_ff_vel) = ff_vel;

    s->old_position_cmd = (*s->hal.pin.position_cmd);

    velocity_error = (*s->hal.pin.velocity_fb) - ff_vel;
    (*s->hal.pin.dbg_vel_error) = velocity_error;

    // Do we need to change speed to match the speed of position-cmd?
    // If maxaccel is 0, there's no accel limit: fix this velocity error
    // by the next servo period!  This leaves acceleration control up to
    // the trajectory planner.
    // If maxaccel is not zero, the user has specified a maxaccel and we
    // adhere to that.
    if (velocity_error > 0.0) {
        if (s->hal.param.maxaccel == 0) {
            match_accel = -velocity_error / f_period_s;
        } else {
            match_accel = -s->hal.param.maxaccel;
        }
    } else if (velocity_error < 0.0) {
        if (s->hal.param.maxaccel == 0) {
            match_accel = velocity_error / f_period_s;
        } else {
            match_accel = s->hal.param.maxaccel;
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
    *s->hal.pin.dbg_s_to_match = seconds_to_vel_match;

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
        if (s->hal.param.maxaccel > 0) {
            if (velocity_cmd > (*s->hal.pin.velocity_fb + (s->hal.param.maxaccel * f_period_s))) {
                velocity_cmd = *s->hal.pin.velocity_fb + (s->hal.param.maxaccel * f_period_s);
            } else if (velocity_cmd < (*s->hal.pin.velocity_fb - (s->hal.param.maxaccel * f_period_s))) {
                velocity_cmd = *s->hal.pin.velocity_fb - (s->hal.param.maxaccel * f_period_s);
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
        velocity_cmd = *s->hal.pin.velocity_fb + (match_accel * f_period_s);
    }

    *new_vel = velocity_cmd;
}

static int export_stepgen(chan_state_ptr chan_state, int i)
{
    char name[HAL_NAME_LEN + 1];
    int r;

    // Pins
    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.position-cmd", modname, i);
    r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].step.hal.pin.position_cmd), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.velocity-cmd", modname, i);
    r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].step.hal.pin.velocity_cmd), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.velocity-fb", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.velocity_fb), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.position-fb", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.position_fb), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.counts", modname, i);
    r = hal_pin_s32_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.counts), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.enable", modname, i);
    r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].step.hal.pin.enable), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.control-type", modname, i);
    r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].step.hal.pin.control_type), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    // debug pins

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_pos_minus_prev_cmd", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_pos_minus_prev_cmd), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_ff_vel", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_ff_vel), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_s_to_match", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_s_to_match), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_vel_error", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_vel_error), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_err_at_match", modname, i);
    r = hal_pin_float_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_err_at_match), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dbg_step_rate", modname, i);
    r = hal_pin_s32_new(name, HAL_OUT, &(chan_state[i].step.hal.pin.dbg_step_rate), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding pin '%s', aborting\n", modname, name);
        return r;
    }


    // Parameters
    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.position-scale", modname, i);
    r = hal_param_float_new(name, HAL_RW, &(chan_state[i].step.hal.param.position_scale), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.maxvel", modname, i);
    r = hal_param_float_new(name, HAL_RW, &(chan_state[i].step.hal.param.maxvel), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.maxaccel", modname, i);
    r = hal_param_float_new(name, HAL_RW, &(chan_state[i].step.hal.param.maxaccel), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.steplen", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.steplen), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.stepspace", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.stepspace), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dirsetup", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.dirsetup), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dirhold", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.dirhold), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.steppin", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.steppin), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    rtapi_snprintf(name, sizeof(name), "%s.stepgen.%02d.dirpin", modname, i);
    r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].step.hal.param.dirpin), comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s: Error adding param '%s', aborting\n", modname, name);
        return r;
    }

    // init
    *(chan_state[i].step.hal.pin.position_cmd) = 0.0;
    *(chan_state[i].step.hal.pin.counts) = 0;
    *(chan_state[i].step.hal.pin.position_fb) = 0.0;
    *(chan_state[i].step.hal.pin.velocity_fb) = 0.0;
    *(chan_state[i].step.hal.pin.enable) = 0;
    *(chan_state[i].step.hal.pin.control_type) = 0;

    chan_state[i].step.hal.param.position_scale = 1.0;
    chan_state[i].step.hal.param.maxvel = 0.0;
    chan_state[i].step.hal.param.maxaccel = 1.0;

    chan_state[i].step.subcounts = 0;

    chan_state[i].step.hal.param.steplen   = (double)DEFAULT_DELAY / ((double)1e9 / (double)PRU_FREQUENCY);
    chan_state[i].step.hal.param.stepspace = (double)DEFAULT_DELAY / ((double)1e9 / (double)PRU_FREQUENCY);
    chan_state[i].step.hal.param.dirsetup  = (double)DEFAULT_DELAY / ((double)1e9 / (double)PRU_FREQUENCY);
    chan_state[i].step.hal.param.dirhold   = (double)DEFAULT_DELAY / ((double)1e9 / (double)PRU_FREQUENCY);

    chan_state[i].step.written_steplen = 0;
    chan_state[i].step.written_stepspace = 0;
    chan_state[i].step.written_dirsetup = 0;
    chan_state[i].step.written_dirhold = 0;
    chan_state[i].step.written_ctrl = 0;

    // Start with 1/2 step offset in accumulator
    chan_state[i].step.PRU.accum = 1 << 26;
    chan_state[i].step.prev_accumulator = chan_state[i].step.PRU.accum;

    chan_state[i].step.hal.param.steppin = PRU_DEFAULT_PIN;
    chan_state[i].step.hal.param.dirpin  = PRU_DEFAULT_PIN;

    return 0;
}

static int export_pru(chan_state_ptr chan_state)
{
    int r, i;
    char name[HAL_NAME_LEN + 1];

    for (i = 0; i < MAX_CHAN; i++) {
        switch (chan_state[i].gen.PRU.ctrl.mode) {

        case eMODE_STEP_DIR :

            r= export_stepgen(chan_state, i);
            if (r != 0) { return r; }

            break;

        case eMODE_DELTA_SIG :

            // Export HAL Pins
            rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.enable", modname, i);
            r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].delta.hal_enable), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.out1", modname, i);
            r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].delta.hal_out1), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.out2", modname, i);
            r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].delta.hal_out2), comp_id);
            if (r != 0) { return r; }

            // Export HAL Parameters
            rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.pin1", modname, i);
            r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].delta.hal_pin1), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.pin2", modname, i);
            r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].delta.hal_pin2), comp_id);
            if (r != 0) { return r; }

            // Initialize HAL Pins
            *(chan_state[i].delta.hal_enable)   = 0;
            *(chan_state[i].delta.hal_out1)     = 0.0;
            *(chan_state[i].delta.hal_out2)     = 0.0;

            // Initialize HAL Parameters
            chan_state[i].delta.hal_pin1        = PRU_DEFAULT_PIN;
            chan_state[i].delta.hal_pin2        = PRU_DEFAULT_PIN;

            break;

        case eMODE_PWM :

            // Export HAL Pins
            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.enable", modname, i);
            r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].pwm.hal_enable), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.period", modname, i);
            r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_period), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out1", modname, i);
            r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_out1), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out2", modname, i);
            r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_out2), comp_id);
            if (r != 0) { return r; }

            // Export HAL Parameters
            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin1", modname, i);
            r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].pwm.hal_pin1), comp_id);
            if (r != 0) { return r; }

            rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin2", modname, i);
            r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].pwm.hal_pin2), comp_id);
            if (r != 0) { return r; }

            // Initialize HAL Pins
            *(chan_state[i].pwm.hal_enable)     = 0;
            *(chan_state[i].pwm.hal_period)     = 0;
            *(chan_state[i].pwm.hal_out1)       = 0;
            *(chan_state[i].pwm.hal_out2)       = 0;

            // Initialize HAL Parameters
            chan_state[i].pwm.hal_pin1          = PRU_DEFAULT_PIN;
            chan_state[i].pwm.hal_pin2          = PRU_DEFAULT_PIN;

            break;

        default :
            // Nothing to export for other types
            break;
        }
    }

    // Export functions
    rtapi_snprintf(name, sizeof(name), "%s.update", modname);
    r = hal_export_funct(name, update_pru, chan_state, 1, 0, comp_id);
    if (r != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: function export failed: %s\n", modname, name);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_snprintf(name, sizeof(name), "%s.capture-position", modname);
    r = hal_export_funct(name, read_pru, chan_state, 1, 0, comp_id);
    if (r != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: function export failed: %s\n", modname, name);
        hal_exit(comp_id);
        return -1;
    }

    return 0;
}


static int assure_module_loaded(const char *module)
{
    FILE *fd;
    char line[100];
    int len = strlen(module);
    int retval;

    fd = fopen("/proc/modules", "r");
    if (fd == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: cannot read /proc/modules\n",
            modname);
    return -1;
    }
    while (fgets(line, sizeof(line), fd)) {
    if (!strncmp(line, module, len)) {
        rtapi_print_msg(RTAPI_MSG_DBG, "%s: module '%s' already loaded\n",
                modname, module);
        fclose(fd);
        return 0;
    }
    }
    fclose(fd);
    rtapi_print_msg(RTAPI_MSG_DBG, "%s: loading module '%s'\n",
            modname, module);
    sprintf(line, "/sbin/modprobe %s", module);
    if ((retval = system(line))) {
    rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: executing '%s'  %d - %s\n",
            modname, line, errno, strerror(errno));
    return -1;
    }
    return 0;
}

static int setup_pru(int pru, char *filename, int disabled, chan_state_ptr chan_state)
{
    PRU_chan_state_ptr pru_state;
    int i, j;
    int retval;

    if (geteuid()) {
    rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: not running as root - need to 'sudo make setuid'?\n",
            modname);
    return -1;
    }
    if ((retval = assure_module_loaded(UIO_PRUSS)))
    return retval;

    // Allocate and initialize memory
    prussdrv_init ();

    // opens an event out and initializes memory mapping
    if (prussdrv_open(event > -1 ? event : PRU_EVTOUT_0) < 0)
    return -1;

    // expose the driver data, filled in by prussdrv_open
    pruss = &prussdrv;

    // Map PRU's INTC
    if (prussdrv_pruintc_init(&pruss_intc_initdata) < 0)
    return -1;

    // Maps the PRU DRAM memory to input pointer
    if (prussdrv_map_prumem(pru ? PRUSS0_PRU1_DATARAM : PRUSS0_PRU0_DATARAM,
            (void **) &pru_data_ram) < 0)
    return -1;

    rtapi_print_msg(RTAPI_MSG_DBG, "%s: PRU data ram mapped at %p\n",
            modname, pru_data_ram);

    if (event > -1) {
    prussdrv_start_irqthread (event, sched_get_priority_max(SCHED_FIFO) - 2,
                  pruevent_thread, (void *) event);
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d listener started\n",
            modname, event);
    }

    pru_state = (PRU_chan_state_ptr) pru_data_ram;

    for (i = 0; i < MAX_CHAN; i++) {
        // Zero PRU state memory
        for (j = 0; j < 8; j++) {
            pru_state[i].raw.dword[j] = 0;
        }

        // Initialize any required values that are not written by update_pru()
        switch (chan_state[i].gen.PRU.ctrl.mode) {

        case eMODE_STEP_DIR :
            // Setup initial accumulator value
            pru_state[i].step.accum = chan_state[i].step.PRU.accum;

            break;

        default :
            // Nothing to do for other types
            break;
        }
    }

    // Load and execute binary on PRU
    if (!strlen(filename))
    filename = EMC2_RTLIB_DIR "/" DEFAULT_CODE;
    retval =  prussdrv_exec_program (pru, filename, disabled);

    return retval;
}

static void *pruevent_thread(void *arg)
{
    int event = (int) arg;
    int event_count;
    do {
    if (prussdrv_pru_wait_event(event, &event_count) < 0)
        continue;
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d received\n",
            modname, event);
    prussdrv_pru_clear_event(pru ? PRU1_ARM_INTERRUPT : PRU0_ARM_INTERRUPT);
    } while (1);
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: pruevent_thread exiting\n",
            modname);
    return NULL; // silence compiler warning
}

static void pru_shutdown(int pru)
{
    // Disable PRU and close memory mappings
    prussdrv_pru_disable(pru);
    prussdrv_exit (); // also joins event listen thread
}

