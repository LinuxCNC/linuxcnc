/*    Copyright (C) 2013 GP Orcullo
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#define SPICLKDIV		32		/* ~8 Mhz */
#define NUMAXES			3		/* X Y Z */

#define SPIBUFSIZE		32		/* SPI buffer size */
#define BUFSIZE			(SPIBUFSIZE/4)

#define STEPBIT			23		/* bit location in DDS accum */
#define STEP_MASK		(1<<STEPBIT)
#define BASEFREQ		80000ul		/* Base freq of the PIC stepgen in Hz */

#define PERIODFP 		((double)1.0 / (double)(BASEFREQ))
#define VELSCALE		((double)(1L << STEPBIT) * PERIODFP)
#define ACCELSCALE		(VELSCALE * PERIODFP)

#define get_position(a)		(rxBuf[2 + (a)])
#define get_inputs()		(rxBuf[2 + NUMAXES])
#define update_velocity(a, b)	(txBuf[1 + (a)] = (b))

/* Broadcom defines */

#define BCM2835_PERI_BASE	0x20000000
#define BCM2835_GPIO_BASE	(BCM2835_PERI_BASE + 0x200000) /* GPIO controller */
#define BCM2835_SPI_BASE	(BCM2835_PERI_BASE + 0x204000) /* SPI controller */

#define BCM2835_GPFSEL0		*(gpio)
#define BCM2835_GPFSEL1		*(gpio + 1)
#define BCM2835_GPFSEL2		*(gpio + 2)
#define BCM2835_GPFSEL3		*(gpio + 3)
#define BCM2835_GPFSEL4		*(gpio + 4)
#define BCM2835_GPFSEL5		*(gpio + 5)
#define BCM2835_GPSET0		*(gpio + 7)
#define BCM2835_GPSET1		*(gpio + 8)
#define BCM2835_GPCLR0		*(gpio + 10)
#define BCM2835_GPCLR1		*(gpio + 11)
#define BCM2835_GPLEV0		*(gpio + 13)
#define BCM2835_GPLEV1		*(gpio + 14)

#define BCM2835_SPICS 		*(spi + 0)
#define BCM2835_SPIFIFO     	*(spi + 1)
#define BCM2835_SPICLK 		*(spi + 2)

#define SPI_CS_LEN_LONG		0x02000000
#define SPI_CS_DMA_LEN		0x01000000
#define SPI_CS_CSPOL2		0x00800000
#define SPI_CS_CSPOL1		0x00400000
#define SPI_CS_CSPOL0		0x00200000
#define SPI_CS_RXF		0x00100000
#define SPI_CS_RXR		0x00080000
#define SPI_CS_TXD		0x00040000
#define SPI_CS_RXD		0x00020000
#define SPI_CS_DONE		0x00010000
#define SPI_CS_LEN		0x00002000
#define SPI_CS_REN		0x00001000
#define SPI_CS_ADCS		0x00000800
#define SPI_CS_INTR		0x00000400
#define SPI_CS_INTD		0x00000200
#define SPI_CS_DMAEN		0x00000100
#define SPI_CS_TA		0x00000080
#define SPI_CS_CSPOL		0x00000040
#define SPI_CS_CLEAR_RX		0x00000020
#define SPI_CS_CLEAR_TX		0x00000010
#define SPI_CS_CPOL		0x00000008
#define SPI_CS_CPHA		0x00000004
#define SPI_CS_CS_10		0x00000002
#define SPI_CS_CS_01		0x00000001

#define PAGE_SIZE		(4*1024)
#define BLOCK_SIZE		(4*1024)

#endif
