// Written by Charles Steinkuehler as an example for the pru_debugger
// This code is released into the public domain
// You may copy, modify, sell, re-license, or do pretty much anything you
// want to do with this code.


// References to the current TI PRU manual (spruhf8) should also be 
// considered pointers to the similar section in the revision c 
// AM335x manual (spruh73c), ie:
// spruhf8  AM335x PRU-ICSS Reference Guide 5.3.2.1.4
// spruh73c AM335x TRM 4.5.3.2.1.4


// Some constants so we don't have to include a header file

// Use the special set/clear register to change the I/O pin, which avoids
// having to do an atomic read-modify-write
// See the AM335x TRM sections 25.4.19-20 for details
#define GPIO1 0x4804c000
#define GPIO2 0x481ac000
#define GPIO_CLEARDATAOUT 0x190

// spruhf8 AM335x PRU-ICSS Reference Guide 5.2.1
#define CONST_PRUCFG C4
#define CONST_IEP C26


// BeagleBone Black SRM section 6.6
#define USR0_BIT 21     // GPIO1_21
#define USR1_BIT 22     // GPIO1_22
#define USR2_BIT 23     // GPIO1_23
#define USR3_BIT 24     // GPIO1_24


// This code doesn't use any calls, but the default call register is r30,
// which overlaps with the PRU direct I/O pins, so let's set it to a
// different location to avoid problems later when you expand this example
// spruhf8 AM335x PRU-ICSS Reference Guide 5.3.2.1.4
.setcallreg r24.w2

.origin 0
.entrypoint START


// Create a structure and map it onto some registers to make the assembly 
// code a bit easier to read
// spruhf8 AM335x PRU-ICSS Reference Guide 5.3.2.1.8
.struct LEDs
    .u8     PWM0        // PWM value for LED0
    .u8     PWM1        // PWM value for LED1
    .u8     PWM2        // PWM value for LED2
    .u8     PWM3        // PWM value for LED3
    .u32    ACCUM       // Accumulator
.ends


START:
    // Clear syscfg[standby_init] to enable ocp master port
    // spruhf8 AM335x PRU-ICSS Reference Guide 11.1.2
    LBCO    r0, CONST_PRUCFG, 4, 4
    CLR     r0, r0, 4
    SBCO    r0, CONST_PRUCFG, 4, 4


    // Setup IEP timer
    // spruhf8 AM335x PRU-ICSS Reference Guide 10.x
    LBCO    r6, CONST_IEP, 0x40, 40                 // Read all 10 32-bit CMP registers into r6-r15
    OR      r6, r6, 0x03                            // Set count reset and enable compare 0 event

    // Set loop period to 10 uS, or 10,000 nS
    MOV     r8, 10000

    SBCO    r6, CONST_IEP, 0x40, 40                 // Save 10 32-bit CMP registers

    MOV     r2, 0x00000551                          // Enable counter, configured to count nS (increments by 5 each clock)
    SBCO    r2, CONST_IEP, 0x00, 4                  // Save IEP GLOBAL_CFG register


    // Setup the initial operating constraints
    // First, map the LEDs struct to some registers:
    // spruhf8 AM335x PRU-ICSS Reference Guide 5.3.2.1.10
    .assign LEDs, r4, r5, LED

    // Start with different values in the LED PWM registers
    // The 'raw' way:
    MOV     r4.b0, 0x00

    // The pretty way:
    MOV     LED.PWM1, 0x40

    // ...but a MOV can load 16 bits at once...
    // You can override the defined struct element sizes using raw registers
    // The efficient way:
    MOV     r4.w2, 0xc080

    // Start with zero in the accumulator
    LDI     LED.ACCUM, 0


    // Start with all LEDs turned on
    LDI     r3, 0x0f
    LSL     r3, r3, USR0_BIT
    MOV     r2, GPIO1 + GPIO_CLEARDATAOUT

    // Write with an offset of 4 = Set data register
    SBBO    r3, r2, 4, 4


MAINLOOP:
    // We either just started, or the timer expired

    ADD     LED.ACCUM, LED.ACCUM, 1

    // Update the PWM output states
    // Note this is written to be doing something interesting in the debugger, 
    // rather than trying to be particularly efficient.

    // The PWM sets all outputs high when the PWM period timer rolls over
    // and sets a particular output low if it's PWM value matches the PWM
    // counter on this pass through the loop

    // Setup the output register to point to GPIO 1
    MOV     r2, GPIO1 + GPIO_CLEARDATAOUT

    // The bit we want to clear
    LDI     r3, 1
    LSL     r3, r3, USR0_BIT
    
    // Skip clearing the output if the PWM setting doesn't match the accumulator
    QBNE    USR1, LED.ACCUM.b0, LED.PWM0

    // Clear the output bit
    SBBO    r3, r2, 0, 4

USR1:
    // USR1 is the next bit in GPIO 1
    LSL     r3, r3, 1

    // Do we have a match?
    QBNE    USR2, LED.ACCUM.b0, LED.PWM1

    // If so, clear the output
    SBBO    r3, r2, 0, 4

USR2:
    // USR2 is the next bit in GPIO 1
    LSL     r3, r3, 1

    // Do we have a match?
    QBNE    USR3, LED.ACCUM.b0, LED.PWM2

    // If so, clear the output
    SBBO    r3, r2, 0, 4

USR3:
    // USR3 is the next bit in GPIO 1
    LSL     r3, r3, 1

    // Do we have a match?
    QBNE    PWM_DONE, LED.ACCUM.b0, LED.PWM3

    // If so, clear the output
    SBBO    r3, r2, 0, 4

PWM_DONE:
    // Is the PWM cycle finished?
    QBNE    WAIT, LED.ACCUM.b0, 255

    // If so, set all the outputs
    LDI     r3, 0x0f
    LSL     r3, r3, USR0_BIT
    MOV     r2, GPIO1 + GPIO_CLEARDATAOUT

    // Write with an offset of 4 = Set data register
    SBBO    r3, r2, 4, 4

    // Update PWM values based on accumulator using a jump-table

    // Grab two higher bits of the accumulator for a value of 0-3
    AND     r8.w0, LED.ACCUM.b2, 0x03

    // Get the start address of the jump table
    LDI     r8.w2, #JUMPTABLE

    // Create a pointer to the desired destination
    // Note instruction memory is indexed by words, there is no need
    // to account for the 32-bit size of the instruction
    ADD     r8.w2, r8.w2, r8.w0

    // ...and Jump
    JMP     r8.w2

    
WAIT:
    LBCO    r2, CONST_IEP, 0x44, 4      // Load CMP_STATUS register
    QBBC    WAIT, r2, 0                 // Wait until counter times out
    SBCO    r2, CONST_IEP, 0x44, 4      // Clear counter timeout bit

    JMP     MAINLOOP


JUMPTABLE:
    JMP     _0_1        // LED position between 0 & 1
    JMP     _1_2        // LED position between 1 & 2
    JMP     _2_3        // LED position between 2 & 3
    JMP     _3_0        // LED position between 3 & 0

_0_1:
    RSB     LED.PWM0, LED.ACCUM.b1, 1
    MOV     LED.PWM1, LED.ACCUM.b1
    LDI     LED.PWM2, 0
    LDI     LED.PWM3, 0
    JMP     WAIT

_1_2:
    LDI     LED.PWM0, 0
    RSB     LED.PWM1, LED.ACCUM.b1, 1
    MOV     LED.PWM2, LED.ACCUM.b1
    LDI     LED.PWM3, 0
    JMP     WAIT

_2_3:
    LDI     LED.PWM0, 0
    LDI     LED.PWM1, 0
    RSB     LED.PWM2, LED.ACCUM.b1, 1
    MOV     LED.PWM3, LED.ACCUM.b1
    JMP     WAIT

_3_0:
    MOV     LED.PWM0, LED.ACCUM.b1
    LDI     LED.PWM1, 0
    LDI     LED.PWM2, 0
    RSB     LED.PWM3, LED.ACCUM.b1, 1
    JMP     WAIT

