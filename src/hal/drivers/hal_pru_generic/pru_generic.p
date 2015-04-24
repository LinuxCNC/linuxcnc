//----------------------------------------------------------------------//
// Description: pru.main.p                                              //
// PRU code implementing the main task loop and routines useful for all //
// task types                                                           //
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

#include "pru.h"
#include "pru_tasks.h"

// Register Usage:
// r0   Scratch / bn count value for LBBO/SBBO/LBCO/SBCO/XIN/XOUT/XCHG/SXIN/SXOUT/SXCHG / XFR Shift Value
// r1   Scratch / Register pointers for MVIx instructions
// r2   Scratch
// r3   Scratch
// r4   Task Data (task specific use)
// r5   Task Data (task specific use)
// r6   Task Data (task specific use)
// r7   Task Data (task specific use)
// r8   Task Data (task specific use)
// r9   Task Data (task specific use)
// r10  Task Data (task specific use)
// r11  Task Data (task specific use)
// r12  TASK_STATUS
// r13  TASK_ADDR
// r14  GPIO0_Set
// r15  GPIO0_Clr
// r16  GPIO1_Set
// r17  GPIO1_Clr
// r18  GPIO2_Set
// r19  GPIO2_Clr
// r20  GPIO3_Set
// r21  GPIO3_Clr
// r22  PRU_Out
// r23  Scratch / Reserved (PRU_Out_Remote)
// r24  Call Register
// r25  Scratch / Reserved (Multiplier mode/status)
// r26  Scratch / Reserved (Multiplier Lower product)
// r27  Scratch / Reserved (Multiplier Upper product)
// r28  Scratch / Reserved (Multiplier Operand)
// r29  Scratch / Reserved (Multiplier Operand)
// r30  Direct Outputs
// r31  Direct Inputs / Event Generation

// Leave r30 available for use as direct I/O
.setcallreg r24.w2

#define STATE_SIZE  8
#define NUMCHAN     3

.struct global_state
    .u32    Scratch0
    .u32    Scratch1
    .u32    Scratch2
    .u32    Scratch3
    .u32    State_Reg0
    .u32    State_Reg1
    .u32    State_Reg2
    .u32    State_Reg3
    .u32    State_Reg4
    .u32    State_Reg5
    .u32    State_Reg6
    .u32    State_Reg7
    .u32    Task_Status
    .u32    Task_Addr
    .u32    GPIO0_Clr
    .u32    GPIO0_Set
    .u32    GPIO1_Clr
    .u32    GPIO1_Set
    .u32    GPIO2_Clr
    .u32    GPIO2_Set
    .u32    GPIO3_Clr
    .u32    GPIO3_Set
    .u32    PRU_Out
    .u16    TaskTable
    .u16    PinTable
    .u32    Call_Reg
    .u32    Mul_Status
    .u32    Mul_Prod_L
    .u32    Mul_Prod_H
    .u32    Mul_Op1
    .u32    Mul_Op2
.ends

.assign global_state, r0, r29, GState

// Overlay task header onto proper GState registers
.assign task_header, r12, r13, GTask

.origin 0
.entrypoint START

// PRU GPIO Write Timing Details
// The actual write instruction to a GPIO pin using SBBO takes two 
// PRU cycles (10 nS).  However, the GPIO logic can only update every 
// 40 nS (8 PRU cycles).  This meas back-to-back writes to GPIO pins 
// will eventually stall the PRU, or you can execute 6 PRU instructions 
// for 'free' when burst writing to the GPIO.
//
// Latency from the PRU write to the actual I/O pin changing state
// (normalized to PRU direct output pins = zero latency) when the
// PRU is writing to GPIO1 and L4_PERPort1 is idle measures 
// 95 nS or 105 nS (apparently depending on clock synchronization)
//
// PRU GPIO Posted Writes
// When L4_PERPort1 is idle, it is possible to burst-write multiple
// values to the GPIO pins without stalling the PRU, as the writes 
// are posted.  With an unrolled loop (SBBO to GPIO followed by a 
// single SET/CLR to R30), the first 20 write cycles (both 
// instructions) took 15 nS each, at which point the PRU began
// to stall and the write cycle settled in to the 40 nS maximum
// update frequency.
//
// PRU GPIO Read Timing Details
// Reading from a GPIO pin when L4_PERPort1 is idle require 165 nS as
// measured using direct PRU I/O updates bracking a LBBO instruction.
// Since there is no speculative execution on the PRU, it is not possible
// to execute any instructions during this time, the PRU just stalls.
//
// Latency from the physical I/O pin to the PRU read seeing valid data
// has not yet been measured.

START:
    FILL    &GState, SIZE(GState)

    // Clear syscfg[standby_init] to enable ocp master port
    LBCO    r0, CONST_PRUCFG, 4, 4
    CLR     r0, r0, 4
    SBCO    r0, CONST_PRUCFG, 4, 4

    // Clear all outputs
    LDI     r30, 0

    // Setup IEP timer
    LBCO    r6, CONST_IEP, 0x40, 40                 // Read all 10 32-bit CMP registers into r6-r15
    OR      r6, r6, 0x03                            // Set count reset and enable compare 0 event

    // Use Task_Addr to point to static variables during init
    MOV     GState.Task_Addr, PRU_DATA_START

    // Load loop period from static variables into CMP0
    LBBO    r8, GState.Task_Addr, OFFSET(pru_statics.period), SIZE(pru_statics.period)

    SBCO    r6, CONST_IEP, 0x40, 40                 // Save 10 32-bit CMP registers

    MOV     r2, 0x00000551                          // Enable counter, configured to count nS (increments by 5 each clock)
    SBCO    r2, CONST_IEP, 0x00, 4                  // Save IEP GLOBAL_CFG register

    // Setup registers

    // Zero all output registers
    ZERO    &GState.GPIO0_Clr, OFFSET(GState.TaskTable) - OFFSET(GState.GPIO0_Clr)

    // Setup Scratch-Pad 0 with GPIO addresses
    MOV     GState.State_Reg0, GPIO0 + GPIO_CLEARDATAOUT
    MOV     GState.State_Reg1, GPIO1 + GPIO_CLEARDATAOUT
    MOV     GState.State_Reg2, GPIO2 + GPIO_CLEARDATAOUT
    MOV     GState.State_Reg3, GPIO3 + GPIO_CLEARDATAOUT

#ifdef DECAMUX
    ZERO    &GState.State_Reg4, 12
    // setup masks for PEPPER signals in GPIO0 & GPIO1
    MOV     GState.State_Reg6.w0, 0x003c            // bits 2,3,4 & 5 for GPIO0 mask (w0)
    MOV     GState.State_Reg6.w2, 0x03f0            // bits 12,13,14,15,16 & 17 for GPIO1 mask (w1)
    XOUT    10, GState.State_Reg0, 28
#else
    XOUT    10, GState.State_Reg0, 16
#endif
    ZERO    &GState.State_Reg0, 16

    LDI     GState.TaskTable, #TASKTABLE                // Base address of jump tables
    LDI     GState.PinTable, #PINTABLE

    // Load start of task list from static variables
    LBBO    GState.Task_Addr, GState.Task_Addr, OFFSET(pru_statics.addr), SIZE(pru_statics.addr)

MAINLOOP:
    // Read task details (Mode, Len, DataX, DataY)
    LBBO    GState.Task_Status, GTask.addr, OFFSET(task_header.mode), SIZE(GState.Task_Status)

    // Make sure mode is valid or we could fall off the end of the jump table!
    QBLT    NEXT_TASK, GTask.mode, TASKTABLEEND - TASKTABLE

    // Index into the jump table and call the routine appropriate for our mode
    ADD     r1, GState.TaskTable, GTask.mode
    JMP     r1

NEXT_TASK:
    // Load the next task address
    LBBO    GTask.addr, GTask.addr, OFFSET(task_header.addr), SIZE(task_header.addr)

    // ...and keep going!
    JMP     MAINLOOP

SET_CLR_BIT:
    // Manipulate passed parameters into an even value from 0-30 to use to
    // index into the pin jump table:
    // Bit:    7  6  5  4  3  2  1  0
    // Value:  0  0  0  T  T  T  S  0
    //   T = Target register, S = Set_Clear

    LSL     r3.b2, r3.b0, 7
    LSR     r3.b3, r3.b1, 5
    LSR     r3.b0, r3.w2, 6
    
    ADD     r3.w2, GState.PinTable, r3.b0
    JMP     r3.w2

PINTABLE:
    RET
    RET
    RET
    RET

    SET     GState.GPIO0_Clr, GState.Scratch3.b1
    RET
    SET     GState.GPIO0_Set, GState.Scratch3.b1
    RET

    SET     GState.GPIO1_Clr, GState.Scratch3.b1
    RET
    SET     GState.GPIO1_Set, GState.Scratch3.b1
    RET

    SET     GState.GPIO2_Clr, GState.Scratch3.b1
    RET
    SET     GState.GPIO2_Set, GState.Scratch3.b1
    RET

    SET     GState.GPIO3_Clr, GState.Scratch3.b1
    RET
    SET     GState.GPIO3_Set, GState.Scratch3.b1
    RET

    CLR     GState.PRU_Out,   GState.Scratch3.b1
    RET
    SET     GState.PRU_Out,   GState.Scratch3.b1
    RET

    RET
    RET
    RET
    RET

    RET
    RET
    RET
    RET
PINTABLEEND:

TASKTABLE:
    JMP     NEXT_TASK           // MODE_NONE
    JMP     MODE_WAIT
    JMP     NEXT_TASK           //     JMP     MODE_WRITE
    JMP     NEXT_TASK           //     JMP     MODE_READ
    JMP     MODE_STEP_DIR
    JMP     NEXT_TASK           //     JMP     MODE_UP_DOWN
    JMP     MODE_DELTA_SIG
    JMP     MODE_PWM
    JMP     MODE_ENCODER
TASKTABLEEND:

    JMP     START

#include "pru_wait.p"
// #include "pru_write.p"
// #include "pru_read.p"
#include "pru_stepdir.p"
// #include "pru_updown.p"
#include "pru_deltasigma.p"
#include "pru_pwm.p"
#include "pru_encoder.p"

// BeagleBone PRU I/O Assignments
//
// There is also a nice table available on github: https://github.com/selsinork/beaglebone-black-pinmux
//
//  AM3359ZCZ           Mux                         BeagleBone
//  Pin Name            Value   PRU I/O Pin         Pin     Name        BeBoPr Use
//  ------------------------------------------------------------------------------
//  A13 mcasp0_aclkx    5       pru0.r30.0          P9.31   SPI1_SCLK
//                      6       pru0.r31.0
//  B13 mcasp0_fsx      5       pru0.r30.1          P9.29   SPI1_D0
//                      6       pru0.r31.1
//  D12 mcasp0_axr0     5       pru0.r30.2          P9.30   SPI1_D1
//                      6       pru1.r31.2
//  C12 mcasp0_ahclkr   5       pru0.r30.3          P9.28   SPI1_CS0
//                      6       pru0.r31.3
//  B12 mcasp0_aclkr    5       pru0.r30.4          -
//                      6       pru0.r31.4
//  C13 mcasp0_fsr      5       pru0.r30.5          P9.27   GPIO3_19
//                      6       pru0.r31.5
//  D13 mcasp0_axr1     5       pru0.r30.6          -
//                      6       pru0.r31.6
//  A14 mcasp0_ahclkx   5       pru0.r30.7          P9.25   GPIO3_21
//                      6       pru0.r31.7
//  F17 mmc0_dat3       5       pru0.r30.8          -
//                      6       pru0.r31.8
//  F18 mmc0_dat2       5       pru0.r30.9          -
//                      6       pru0.r31.9
//  G15 mmc0_dat1       5       pru0.r30.10         -
//                      6       pru0.r31.10
//  G16 mmc0_dat0       5       pru0.r30.11         -
//                      6       pru0.r31.11
//  G17 mmc0_clk        5       pru0.r30.12         -
//                      6       pru0.r31.12
//  G18 mmc0_cmd        5       pru0.r30.13         -
//                      6       pru0.r31.13
//  T12 gpmc_ad12       6       pru0.r30.14         P8.12   GPIO1_12    
//  V13 gpmc_ad14       6       pru0.r31.14         P8.16   GPIO1_14
//  R12 gpmc_ad13       6       pru0.r30.15         P8.11   GPIO1_13    
//  U13 gpmc_ad15       6       pru0.r31.15         P8.15   GPIO1_15
//  D14 xdma_event_intr1 5      pru0.r31.16 P9.41   CLKOUT2
//  D15 uart1_txd       6       pru0.r31.16 P9.24   UART1_TXD
//  
//  R1  lcd_data0       5       pru1.r30.0          P8.45   GPIO2_6     J3
//                      6       pru1.r31.0
//  R2  lcd_data1       5       pru1.r30.1          P8.46   GPIO2_7     J2
//                      6       pru1.r31.1
//  R3  lcd_data2       5       pru1.r30.2          P8.43   GPIO2_8     X Step
//                      6       pru1.r31.2
//  R4  lcd_data3       5       pru1.r30.3          P8.44   GPIO2_9     X Dir
//                      6       pru1.r31.3
//  T1  lcd_data4       5       pru1.r30.4          P8.41   GPIO2_10    X Enable    
//                      6       pru1.r31.4
//  T2  lcd_data5       5       pru1.r30.5          P8.42   GPIO2_11    Y Step
//                      6       pru1.r31.5
//  T3  lcd_data6       5       pru1.r30.6          P8.39   GPIO2_12    Y Dir
//                      6       pru1.r31.6
//  T4  lcd_data7       5       pru1.r30.7          P8.40   GPIO2_13    Y Enable
//                      6       pru1.r31.7
//  U8  lcd_vsync       5       pru1.r30.8          P8.27   GPIO2_22    Z Step
//                      6       pru1.r31.8
//  R5  lcd_hsync       5       pru1.r30.9          P8.29   GPIO2_23    Z Dir
//                      6       pru1.r31.9
//  V5  lcd_pclk        5       pru1.r30.10         P8.28   GPIO2_24    Z Enable
//                      6       pru1.r31.10
//  R6  lcd_ac_bias_en  5       pru1.r30.11         P8.30   GPIO2_25    E Step
//                      6       pru1.r31.11
//  U9  gpmc_csn1       5       pru1.r30.12         P8.21   GPIO1_30    E Dir
//                      6       pru1.r31.12
//  V9  gpmc_csn2       5       pru1.r30.13         P8.20   GPIO1_31    E Enable
//                      6       pru1.r31.13
//  E15 uart0_rxd       5       pru1.r30.14         -
//                      6       pru1.r31.14
//  E16 uart0_txd       5       pru1.r30.15         -
//                      6       pru1.r31.15
//  A15 xdma_event_intr0 5      pru1.r31.16         -
//  D16 uart1_rxd       6       pru1.r31.16 P9.26   UART1_RXD
//
//  U3  lcd_data10      -       -                   P8.36   UART3_CTSN  J4
//
//      ecap0_in_pwm0_out 3     ecap0_ecap_capin_apwm_o
//                  




//                          BeagleBone          AM3359ZCZ           Mux                
//  Replicape   BeBoPr      Pin     Name        Pin Name            Value   PRU I/O Pin
//  ----------------------------------------------------------------------------------
//  
//                          P8.1    GND
//                          P8.2    GND
//  E2_Step     Enable      P8.3    GPIO1_6     R9  gpmc_ad6
//  E1_Dir                  P8.4    GPIO1_7     T9  gpmc_ad7
//              Enable_n    P8.5    GPIO1_2     R8  gpmc_ad2
//                          P8.6    GPIO1_3     T8  gpmc_ad3
//                          P8.7    TIMER4      R7  gpmc_advn_ale
//  E2_Fault                P8.8    TIMER7      T7  gpmc_oen_ren
//                          P8.9    TIMER5      T6  gpmc_be0n_cle
//  X_Fault                 P8.10   TIMER6      U6  gpmc_wen
//  X_Dir                   P8.11   GPIO1_13    R12 gpmc_ad13       6       pru0.r30.15         
//  X_Step                  P8.12   GPIO1_12    T12 gpmc_ad12       6       pru0.r30.14
//  E1_Heat                 P8.13   EHRPWM2B    T10 gpmc_ad9
//  Y_Stop_1                P8.14   GPIO0_26    T11 gpmc_ad10
//  Y_Fault                 P8.15   GPIO1_15    U13 gpmc_ad15       6       pru0.r31.15         
//  E2_Step                 P8.16   GPIO1_14    V13 gpmc_ad14       6       pru0.r31.14         
//  Z_Fault                 P8.17   GPIO0_27    U12 gpmc_ad11
//  E1_Fault                P8.18   GPIO2_1     V12 gpmc_clk_mux0
//  E2_Heat                 P8.19   EHRPWM2A    U10 gpmc_ad8
//  Y_Step      E_Ena       P8.20   GPIO1_31    V9  gpmc_csn2       5       pru1.r30.13
//  Y_Dir       E_Dir       P8.21   GPIO1_30    U9  gpmc_csn1       5       pru1.r30.12
//  X_Stop_1                P8.22   GPIO1_5     V8  gpmc_ad5
//                          P8.23   GPIO1_4     U8  gpmc_ad4
//  Z_Step                  P8.24   GPIO1_1     V7  gpmc_ad1
//                          P8.25   GPIO1_0     U7  gpmc_ad0
//  Z_Stop_1                P8.26   GPIO1_29    V6  gpmc_csn0
//              Z_Step      P8.27   GPIO2_22    U5  lcd_vsync       5       pru1.r30.8 
//              Z_Ena       P8.28   GPIO2_24    V5  lcd_pclk        5       pru1.r30.10
//              Z_Dir       P8.29   GPIO2_23    R5  lcd_hsync       5       pru1.r30.9 
//              E_Step      P8.30   GPIO2_25    R6  lcd_ac_bias_en  5       pru1.r30.11
//                          P8.31   UART5_CTSN  V4  lcd_data14
//                          P8.32   UART5_RTSN  T5  lcd_data15
//                          P8.33   UART4_RTSN  V3  lcd_data13
//                          P8.34   UART3_RTSN  U4  lcd_data11
//                          P8.35   UART4_CTSN  V2  lcd_data12
//              J4          P8.36   UART3_CTSN  U3  lcd_data10
//                          P8.37   UART5_TXD   U1  lcd_data8
//                          P8.38   UART5_RXD   U2  lcd_data9
//              Y_Dir       P8.39   GPIO2_12    T3  lcd_data6       5       pru1.r30.6 
//              Y_Ena       P8.40   GPIO2_13    T4  lcd_data7       5       pru1.r30.7 
//              X_Ena       P8.41   GPIO2_10    T1  lcd_data4       5       pru1.r30.4 
//              Y_Step      P8.42   GPIO2_11    T2  lcd_data5       5       pru1.r30.5 
//              X_Step      P8.43   GPIO2_8     R3  lcd_data2       5       pru1.r30.2 
//              X_Dir       P8.44   GPIO2_9     R4  lcd_data3       5       pru1.r30.3 
//              J3          P8.45   GPIO2_6     R1  lcd_data0       5       pru1.r30.0 
//              J2          P8.46   GPIO2_7     R2  lcd_data1       5       pru1.r30.1 
//  
//                          P9.1    GND
//                          P9.2    GND
//                          P9.3    VDD_3V3EXP
//                          P9.4    VDD_3V3EXP
//                          P9.5    VDD_5V
//                          P9.6    VDD_5V
//                          P9.7    SYS_5V
//                          P9.8    SYS_5V
//                          P9.9    PWR_BUT
//                          P9.10   SYS_RESETn  A10 RESET_OUT
//                          P9.11   UART4_RXD   T17 gpmc_wait0
//  Z_Stop_2                P9.12   GPIO1_28    U18 gpmc_be1n
//  X_Stop_2                P9.13   UART4_TXD   U17 gpmc_wpn
//                          P9.14   EHRPWM1A    U14 gpmc_a2
//                          P9.15   GPIO1_16    R13 gpmc_a0
//                          P9.16   EHRPWM1B    T14 gpmc_a3
//  Y_Stop_2                P9.17   I2C1_SCL    A16 spi0_cs0
//                          P9.18   I2C1_SDA    B16 spi0_d1
//                          P9.19   I2C2_SCL    D17 uart1_rtsn
//                          P9.20   I2C2_SDA    D18 uart1_ctsn
//                          P9.21   UART2_TXD   B17 spi0_d0
//  HBP_Heat                P9.22   UART2_RXD   A17 spi0_sclk
//                          P9.23   GPIO1_17    V14 gpmc_a1
//                          P9.24   UART1_TXD   D15 uart1_txd
//  E1_Step                 P9.25   GPIO3_21    A14 mcasp0_ahclkx
//                          P9.26   UART1_RXD   D16 uart1_rxd
//                          P9.27   GPIO3_19    C13 mcasp0_fsr
//                          P9.28   SPI1_CS0    C12 mcasp0_ahclkr
//                          P9.29   SPI1_D0     B13 mcasp0_fsx
//                          P9.30   SPI1_D1     D12 mcasp0_axr0
//                          P9.31   SPI1_SCLK   A13 mcasp0_aclkx
//                          P9.32   VDD_ADC
//  E2_Temp                 P9.33   AIN4        C8  AIN4
//                          P9.34   GNDA_ADC
//  E1_Temp                 P9.35   AIN6        A8  AIN6
//  HBP_Temp    Temp2 J8    P9.36   AIN5        B8  AIN5
//                          P9.37   AIN2        B7  AIN2
//              Temp1 J7    P9.38   AIN3        A7  AIN3
//                          P9.39   AIN0        B6  AIN0
//              Temp0 J6    P9.40   AIN1        C7  AIN1
//                          P9.41   CLKOUT2     D14 xdma_event_intr1
//                          bbb*    GPIO3_20    D13 mcasp0_axr1
//                          P9.42   GPIO0_7     C18 eCAP0_in_PWM0_out
//                          bbb*    GPIO3_18    B12 Mcasp0_aclkrp
//                          P9.43   GND             
//                          P9.44   GND             
//                          P9.45   GND
//                          P9.46   GND             
//
//                          bbb* = Only present on BeagleBone Black
