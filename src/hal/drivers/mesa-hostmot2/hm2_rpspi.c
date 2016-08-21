/*    This is a component for RaspberryPi to hostmot2 over SPI for linuxcnc.
 *    Copyright 2016 Matsche <tinker@play-pla.net>
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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Without Source Tree */
#undef WOST

#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fsuid.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"

#include "rtapi_stdint.h"
#include "rtapi_bool.h"
#include "rtapi_gfp.h"

#include "rtapi_bool.h"

#if defined (WOST)
#include "include/hostmot2-lowlevel.h"
#include "include/hostmot2.h"
#else
#include "hostmot2-lowlevel.h"
#include "hostmot2.h"
#endif

#include "spi_common_rpspi.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matsche");
MODULE_DESCRIPTION("Driver for HostMot2 devices connected via SPI to RaspberryPi");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-7i90");

#define MAX_BOARDS (1)
#define MAX_MSG (512)

volatile unsigned *gpio, *spi;
//static volatile uint32_t txBuf[MAX_MSG], rxBuf[MAX_MSG];
static platform_t platform;

typedef struct hm2_rpspi_struct hm2_rpspi_t;

static uint32_t mk_read_cmd(uint16_t, unsigned, bool);
static uint32_t mk_write_cmd(uint16_t, unsigned, bool);
static int hm2_rpspi_write(hm2_lowlevel_io_t *, uint32_t, void *, int);
static int hm2_rpspi_read(hm2_lowlevel_io_t *, uint32_t, void *, int);
static int check_cookie(hm2_rpspi_t *);
static int read_ident(hm2_rpspi_t *, char *);
static int probe_board(hm2_rpspi_t *, uint16_t);
static void hm2_rpspi_cleanup(void);
static int hm2_rpspi_setup(void);

static int map_gpio();
static void setup_gpio(uint16_t);
static void restore_gpio();
static platform_t check_platform(void);

static char *config[MAX_BOARDS];
RTAPI_MP_ARRAY_STRING(config, MAX_BOARDS, "config string for the AnyIO boards (see hostmot2(9) manpage)")

struct hm2_rpspi_struct {
	hm2_lowlevel_io_t llio;
	int nr;
	uint32_t txBuf[MAX_MSG];
	uint32_t rxBuf[MAX_MSG];
	uint16_t spiclkdiv;
	uint8_t spibpf;	//bits per frame
};

static hm2_rpspi_t boards[MAX_BOARDS];
static int comp_id;

/*************************************************/
// aib (address increment bit)
static uint32_t mk_read_cmd(uint16_t addr, unsigned msglen, bool aib) {
	return 0 | (addr << 16) | 0xA000 | (aib ? 0x800 : 0) | (msglen << 4);
}

/*************************************************/
static uint32_t mk_write_cmd(uint16_t addr, unsigned msglen, bool aib) {
	return 0 | (addr << 16) | 0xB000 | (aib ? 0x800 : 0) | (msglen << 4);
}

/*************************************************/
/*
SPI_CS_RXF		0x00100000	
		RXF - RX FIFO Full
		0 = RXFIFO is not full.
		1 = RX FIFO is full. No further serial data will be sent/ received until data is read from FIFO.

SPI_CS_RXR		0x00080000
		RXR RX FIFO needs Reading ( full)
		0 = RX FIFO is less than full (or not active TA = 0).
		1 = RX FIFO is or more full. Cleared by reading sufficient data from the RX FIFO or setting TA to	0.

SPI_CS_TXD		0x00040000
		TXD TX FIFO can accept Data
		0 = TX FIFO is full and so cannot accept more data.
		1 = TX FIFO has space for at least 1 byte.

SPI_CS_RXD		0x00020000
		RXD RX FIFO contains Data
		0 = RX FIFO is empty.
		1 = RX FIFO contains at least 1 byte.
*/
static int hm2_rpspi_write(hm2_lowlevel_io_t *llio, uint32_t addr, void *buffer, int size) {
	//rtapi_print_msg(RTAPI_MSG_ERR, "hm2_rpspi_write addr: %08x, size: %d", addr, size);
	hm2_rpspi_t *this = (hm2_rpspi_t*) llio;
	if(size == 0) return 0;
	if(size % 4) return -EINVAL;

	int msgsize = size/4;
	uint8_t *tbuff;
	uint32_t *buffer32 = (uint32_t *)buffer;
	int i=0, j=0;
	uint8_t  gully __attribute__((unused));	// it's like a drain or nulldevice.

	this->txBuf[0] = mk_write_cmd(addr, msgsize, true);

	 for(i=0; i<msgsize; i++)
		this->txBuf[i+1] = buffer32[i];
	
	/* activate transfer and clear fifos */
	//BCM2835_SPICS = SPI_CS_TA|SPI_CS_CPHA|SPI_CS_CLEAR_TX|SPI_CS_CLEAR_RX;
	BCM2835_SPICS |= SPI_CS_TA|SPI_CS_CLEAR_TX|SPI_CS_CLEAR_RX;

	tbuff = (uint8_t *)this->txBuf;

	/* send txBuf */
	/* RPi SPI transfers 8 bits at a time only */
	for (i=0; i<msgsize+1; i++) {
		for(j=3; j>=0; j--){
			BCM2835_SPIFIFO = tbuff[j];
			if(!(BCM2835_SPICS & SPI_CS_TXD))
				rtapi_print_msg(RTAPI_MSG_ERR, "TX-FIFO is full!\n");
		}
		/* wait until transfer is finished */
		while (!(BCM2835_SPICS & SPI_CS_DONE));
		
		if((BCM2835_SPICS & SPI_CS_RXR) || (BCM2835_SPICS & SPI_CS_RXF))
			rtapi_print_msg(RTAPI_MSG_ERR, "RX-FIFO is full!\n");
		
		for(j=3; j>=0; j--){
			if(!(BCM2835_SPICS & SPI_CS_RXD))
				rtapi_print_msg(RTAPI_MSG_ERR, "RX-FIFO is empty!\n");
			gully = BCM2835_SPIFIFO;	// discard one byte from read FIFO
		}
		
		tbuff += 4;
	}
	
	/* Stop transfer */
	BCM2835_SPICS &= ~SPI_CS_TA;

	return 1;
}

/*************************************************/
static int hm2_rpspi_read(hm2_lowlevel_io_t *llio, uint32_t addr, void *buffer, int size) {
	//rtapi_print_msg(RTAPI_MSG_ERR, "hm2_rpspi_read addr: %08x, size: %d", addr, size);
	
	hm2_rpspi_t *this = (hm2_rpspi_t*) llio;
	if(size == 0) return 0;
	if(size % 4) return -EINVAL;

	int msgsize = size/4;
	uint8_t *tbuff, *rbuff;
	uint32_t *buffer32 = (uint32_t *)buffer;
	int j=0, i=0;
	this->txBuf[0] = mk_read_cmd(addr, msgsize, true);
	
	for(i=0; i<msgsize; i++)
		this->txBuf[i+1] = 0;	//just zeros for read
	
	/* activate transfer and clear fifos */
	//BCM2835_SPICS = SPI_CS_TA|SPI_CS_CPHA|SPI_CS_CLEAR_TX|SPI_CS_CLEAR_RX;
	BCM2835_SPICS |= SPI_CS_TA|SPI_CS_CLEAR_TX|SPI_CS_CLEAR_RX;

	/*rtapi_print_msg(RTAPI_MSG_ERR, "Out-Data: ");
	for (i=0; i<msgsize; i++) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%08x:", this->txBuf[i]);
	} 
		rtapi_print_msg(RTAPI_MSG_ERR, "1 BCM2835_SPICS: %08x:", BCM2835_SPICS);
	*/
	
	tbuff = (uint8_t *)this->txBuf;
	rbuff = (uint8_t *)this->rxBuf;
	
	
	/* send txBuf */
	/* RPi SPI transfers 8 bits at a time only */
	for (i=0; i<msgsize+1; i++) {
		for(j=3; j>=0; j--){
			//while(!(BCM2835_SPICS & SPI_CS_TXD));	// wait for  free space in fifo
			BCM2835_SPIFIFO = tbuff[j];
			if(!(BCM2835_SPICS & SPI_CS_TXD))
				rtapi_print_msg(RTAPI_MSG_ERR, "TX-FIFO is full!\n");
		}
		
		/* wait until transfer is finished */
		while (!(BCM2835_SPICS & SPI_CS_DONE));
		
		//rtapi_print_msg(RTAPI_MSG_ERR, "2 BCM2835_SPICS: %08x:", BCM2835_SPICS);
		
		if((BCM2835_SPICS & SPI_CS_RXR) || (BCM2835_SPICS & SPI_CS_RXF))
			rtapi_print_msg(RTAPI_MSG_ERR, "RX-FIFO is full!\n");
		
		for(j=3; j>=0; j--){
			//while(!(BCM2835_SPICS & SPI_CS_RXD));	// wait for data in fifo
			if(!(BCM2835_SPICS & SPI_CS_RXD))
				rtapi_print_msg(RTAPI_MSG_ERR, "RX-FIFO is empty!\n");
			rbuff[j] = BCM2835_SPIFIFO;
		}
		tbuff += 4;
		rbuff += 4;
	}
	
	/*rtapi_print_msg(RTAPI_MSG_ERR, "In-Data: ");
	for (i=0; i<msgsize; i++) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%08x:", this->rxBuf[i]);
	}
	rtapi_print_msg(RTAPI_MSG_ERR, "\n"); */
	
	/* Stop transfer */
	BCM2835_SPICS &= ~SPI_CS_TA;
	
	//rtapi_print_msg(RTAPI_MSG_ERR, "3 BCM2835_SPICS: %08x:", BCM2835_SPICS);
	
	/*buff = (char *)this->rxBuf;
	buff += 4; //skip the cookie */
	for (i=0; i<msgsize; i++) {
		buffer32[i] = this->rxBuf[i+1];
	}
	
	return 1;
}

/*************************************************/
static int check_cookie(hm2_rpspi_t *board) {
	uint32_t cookie[4];
	uint32_t xcookie[4] = {0x55aacafe, 0x54534f48, 0x32544f4d, 0x00000400};
	int r = hm2_rpspi_read(&board->llio, 0x100, cookie, 16);
	if(r < 0) return -errno;

	if(memcmp(cookie, xcookie, sizeof(cookie))) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Invalid cookie\n");
		rtapi_print_msg(RTAPI_MSG_ERR, "Read: %08x %08x %08x %08x\n",
			cookie[0], cookie[1], cookie[2], cookie[3]);
		return -ENODEV;
	}
	return 0;
}

/*************************************************/
static int read_ident(hm2_rpspi_t *board, char *ident) {
	return hm2_rpspi_read(&board->llio, 0x40c, ident, 8);
}

/*************************************************/
static int probe_board(hm2_rpspi_t *board, uint16_t spiclkdiv) {
	//printf("probe %d\n", spiclkdiv);
   
	board->spiclkdiv = spiclkdiv;
	board->spibpf = 32;

	int ret = check_cookie(board);
	if(ret < 0) return ret;

	char ident[8];
	ret = read_ident(board, ident);
	if(ret < 0) return ret;

	char *base;

	if(!memcmp(ident, "MESA7I90", 8)) {
		base = "hm2_7i90";
		board->llio.num_ioport_connectors = 3;
		board->llio.pins_per_connector = 24;
		board->llio.ioport_connector_name[0] = "P1";
		board->llio.ioport_connector_name[1] = "P2";
		board->llio.ioport_connector_name[2] = "P3";
		board->llio.num_leds = 2;
		board->llio.fpga_part_number = "xc6slx9tq144";
	} else {
		int i=0;
		for(i=0; i<sizeof(ident); i++)
			if(!isprint(ident[i])) ident[i] = '?';
		rtapi_print_msg(RTAPI_MSG_ERR, "Unknown board: %.8s\n", ident);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_ERR, "Base:%s.%d", base, board->nr);
	rtapi_snprintf(board->llio.name, sizeof(board->llio.name),
		"%s.%d", base, board->nr);
	board->llio.comp_id = comp_id;
	board->llio.private = &board;
	board->llio.read = hm2_rpspi_read;
	board->llio.write = hm2_rpspi_write;
	board->llio.queue_read = 0;
	board->llio.queue_write = 0;

	return 0;
}

/*************************************************/
static int hm2_rpspi_setup(void) {
	int i, retval = -1;
	uint16_t spiclkdiv = 3;  // 3 = approx. 32 MHz
	
	platform = check_platform();
	if (platform == UNSUPPORTED) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"HAL_hm2_rpspi: ERROR: Unsupported Platform, only Raspberry1/2/3 supported...\n");
		return retval;
	}
	retval = map_gpio();
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"HAL_hm2_rpspi: ERROR: cannot map GPIO memory\n");
		return retval;
	}
	setup_gpio(spiclkdiv);

	 for(i=0; i<MAX_BOARDS; i++) {
		boards[i].nr = i;
		retval = probe_board(&boards[i], spiclkdiv);
		if(retval < 0) return retval;
		retval = hm2_register(&boards[i].llio, config[i]);
		if(retval < 0){
			rtapi_print_msg(RTAPI_MSG_ERR, "hm2_register failed...!");
			return retval;
		}
	}

	return 0;
}

/*************************************************/
static void hm2_rpspi_cleanup(void) {
	// reset the FPGA

	if (gpio != MAP_FAILED && spi != MAP_FAILED) {
		
		restore_gpio();
		munmap((void *)gpio,BLOCK_SIZE);
		munmap((void *)spi,BLOCK_SIZE);
	}
}

/*************************************************/
int rtapi_app_main() {
	int ret;
	
	comp_id = ret = hal_init("hm2_rpspi");
	if(ret < 0) goto fail;

	ret = hm2_rpspi_setup();
	if(ret < 0) goto fail;
	
	hal_ready(comp_id);
	return 0;

fail:
	hm2_rpspi_cleanup();
	return ret;
}

/*************************************************/
void rtapi_app_exit(void) {
	hal_exit(comp_id);
}

/*************************************************/
static int map_gpio() {
	int fd;
	static uint32_t mem_base, mem_spi_base;

	switch (platform) {
	case RPI:
		mem_base = BCM2835_GPIO_BASE;
		mem_spi_base = BCM2835_SPI_BASE;
		break;
	case RPI_2:	//for RPI 3 too
		mem_base = BCM2835_GPIO_BASE + BCM2709_OFFSET;
		mem_spi_base = BCM2835_SPI_BASE + BCM2709_OFFSET;
		break;
	default:
		rtapi_print_msg(RTAPI_MSG_ERR,"HAL_hm2_rpspi: Plattform not supported! \n");
		return -1;
		break;
	}

	//fd = open("/dev/mem", O_RDWR | O_SYNC);
	fd = rtapi_open_as_root("/dev/mem", O_RDWR | O_SYNC);
	
	if (fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"HAL_hm2_rpspi: can't open /dev/mem \n");
		return -1;
	}

	/* mmap GPIO */
	gpio = mmap(
			NULL,
			BLOCK_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			fd,
			mem_base);

	/* mmap SPI */
	spi = mmap(
			NULL,
			BLOCK_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			fd,
			mem_spi_base);

	close(fd);

	if (gpio == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"HAL_hm2_rpspi: can't map gpio\n");
		return -2;
	}

	if (spi == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"HAL_hm2_rpspi: can't map spi\n");
		return -3;
	}

	return 0;
}

/*************************************************/
static void setup_gpio(uint16_t spiclkdiv) {
	uint32_t x;

	/* change SPI pins */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (8*3) | 0b111 << (9*3));
	x |= (0b100 << (8*3) | 0b100 << (9*3));	// pin8 pin9 alternate0 (SPI)
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	x |= (0b100 << (0*3) | 0b100 << (1*3));	// pin10 pin11 alternate0 (SPI)
	BCM2835_GPFSEL1 = x;

	/* set up SPI */
	BCM2835_SPICLK = (1 << spiclkdiv);

	BCM2835_SPICS = 0;
	
	/* CPHA=0, CPOL=0 */
	BCM2835_SPICS |= SPI_CS_CPHA;
	
	/* clear FIFOs */
	BCM2835_SPICS |= SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX;

	/* Stop transfer */
	BCM2835_SPICS &= ~SPI_CS_TA;
}

/*************************************************/
static void restore_gpio() {
	uint32_t x;

	/* change SPI pins to inputs*/
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (8*3) | 0b111 << (9*3));
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	BCM2835_GPFSEL1 = x;
}

/*************************************************/
static platform_t check_platform(void)
{
	FILE *fp;
	char buf[2048];
	size_t fsize;

	fp = fopen("/proc/cpuinfo", "r");
	fsize = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
	
	if (fsize == 0 || fsize == sizeof(buf))
		return 0;

	/* NUL terminate the buffer */
	buf[fsize] = '\0';

	if (NULL != strstr(buf, "BCM2708"))
		return RPI;
	else if (NULL != strstr(buf, "BCM2709"))
		return RPI_2;	//for RPI 3 too
	else
		return UNSUPPORTED;
}
