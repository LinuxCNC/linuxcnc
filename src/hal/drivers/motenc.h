/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 *
 * Hardware register defines for Vital Systems MOTENC-100 board.
 * The board includes an 8 channel quadrature decoder, 8 analog inputs,
 * 8 analog outputs, 68 digital inputs, 32 digital outputs, programable
 * timer interrupts, a watch dog timer, and a hardware E-STOP circuit.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 * THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 * ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 * TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 * harming persons must have provisions for completely removing power
 * from all motors, etc, before persons enter any danger area.  All
 * machinery must be designed to comply with local and national safety
 * codes, and the authors of this software can not, and do not, take
 * any responsibility for such compliance.
 *
 * This code was written as part of the EMC HAL project.  For more
 * information, go to www.linuxcnc.org.
 *
 ******************************************************************************/

#ifndef _MOTENC_H_
#define _MOTENC_H_


#include "hal.h"


// Vendor and device ID.
#define MOTENC_VENDOR_ID		0x10B5	// PLX.
#define MOTENC_DEVICE_ID		0x3001	// MOTENC-100.
#define MOTENC_SUB_SYS_VENDOR_ID	0x10B5	// PLX.
#define MOTENC_SUB_SYS_DEVICE_ID	0x9030	// 9030 SMARTarget I/O Accelerator.

#define MOTENC_PCI_MEM_LEN		512

#define MOTENC_NUM_ADC_CHANNELS		8
#define MOTENC_NUM_DAC_CHANNELS		8

#define MOTENC_FPGA_NUM_ENCODER_CHANNELS 4
#define MOTENC_FPGA_NUM_DIGITAL_INPUTS	36
#define MOTENC_FPGA_NUM_DIGITAL_OUTPUTS	16

#define MOTENC_NUM_FPGA		2
#define MOTENC_NUM_ENCODER_CHANNELS	(MOTENC_NUM_FPGA * MOTENC_FPGA_NUM_ENCODER_CHANNELS)
#define MOTENC_NUM_DIGITAL_INPUTS	(MOTENC_NUM_FPGA * MOTENC_FPGA_NUM_DIGITAL_INPUTS - 4)
#define MOTENC_NUM_DIGITAL_OUTPUTS	(MOTENC_NUM_FPGA * MOTENC_FPGA_NUM_DIGITAL_OUTPUTS)



typedef struct {
    hal_s32_t				encoderCount[MOTENC_FPGA_NUM_ENCODER_CHANNELS];
    hal_u32_t				digitalIo;
    hal_u32_t				statusControl;
    hal_u32_t				reserved;
    hal_u32_t				boardVersion;
} volatile MotencFpgaRegMap;

// For use with digitalIo reg.
#define MOTENC_DIGITAL_OUT		0x0000FFFF
#define MOTENC_DIGITAL_OUT_SHFT		0
#define MOTENC_DIGITAL_IN		0xFFFF0000
#define MOTENC_DIGITAL_IN_SHFT		16

// For use with statusControl reg.
#define MOTENC_CONTROL_ENCODER_RESET    0x0000000F
#define MOTENC_CONTROL_ENCODER_RESET_SHFT 0

#define MOTENC_STATUS_DIGITAL_IN	0x0000FFFF
#define MOTENC_STATUS_DIGITAL_IN_SHFT	0
#define MOTENC_STATUS_BOARD_ID		0x00030000	// FPGA 0.
#define MOTENC_STATUS_BOARD_ID_SHFT	16
#define MOTENC_STATUS_ADC_DONE		0x00040000
#define MOTENC_STATUS_ESTOP		0x00080000
#define MOTENC_STATUS_DIGITAL_IN2	0x000F0000	// FPGA 1.
#define MOTENC_STATUS_DIGITAL_IN2_SHFT	16
#define MOTENC_STATUS_INDEX		0x00F00000
#define MOTENC_STATUS_INDEX_SHFT	20
#define MOTENC_STATUS_INDEX_LATCH	0x0F000000
#define MOTENC_STATUS_INDEX_LATCH_SHFT	24

typedef struct {
    MotencFpgaRegMap			fpga[MOTENC_NUM_FPGA];
    hal_u32_t				timerCompare;
    hal_u32_t				timerIrqDisable;
    hal_u32_t				timerIrqEnable;
    hal_u32_t				watchdogControl;
    hal_u32_t				watchdogReset;
    hal_u32_t				reserved1[3];
    hal_u32_t				dac[MOTENC_NUM_DAC_CHANNELS];
    hal_u32_t				adcDataCommand;
    hal_u32_t				reserved2[7];
    hal_u32_t				adcStartConversion;
} volatile MotencRegMap;

// For use with watchdogControl reg.
#define MOTENC_WATCHDOG_CTL_TIMEBASE    0x00000001
#define MOTENC_WATCHDOG_CTL_8MS		0x00000000
#define MOTENC_WATCHDOG_CTL_16MS	0x00000001
#define MOTENC_WATCHDOG_CTL_ENABLE	0x00000004
#define MOTENC_WATCHDOG_CTL_AUTO_RESET	0x00000010	// Reset by DAC writes.

// For use with watchdogReset reg.
#define MOTENC_WATCHDOG_RESET_VALUE	0x0000005A

// For use with dac reg.
#define MOTENC_DAC_COUNT_ZERO		0x00001000	// 0 volts.
#define MOTENC_DAC_VOLTS_MIN		-10.0		// For converting volts to counts.
#define MOTENC_DAC_VOLTS_MAX		10.0
#define MOTENC_DAC_SCALE_MULTIPLY	-8191.0		// Higher counts are lower voltage.
#define MOTENC_DAC_SCALE_DIVIDE		(MOTENC_DAC_VOLTS_MAX - MOTENC_DAC_VOLTS_MIN)

// For use with adcDataCommand reg.
#define MOTENC_ADC_COMMAND_CHN_0	0
#define MOTENC_ADC_COMMAND_CHN_0_1	1
#define MOTENC_ADC_COMMAND_CHN_0_1_2    2
#define MOTENC_ADC_COMMAND_CHN_0_1_2_3	3
#define MOTENC_ADC_COMMAND_CHN_4	4
#define MOTENC_ADC_COMMAND_CHN_4_5	5
#define MOTENC_ADC_COMMAND_CHN_4_5_6    6
#define MOTENC_ADC_COMMAND_CHN_4_5_6_7	7
#define MOTENC_ADC_COMMAND_POWER_DOWN	8
#define MOTENC_ADC_SIGN_BIT		0x00002000
#define MOTENC_ADC_SIGN_EXTEND		0xFFFFC000
#define MOTENC_ADC_VOLTS_MIN		-5.0		// For converting counts to volts.
#define MOTENC_ADC_VOLTS_MAX		5.0
#define MOTENC_ADC_SCALE_MULTIPLY	(MOTENC_ADC_VOLTS_MAX - MOTENC_ADC_VOLTS_MIN)
#define MOTENC_ADC_SCALE_DIVIDE		16384.0


#endif
