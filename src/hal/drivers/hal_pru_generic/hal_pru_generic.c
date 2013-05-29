//----------------------------------------------------------------------//
// Description: hal_pru_generic.c                                       //
// HAL module to communicate with PRU code implementing step/dir        //
// generation and other functions of hopeful use to off-load timing     //
// critical code from LinuxCNC HAL                                      //
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

// Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H
#include "config_module.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include RTAPI_INC_LIST_H
#include "rtapi.h"          /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
#include "rtapi_math.h"
#include "hal.h"            /* HAL public API decls */
#include <pthread.h>

#include "prussdrv.h"           // UIO interface to uio_pruss
//#include "pru.h"                // PRU-related defines
#include "pruss_intc_mapping.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "hal/drivers/hal_pru_generic/hal_pru_generic.h"

MODULE_AUTHOR("Charles Steinkuehler");
MODULE_DESCRIPTION("AM335x PRU demo component");
MODULE_LICENSE("GPL");

/***********************************************************************
*                    MODULE PARAMETERS AND DEFINES                     *
************************************************************************/

// Maximum number of PRU "channels"
#define MAX_CHAN 8

// Default PRU code to load (prefixed by EMC_RTLIB_DIR)
// Fixme: This should probably be compiled in, via a header file generated
//        by pasm -PRUv2 -c myprucode.p
#define  DEFAULT_CODE  "stepgen.bin"

// The kernel module required to talk to the PRU
#define UIO_PRUSS  "uio_pruss"

// Default pin to use for PRU modules...use a pin that does not leave the PRU
#define PRU_DEFAULT_PIN 17

// Start out with default pulse length/width and setup/hold delays of 1 mS (1000000 nS) 
#define DEFAULT_DELAY 1000000

#define f_period_s ((double)(l_period_ns * 1e-9))

static int num_stepgens = 0;
RTAPI_MP_INT(num_stepgens, "Number of step generators (default: 0)");

static int num_pwmgens = 0;
RTAPI_MP_INT(num_pwmgens, "Number of PWM outputs (default: 0)");
//int num_pwmgens[MAX_CHAN] = { -1, -1, -1, -1, -1, -1, -1, -1 };
//RTAPI_MP_ARRAY_INT(num_pwmgens, "Number of PWM outputs for up to 8 banks (default: 0)");

static char *prucode = "";
RTAPI_MP_STRING(prucode, "filename of PRU code (.bin, default: stepgen.bin)");

static int pru = 1;
RTAPI_MP_INT(pru, "PRU number to execute this code (0 or 1, default: 1)");

static int pru_period = 10000;
RTAPI_MP_INT(pru_period, "PRU task period (in nS, default: 10,000 nS or 100 KHz)");

static int disabled = 0;
RTAPI_MP_INT(disabled, "start the PRU in disabled state for debugging (0=enabled, 1=disabled, default: enabled");

static int event = -1;
RTAPI_MP_INT(event, "PRU event number to listen for (0..7, default: none)");

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

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
static void hpg_read(void *void_hpg, long period);
static void hpg_write(void *arg, long l);
int export_pru(hal_pru_generic_t *hpg);
int pru_init(int pru, char *filename, int disabled, hal_pru_generic_t *hpg);
int setup_pru(int pru, char *filename, int disabled, hal_pru_generic_t *hpg);
void pru_shutdown(int pru);
static void *pruevent_thread(void *arg);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    hal_pru_generic_t *hpg;
    int retval;

    comp_id = hal_init("hal_pru_generic");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
        return -1;
    }

    // Allocate HAL shared memory for state data
    hpg = hal_malloc(sizeof(hal_pru_generic_t));
    if (hpg == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
	        "%s: ERROR: hal_malloc() failed\n", modname);
	    hal_exit(comp_id);
	    return -1;
    }

    // Clear memory
    memset(hpg, 0, sizeof(hal_pru_generic_t));

    // Initialize PRU and map PRU data memory
    if ((retval = pru_init(pru, prucode, disabled, hpg))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: ERROR: failed to initialize PRU\n", modname);
        hal_exit(comp_id);
        return -1;
    }

    // Setup global state
    hpg->config.num_pwmgens  = num_pwmgens;
    hpg->config.num_stepgens = num_stepgens;
    hpg->config.comp_id      = comp_id;
    hpg->config.pru_period   = pru_period;
    hpg->config.name         = modname;

rtapi_print_msg(RTAPI_MSG_ERR, "%s: num_pwmgens : %d\n",modname, num_pwmgens);
rtapi_print_msg(RTAPI_MSG_ERR, "%s: num_stepgens: %d\n",modname, num_stepgens);

    // Initialize various functions and generate PRU data ram contents
    if ((retval = hpg_pwmgen_init(hpg))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: pwmgen init failed: %d\n",modname, retval);
        hal_exit(comp_id);
        return -1;
    }

    if ((retval = hpg_stepgen_init(hpg))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: stepgen init failed: %d\n",modname, retval);
        hal_exit(comp_id);
        return -1;
    }

hpg->wait.task_addr = pru_malloc(hpg, sizeof(hpg->wait.pru));
hpg->wait.pru.task.hdr.mode = eMODE_WAIT;

PRU_task_wait_t *prux = (PRU_task_wait_t *) ((u32) hpg->pru_data + (u32) hpg->wait.task_addr);
prux->task.hdr.mode  = eMODE_WAIT;
prux->task.hdr.dataX = 0x80;
prux->task.hdr.dataY = 0x00;

pru_task_add(hpg, hpg->wait.task_addr);

    if ((retval = export_pru(hpg))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: var export failed: %d\n",modname, retval);
        hal_exit(comp_id);
        return -1;
    }

    if ((retval = setup_pru(pru, prucode, disabled, hpg))) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "%s: ERROR: failed to initialize PRU\n", modname);
        hal_exit(comp_id);
        return -1;
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed\n", modname);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    pru_shutdown(pru);
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/
static void hpg_read(void *void_hpg, long period) {
//     // Read data from the PRU here...
//     hal_pru_generic_t *hpg = void_hpg;
//     PRU_chan_state_raw_t pru = (PRU_chan_state_raw_t) pru_data_ram;
//     int i;
// 
//     for (i = 0; i < MAX_CHAN; i++) {
//         s64 x, y;
//         u32 acc;
//         s64 acc_delta;
// 
//         switch (chan_state[i].gen.PRU.ctrl.mode) {
// 
//         case eMODE_STEP_DIR :
// 
//             // "atomic" read of accumulator and position register from PRU
// 	        do {
//              x = * (s64 *) hpg->pru_data + hpg->stepgen.instance[i].task_addr + offsetof(PRU_task_stepdir_t, accum);
// 	            x = pru[i].raw.qword[2];
// 	            y = pru[i].raw.qword[2];
// 	        } while ( x != y );
// 
//             // Update internal state and HAL outputs
//             chan_state[i].raw.PRU.qword[2] = x;
// 
//     *(chan_state[i].step.hal.pin.test1) = chan_state[i].step.PRU.accum;
//     *(chan_state[i].step.hal.pin.test2) = chan_state[i].step.PRU.pos;
//     
//             // Mangle 32-bit step count and 27 bit accumulator (with 5 bits of status)
//             // into a 16.16 value as expected by the hostmot2 stepgen logic
//             acc = (chan_state[i].step.PRU.accum >> 11) & 0x0000FFFF;
//             acc |= (chan_state[i].step.PRU.pos << 16);
// 
//     *(chan_state[i].step.hal.pin.test3) = acc;
// 
//             // those tricky users are always trying to get us to divide by zero
//             if (fabs(chan_state[i].step.hal.param.position_scale) < 1e-6) {
//                 if (chan_state[i].step.hal.param.position_scale >= 0.0) {
//                     chan_state[i].step.hal.param.position_scale = 1.0;
//                     rtapi_print_msg(RTAPI_MSG_ERR,
//                             "%s: stepgen %d position_scale is too close to 0, resetting to 1.0\n", modname, i);
//                 } else {
//                     chan_state[i].step.hal.param.position_scale = -1.0;
//                     rtapi_print_msg(RTAPI_MSG_ERR,
//                             "%s: stepgen %d position_scale is too close to 0, resetting to -1.0\n", modname, i);
//                 }
//             }
// 
//             // The HM2 Accumulator Register is a 16.16 bit fixed-point
//             // representation of the current stepper position.
//             // The fractional part gives accurate velocity at low speeds, and
//             // sub-step position feedback (like sw stepgen).
//             acc_delta = (s64)acc - (s64)chan_state[i].step.prev_accumulator;
//             if (acc_delta > INT32_MAX) {
//                 acc_delta -= UINT32_MAX;
//             } else if (acc_delta < INT32_MIN) {
//                 acc_delta += UINT32_MAX;
//             }
// 
//             chan_state[i].step.subcounts += acc_delta;
// 
//             *(chan_state[i].step.hal.pin.counts) = chan_state[i].step.subcounts >> 16;
// 
//             // note that it's important to use "subcounts/65536.0" instead of just
//             // "counts" when computing position_fb, because position_fb needs sub-count
//             // precision
//             *(chan_state[i].step.hal.pin.position_fb) = ((double)chan_state[i].step.subcounts / 65536.0) / chan_state[i].step.hal.param.position_scale;
// 
//             chan_state[i].step.prev_accumulator = acc;
// 
//             break;
// 
//         default :
//             // Nothing to export for other types
//             break;
//         }
//     }
// 
}

u16 ns2periods(hal_pru_generic_t *hpg, hal_u32_t ns) {
    u16 p = ceil((double)ns / (double)hpg->config.pru_period);
    return p;
}

static void hpg_write(void *void_hpg, long period) {
    hal_pru_generic_t *hpg      = void_hpg;
//     PRU_chan_state_ptr pru      = (PRU_chan_state_ptr) pru_data_ram;
//     int i, j;
// 
//     for (i = 0; i < MAX_CHAN; i++) {
//         switch (chan_state[i].gen.PRU.ctrl.mode) {
// 
//         case eMODE_STEP_DIR :
// 
//             // Update shadow of PRU control registers
//             chan_state[i].step.PRU.ctrl.enable  = *(chan_state[i].step.hal.pin.enable);
//             chan_state[i].step.PRU.ctrl.pin1    = chan_state[i].step.hal.param.steppin;
//             chan_state[i].step.PRU.ctrl.pin2    = chan_state[i].step.hal.param.dirpin;
// 
//             if (*(chan_state[i].step.hal.pin.enable) == 0) {
//                 chan_state[i].step.PRU.rate = 0;
//                 chan_state[i].step.old_position_cmd = *(chan_state[i].step.hal.pin.position_cmd);
//                 *(chan_state[i].step.hal.pin.velocity_fb) = 0;
//             } else {
//                 // call update function
//                 update_stepgen(hpg, period, i);
//             }
// 
//             // Update timing parameters if changed
//             if ((chan_state[i].step.hal.param.dirsetup  != chan_state[i].step.written_dirsetup ) ||
//                 (chan_state[i].step.hal.param.dirhold   != chan_state[i].step.written_dirhold  ) ||
//                 (chan_state[i].step.hal.param.steplen   != chan_state[i].step.written_steplen  ) ||
//                 (chan_state[i].step.hal.param.stepspace != chan_state[i].step.written_stepspace))
//             {
//                 chan_state[i].step.PRU.dirsetup     = ns2periods(chan_state[i].step.hal.param.dirsetup);
//                 chan_state[i].step.PRU.dirhold      = ns2periods(chan_state[i].step.hal.param.dirhold);
//                 chan_state[i].step.PRU.steplen      = ns2periods(chan_state[i].step.hal.param.steplen);
//                 chan_state[i].step.PRU.stepspace    = ns2periods(chan_state[i].step.hal.param.stepspace);
// 
//                 // Send new value(s) to the PRU
//                 pru[i].raw.dword[2] = chan_state[i].raw.PRU.dword[2];
//                 pru[i].raw.dword[3] = chan_state[i].raw.PRU.dword[3];
// 
//                 // Stash values written
//                 chan_state[i].step.written_dirsetup  = chan_state[i].step.hal.param.dirsetup;
//                 chan_state[i].step.written_dirhold   = chan_state[i].step.hal.param.dirhold;
//                 chan_state[i].step.written_steplen   = chan_state[i].step.hal.param.steplen;
//                 chan_state[i].step.written_stepspace = chan_state[i].step.hal.param.stepspace;
//             }
// 
//             // Update control word if changed
//             if (chan_state[i].raw.PRU.dword[0] != chan_state[i].step.written_ctrl) {
//                 pru[i].raw.dword[0] = chan_state[i].raw.PRU.dword[0];
//                 chan_state[i].step.written_ctrl = chan_state[i].raw.PRU.dword[0];
//             }
// 
//             // Send rate update to the PRU
//             pru[i].step.rate = chan_state[i].step.PRU.rate;
// 
//             break;
// 
//         case eMODE_DELTA_SIG :
// 
//             // Update shadow of PRU control registers
//             chan_state[i].delta.PRU.ctrl.enable  = *(chan_state[i].delta.hal_enable);
// 
//             if (*(chan_state[i].delta.hal_out1) >= 1.0) {
//                 chan_state[i].delta.PRU.value1 = 0x4000;
//             } else if (*(chan_state[i].delta.hal_out1) <= 0.0) {
//                 chan_state[i].delta.PRU.value1 = 0x0000;
//             } else {
//                 chan_state[i].delta.PRU.value1 = 
//                     (u32) (*(chan_state[i].delta.hal_out1) * (1 << 14)) & 0x3FFF;
//             }
// 
//             if (*(chan_state[i].delta.hal_out2) == 1.0) {
//                 chan_state[i].delta.PRU.value2 = 0x4000;
//             } else if (*(chan_state[i].delta.hal_out2) <= 0.0) {
//                 chan_state[i].delta.PRU.value2 = 0x0000;
//             } else {
//                 chan_state[i].delta.PRU.value2 =
//                     (u32) (*(chan_state[i].delta.hal_out2) * (1 << 14)) & 0x3FFF;
//             }
// 
//             chan_state[i].delta.PRU.ctrl.pin1   = chan_state[i].delta.hal_pin1;
//             chan_state[i].delta.PRU.ctrl.pin2   = chan_state[i].delta.hal_pin2;
// 
//             // Send updates to PRU
//             for (j = 0; j < 2; j++) {
//                 pru[i].raw.dword[j] = chan_state[i].raw.PRU.dword[j];
//             }
//             break;
// 
//         case eMODE_PWM :
// 
//             // Update shadow of PRU control registers
//             chan_state[i].pwm.PRU.ctrl.enable   = *(chan_state[i].pwm.hal_enable);
//             chan_state[i].pwm.PRU.period        = *(chan_state[i].pwm.hal_period);
//             chan_state[i].pwm.PRU.high1         = *(chan_state[i].pwm.hal_out1);
//             chan_state[i].pwm.PRU.high2         = *(chan_state[i].pwm.hal_out2);
// 
//             chan_state[i].pwm.PRU.ctrl.pin1     = chan_state[i].pwm.hal_pin1;
//             chan_state[i].pwm.PRU.ctrl.pin2     = chan_state[i].pwm.hal_pin2;
// 
//             // Send updates to PRU
//             for (j = 0; j < 4; j++) {
//                 pru[i].raw.dword[j] = chan_state[i].raw.PRU.dword[j];
//             }
//             break;
// 
//         default :
//             // Nothing to export for other types
//             break;
//         }
//     }
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

// Allocate 32-bit words from PRU data memory
// Start at the beginning of memory, and contiguously allocate RAM
// until we run out of requests.  No free, no garbage colletion, etc.
// Feel free to enhance this when you start adding and removing PRU
// tasks at run-time!  :)
pru_addr_t pru_malloc(hal_pru_generic_t *hpg, int len) {
    // Return address is first free memory location
    pru_addr_t a = hpg->pru_data_free;

    // Insure length is a natural 32-bit length
    int len32 = (len & ~0x03);
    if ((len % 4) != 0) len32 += 4;

    // Point to the next free location
    hpg->pru_data_free += len32;

rtapi_print_msg(RTAPI_MSG_ERR, "%s: pru_malloc requested %d bytes, allocated %d bytes starting at %04x\n",modname, len, len32, a);

    // ...and we're done
    return a;
}

int export_pru(hal_pru_generic_t *hpg)
{
    int r;
    char name[HAL_NAME_LEN + 1];

//         switch (chan_state[i].gen.PRU.ctrl.mode) {
// 
//         case eMODE_STEP_DIR :
// 
//             r= export_stepgenx(chan_state, i);
//             if (r != 0) { return r; }
// 
//             break;
// 
//         case eMODE_DELTA_SIG :
// 
//             // Export HAL Pins
//             rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.enable", modname, i);
//             r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].delta.hal_enable), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.out1", modname, i);
//             r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].delta.hal_out1), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.out2", modname, i);
//             r = hal_pin_float_new(name, HAL_IN, &(chan_state[i].delta.hal_out2), comp_id);
//             if (r != 0) { return r; }
// 
//             // Export HAL Parameters
//             rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.pin1", modname, i);
//             r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].delta.hal_pin1), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.delta.%02d.pin2", modname, i);
//             r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].delta.hal_pin2), comp_id);
//             if (r != 0) { return r; }
// 
//             // Initialize HAL Pins
//             *(chan_state[i].delta.hal_enable)   = 0;
//             *(chan_state[i].delta.hal_out1)     = 0.0;
//             *(chan_state[i].delta.hal_out2)     = 0.0;
// 
//             // Initialize HAL Parameters
//             chan_state[i].delta.hal_pin1        = PRU_DEFAULT_PIN;
//             chan_state[i].delta.hal_pin2        = PRU_DEFAULT_PIN;
// 
//             break;
// 
//         case eMODE_PWM :
// 
//             // Export HAL Pins
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.enable", modname, i);
//             r = hal_pin_bit_new(name, HAL_IN, &(chan_state[i].pwm.hal_enable), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.period", modname, i);
//             r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_period), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out1", modname, i);
//             r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_out1), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out2", modname, i);
//             r = hal_pin_u32_new(name, HAL_IN, &(chan_state[i].pwm.hal_out2), comp_id);
//             if (r != 0) { return r; }
// 
//             // Export HAL Parameters
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin1", modname, i);
//             r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].pwm.hal_pin1), comp_id);
//             if (r != 0) { return r; }
// 
//             rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin2", modname, i);
//             r = hal_param_u32_new(name, HAL_RW, &(chan_state[i].pwm.hal_pin2), comp_id);
//             if (r != 0) { return r; }
// 
//             // Initialize HAL Pins
//             *(chan_state[i].pwm.hal_enable)     = 0;
//             *(chan_state[i].pwm.hal_period)     = 0;
//             *(chan_state[i].pwm.hal_out1)       = 0;
//             *(chan_state[i].pwm.hal_out2)       = 0;
// 
//             // Initialize HAL Parameters
//             chan_state[i].pwm.hal_pin1          = PRU_DEFAULT_PIN;
//             chan_state[i].pwm.hal_pin2          = PRU_DEFAULT_PIN;
// 
//             break;
// 
//         default :
//             // Nothing to export for other types
//             break;
//         }

    // Export functions
    rtapi_snprintf(name, sizeof(name), "%s.update", modname);
    r = hal_export_funct(name, hpg_write, hpg, 1, 0, comp_id);
    if (r != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: function export failed: %s\n", modname, name);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_snprintf(name, sizeof(name), "%s.capture-position", modname);
    r = hal_export_funct(name, hpg_read, hpg, 1, 0, comp_id);
    if (r != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: function export failed: %s\n", modname, name);
        hal_exit(comp_id);
        return -1;
    }

    return 0;
}


int assure_module_loaded(const char *module)
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

int pru_init(int pru, char *filename, int disabled, hal_pru_generic_t *hpg) {
    
    int i;
    int retval;

    if (pru != 1) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: WARNING: PRU is %d and not 1\n",
            modname, pru);
    }

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

    hpg->pru_data = (u32 *) pru_data_ram;

    // Zero PRU data memory
    for (i = 0; i < 8192/4; i++) {
        hpg->pru_data[i] = 0;
    }

    // Reserve memory for PRU static variables
    hpg->pru_stat = 0;
    hpg->pru_data_free = sizeof(PRU_statics_t);

    // Setup PRU globals
    PRU_statics_t *stat = (PRU_statics_t *) ((u32) hpg->pru_data + (u32) hpg->pru_stat);
stat->task.hdr.dataX = 0xAB;
stat->task.hdr.dataY = 0xFE;
    stat->period = pru_period;
    hpg->config.pru_period = pru_period;

    return 0;
}

int setup_pru(int pru, char *filename, int disabled, hal_pru_generic_t *hpg) {
    
    int retval;

    if (event > -1) {
    prussdrv_start_irqthread (event, sched_get_priority_max(SCHED_FIFO) - 2,
                  pruevent_thread, (void *) event);
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: PRU event %d listener started\n",
            modname, event);
    }

    // Load and execute binary on PRU
    if (!strlen(filename))
    filename = EMC2_RTLIB_DIR "/" DEFAULT_CODE;
    retval =  prussdrv_exec_program (pru, filename, disabled);

    return retval;
}

void pru_task_add(hal_pru_generic_t *hpg, pru_addr_t addr) {
    // Be *VERY* careful with pointer math!  C likes to multiple integers by sizeof() the referenced object!
    PRU_statics_t     *stat = (PRU_statics_t *)     ((u32) hpg->pru_data + (u32) hpg->pru_stat);
    PRU_task_header_t *task = (PRU_task_header_t *) ((u32) hpg->pru_data + (u32) addr);

HPG_ERR("Data: %08x Statics: %08x Task: %08x Addr: %04x\n", hpg->pru_data, stat, task, addr);

HPG_ERR("Stat.hdr.addr = %04x\n", stat->task.hdr.addr);

    if (stat->task.hdr.addr == 0) {
        // This is the first task
        stat->task.hdr.addr = addr;     // PRU start of task list static variable
        task->hdr.addr  = addr;         // Point the next task pointer to ourselves
        hpg->last_task  = addr;         // Track the last task location
    }
    else {
        // Add this task to the end of the task list
        PRU_task_header_t *prev = (PRU_task_header_t *) ((u32) hpg->pru_data + (u32) hpg->last_task);

        task->hdr.addr = stat->task.hdr.addr;   // Point the next task pointer to the start of the task list
        prev->hdr.addr = addr;                  // Point the new task to the previous task
        hpg->last_task = addr;                  // Update the last task location
    }
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

void pru_shutdown(int pru)
{
    // Disable PRU and close memory mappings
    prussdrv_pru_disable(pru);
    prussdrv_exit (); // also joins event listen thread
}

