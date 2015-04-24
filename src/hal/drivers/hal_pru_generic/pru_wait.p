//----------------------------------------------------------------------//
// Description: pru.wait.p                                              //
// PRU code implementing the wait task, delaying execution until the    //
// next "timer tick" and updating the PRU and GPIO outputs              //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2015-Apr    Charles Steinkuehler                                     //
//             Merge DECAMUX support                                    //
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

MODE_WAIT:
.enter WAIT_SCOPE

.struct wait_state
    .u32    GPIO0_Clr_Addr
    .u32    GPIO1_Clr_Addr
    .u32    GPIO2_Clr_Addr
    .u32    GPIO3_Clr_Addr
#ifdef DECAMUX   
   // GPIO0 values align to b0, GPIO1 to b1 !
   .u16     PEPPER1_GPIO0
   .u16     PEPPER1_GPIO1
   .u16     PEPPER2_GPIO0
   .u16     PEPPER2_GPIO1
   .u16     GPIO0_Mask
   .u16     GPIO1_Mask
.ends

.struct pepper1_shadow
   .u32     GPIO01
.ends

.struct pepper2_shadow
   .u32     GPIO01
.ends

.struct pepper_masks
   .u32     Masks
#endif

.ends

.assign wait_state, GState.State_Reg0, *, State


    XIN     10, State, SIZE( State)          // Pull the GPIO addresses from first scratchpad

#ifdef DECAMUX

.assign pepper1_shadow, State.PEPPER1_GPIO0, *, State_PEPPER1
.assign pepper2_shadow, State.PEPPER2_GPIO0, *, State_PEPPER2
.assign pepper_masks, State.GPIO0_Mask, *, State_GPIO01

    // Put PEPPER2 signals on GPIO0 & GPIO1 outputs before generating strobe
    // Note that here we will only change PEPPER signals on GPIO0 & GPIO1 !

    AND     r3, State.PEPPER2_GPIO0, State.GPIO0_Mask // determine bits to be set
    XOR     r2, r3.w0, State.GPIO0_Mask      // determine bits to be cleared
    SBBO    r2, State.GPIO0_CLR_ADDR, 0, 8   // Write both CLR & SET registers

    XOR     r1.b3, r0, r0                    // clear leading byte for SBBO
    AND     r3, State.PEPPER2_GPIO1, State.GPIO1_Mask // determine bits to be set
    XOR     r2, r3, State.GPIO1_Mask            // determine bits to be cleared 
    SBBO    r1.b3, State.GPIO1_CLR_ADDR, 0, 8   // Write both CLR & SET registers

    // Negate DECAMUX clock (GPIO1.28) after having set PEPPER #2 data.
    // This generates a falling edge on the DM_CLK signal that on its
    // turn creates the enable for the latch on the DECAMUX that stores
    // the signals for the second PEPPER.

// FIXME: move to later moment giving more data setup time ???

    MOV     r2, (1 << 28)                     // GPIO1.28
    SBBO    r2, State.GPIO1_CLR_ADDR, 0, 4    // Write CLR register

    // Calculate new state for PEPPER #1 stepper signals
    // Since we are switching between PEPPER1 and PEPPER2 we need
    // to reconstruct the entire output pattern !

    OR      State.PEPPER1_GPIO0, State.PEPPER1_GPIO0, GState.GPIO0_SET.w0
    OR      State.PEPPER1_GPIO1, State.PEPPER1_GPIO1, GState.GPIO1_SET.w1
    AND     State_PEPPER1.GPIO01, State_PEPPER1.GPIO01, State_GPIO01.Masks
    AND     r2.w0, State.PEPPER1_GPIO0, GState.GPIO0_CLR.w0
    AND     r2.w2, State.PEPPER1_GPIO1, GState.GPIO1_CLR.w1
    XOR     State_PEPPER1.GPIO01, State_PEPPER1.GPIO01, r2

    // Add PEPPER1 stepper signals to SET and CLR masks for GPIO0 & GPIO1

    OR      GState.GPIO0_SET.w0, GState.GPIO0_SET.w0, State.PEPPER1_GPIO0 // merge bits to be set
    OR      GState.GPIO1_SET.w1, GState.GPIO1_SET.w1, State.PEPPER1_GPIO1 //
    XOR     r2, State_PEPPER1.GPIO01, State_GPIO01.Masks                  // find bits to clear
    OR      GState.GPIO0_CLR.w0, GState.GPIO0_CLR.w0, r2.w0               // merge bits to be cleared
    OR      GState.GPIO1_CLR.w1, GState.GPIO1_CLR.w1, r2.w2               //
      
    XOUT    10, State_PEPPER1.GPIO01, SIZE( State_PEPPER1)  // update GPIO0 & GPIO1 state on scratchpad

#endif
   
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

#ifdef DECAMUX   

    SBBO    GState.GPIO0_CLR, State.GPIO0_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO1_CLR, State.GPIO1_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
//    SBBO    GState.GPIO2_CLR, State.GPIO2_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
//    SBBO    GState.GPIO3_CLR, State.GPIO3_CLR_ADDR, 0, 8    // Writes both CLR and SET registers

   
    // Assert DECAMUX clock (GPIO1.28) after having set PEPPER #1 data
    // This generates a rising edge on the DM_CLK signal that in its
    // turn creates the enable for the latch on the DECAMUX that stores
    // the signals for the first PEPPER.
    SET     GState.GPIO1_SET, #28             // assert GPIO1.28 (DM_CLK)
    SBBO    GState.GPIO1_SET, State.GPIO1_CLR_ADDR, 4, 4    // Write SET register

// Quick hack for BeBoPr with two PEPPER drivers for 10 axes:
// Use output definitions from GPIO2 and GPIO3 for second PEPPER
// i.e. these map to GPIO0 and GPIO1 respectively.

    // Calculate new state for PEPPER #2 stepper signals

    OR      State.PEPPER2_GPIO0, State.PEPPER2_GPIO0, GState.GPIO2_SET.w0
    OR      State.PEPPER2_GPIO1, State.PEPPER2_GPIO1, GState.GPIO3_SET.w1
    AND     State_PEPPER2.GPIO01, State_PEPPER2.GPIO01, State_GPIO01.Masks
    AND     r2.w0, State.PEPPER2_GPIO0, GState.GPIO2_CLR.w0
    AND     r2.w2, State.PEPPER2_GPIO1, GState.GPIO3_CLR.w1
    XOR     State_PEPPER2.GPIO01, State_PEPPER2.GPIO01, r2

    XOUT    10, State_PEPPER2.GPIO01, SIZE( State_PEPPER2)  // update GPIO0 & GPIO1 state on scratchpad

#else

    SBBO    GState.GPIO0_CLR, State.GPIO0_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO1_CLR, State.GPIO1_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO2_CLR, State.GPIO2_CLR_ADDR, 0, 8    // Writes both CLR and SET registers
    SBBO    GState.GPIO3_CLR, State.GPIO3_CLR_ADDR, 0, 8    // Writes both CLR and SET registers

#endif
   
    // Clear the GPIO set/clear registers
    ZERO    &GState.GPIO0_Clr, OFFSET(GState.PRU_Out) - OFFSET(GState.GPIO0_Clr)

    // Save channel state data
    SBBO    GTask.dataY, GTask.addr, OFFSET(task_header.dataY), SIZE(task_header.dataY)

    // We're done here...carry on with the next task
    JMP     NEXT_TASK

.leave WAIT_SCOPE
