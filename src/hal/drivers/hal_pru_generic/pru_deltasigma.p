//----------------------------------------------------------------------//
// Description: pru.deltasigma.p                                        //
// PRU code implementing Delta Sigma modulation task                    //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Last change:                                                         //
// 2013-May-20 Charles Steinkuehler                                     //
//             Split into several files                                 //
//             Altered main loop to support a linked list of tasks      //
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

MODE_DELTA_SIG:
.enter DELTA_SIG_SCOPE

.assign delta_state,  GState.State_Reg0, GState.State_Reg0, State
.assign delta_index,  GState.State_Reg1, GState.State_Reg1, Index
.assign delta_output, GState.State_reg2, GState.State_Reg3, Output

    // Skip everything if no outputs are configured
    QBEQ    DELTA_DONE, GTask.len, 0

    // Read in task state data
    LBBO State, GTask.addr, SIZE(task_header), SIZE(State)

    // Cycle through all configured outputs one at a time
    LDI     Index.Offset, SIZE(task_header) + SIZE(State)

DELTA_OUT_LOOP:
    LBBO    Output, GTask.addr, Index.Offset, SIZE(Output)

    // Update integrator state
    SUB     r1, Output.Value, Output.Quantize
    ADD     Output.Integrate, Output.Integrate, r1.w0
    
    // Calculate Output State and store quantized value for next loop
    QBBC    DELTA_OUT_Zero, Output.Integrate, 15
    LDI     Output.Quantize, 0xC000
    JMP     DELTA_DO_PIN

    DELTA_OUT_Zero:
    LDI     Output.Quantize, 0x0000

    DELTA_DO_PIN:
    MOV     r3.b1, Output.Pin
    MIN     r3.b0, Output.Quantize, 1
    CALL    SET_CLR_BIT

    // Save output state data
    SBBO    Output.Integrate, GTask.addr, Index.Offset, SIZE(Output) - OFFSET(Output.Integrate)

    // ...and loop until we're done
    ADD     Index.Offset, Index.Offset, SIZE(Output)
    SUB     GTask.len, GTask.len, 1

    QBNE    DELTA_OUT_LOOP, GTask.len, 0

DELTA_DONE:
    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave DELTA_SIG_SCOPE
