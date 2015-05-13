//----------------------------------------------------------------------//
// hal_pru_generic.h                                                    //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2013-May    Charles Steinkuehler                                     //
//             Split into several files                                 //
//             Added support for PRU task list                          //
//             Refactored code to more closely match mesa-hostmot2      //
// 2013-May-20 Charles Steinkuehler                                     //
//             Initial version                                          //
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

#ifndef _hal_pru_generic_H_
#define _hal_pru_generic_H_

// please God where do these live in real life?
#define INT32_MIN (-2147483647-1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295U)
//#include <stdint.h>

#include "config_module.h"
#include RTAPI_INC_LIST_H
#include "rtapi.h"
#include "hal.h"

#include "hal/drivers/hal_pru_generic/pru_tasks.h"

#define HPG_VERSION "0.01"
#define HPG_NAME    "hpg"

#define HPG_PRINT(fmt, args...)  rtapi_print(HPG_NAME ": " fmt, ## args)

#define HPG_ERR(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  HPG_NAME ": " fmt, ## args)
#define HPG_WARN(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, HPG_NAME ": " fmt, ## args)
#define HPG_INFO(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, HPG_NAME ": " fmt, ## args)
#define HPG_DBG(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  HPG_NAME ": " fmt, ## args)

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

// Default pin to use for PRU modules...use a pin that does not leave the PRU
#define PRU_DEFAULT_PIN 17

typedef struct {
    pru_addr_t  addr;
    pru_addr_t  next;
} pru_task_t;

typedef struct {
    PRU_task_stepdir_t pru;

    pru_task_t task;

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

            hal_s32_t       *test1;
            hal_s32_t       *test2;
            hal_s32_t       *test3;

            hal_float_t     *position_scale;
            hal_float_t     *maxvel;
            hal_float_t     *maxaccel;

            hal_u32_t       *steplen;
            hal_u32_t       *stepspace;
            hal_u32_t       *dirsetup;
            hal_u32_t       *dirhold;

            hal_u32_t       *steppin;
            hal_u32_t       *dirpin;

            hal_bit_t       *stepinv;
        } pin;

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
    u32 written_steppin;
    u32 written_dirpin;
    u32 written_task;
    hal_bit_t written_stepinv;
} hpg_stepgen_instance_t;

typedef struct {
    int num_instances;
    hpg_stepgen_instance_t  *instance;
} hpg_stepgen_t;

typedef struct {
    // PRU control and state data
    PRU_task_delta_t PRU;

    // HAL Pins
    hal_bit_t       *hal_enable;

    hal_float_t     *hal_out1;
    hal_float_t     *hal_out2;
    
    hal_u32_t       *hal_pin1;
    hal_u32_t       *hal_pin2;

} hpg_deltasig_instance_t;

typedef struct {
    int num_instances;
    hpg_deltasig_instance_t  *instance;
} hpg_deltasig_t;

typedef struct {

    PRU_pwm_output_t    pru;

    struct {

        struct {
            hal_float_t *value;
            hal_bit_t   *enable;
            hal_float_t *scale;
            hal_u32_t   *pin;
        } pin;

    } hal;

    u32 written_pin;
} hpg_pwmgen_output_instance_t;

typedef struct {
    // PRU control and state data
    PRU_task_pwm_t      pru;
    pru_task_t          task;

    int num_outputs;
    hpg_pwmgen_output_instance_t    *out;

    // Instance-wide HAL variables
    struct {
        struct {
            hal_u32_t   *pwm_period;
        } pin;
    } hal;

    u32 written_pwm_period;
} hpg_pwmgen_instance_t;

typedef struct {
    int num_instances;
    hpg_pwmgen_instance_t  *instance;
} hpg_pwmgen_t;

//
// encoder
//

typedef struct {

    PRU_encoder_chan_t  pru;
    
    struct {

        struct {
            hal_s32_t   *rawcounts;     // raw encoder counts
            hal_s32_t   *rawlatch;      // raw encoder of latch
            hal_s32_t   *count;         // (rawcounts - zero_offset)
            hal_s32_t   *count_latch;   // (rawlatch - zero_offset)
            hal_float_t *position;
            hal_float_t *position_latch;
            hal_float_t *velocity;
            hal_bit_t   *reset;
            hal_bit_t   *index_enable;
            hal_bit_t   *latch_enable;
            hal_bit_t   *latch_polarity;
            hal_bit_t   *quadrature_error;
            hal_float_t *scale;
            hal_u32_t   *A_pin;
            hal_bit_t   *A_invert;
            hal_u32_t   *B_pin;
            hal_bit_t   *B_invert;
            hal_u32_t   *index_pin;
            hal_bit_t   *index_invert;
            hal_bit_t   *index_mask;
            hal_bit_t   *index_mask_invert;
            hal_u32_t   *counter_mode;
            hal_bit_t   *filter;
            hal_float_t *vel_timeout;
        } pin;

    } hal;

    s32 zero_offset;  // *hal.pin.counts == (*hal.pin.rawcounts - zero_offset)

    u16 prev_reg_count;  // from this and the current count in the register we compute a change-in-counts, which we add to rawcounts

    s32 prev_dS_counts;  // last time the function ran, it saw this many counts from the time before *that*

    u32 written_state;

    // these two are the datapoint last time we moved (only valid if state == HM2_ENCODER_MOVING)
    s32 prev_event_rawcounts;
    u16 prev_event_reg_timestamp;

    s32 tsc_num_rollovers;
    u16 prev_time_of_interest;

    enum { HM2_ENCODER_STOPPED, HM2_ENCODER_MOVING } state;

} hpg_encoder_channel_instance_t;

typedef struct {
    // PRU control and state data
    PRU_task_encoder_t  pru;
    pru_task_t          task;

    int num_channels;
    hpg_encoder_channel_instance_t  *chan;

    // Instance-wide HAL variables
    // ...nothing to see here...

    pru_addr_t LUT;
    u32 written_pin_invert;
} hpg_encoder_instance_t;

typedef struct {
    int num_instances;
    hpg_encoder_instance_t  *instance;
} hpg_encoder_t;


typedef struct {
    PRU_task_wait_t     pru;
    pru_task_t          task;
} hpg_wait_t;

typedef struct {

    struct {
        int pru_period;
        int num_pwmgens;
        int num_stepgens;
        int num_encoders;
        int comp_id;
        const char *name;
        const char *halname;
    } config;

    struct {
        struct {
            hal_u32_t  * pru_busy_pin;
        } pin;
    } hal;

    u32 *pru_data;              // ARM pointer to mapped PRU data memory
    pru_addr_t pru_data_free;   // Offset to first free data

    PRU_statics_t   pru_stat;
    pru_addr_t      pru_stat_addr;  // Offset to PRU static variables
//  pru_addr_t      last_task;      // Offset to last task in the task list
    pru_task_t      *last_task;     // Pointer to the last task in the task-list

    // this keeps track of all the tram entries
    struct list_head tram_read_entries;
    u32 *tram_read_buffer;
    u16 tram_read_size;

    struct list_head tram_write_entries;
    u32 *tram_write_buffer;
    u16 tram_write_size;

    hpg_pwmgen_t    pwmgen;
    hpg_stepgen_t   stepgen;
    hpg_deltasig_t  deltasig;
    hpg_encoder_t   encoder;

    hpg_wait_t      wait;

} hal_pru_generic_t;


//
// helper functions
//

pru_addr_t pru_malloc(hal_pru_generic_t *hpg, int len);
void pru_task_add(hal_pru_generic_t *hpg, pru_task_t *task);
int fixup_pin(u32 hal_pin);

//
// pwmgen functions
//

int hpg_pwmgen_init(hal_pru_generic_t *hpg);
void hpg_pwmgen_force_write(hal_pru_generic_t *hpg);
void hpg_pwmgen_update(hal_pru_generic_t *hpg);


//
// stepgen functions
//

int hpg_stepgen_init(hal_pru_generic_t *hpg);
void hpg_stepgen_force_write(hal_pru_generic_t *hpg);
void hpg_stepgen_update(hal_pru_generic_t *hpg, long l_period_ns);
u16 ns2periods(hal_pru_generic_t *hpg, hal_u32_t ns);
void hpg_stepgen_read(hal_pru_generic_t *hpg, long l_period_ns);


//
// encoder functions
//

int hpg_encoder_init(hal_pru_generic_t *hpg);
void hpg_encoder_force_write(hal_pru_generic_t *hpg);
void hpg_encoder_update(hal_pru_generic_t *hpg);
void hpg_encoder_read(hal_pru_generic_t *hpg);

#endif
