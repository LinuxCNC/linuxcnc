#ifndef STG_H
#define STG_H

/** This is the driver for an Servo-To-Go Model I board.
    The board includes 8 channels of quadrature encoder input,
    8 channels of analog input and output, 32 bits digital I/O,
    and an interval timer with interrupt.
**/

/** Copyright (C) 2005 Alex Joni
                       <alex DOT joni AT robcon DOT ro>
**/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
**/

/*
    Servo To Go board decls
*/
#define MAX_CHANS  8

#define CNT0_D     0x00
#define CNT1_D     0x01
#define CNT0_C     0x02
#define CNT1_C     0x03
#define CNT2_D     0x04
#define CNT3_D     0x05
#define CNT2_C     0x06
#define CNT3_C     0x07
#define CNT4_D     0x08
#define CNT5_D     0x09
#define CNT4_C     0x0a
#define CNT5_C     0x0b
#define CNT6_D     0x0c
#define CNT7_D     0x0d
#define CNT6_C     0x0e
#define CNT7_C     0x0f

#define DAC_0      0x10
#define DAC_1      0x12
#define DAC_2      0x14
#define DAC_3      0x16
#define DAC_4      0x18
#define DAC_5      0x1a
#define DAC_6      0x1c
#define DAC_7      0x1e

#define ADC_0      0x410
#define ADC_1      0x412
#define ADC_2      0x414
#define ADC_3      0x416
#define ADC_4      0x418
#define ADC_5      0x41a
#define ADC_6      0x41c
#define ADC_7      0x41e

#define BRDTST     0x403

#define DIO_A      0x400  /* Model 1 */
#define DIO_B      0x402  /* Model 1 */
#define DIO_C      0x404  /* Model 1 */
#define DIO_D      0x401  /* Model 1 */
#define PORT_A     0x400  /* Model 2, same function as DIO_A */
#define PORT_B     0x402  /* Model 2, same function as DIO_B */
#define PORT_C     0x404  /* Model 2, same function as DIO_C */
#define PORT_D     0x405  /* Model 2, same function as DIO_D */

#define CNTRL0     0x401  /* Model 2 */
#define CNTRL1     0x40f  /* Model 2 */
#define ABC_DIR    0x406  /* Model 2 */
#define D_DIR      0x407  /* Model 2 */

#define MIO_1      0x406
#define INTC       0x405
#define MIO_2      0x407
#define ODDRST     0x407
#define TIMER_0    0x408
#define TIMER_1    0x40a
#define TIMER_2    0x40c
#define TMRCMD     0x40e
#define ICW1       0x409
#define ICW2       0x40b
#define OCW1       0x40b
#define OCW2       0x409
#define OCW3       0x409
#define IRR        0x409
#define ISR        0x409
#define IMR        0x40b

#define IDLEN      0x409
#define SELDI      0x40B
#define IDL        0x40D

/*
 * Some bit masks for various registers for Model 1
 */
// for IRR, ISR, and IMR
#define IXEVN      0x80
#define IXODD      0x40
#define LIXEVN     0x20
#define LIXODD     0x10
#define EOC        0x08
#define TP0        0x04
#define USR_INT    0x02
#define TP2        0x01
// for INTC
#define AUTOZERO   0x80
#define IXLVL      0x40
#define IXS1       0x20
#define IXS0       0x10
#define USRINT     0x08
#define IA2        0x04
#define IA1        0x02
#define IA0        0x01
 
#endif
