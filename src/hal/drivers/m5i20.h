/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 * Hardware register defines for the Mesa Electronics 5i20 board.
 * The board includes a user programable FPGA. This driver
 * is written for the 4 axis host based motion control FPGA
 * configuration. It includes 8 quadrature decoder channels, 4
 * PWM output channels, and 48 I/O channels.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/07/12 18:26:41  petev
 * Initial revision.
 *
 *
 ******************************************************************************/

#ifndef _M5I20_H_
#define _M5I20_H_


#include "hal.h"


// Vendor and device ID.
#define M5I20_VENDOR_ID			0x10B5		// PLX.
#define M5I20_DEVICE_ID			0x9030		// 9030 SMARTarget I/O Accelerator.
#define M5I20_SUBSYS_VENDOR_ID		0x10B5		// PLX.
#define M5I20_SUBSYS_DEVICE_ID		0x3131		// Mesa 5i20.

// For use with HOSTM54E FPGA firmware.
#define M5I20_MAX_PWM_CHANNELS		16
#define M5I20_NUM_PWM_CHANNELS		4
#define M5I20_MAX_PRIMARY_ENCODERS	8
#define M5I20_MAX_SECONDARY_ENCODERS	8
#define M5I20_MAX_ENCODER_CHANNELS	(M5I20_MAX_PRIMARY_ENCODERS + M5I20_MAX_SECONDARY_ENCODERS)
#define M5I20_NUM_PRIMARY_ENCODERS	4
#define M5I20_NUM_SECONDARY_ENCODERS	4
#define M5I20_NUM_ENCODER_CHANNELS	(M5I20_NUM_PRIMARY_ENCODERS + M5I20_NUM_SECONDARY_ENCODERS)
#define M5I20_MAX_DIGITAL_IO_PORTS	4
#define M5I20_NUM_DIGITAL_IO_PORTS	2
#define M5I20_NUM_DIGITAL_INPUTS	(M5I20_NUM_DIGITAL_IO_PORTS * 16)
#define M5I20_NUM_DIGITAL_OUTPUTS	(M5I20_NUM_DIGITAL_IO_PORTS * 8)


typedef struct {
    hal_u32_t				data;
    hal_u32_t				direction;	// 1 = out.
} volatile M5i20DioRegMap;

// For use with HOSTMOT FPGA firmware.
typedef struct {
    hal_s32_t				encoderCount[M5I20_MAX_ENCODER_CHANNELS];
    hal_u16_t				encoderControl[M5I20_MAX_ENCODER_CHANNELS];

    hal_u16_t				pwmValue[M5I20_MAX_PWM_CHANNELS];
    hal_u16_t				pwmControl[M5I20_MAX_PWM_CHANNELS];

    M5i20DioRegMap			digitalIo[M5I20_MAX_DIGITAL_IO_PORTS];

    hal_u16_t				control;
    hal_u16_t				mode;
    hal_u16_t				irqDivisor;
    hal_u16_t				pwmRate;
    hal_u16_t				wdTimeout;	// In micro-seconds.
    hal_u16_t				wdTimer;
    hal_u16_t				ledView;
    hal_u16_t				reserved1;
    hal_u8_t				fpgaCfgData;	// This can be anywhere in map on bits 0..7.
} volatile M5i20HostMotRegMap;

// For use with digitalIo reg.
#define M5I20_DIGITAL_IN		0x0000FFFF
#define M5I20_DIGITAL_IN_SHFT		0
#define M5I20_DIGITAL_OUT		0x00FF0000
#define M5I20_DIGITAL_OUT_SHFT		16

// For use with encoderControl reg.
#define M5I20_ENC_CTL_PHASE_A		0x0001
#define M5I20_ENC_CTL_PHASE_B		0x0002
#define M5I20_ENC_CTL_INDEX		0x0004		// Read only.
#define M5I20_ENC_CTL_LOCAL_CLEAR	0x0004		// Write only.
#define M5I20_ENC_CTL_LATCH_ON_READ	0x0008		// Latch count on read (ie. transparent mode).
#define M5I20_ENC_CTL_INDEX_POLARITY	0x0010
#define M5I20_ENC_CTL_INDEX_ACTIVE_HI	0x0010
#define M5I20_ENC_CTL_INDEX_ACTIVE_LO	0x0000
#define M5I20_ENC_CTL_CLEAR_ON_INDEX	0x0020		// 1 = clear count on index.
#define M5I20_ENC_CTL_CLEAR_ONCE	0x0040		// 1 = clear COI bit when index detected.
#define M5I20_ENC_CTL_INDEX_GATE	0x0080		// 1 = index detected only when A = B = 0.
#define M5I20_ENC_CTL_LOCAL_HOLD	0x0100		// 1 = hold counter.
#define M5I20_ENC_CTL_QUAD_FILTER	0x0200
#define M5I20_ENC_CTL_QUAD_FILTER_4MHZ	0x0200
#define M5I20_ENC_CTL_QUAD_FILTER_11MHZ	0x0000
#define M5I20_ENC_CTL_COUNT_MODE	0x0400
#define M5I20_ENC_CTL_COUNT_UP_DOWN	0x0400
#define M5I20_ENC_CTL_COUNT_QUADRATURE	0x0000
#define M5I20_ENC_CTL_AUTO_COUNT	0x0800		// 1 = counter counts up at PCI clk rate, 0 = normal.
#define M5I20_ENC_CTL_LATCH_ON_INDEX	0x1000		// 1 = latch count on index.
#define M5I20_ENC_CTL_LATCH_ONCE	0x2000		// 1 = clear LOI bit when index detected.
#define M5I20_ENC_CTL_INDEXM_ENABLE	0x4000		// 1 = IM must be true to detect, 0 = IM is don't care.
#define M5I20_ENC_CTL_INDEXM_POLARITY	0x8000
#define M5I20_ENC_CTL_INDEXM_ACTIVE_HI	0x8000
#define M5I20_ENC_CTL_INDEXM_ACTIVE_LO	0x0000

// For use with pwmControl reg.
#define M5I20_PWM_CTL_ENABLE		0x0001
#define M5I20_PWM_CTL_INTERLACED	0x0002
#define M5I20_PWM_CTL_NON_INTERLACED	0x0000
#define M5I20_PWM_CTL_UNSIGNED		0x0004
#define M5I20_PWM_CTL_SIGNED		0x0000
#define M5I20_PWM_CTL_PWM_OUTPUT	0x0008		// Can read PWM output bit for debug.
#define M5I20_PWM_CTL_DIRECTION      	0x0010		// Sign output or input in unsigned mode.

// For use with control reg.
#define M5I20_CONTROL_CLEAR_ENCODERS	0x0001		// Clear all encoder counters.
#define M5I20_CONTROL_LATCH_ENCODERS	0x0002		// Latch all encoder counters.
#define M5I20_CONTROL_CLEAR_PWMS	0x0004		// Clear all pwms.
#define M5I20_CONTROL_CLEAR_IRQ		0x0008		// Clear IRQ.
#define M5I20_CONTROL_RESET_WDT		0x0010		// Reset the watchdog timer.

// For use with mode reg.
#define M5I20_MODE_COUNTER_ENABLE	0x0001		// Enable all counters.
#define M5I20_MODE_PWM_ENABLE		0x0002		// Enable all pwms.
#define M5I20_MODE_LATCH_ON_IRQ		0x0004		// Latch on IRQ.
#define M5I20_MODE_STOP_ON_MISSED_IRQ	0x0008		// Stop on missed IRQ.
#define M5I20_MODE_MISSED_IRQ		0x0010		// Missed IRQ status.
#define M5I20_MODE_IRQ_ENABLE		0x0020		// IRQ enable.
#define M5I20_MODE_IRQ_STATUS		0x0040		// IRQ status.
#define M5I20_MODE_STOP_ON_WDT		0x0080		// Stop on watdog timer timeout.

// For use with pwm as DAC.
#define M5I20_DAC_COUNT_ZERO		0x0000		// 0 volts.
#define M5I20_DAC_VOLTS_MIN		-10.0		// For converting volts to counts.
#define M5I20_DAC_VOLTS_MAX		10.0
#define M5I20_DAC_SCALE_MULTIPLY	32767.0		// 16 bit signed.
#define M5I20_DAC_SCALE_DIVIDE		(M5I20_DAC_VOLTS_MAX)


// For use with Plx9030LocalRegMap.gpioc reg.
#define M5I20_PLX_GPIOC_AND_MASK	0xFFDF7FFF	// To initialize GPIO pins.
#define M5I20_PLX_GPIOC_OR_MASK		0x02410000
#define M5I20_PLX_GPIOC_CFG_NPROGRAM	0x04000000
#define M5I20_PLX_GPIOC_CFG_NWRITE	0x00800000
#define M5I20_PLX_GPIOC_LED_OFF		0x00020000	// Red LED.
#define M5I20_PLX_GPIOC_CFG_NINIT	0x00004000
#define M5I20_PLX_GPIOC_CFG_DONE	0x00000800


#endif
