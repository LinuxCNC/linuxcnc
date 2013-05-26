//----------------------------------------------------------------------//
// hal_pru_generic.h                                                    //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Last change:                                                         //
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

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

// PRU_* (Programmable Realtime Unit) names relate to software running 
// on the PRU, and likely need to match the PRU assembly code
//
// hpg_* (HAL PRU generic) defines are specific to the LinuxCNC HAL 
// driver side of things and need only be self-consistent with the 
// hal_pru_generic C code 

typedef u32 pru_addr_t;

typedef enum { 
    eMODE_INVALID   = -1,
    eMODE_NONE      = 0,
    eMODE_WAIT      = 1,
    eMODE_STEP_DIR  = 2,
    eMODE_UP_DOWN   = 3,    // Not implemented yet!
    eMODE_DELTA_SIG = 4,
    eMODE_PWM       = 5
} MODE;

typedef struct {
    u8      mode;
    u8      len;
    u8      dataX;
    u8      dataY;
    u32     addr;
} PRU_task_hdr_t;

typedef union {
    u32     dword[2];
    u16     word[4];
    u8      byte[8];
} PRU_task_raw_t;

typedef union {
    PRU_task_raw_t raw;
    PRU_task_hdr_t hdr;
} PRU_task_header_t;

typedef struct {
    PRU_task_header_t task;
    u32     period;
} PRU_statics_t;

typedef struct  {
    PRU_task_header_t task;

    s32     rate;
    u16     steplen;
    u16     dirhold;
    u16     stepspace;
    u16     dirsetup;
    u32     accum;
    u32     pos;
    u32     reserved[2];
} PRU_task_stepdir_t;

typedef struct {
    PRU_task_header_t task;

    u16     value1;         // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    u16     value2;         // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    u32     reserved[6];
} PRU_task_delta_t;

typedef struct {
    u16     output_n;
    u8      pin_n;
    u8      reserved_n;
} PRU_pwm_output_t;

typedef struct {
    PRU_task_header_t task;

    u16     prescale;
    u16     period;
    u32     reserved;
//  PRU_pwm_output_t out[task.len];
} PRU_task_pwm_t;

typedef struct {
    PRU_task_stepdir_t PRU;

    pru_addr_t task_addr;

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

    // HAL Parameters
    hal_u32_t       hal_pin1;
    hal_u32_t       hal_pin2;

} hpg_deltasig_instance_t;

typedef struct {
    int num_instances;
    hpg_deltasig_instance_t  *instance;
} hpg_deltasig_t;

typedef struct {
    // PRU control and state data
    PRU_task_pwm_t      PRU;
    PRU_pwm_output_t    *out;

    // HAL Pins
    hal_bit_t       *hal_enable;

    hal_u32_t       *hal_period;
    hal_u32_t       *hal_out1;
    hal_u32_t       *hal_out2;

    // HAL Parameters
    hal_u32_t       hal_pin1;
    hal_u32_t       hal_pin2;
} hpg_pwmgen_instance_t;

typedef struct {
    int num_instances;
    hpg_pwmgen_instance_t  *instance;
} hpg_pwmgen_t;

typedef struct {

    struct {
        int num_pwmgens;
        int num_stepgens;
    } config;

    u32 *pru_data;              // ARM pointer to mapped PRU data memory
    pru_addr_t pru_data_free;   // Offset to first free data
    pru_addr_t pru_stat;        // Offset to PRU static variables
    pru_addr_t last_task;       // Offset to last task in the task list

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

} hal_pru_generic_t;
