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

    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave DELTA_SIG_SCOPE
