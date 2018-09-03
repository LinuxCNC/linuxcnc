//----------------------------------------------------------------------//
// Description: pru_tasks.h                                             //
// Header with definitions that MUST match between the PRU assembly     //
// and the LinuxCNC HAL driver code.  In hopes of keeping these         //
// definitions consistant, both versions are contained in this file,    //
// separated by #ifdefs                                                 //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2013-May    Charles Steinkuehler                                     //
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

// This file defines the task structures and static variables needed to
// communicate between the HAL code running on the ARM and real time
// code running on the PRU
//
// Pre-processor #ifdef stanzas are used to allow both PRU assembly and
// C style typedefs to exist in the same file, since it is critical that
// the data structures for both sides match exactly.
//
// The _hal_pru_generic_H_ define (from hal_pru_generic.h) is used to 
// deteremine if we are assembling pru code or compiling C code.

//
// Basic types used elsewhere
//

#define PRU_DATA_START 0

#ifndef _hal_pru_generic_H_
    // pru_addr_t
    
    // pru_task_mode_t

#else
    typedef u32 pru_addr_t;

    // Insure these values match the JUMPTABLE in the pru assembly code!
    typedef enum { 
        eMODE_INVALID   = -1,
        eMODE_NONE      = 0,
        eMODE_WAIT      = 1,
        eMODE_WRITE     = 2,    // Not implemented yet!
        eMODE_READ      = 3,    // Not implemented yet!
        eMODE_STEP_DIR  = 4,
        eMODE_UP_DOWN   = 5,    // Not implemented yet!
        eMODE_DELTA_SIG = 6,
        eMODE_PWM       = 7,
        eMODE_ENCODER   = 8
    } pru_task_mode_t;
#endif

//
// Task header
//

#ifndef _hal_pru_generic_H_
    .struct task_header
        .u8     mode
        .u8     len
        .u8     dataX
        .u8     dataY
        .u32    addr
    .ends
#else
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
#endif

//
// Static variables
//

#ifndef _hal_pru_generic_H_
    .struct pru_statics
        .u8     mode
        .u8     len
        .u8     dataX
        .u8     dataY
        .u32    addr
        .u32    period
    .ends
#else
    typedef struct {
        PRU_task_header_t task;
        u32     period;
    } PRU_statics_t;
#endif

//
// Task structures, one for each 'flavor'
// The PRU versions do not include the task header, as the current task header
// is a global variable in the PRU assembly code
//

//
// stepgen task
//

#ifndef _hal_pru_generic_H_
    .struct stepgen_state
        .u32    Rate
        .u16    Dly_step_len
        .u16    Dly_dir_hold
        .u16    Dly_step_space
        .u16    Dly_dir_setup
        .u32    Accum
        .u32    Pos
        .u16    T_Pulse
        .u16    T_Dir
        .u8     StepQ
        .u8     RateQ
        .u8     Reserved1
        .u8     StepInvert
    .ends
#else
    typedef struct  {
        PRU_task_header_t task;

        s32     rate;
        u16     steplen;
        u16     dirhold;
        u16     stepspace;
        u16     dirsetup;
        u32     accum;
        u32     pos;
        u8      reserved[7];
        u8      stepinv;
    } PRU_task_stepdir_t;
#endif

//
// delta-sigma modulator task
//

#ifndef _hal_pru_generic_H_
    .struct delta_index
        .u16    Offset
        .u16    Reserved
    .ends

    .struct delta_output
        .u16    Value           // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
        .u8     Pin
        .u8     Reserved
        .u16    Integrate
        .u16    Quantize
    .ends

    .struct delta_state
        .u32    Reserved
    .ends
#else
    typedef struct {
        u16     value;          // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
        u8      pin;
        u8      reserved;
        u32     state;
    } PRU_delta_output_t;

    typedef struct {
        PRU_task_header_t task;

        u32     reserved;
    //  PRU_delta_output_t out[task.len];
    } PRU_task_delta_t;
#endif

//
// pwmgen task
//

#ifndef _hal_pru_generic_H_
    .struct pwm_index
        .u16    Offset
        .u16    Reserved
    .ends

    .struct pwm_output
        .u16    Value
        .u8     Pin
        .u8     Reserved
    .ends

    .struct pwm_state
        .u16    Prescale
        .u16    Period
        .u16    T_Prescale
        .u16    T_Period
    .ends
#else
    typedef struct {
        u16     value;
        u8      pin;
        u8      reserved;
    } PRU_pwm_output_t;

    typedef struct {
        PRU_task_header_t task;

        u16     prescale;
        u16     period;
        u32     reserved;
    //  PRU_pwm_output_t out[task.len];
    } PRU_task_pwm_t;
#endif

//
// encoder task
//

#ifndef _hal_pru_generic_H_
    .struct encoder_index
        .u32    wraddr          // Task address + sizeof(read-only objects in encoder_chan)
        .u16    Offset
        .u16    Reserved
    .ends

    .struct encoder_chan
        .u8     A_pin
        .u8     B_pin
        .u8     Z_pin           // Index
        .u8     mode

        .u8     AB_State
        .u8     AB_scratch
        .u16    count

        .u16    Z_capture
        .u8     Z_count         // Used by driver to compute "index seen"
        .u8     Z_State

    .ends

    .struct encoder_state
        .u32    pins            // XOR mask to invert all input pins in one instruction
        .u32    LUT             // Base address of LUT for counter modes
    .ends
#else
    typedef struct {
        u8      A_pin;
        u8      B_pin;
        u8      Z_pin;          // Index
        u8      mode;

        u8      AB_State;
        u8      AB_scratch;
        u16     count;

        u16     Z_capture;
        u8      Z_count;        // Used by driver to compute "index seen"
        u8      Z_State;
    } PRU_encoder_hdr_t;

    typedef union {
        u32     dword[3];
        u16     word[6];
        u8      byte[12];
    } PRU_encoder_raw_t;

    typedef union {
        PRU_encoder_raw_t raw;
        PRU_encoder_hdr_t hdr;
    } PRU_encoder_chan_t;

    typedef struct {
        u8      byte[64];
    } PRU_encoder_LUT_t;

    typedef struct {
        PRU_task_header_t task;

        u32     pin_invert;     // XOR mask to invert all input pins in one instruction
        u32     LUT;            // Base address of LUT for counter modes
    //  PRU_encoder_chan_t enc[task.len];
    } PRU_task_encoder_t;
#endif

//
// wait task
//

#ifndef _hal_pru_generic_H_

#else
    typedef struct {
        PRU_task_header_t task;
    } PRU_task_wait_t;
#endif
