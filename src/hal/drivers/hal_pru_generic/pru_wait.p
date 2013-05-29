//----------------------------------------------------------------------//
// Description: pru.wait.p                                              //
// PRU code implementing the wait task, delaying execution until the    //
// next "timer tick" and updating the PRU and GPIO outputs              //
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

MODE_WAIT:
.enter WAIT_SCOPE

.struct wait_state
    .u32    GPIO0_Clr_Addr
    .u32    GPIO1_Clr_Addr
    .u32    GPIO2_Clr_Addr
    .u32    GPIO3_Clr_Addr
.ends

.assign wait_state, GState.State_Reg0, *, State

    // FIXME:
    // This scheme requires the GPIO addresses to be stored as local data
    // in the wait task.  A better solution would be to use the Register
    // Transfer instructions and pull the data from a scratchpad

    // Read channel state data
//    LBBO    STATE_REG, GTask.addr, 8, SIZE(State)
    XIN     10, State, 16

    // Debugging:
    // Task_DataY indicates we had a real-time error, or the timer tick
    // occurred before we began waiting for it!
    
    LBCO    r2, CONST_IEP, 0x44, 4      // Load CMP_STATUS register
    QBBC    BUSY_CHECK, r2, 0           // Check to see if timer has expired already
    SET     GTask.dataY,7               // Set MSB if error

    // Debugging:
    // Task_DataX is used to indicate we should set a busy bit when code
    // begins executing after a timer tick, and clear it once all work
    // is complete and we are waiting for the next timer tick
BUSY_CHECK:
    QBBC    WAITLOOP, GTask.dataX, 7        // If MSB is set, we should twiddle the busy bit
    CLR     r30, GTask.dataX                // Clear busy bit
    SET     GState.PRU_Out, GTask.dataX     // Set busy bit with all other outputs after we wait for a timer tick

WAITLOOP:
    // Wait until the next timer tick...
    // FIXME:
    // Set up interrupt routing for the IEP timer and watch for the appropriate event
    // (bit set in r31) instead of polling the IEP status register.
    //  WBC     r31, 30

    LBCO    r2, CONST_IEP, 0x44, 4      // Load CMP_STATUS register
    QBBC    WAITLOOP, r2, 0             // Wait until counter times out
    SBCO    r2, CONST_IEP, 0x44, 4      // Clear counter timeout bit

    // The timer just ticked...
    // ...write out the pre-computed output bits:
    MOV     r30, GState.PRU_Out
    SBBO    GState.GPIO0_CLR, State.GPIO0_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO1_CLR, State.GPIO1_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO2_CLR, State.GPIO2_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO3_CLR, State.GPIO3_CLR_ADDR, 0, 8    // Writes both CLR and SET registers

    // Clear the GPIO set/clear registers
    ZERO    &GState.GPIO0_Clr, OFFSET(GState.PRU_Out) - OFFSET(GState.GPIO0_Clr)

    // Save channel state data
    SBBO    GTask.dataY, GTask.addr, OFFSET(task_header.dataY), SIZE(task_header.dataY)

    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave WAIT_SCOPE
