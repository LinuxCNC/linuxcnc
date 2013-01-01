//----------------------------------------------------------------------//
// Description: stepgen.p                                               //
// PRU code implementing step/dir generation and other functions of     //
// hopeful use to off-load timing critical code from LinuxCNC HAL       //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Last change:                                                         //
// 2012-Dec-27 Charles Steinkuehler                                     //
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

#include "pru_support/pru.h"
#define LED_OFFSET 22

// Leave r30 available for use as direct I/O
.setcallreg r29.w2

.struct global_state
    .u32    Data                    // Pointer to per-channel data, doubles as loop counter
    .u32    Table                   // Jump Table base address
    .u32    Output                  // Shadow of r30 output bits

    // The following values overlap with the per-mode state definitions that follow
    // These definitions should be universal to all modes
    .u8     Status
    .u8     Mode
    .u8     Pin1
    .u8     Pin2
.ends

.struct chan_state
    .u32    Reserved1
    .u32    Reserved2
    .u32    Reserved3
    .u32    Reserved4
    .u32    Reserved5
    .u32    Reserved6
    .u32    Reserved7
    .u32    Reserved8
.ends

#define STATE_SIZE SIZE(chan_state)
#define NUMCHAN 7

.origin 0
.entrypoint START

START:
    // Clear syscfg[standby_init] to enable ocp master port
    LBCO    r0, CONST_PRUCFG, 4, 4
    CLR     r0, r0, 4
    SBCO    r0, CONST_PRUCFG, 4, 4

    // Clear all outputs
    LDI     r30, 0

    // Setup IEP timer
    LBCO    r6, C26, 0x40, 40       // Read 10 32-bit CMP registers
    OR      r6, r6, 0x03            // Set count reset and enable compare 0 event
    LDI     r8, 0x2710              // Set 10 uS timeout for CMP0
    SBCO    r6, C26, 0x40, 40       // Save 10 32-bit CMP registers

    LBCO    r2, C26, 0x00, 16       // Load Base IEP control register
    SET     r2, 0                   // Enable counter
    SBCO    r2, C26, 0x00, 4        // Save IEP GLOBAL_CFG register

    .assign global_state, r5, r8, GState

    // Setup registers
    LDI     GState.Data, STATE_SIZE * NUMCHAN       // Loop count & state data pointer
    LDI     GState.Table, #JUMPTABLE                // Base address of jump table
    LDI     GState.Output, 0                        // Output state

MAINLOOP:
    // Pre-decrement data pointer
    SUB     GState.Data, GState.Data, STATE_SIZE

    // Read channel state data into r8-r15
    LBBO    r8, GState.Data, 0, STATE_SIZE

    // Check to see if the channel is enabled
    QBEQ    CHAN_DISABLED, GState.Status, 0

    // Make sure mode is valid or we could fall off the end of the jump table!
    QBLT    CHAN_DISABLED, GState.Mode, JUMPTABLEEND - JUMPTABLE

    // Index into the jump table and call the routine appropriate for our mode
    ADD     r1, GState.Table, GState.Mode
    CALL    r1

    CHAN_DISABLED:
    // Keep going until all channels are processed
    QBNE    MAINLOOP, GState.Data, 0

// Debugging
    CLR     r30, 0                  // Clear busy bit
    SET     GState.Output, 0        // Set busy bit with all other outputs after we wait for a timer tick

    // Wait until the next timer tick...
// FIXME:
// Set up interrupt routing for the IEP timer and watch for the appropriate event
// (bit set in r31) instead of polling the IEP status register.
//  WBC     r31, 30
WAITLOOP:
    LBCO    r2, C26, 0x44, 4        // Load CMP_STATUS register
    QBBC    WAITLOOP, r2, 0         // Wait until counter times out
    SBCO    r2, C26, 0x44, 4        // Clear counter timeout bit

    // ...write out the pre-computed output bits...
    MOV     r30, GState.Output

    // ...and do it all again
    LDI     GState.Data, STATE_SIZE * NUMCHAN
    JMP     MAINLOOP

JUMPTABLE:
    JMP     MODE_NONE
    JMP     MODE_STEP_DIR
    JMP     MODE_UP_DOWN
    JMP     MODE_DELTA_SIG
    JMP     MODE_PWM
    JMP     MODE_GPIO
    JMP     MODE_NONE
    JMP     MODE_NONE
JUMPTABLEEND:

MODE_STEP_DIR:
.enter STEP_DIR_SCOPE

.struct stepgen_state
    .u8     Status
    .u8     Mode
    .u8     Pin1
    .u8     Pin2
    .u32    Rate
    .u16    Dly_step_high
    .u16    Dly_dir_hold
    .u16    Dly_step_low
    .u16    Dly_dir_setup
    .u32    Accum
    .u32    Pos
    .u16    T_Pulse
    .u16    T_Dir
    .u16    Reserved1
    .u8     Reserved2
    .u8     RateQ
.ends

.assign stepgen_state, r8, r15, State

#define DirHoldBit      31
#define DirChgBit       30
#define PulseHoldBit    29
#define GuardBit        28
#define StepBit         27

#define HoldMask        0x1F
#define DirHoldMask     0x3F

    // Accumulator MSBs are used for state/status encoding:
    // t31 = Dir Hold (set if we're waiting for direction setup/hold)
    // t30 = Dir Changed (set if rate changed direction and we need to update the direction output)
    // t29 = Pulse Hold (set if we're waiting for minimum high/low pulse length)
    // t28 = Guard bit (protets higher status bits from accumulator wrapping)
    // t27 = Overflow bit (indicates we should generate a step)

    // If the accumulator overflow bit is set here, we are holding for some reason
    // (bits 29-31 should tell us why, but we'll deal with that later)
    QBBS    ACC_HOLD, State.Accum, StepBit
    ADD     State.Accum, State.Accum, State.Rate
ACC_HOLD:

    // Check if direction changed
    XOR     r1.b0, State.Rate.b3, State.RateQ
    MOV     State.RateQ, State.Rate.b3
    QBBC    DIR_CHG_DONE, r1.b0, 7
    SET     State.Accum, DirChgBit

DIR_CHG_DONE:

    // Update the pulse timings, if required
    QBBC    PULSE_DONE, State.Accum, PulseHoldBit

    // Decrement timeout
    SUB     State.T_Pulse, State.T_Pulse, 1
    QBNE    PULSE_DONE, State.T_Pulse, 0

    // Pulse timer expired

    // Check to see if step output is high or low
    QBBC    PULSE_DELAY_OVER, GState.Output, State.Pin1

    // Step pulse output is high, clear it and setup pulse low delay
    CLR     GState.Output, State.Pin1
    MOV     State.T_Pulse, State.Dly_step_low
    JMP     PULSE_DONE

PULSE_DELAY_OVER:

    // Step pulse output is low and pulse low timer expired,
    // so clear Pulse Hold bit in accumulator and we're done
    CLR     State.Accum, PulseHoldBit

PULSE_DONE:

    // Decrement Direction timer if non-zero
    QBEQ    DIR_SKIP_SUB, State.T_Dir, 0
    SUB     State.T_Dir, State.T_Dir, 1

DIR_SKIP_SUB:

    // Process direction updates if required (either DirHoldBit or DirChgBit is set)
    QBGE    DIR_DONE, State.Accum.b3, DirHoldMask

    // Wait for any pending timeout
    QBNE    DIR_DONE, State.T_Dir, 0

    // Direction timer expired

    QBBC    DIR_SETUP_DLY, State.Accum, DirChgBit

    // Dir Changed bit is set, we need to update Dir output and configure dir setup timer

    // Update Direction output
    SET     GState.Output, State.Pin2
    QBBS    DIR_OUT_HIGH, State.Rate, 31
    CLR     GState.Output, State.Pin2
DIR_OUT_HIGH:

    // Clear Dir Changed Bit
    CLR     State.Accum, DirChgBit
    SET     State.Accum, DirHoldBit
    MOV     State.T_Pulse, State.Dly_dir_setup
    JMP     DIR_DONE

DIR_SETUP_DLY:
    CLR     State.Accum, DirHoldBit

DIR_DONE:

    QBBC    STEP_DONE, State.Accum, StepBit
    QBLT    STEP_DONE, State.Accum.b3, HoldMask

    // Time for a step!

    // Reset Accumulator status bits
    CLR     State.Accum, StepBit

    OR      State.Accum.b3, State.Accum.b3, 0x30    // Set GuardBit and PulseHoldBit
//    SET     State.Accum, GuardBit
//    SET     State.Accum, PulseHoldBit

    // Update position register
    ADD     State.Pos, State.Pos, 1
    QBBC    DIR_UP, State.Rate, 31
    SUB     State.Pos, State.Pos, 2
DIR_UP:

    // Update state
    SET     GState.Output, State.Pin1
    // Fixme: The following could be one 32-bit MOV
    MOV     State.T_Pulse, State.Dly_step_high
    MOV     State.T_Dir, State.Dly_dir_hold

STEP_DONE:
    // Save channel state data
    SBBO    State.Accum, GState.Data, OFFSET(State.Accum), STATE_SIZE - OFFSET(State.Accum)

    RET
.leave STEP_DIR_SCOPE

MODE_DELTA_SIG:
.enter DELTA_SIG_SCOPE
.struct delta_state
    .u8     Status
    .u8     Mode
    .u8     Pin1
    .u8     Pin2
    .u16    Value1          // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    .u16    Value2          // WARNING: Range is 14-bits: 0x0000 to 0x4000 inclusive!
    .u16    Reserved1
    .u16    Reserved2
    .u32    Reserved3
    .u16    Integrate1a
    .u16    Integrate2a
    .u16    Integrate1b
    .u16    Integrate2b
    .u16    Quant1
    .u16    Quant2
    .u32    Reserved4
.ends

.assign delta_state, r8, r15, State

    // Update Pin 1 integrator state
//    SUB     r1.w0, State.Value1, State.Quant1
//    XOR     r1.b1, r1.b1, 0x80
//    ADD     State.Integrate1a, State.Integrate1a, r1.w0
    SUB     r1, State.Value1, State.Quant1
    ADD     State.Integrate1b, State.Integrate1b, r1.w0
    
    // Update Pin 2 integrator state
//    SUB     r1.w0, State.Value2, State.Quant2
//    XOR     r1.b1, r1.b1, 0x80
//    ADD     State.Integrate2a, State.Integrate2a, r1.w0
    SUB     r1, State.Value2, State.Quant2
    ADD     State.Integrate2b, State.Integrate2b, r1.w0

    // Update Pin 1 Output State
    QBBC    OUT1Zero, State.Integrate1b, 15
    SET     GState.Output, State.Pin1
    LDI     State.Quant1, 0xC000
    JMP     Test2

    OUT1Zero:
    CLR     GState.Output, State.Pin1
    LDI     State.Quant1, 0x0000

    // Update Pin 2 Output State
    Test2:
    QBBC    OUT2Zero, State.Integrate2b, 15
    SET     GState.Output, State.Pin2
    LDI     State.Quant2, 0xC000
    JMP     DONE

    OUT2Zero:
    CLR     GState.Output, State.Pin2
    LDI     State.Quant2, 0x0000

    DONE:
    // Save channel state data
    SBBO    State.Integrate1a, GState.Data, OFFSET(State.Integrate1a), STATE_SIZE - OFFSET(State.Integrate1a)

    RET
.leave DELTA_SIG_SCOPE

MODE_PWM:
.enter PWM_SCOPE
.struct pwm_state
    .u8     Status
    .u8     Mode
    .u8     Pin1
    .u8     Pin2
    .u32    Period
    .u32    High1
    .u32    High2
    .u32    T_Period
    .u32    PeriodQ
    .u32    High1Q
    .u32    High2Q
.ends

.assign pwm_state, r8, r15, State

    // Increment Period Counter
    ADD     State.T_Period, State.T_Period, 1

    // Are we finished with this period?
    QBGT    PeriodNE, State.T_Period, State.PeriodQ
    SET     GState.Output, State.Pin1
    SET     GState.Output, State.Pin2

    LDI     State.T_Period, 0
    MOV     State.PeriodQ, State.Period
    MOV     State.High1Q, State.High1
    MOV     State.High2Q, State.High2
    PeriodNE:

    // See if we need to clear any outputs
    QBNE    High1NE, State.T_Period, State.High1Q
    CLR     GState.Output, State.Pin1
    High1NE:
    QBNE    High2NE, State.T_Period, State.High2Q
    CLR     GState.Output, State.Pin2
    High2NE:

    // Save channel state data
    SBBO    State.T_Period, GState.Data, OFFSET(State.T_Period), STATE_SIZE - OFFSET(State.T_Period)

    RET
.leave PWM_SCOPE

MODE_UP_DOWN:
MODE_GPIO:
MODE_NONE:
    RET


// BeagleBone PRU I/O Assignments
//
//  Chip            Mux                     BeagleBone
//  Pin Name        Value   PRU I/O Pin     Pin     Name        BeBoPr Use
//  ----------------------------------------------------------------------
//  mcasp0_aclkx    5       pru0.r30.0      
//  mcasp0_fsx      5       pru0.r30.1      
//  mcasp0_axr0     5       pru0.r30.2      
//  mcasp0_ahclkr   5       pru0.r30.3      
//  mcasp0_aclkr    5       pru0.r30.4      
//  mcasp0_fsr      5       pru0.r30.5      
//  mcasp0_axr1     5       pru0.r30.6      
//  mcasp0_ahclkx   5       pru0.r30.7      
//  mmc0_dat3       5       pru0.r30.8      
//  mmc0_dat2       5       pru0.r30.9      
//  mmc0_dat1       5       pru0.r30.10     
//  mmc0_dat0       5       pru0.r30.11     
//  mmc0_clk        5       pru0.r30.12
//  mmc0_cmd        5       pru0.r30.13
//  gpmc_ad12       6       pru0.r30.14     P8.12   GPIO1_12    
//  gpmc_ad13       6       pru0.r30.15     P8.11   GPIO1_13    
//  
//  lcd_data0       5       pru1.r30.0      P8.45   GPIO2_6     J3
//  lcd_data1       5       pru1.r30.1      P8.46   GPIO2_7     J2
//  lcd_data2       5       pru1.r30.2      P8.43   GPIO2_8     X Step
//  lcd_data3       5       pru1.r30.3      P8.44   GPIO2_9     X Dir
//  lcd_data4       5       pru1.r30.4      P8.41   GPIO2_10    X Enable    
//  lcd_data5       5       pru1.r30.5      P8.42   GPIO2_11    Y Step
//  lcd_data6       5       pru1.r30.6      P8.39   GPIO2_12    Y Dir
//  lcd_data7       5       pru1.r30.7      P8.40   GPIO2_13    Y Enable
//  lcd_vsync       5       pru1.r30.8      P8.27   GPIO2_22    Z Step
//  lcd_hsync       5       pru1.r30.9      P8.29   GPIO2_23    Z Dir
//  lcd_pclk        5       pru1.r30.10     P8.28   GPIO2_24    Z Enable
//  lcd_ac_bias_en  5       pru1.r30.11     P8.30   GPIO2_25    E Step
//  gpmc_csn1       5       pru1.r30.12     P8.21   GPIO1_30    E Dir
//  gpmc_csn2       5       pru1.r30.13     P8.20   GPIO1_31    E Enable
//  uart0_rxd       5       pru1.r30.14
//  uart0_txd       5       pru1.r30.15
//
//                          pru0.r30.12?    P8.36   UART3_CTSN  J4
//  ecap0_in_pwm0_out 3     ecap0_ecap_capin_apwm_o
//                  