/*   This is a component  for RaspberryPi to FPGA over SPI for linuxcnc.
 *    Copyright 2013 Matsche <tinker@play-pla.net>
 *                               based on GP Orcullo's picnc driver and
 *				 based on the pluto_common.h from Jeff Epler <jepler@unpythonic.net>
 *    Copyright 2017 B.Stultiens <lcnc@vagrearg.org>
 *
 *
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307 USA
 */

#ifndef HAL_RPSPI_H
#define HAL_RPSPI_H

/*
 * Broadcom defines
 *
 * Peripheral description see:
 * - https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2835/BCM2835-ARM-Peripherals.pdf
 * - http://elinux.org/RPi_BCM2835_GPIOs
 * - http://elinux.org/BCM2835_datasheet_errata
 */

#define BCM2835_PERI_BASE	0x20000000
#define BCM2835_GPIO_OFFSET	0x200000
#define BCM2835_SPI_OFFSET	0x204000
#define BCM2835_AUX_OFFSET	0x215000

#define BCM2835_GPIO_BASE	(BCM2835_PERI_BASE + BCM2835_GPIO_OFFSET)	/* GPIO controller */
#define BCM2835_SPI_BASE	(BCM2835_PERI_BASE + BCM2835_SPI_OFFSET)	/* SPI controller */
#define BCM2835_AUX_BASE	(BCM2835_PERI_BASE + BCM2835_AUX_OFFSET)	/* AUX controller */
#define BCM2835_GPIO_END	(BCM2835_GPIO_BASE + sizeof(bcm2835_gpio_t))
#define BCM2835_SPI_END		(BCM2835_SPI_BASE + sizeof(bcm2835_spi_t))
#define BCM2835_AUX_END		(BCM2835_AUX_BASE + sizeof(bcm2835_aux_t))

#define BCM2709_OFFSET		0x1F000000	// For RPI 2 and 3

#if !defined(__I) && !defined(__O) && !defined(__IO)
#ifdef __cplusplus
#define __I	volatile	/* read only permission */
#else
#define __I	volatile const	/* read only permission */
#endif
#define __O	volatile	/* write only permission */
#define __IO	volatile	/* read/write permission */
#else
#error "Possible define collision for __I, __O and __IO"
#endif

/*
 * Alternate function specification see
 */
#define GPIO_FSEL_X_GPIO_INPUT		0	/* All pin's function 0 is input */
#define GPIO_FSEL_X_GPIO_OUTPUT		1	/* All pin's function 1 is output */

#define GPIO_FSEL_0_SDA0		4
#define GPIO_FSEL_0_SA5			5
#define GPIO_FSEL_0_PCLK		6
#define GPIO_FSEL_0_SDA0		4
#define GPIO_FSEL_0_SA5			5
#define GPIO_FSEL_0_PCLK		6
#define GPIO_FSEL_1_SCL0		4
#define GPIO_FSEL_1_SA4			5
#define GPIO_FSEL_1_DE			6
#define GPIO_FSEL_2_SDA1		4
#define GPIO_FSEL_2_SA3			5
#define GPIO_FSEL_2_LCD_VSYNC		6
#define GPIO_FSEL_3_SCL1		4
#define GPIO_FSEL_3_SA2			5
#define GPIO_FSEL_3_LCD_HSYNC		6
#define GPIO_FSEL_4_GPCLK0		4
#define GPIO_FSEL_4_SA1			5
#define GPIO_FSEL_4_DPI_D0		6
#define GPIO_FSEL_4_ARM_TDI		2
#define GPIO_FSEL_5_GPCLK1		4
#define GPIO_FSEL_5_SA0			5
#define GPIO_FSEL_5_DPI_D1		6
#define GPIO_FSEL_5_ARM_TDO		2
#define GPIO_FSEL_6_GPCLK2		4
#define GPIO_FSEL_6_SOE_N		5
#define GPIO_FSEL_6_SE			5
#define GPIO_FSEL_6_DPI_D2		6
#define GPIO_FSEL_6_ARM_RTCK		2
#define GPIO_FSEL_7_SPI0_CE1_N		4
#define GPIO_FSEL_7_SWE_N		5
#define GPIO_FSEL_7_SRW_N		5
#define GPIO_FSEL_7_DPI_D3		6
#define GPIO_FSEL_8_SPI0_CE0_N		4
#define GPIO_FSEL_8_SD0			5
#define GPIO_FSEL_8_DPI_D4		6
#define GPIO_FSEL_9_SPI0_MISO		4
#define GPIO_FSEL_9_SD1			5
#define GPIO_FSEL_9_DPI_D5		6
#define GPIO_FSEL_10_SPI0_MOSI		4
#define GPIO_FSEL_10_SD2		5
#define GPIO_FSEL_10_DPI_D6		6
#define GPIO_FSEL_11_SPI0_SCLK		4
#define GPIO_FSEL_11_SD3		5
#define GPIO_FSEL_11_DPI_D7		6
#define GPIO_FSEL_12_PWM0		4
#define GPIO_FSEL_12_SD4		5
#define GPIO_FSEL_12_DPI_D8		6
#define GPIO_FSEL_12_ARM_TMS		2
#define GPIO_FSEL_13_PWM1		4
#define GPIO_FSEL_13_SD5		5
#define GPIO_FSEL_13_DPI_D9		6
#define GPIO_FSEL_13_ARM_TCK		2
#define GPIO_FSEL_14_TXD0		4
#define GPIO_FSEL_14_SD6		5
#define GPIO_FSEL_14_DPI_D10		6
#define GPIO_FSEL_14_TXD1		2
#define GPIO_FSEL_15_RXD0		4
#define GPIO_FSEL_15_SD7		5
#define GPIO_FSEL_15_DPI_D11		6
#define GPIO_FSEL_15_RXD1		2
#define GPIO_FSEL_16_FL0		4
#define GPIO_FSEL_16_SD8		5
#define GPIO_FSEL_16_DPI_D12		6
#define GPIO_FSEL_16_CTS0		7
#define GPIO_FSEL_16_SPI1_CE2_N		3
#define GPIO_FSEL_16_CTS1		2
#define GPIO_FSEL_17_FL1		4
#define GPIO_FSEL_17_SD9		5
#define GPIO_FSEL_17_DPI_D13		6
#define GPIO_FSEL_17_RTS0		7
#define GPIO_FSEL_17_SPI1_CE1_N		3
#define GPIO_FSEL_17_RTS1		2
#define GPIO_FSEL_18_PCM_CLK		4
#define GPIO_FSEL_18_SD10		5
#define GPIO_FSEL_18_DPI_D14		6
#define GPIO_FSEL_18_BSCSLSDA		7
#define GPIO_FSEL_18_MOSI		7
#define GPIO_FSEL_18_SPI1_CE0_N		3
#define GPIO_FSEL_18_PWM0		2
#define GPIO_FSEL_19_PCM_FS		4
#define GPIO_FSEL_19_SD11		5
#define GPIO_FSEL_19_DPI_D15		6
#define GPIO_FSEL_19_BSCSLSCL		7
#define GPIO_FSEL_19_SCLK		7
#define GPIO_FSEL_19_SPI1_MISO		3
#define GPIO_FSEL_19_PWM1		2
#define GPIO_FSEL_20_PCM_DIN		4
#define GPIO_FSEL_20_SD12		5
#define GPIO_FSEL_20_DPI_D16		6
#define GPIO_FSEL_20_BSCSL		7
#define GPIO_FSEL_20_MISO		7
#define GPIO_FSEL_20_SPI1_MOSI		3
#define GPIO_FSEL_20_GPCLK0		2
#define GPIO_FSEL_21_PCM_DOUT		4
#define GPIO_FSEL_21_SD13		5
#define GPIO_FSEL_21_DPI_D17		6
#define GPIO_FSEL_21_BSCSL		7
#define GPIO_FSEL_21_CE_N		7
#define GPIO_FSEL_21_SPI1_SCLK		3
#define GPIO_FSEL_21_GPCLK1		2
#define GPIO_FSEL_22_SD0_CLK		4
#define GPIO_FSEL_22_SD14		5
#define GPIO_FSEL_22_DPI_D18		6
#define GPIO_FSEL_22_SD1_CLK		7
#define GPIO_FSEL_22_ARM_TRST		3
#define GPIO_FSEL_23_SD0_CMD		4
#define GPIO_FSEL_23_SD15		5
#define GPIO_FSEL_23_DPI_D19		6
#define GPIO_FSEL_23_SD1_CMD		7
#define GPIO_FSEL_23_ARM_RTCK		3
#define GPIO_FSEL_24_SD0_DAT0		4
#define GPIO_FSEL_24_SD16		5
#define GPIO_FSEL_24_DPI_D20		6
#define GPIO_FSEL_24_SD1_DAT0		7
#define GPIO_FSEL_24_ARM_TDO		3
#define GPIO_FSEL_25_SD0_DAT1		4
#define GPIO_FSEL_25_SD17		5
#define GPIO_FSEL_25_DPI_D21		6
#define GPIO_FSEL_25_SD1_DAT1		7
#define GPIO_FSEL_25_ARM_TCK		3
#define GPIO_FSEL_26_SD0_DAT2		4
#define GPIO_FSEL_26_TE0		5
#define GPIO_FSEL_26_DPI_D22		6
#define GPIO_FSEL_26_SD1_DAT2		7
#define GPIO_FSEL_26_ARM_TDI		3
#define GPIO_FSEL_27_SD0_DAT3		4
#define GPIO_FSEL_27_TE1		5
#define GPIO_FSEL_27_DPI_D23		6
#define GPIO_FSEL_27_SD1_DAT3		7
#define GPIO_FSEL_27_ARM_TMS		3
#define GPIO_FSEL_28_SDA0		4
#define GPIO_FSEL_28_SA5		5
#define GPIO_FSEL_28_PCM_CLK		6
#define GPIO_FSEL_28_FL0		7
#define GPIO_FSEL_29_SCL0		4
#define GPIO_FSEL_29_SA4		5
#define GPIO_FSEL_29_PCM_FS		6
#define GPIO_FSEL_29_FL1		7
#define GPIO_FSEL_30_TE0		4
#define GPIO_FSEL_30_SA3		5
#define GPIO_FSEL_30_PCM_DIN		6
#define GPIO_FSEL_30_CTS0		7
#define GPIO_FSEL_30_CTS1		2
#define GPIO_FSEL_31_FL0		4
#define GPIO_FSEL_31_SA2		5
#define GPIO_FSEL_31_PCM_DOUT		6
#define GPIO_FSEL_31_RTS0		7
#define GPIO_FSEL_31_RTS1		2
#define GPIO_FSEL_32_GPCLK0		4
#define GPIO_FSEL_32_SA1		5
#define GPIO_FSEL_32_RING_OCLK		6
#define GPIO_FSEL_32_TXD0		7
#define GPIO_FSEL_32_TXD1		2
#define GPIO_FSEL_33_FL1		4
#define GPIO_FSEL_33_SA0		5
#define GPIO_FSEL_33_TE1		6
#define GPIO_FSEL_33_RXD0		7
#define GPIO_FSEL_33_RXD1		2
#define GPIO_FSEL_34_GPCLK0		4
#define GPIO_FSEL_34_SOE_N		5
#define GPIO_FSEL_34_SE			5
#define GPIO_FSEL_34_TE2		6
#define GPIO_FSEL_34_SD1_CLK		7
#define GPIO_FSEL_35_SPI0_CE1_N		4
#define GPIO_FSEL_35_SWE_N		5
#define GPIO_FSEL_35_SRW_N		5
#define GPIO_FSEL_35_SD1_CMD		7
#define GPIO_FSEL_36_SPI0_CE0_N		4
#define GPIO_FSEL_36_SD0		5
#define GPIO_FSEL_36_TXD0		6
#define GPIO_FSEL_36_SD1_DAT0		7
#define GPIO_FSEL_37_SPI0_MISO		4
#define GPIO_FSEL_37_SD1		5
#define GPIO_FSEL_37_RXD0		6
#define GPIO_FSEL_37_SD1_DAT1		7
#define GPIO_FSEL_38_SPI0_MOSI		4
#define GPIO_FSEL_38_SD2		5
#define GPIO_FSEL_38_RTS0		6
#define GPIO_FSEL_38_SD1_DAT2		7
#define GPIO_FSEL_39_SPI0_SCLK		4
#define GPIO_FSEL_39_SD3		5
#define GPIO_FSEL_39_CTS0		6
#define GPIO_FSEL_39_SD1_DAT3		7
#define GPIO_FSEL_40_PWM0		4
#define GPIO_FSEL_40_SD4		5
#define GPIO_FSEL_40_SD1_DAT4		7
#define GPIO_FSEL_40_SPI2_MISO		3
#define GPIO_FSEL_40_TXD1		2
#define GPIO_FSEL_41_PWM1		4
#define GPIO_FSEL_41_SD5		5
#define GPIO_FSEL_41_TE0		6
#define GPIO_FSEL_41_SD1_DAT5		7
#define GPIO_FSEL_41_SPI2_MOSI		3
#define GPIO_FSEL_41_RXD1		2
#define GPIO_FSEL_42_GPCLK1		4
#define GPIO_FSEL_42_SD6		5
#define GPIO_FSEL_42_TE1		6
#define GPIO_FSEL_42_SD1_DAT6		7
#define GPIO_FSEL_42_SPI2_SCLK		3
#define GPIO_FSEL_42_RTS1		2
#define GPIO_FSEL_43_GPCLK2		4
#define GPIO_FSEL_43_SD7		5
#define GPIO_FSEL_43_TE2		6
#define GPIO_FSEL_43_SD1_DAT7		7
#define GPIO_FSEL_43_SPI2_CE0_N		3
#define GPIO_FSEL_43_CTS1		2
#define GPIO_FSEL_44_GPCLK1		4
#define GPIO_FSEL_44_SDA0		5
#define GPIO_FSEL_44_SDA1		6
#define GPIO_FSEL_44_TE0		7
#define GPIO_FSEL_44_SPI2_CE1_N		3
#define GPIO_FSEL_45_PWM1		4
#define GPIO_FSEL_45_SCL0		5
#define GPIO_FSEL_45_SCL1		6
#define GPIO_FSEL_45_TE1		7
#define GPIO_FSEL_45_SPI2_CE2_N		3
/*
 * GPIO 46..53 are on port 2, but only available on the compute module. Anyway,
 * these are SD-card interface lines and better not meddled with.
 */

#define GPIO_GPPUD_OFF		0
#define GPIO_GPPUD_PULLDOWN	1
#define GPIO_GPPUD_PULLUP	2

/* BCM2835 GPIO peripheral register map specification */
typedef struct __bcm2835_gpio_t {
	union {
		struct {
			__IO	uint32_t	gpfsel0;	/* 0x000 GPIO Function Select 0 */
			__IO	uint32_t	gpfsel1;	/* 0x004 GPIO Function Select 1 */
			__IO	uint32_t	gpfsel2;	/* 0x008 GPIO Function Select 2 */
			__IO	uint32_t	gpfsel3;	/* 0x00c GPIO Function Select 3 */
			__IO	uint32_t	gpfsel4;	/* 0x010 GPIO Function Select 4 */
			__IO	uint32_t	gpfsel5;	/* 0x014 GPIO Function Select 5 */
		};
		__IO	uint32_t	gpfsel[6];
	};
	__I	uint32_t	reserved018;
	union {
		struct {
			__O	uint32_t	gpset0;		/* 0x01c GPIO Pin Output Set 0 */
			__O	uint32_t	gpset1;		/* 0x020 GPIO Pin Output Set 1 */
		};
		__O	uint32_t	gpset[2];
	};
	__I	uint32_t	reserved024;
	union {
		struct {
			__O	uint32_t	gpclr0;		/* 0x028 GPIO Pin Output Clear 0 */
			__O	uint32_t	gpclr1;		/* 0x02c GPIO Pin Output Clear 1 */
		};
		__O	uint32_t	gpclr[2];
	};
	__I	uint32_t	reserved030;
	__I	uint32_t	gplev0;		/* 0x034 GPIO Pin Level 0 */
	__I	uint32_t	gplev1;		/* 0x038 GPIO Pin Level 1 */
	__I	uint32_t	reserved03c;
	__IO	uint32_t	gpeds0;		/* 0x040 GPIO Pin Event Detect Status 0 */
	__IO	uint32_t	gpeds1;		/* 0x044 GPIO Pin Event Detect Status 1 */
	__I	uint32_t	reserved048;
	__IO	uint32_t	gpren0;		/* 0x04c GPIO Pin Rising Edge Detect Enable 0 */
	__IO	uint32_t	gpren1;		/* 0x050 GPIO Pin Rising Edge Detect Enable 1 */
	__I	uint32_t	reserved054;
	__IO	uint32_t	gpfen0;		/* 0x058 GPIO Pin Falling Edge Detect Enable 0 */
	__IO	uint32_t	gpfen1;		/* 0x05c GPIO Pin Falling Edge Detect Enable 1 */
	__I	uint32_t	reserved060;
	__IO	uint32_t	gphen0;		/* 0x064 GPIO Pin High Detect Enable 0 */
	__IO	uint32_t	gphen1;		/* 0x068 GPIO Pin High Detect Enable 1 */
	__I	uint32_t	reserved06c;
	__IO	uint32_t	gplen0;		/* 0x070 GPIO Pin Low Detect Enable 0 */
	__IO	uint32_t	gplen1;		/* 0x074 GPIO Pin Low Detect Enable 1 */
	__I	uint32_t	reserved078;
	__IO	uint32_t	gparen0;	/* 0x07c GPIO Pin Async. Rising Edge Detect 0 */
	__IO	uint32_t	gparen1;	/* 0x080 GPIO Pin Async. Rising Edge Detect 1 */
	__I	uint32_t	reserved084;
	__IO	uint32_t	gpafen0;	/* 0x088 GPIO Pin Async. Falling Edge Detect 0 */
	__IO	uint32_t	gpafen1;	/* 0x08c GPIO Pin Async. Falling Edge Detect 1 */
	__I	uint32_t	reserved090;
	__IO	uint32_t	gppud;		/* 0x094 GPIO Pin Pull-up/down Enable */
	__IO	uint32_t	gppudclk0;	/* 0x098 GPIO Pin Pull-up/down Enable Clock 0 */
	__IO	uint32_t	gppudclk1;	/* 0x09c GPIO Pin Pull-up/down Enable Clock 1 */
	__I	uint32_t	reserved0a0[4];
	__IO	uint32_t	test;		/* 0x0b0 Test */
} bcm2835_gpio_t;


#define SPI_CS_CS_01		(1 << 0)
#define SPI_CS_CS_10		(1 << 1)
#define SPI_CS_CPHA		(1 << 2)
#define SPI_CS_CPOL		(1 << 3)
#define SPI_CS_CLEAR_TX		(1 << 4)
#define SPI_CS_CLEAR_RX		(1 << 5)
#define SPI_CS_CSPOL		(1 << 6)
#define SPI_CS_TA		(1 << 7)
#define SPI_CS_DMAEN		(1 << 8)
#define SPI_CS_INTD		(1 << 9)
#define SPI_CS_INTR		(1 << 10)
#define SPI_CS_ADCS		(1 << 11)
#define SPI_CS_REN		(1 << 12)
#define SPI_CS_LEN		(1 << 13)
#define SPI_CS_DONE		(1 << 16)
#define SPI_CS_RXD		(1 << 17)
#define SPI_CS_TXD		(1 << 18)
#define SPI_CS_RXR		(1 << 19)
#define SPI_CS_RXF		(1 << 20)
#define SPI_CS_CSPOL0		(1 << 21)
#define SPI_CS_CSPOL1		(1 << 22)
#define SPI_CS_CSPOL2		(1 << 23)
#define SPI_CS_DMA_LEN		(1 << 24)
#define SPI_CS_LEN_LONG		(1 << 25)

/* BCM2835 SPI peripheral register map specification */
typedef struct __bcm2835_spi_t {
	__IO	uint32_t	cs;	/* 0x000 SPI Master Control and Status */
	__IO	uint32_t	fifo;	/* 0x004 SPI Master TX and RX FIFOs */
	__IO	uint32_t	clk;	/* 0x008 SPI Master Clock Divider */
	__IO	uint32_t	dlen;	/* 0x00c SPI Master Data Length */
	__IO	uint32_t	ltoh;	/* 0x010 SPI LOSSI mode TOH */
	__IO	uint32_t	dc;	/* 0x014 SPI DMA DREQ Controls */
} bcm2835_spi_t;


/*
 * AUX peripheral, contains:
 * - mini-uart
 * - SPI1
 * - SPI2
 */
#define AUX_ENABLES_MU				(1 << 0)
#define AUX_ENABLES_SPI1			(1 << 1)
#define AUX_ENABLES_SPI2			(1 << 2)

/* Bits 0...5 are shift length */
#define AUX_SPI_CNTL0_SHIFT_LENGTH_MASK_U	0x0000003f
#define AUX_SPI_CNTL0_SHIFT_LENGTH_MASK		(AUX_SPI_CNTL0_SHIFT_LENGTH_MASK_U << 0)
#define AUX_SPI_CNTL0_SHIFT_LENGTH(x)		(((x) & AUX_SPI_CNTL0_SHIFT_LENGTH_MASK_U) << 0)
#define AUX_SPI_CNTL0_MSB_OUT			(1 << 6)
#define AUX_SPI_CNTL0_LSB_OUT			(0 << 6)
#define AUX_SPI_CNTL0_CPOL			(1 << 7)
#define AUX_SPI_CNTL0_OUT_RISING		(1 << 8)
#define AUX_SPI_CNTL0_CLEARFIFO			(1 << 9)
#define AUX_SPI_CNTL0_IN_RISING			(1 << 10)
#define AUX_SPI_CNTL0_ENABLE			(1 << 11)
#define AUX_SPI_CNTL0_DOUT_HOLD_0		(0 << 12)
#define AUX_SPI_CNTL0_DOUT_HOLD_1		(1 << 12)
#define AUX_SPI_CNTL0_DOUT_HOLD_4		(2 << 12)
#define AUX_SPI_CNTL0_DOUT_HOLD_7		(3 << 12)
#define AUX_SPI_CNTL0_VAR_WIDTH			(1 << 14)
#define AUX_SPI_CNTL0_VAR_CS			(1 << 15)
#define AUX_SPI_CNTL0_POST_INPUT		(1 << 16)
#define AUX_SPI_CNTL0_CS_0			(1 << 17)
#define AUX_SPI_CNTL0_CS_1			(1 << 18)
#define AUX_SPI_CNTL0_CS_2			(1 << 19)
/* Bits 20...31 are divider */
#define AUX_SPI_CNTL0_SPEED_MASK_U		0x00000fff
#define AUX_SPI_CNTL0_SPEED_MASK		(AUX_SPI_CNTL0_SPEED_MASK_U << 20)
#define AUX_SPI_CNTL0_SPEED(x)			(((x) & AUX_SPI_CNTL0_SPEED_MASK_U) << 20)

#define AUX_SPI_CNTL1_KEEP_INPUT		(1 << 0)
#define AUX_SPI_CNTL1_MSB_IN			(1 << 1)
#define AUX_SPI_CNTL1_LSB_IN			(0 << 1)
#define AUX_SPI_CNTL1_DONEIRQ			(1 << 6)
#define AUX_SPI_CNTL1_TXIRQ			(1 << 7)
#define AUX_SPI_CNTL1_CSHIGH_MASK_U		0x00000003
#define AUX_SPI_CNTL1_CSHIGH_MASK		(AUX_SPI_CNTL1_CSHIGH_MASK_U << 8)
#define AUX_SPI_CNTL1_CSHIGH(x)			(((x) & AUX_SPI_CNTL1_CSHIGH_MASK_U) << 8)

#define AUX_SPI_STAT_COUNT_MASK			0x0000003f
#define AUX_SPI_STAT_BUSY			(1 << 6)
#define AUX_SPI_STAT_RX_EMPTY			(1 << 7)
#define AUX_SPI_STAT_RX_FULL			(1 << 8)
#define AUX_SPI_STAT_TX_EMPTY			(1 << 9)
#define AUX_SPI_STAT_TX_FULL			(1 << 10)
#define AUX_SPI_STAT_RX_LVL_MASK		0x00ff0000
#define AUX_SPI_STAT_TX_LVL_MASK		0xff000000

typedef struct __bcm2835_aux_t {
	__IO	uint32_t	irq;		/* 0x000 Auxiliary Interrupt status */
	__IO	uint32_t	enables;	/* 0x004 Auxiliary enables */
	__I	uint32_t	reserved008[14];
	__IO	uint32_t	mu_io;		/* 0x040 Mini Uart I/O Data */
	__IO	uint32_t	mu_ier;		/* 0x044 Mini Uart Interrupt Enable */
	__IO	uint32_t	mu_iir;		/* 0x048 Mini Uart Interrupt Identify */
	__IO	uint32_t	mu_lcr;		/* 0x04c Mini Uart Line Control */
	__IO	uint32_t	mu_mcr;		/* 0x050 Mini Uart Modem Control */
	__IO	uint32_t	mu_lsr;		/* 0x054 Mini Uart Line Status */
	__IO	uint32_t	mu_msr;		/* 0x058 Mini Uart Modem Status */
	__IO	uint32_t	mu_scratch;	/* 0x05c Mini Uart Scratch */
	__IO	uint32_t	mu_cntl;	/* 0x060 Mini Uart Extra Control */
	__IO	uint32_t	mu_stat;	/* 0x064 Mini Uart Extra Status */
	__IO	uint32_t	mu_baud;	/* 0x068 Mini Uart Baudrate */
	__I	uint32_t	reserved06c[5];
	// XXX: The SPI registers are different than what the documentation
	// states. These offsets are taken from the Linux kernel module
	// spi-bcm2835aux.c, which are most likely the correct offsets.
	// The kernel module actually makes a reference to "garbage" in the
	// official documentation.
	__IO	uint32_t	spi0_cntl0;	/* 0x080 SPI 1 Control register 0 */
	__IO	uint32_t	spi0_cntl1;	/* 0x084 SPI 1 Control register 1 */
	__IO	uint32_t	spi0_stat;	/* 0x088 SPI 1 Status */
	__IO	uint32_t	spi0_peek;	/* 0x08c SPI 1 Peek */
	__I	uint32_t	reserved090[4];
	__IO	uint32_t	spi0_io;	/* 0x0a0 SPI 1 Data */
	__I	uint32_t	reserved0a4[3];
	__IO	uint32_t	spi0_hold;	/* 0x0b0 SPI 1 Hold */
	__I	uint32_t	reserved0b4[3];
	__IO	uint32_t	spi1_cntl0;	/* 0x0c0 SPI 2 Control register 0 */
	__IO	uint32_t	spi1_cntl1;	/* 0x0c4 SPI 2 Control register 1 */
	__IO	uint32_t	spi1_stat;	/* 0x0c8 SPI 2 Status */
	__IO	uint32_t	spi1_peek;	/* 0x0cc SPI 2 Peek */
	__I	uint32_t	reserved0d0[4];
	__IO	uint32_t	spi1_io;	/* 0x0e0 SPI 2 Data */
	__I	uint32_t	reserved0e4[3];
	__IO	uint32_t	spi1_hold;	/* 0x0f0 SPI 2 Hold */
} bcm2835_aux_t;

#endif
