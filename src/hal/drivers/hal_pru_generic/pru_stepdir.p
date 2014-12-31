//----------------------------------------------------------------------//
// Description: pru.stepdir.p                                           //
// PRU code implementing step/dir generation task                       //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2013-May    Charles Steinkuehler                                     //
//             Split into several files                                 //
//             Altered main loop to support a linked list of tasks      //
//             Added support for GPIO pins in addition to PRU outputs   //
// 2012-Dec-27 Charles Steinkuehler                                     //
//             Initial version                                          //
//----------------------------------------------------------------------//
// This file is part of LinuxCNC HAL                                    //
//                                                                      //
// Copyright (C) 2013  Charles Steinkuehler                             //
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

MODE_STEP_DIR:
.enter STEP_DIR_SCOPE

.assign stepgen_state, GState.State_Reg0, GState.State_Reg6, State

#define DirHoldBit      31
#define DirChgBit       30
#define PulseHoldBit    29
#define GuardBit        28
#define StepBit         27

#define HoldMask        0x1F
#define DirHoldMask     0x3F

    // Read in task state data
    LBBO State, GTask.addr, SIZE(task_header), SIZE(State)

    // Accumulator MSBs are used for state/status encoding:
    // t31 = Dir Hold (set if we're waiting for direction setup/hold)
    // t30 = Dir Changed (set if rate changed direction and we need to update the direction output)
    // t29 = Pulse Hold (set if we're waiting for minimum high/low pulse length)
    // t28 = Guard bit (protects higher status bits from accumulator wrapping)
    // t27 = Overflow bit (indicates we should generate a step)

    // If the accumulator overflow bit is set here, we are holding for some reason
    // (bits 29-31 should tell us why, but we'll deal with that later)
    QBBS    SD_ACC_HOLD, State.Accum, StepBit
    ADD     State.Accum, State.Accum, State.Rate
SD_ACC_HOLD:

    // Check if direction changed
    XOR     r1.b0, State.Rate.b3, State.RateQ
    MOV     State.RateQ, State.Rate.b3
    QBBC    SD_DIR_CHG_DONE, r1.b0, 7

    // Flag direction change
    SET     State.Accum, DirChgBit

SD_DIR_CHG_DONE:

    // Update the pulse timings, if required
    QBBC    SD_PULSE_DONE, State.Accum, PulseHoldBit

    // Decrement timeout
    SUB     State.T_Pulse, State.T_Pulse, 1
    QBNE    SD_PULSE_DONE, State.T_Pulse, 0

    // Pulse timer expired

    // Check to see if step output is active
    QBEQ    SD_PULSE_DELAY_OVER, State.StepQ, 0

    // Step pulse output is active, clear it and setup pulse low delay
    MOV     r3.b1, GTask.dataX
    MOV     r3.b0, State.StepInvert
    CALL    SET_CLR_BIT
    LDI     State.StepQ, 0
    MOV     State.T_Pulse, State.Dly_step_space
    JMP     SD_PULSE_DONE

SD_PULSE_DELAY_OVER:

    // Step pulse output is low and pulse low timer expired,
    // so clear Pulse Hold bit in accumulator and we're done
    CLR     State.Accum, PulseHoldBit

SD_PULSE_DONE:

    // Decrement Direction timer if non-zero
    QBEQ    SD_DIR_SKIP_SUB, State.T_Dir, 0
    SUB     State.T_Dir, State.T_Dir, 1

SD_DIR_SKIP_SUB:

    // Process direction updates if required (either DirHoldBit or DirChgBit is set)
    QBGE    SD_DIR_DONE, State.Accum.b3, DirHoldMask

    // Wait for any pending timeout
    QBNE    SD_DIR_DONE, State.T_Dir, 0

    // Direction timer expired

    QBBC    SD_DIR_SETUP_DLY, State.Accum, DirChgBit

    // Dir Changed bit is set, we need to update Dir output and configure dir setup timer

    // Update Direction output
    MOV     r3.b1, GTask.dataY
    LSR     r3.b0, State.Rate, 31
    CALL    SET_CLR_BIT

    // Clear Dir Changed Bit
    CLR     State.Accum, DirChgBit
    SET     State.Accum, DirHoldBit
    MOV     State.T_Pulse, State.Dly_dir_setup
    JMP     SD_DIR_DONE

SD_DIR_SETUP_DLY:
    CLR     State.Accum, DirHoldBit

SD_DIR_DONE:

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
    QBBC    SD_DIR_UP, State.Rate, 31
    SUB     State.Pos, State.Pos, 2
SD_DIR_UP:

    // Update state
    MOV     r3.b1, GTask.dataX
    XOR     r3.b0, State.StepInvert, 1
    CALL    SET_CLR_BIT
    SET     State.StepQ, 0
    MVID    State.T_Pulse, *&State.Dly_step_len
// The above MVID instruction performs the two 16-bit MOVs below as a single 32-bit move
//  MOV     State.T_Pulse, State.Dly_step_len
//  MOV     State.T_Dir, State.Dly_dir_hold

STEP_DONE:
    // Save channel state data
    SBBO    State.Accum, GTask.addr, SIZE(task_header) + OFFSET(State.Accum), SIZE(State) - SIZE(State.StepInvert) - SIZE(State.Reserved1) - OFFSET(State.Accum)

    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave STEP_DIR_SCOPE

