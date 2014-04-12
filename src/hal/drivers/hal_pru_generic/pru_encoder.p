//----------------------------------------------------------------------//
// Description: pru_encoder.p                                           //
// PRU code implementing quadrature encoder input                       //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2014-Feb    Charles Steinkuehler                                     //
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

MODE_ENCODER:
.enter ENCODER_SCOPE

.assign encoder_state,  GState.State_Reg0, GState.State_Reg1, State
.assign encoder_index,  GState.State_Reg2, GState.State_Reg3, Index
.assign encoder_chan,   GState.State_Reg4, GState.State_Reg6, Encoder

// First writable element in Encoder struct, elements before this are read-only
#define Encoder_First_Wr Encoder.AB_State

    // Skip everything if no outputs are configured
    QBEQ    ENCODER_DONE, GTask.len, 0

    // Read in task state data
    LBBO State, GTask.addr, SIZE(task_header), SIZE(State)

    // Read and optionally invert direct input pins
    // State.pins contains the xor mask read in above
    // Overwrite with the xor result since we're done with the mask until next time
    XOR     State.pins, r31, State.pins

    // Load the write pointer with the task address pointer offset by the number
    // of read-only bytes in the per-channel Encoder struct
    ADD     Index.wraddr, GTask.addr, OFFSET(Encoder_First_Wr)

    // Point to the first Encoder definition
    LDI     Index.Offset, SIZE(task_header) + SIZE(State)

ENCODER_LOOP:
    // Read previous Encoder state
    LBBO    Encoder, GTask.addr, Index.Offset, SIZE(Encoder)

    // !!!!!!!!!!!!!!!
    // !!! WARNING !!!
    // !!!!!!!!!!!!!!!
    //
    // The code below mixes reference by name between two different structs
    // (Encoder and State), which is required in order to do a 16-bit shift
    // on the combined Encoder.AB_scratch and Encoder.AB_state fields
    // If anything changes the alignment of these two structures, THIS CODE
    // WILL BREAK!
    //
    // GState.State_reg5.w0 = (Encoder.AB_scratch << 8) | Encoder.AB_state
    //
    // Use a define to make this easy to update later, if things change

#define Encoder_AB_16 GState.State_reg5.w0

    // Manipulate input bits to generate a LUT index value consisting of:
    // 0 0 Mode1 Mode0 B_new A_new B_old A_old
    LSR     Encoder.AB_scratch, State.pins, Encoder.A_pin   // A into LSB of scratch
    LSR     Encoder_AB_16, Encoder_AB_16, 1                 // Shift A into AB_state
    LSR     Encoder.AB_scratch, State.pins, Encoder.B_pin   // B into LSB of scratch
    LSR     Encoder_AB_16, Encoder_AB_16, 1                 // Shift B into AB_state

    // AB_State is now B_new A_new B_old A_old x x x x

    AND     Encoder.AB_scratch, Encoder.mode, 0x03          // Load mode bits next to AB_state
    LSR     Encoder.AB_scratch, Encoder_AB_16, 4     // Combine mode with AB_state and place in AB_scratch

    // AB_Scratch is now 0 0 Mode1 Mode0 B_new A_new B_old A_old
    // Lookup count value based on LUT index in AB_scratch
    LBBO    GState.Scratch0.b0, State.LUT, Encoder.AB_scratch, 1

    // Update count based on LUT results
    // PRU only does unsigned math, so LUT result is 0, 1, or 2
    // and we update the count using newcount = count + LUT - 1
    // to get +/- 1 so we can count up and down
    ADD     Encoder.count, Encoder.count, GState.Scratch0.b0
    SUB     Encoder.count, Encoder.count, 1

    // Capture count on rising edge of index pulse
    QBBC    Z_DONE, State.pins, Encoder.Z_pin               // No rising edge if new value is zero
    QBBS    Z_DONE, Encoder.Z_state, 0                      // No rising edge if old value is one

    // Rising edge on Z

    MOV     Encoder.Z_capture, Encoder.count                // Capture count value
    ADD     Encoder.Z_count, Encoder.Z_count, 1             // Add one to Z_count so SW knows we saw an index pulse

Z_DONE:

    // Save state data for this encoder
    SBBO    Encoder_First_Wr, Index.wraddr, Index.Offset, SIZE(Encoder) - OFFSET(Encoder_First_Wr)
    
    // Point to the next Encoder struct and carry on...
    ADD     Index.Offset, Index.Offset, SIZE(Encoder)
    SUB     GTask.len, GTask.len, 1
    QBNE    ENCODER_LOOP, GTask.len, 0

ENCODER_DONE:
    // No global task data to save, just per-channel data saved above

    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave ENCODER_SCOPE
