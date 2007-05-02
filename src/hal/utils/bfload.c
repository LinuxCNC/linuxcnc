/*************************************************************************
*
* bfload - loads xilinx bitfile into mesa 5i20 board FPGA
*
* Copyright (C) 2007 John Kasunich (jmkasunich at fastmail dot fm)
* portions based on m5i20cfg by Peter C. Wallace
*
**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of version 2 of the GNU General
Public License as published by the Free Software Foundation.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

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

**************************************************************************

Info about programming the 5i20:

Board wiring (related to programming):

 ????				<-- 9030 pin 156, GPIO2, bit  8 reg 0x54
 FPGA pin 104 = DONE		--> 9030 pin 157, GPIO3, bit 11 reg 0x54
 FGPA pin 107 = /INIT		--> 9030 pin 137, GPIO4, bit 14 reg 0x54
 STATUS LED CR11 (low = on)	<-- 9030 pin 136, GPIO5, bit 17 reg 0x54
 ????				<-- 9030 pin 135, GPIO6, bit 20 reg 0x54
 FPGA pin 161 = /WRITE		<-- 9030 pin 134, GPIO7, but 23 reg 0x54
 FPGA pin 106 = /PROGRAM,	<-- 9030 pin  94, GPIO8, bit 26 reg 0x54
 FPGA pin 155 = CCLK		<-> FPGA pin 182 (LCLK 33MHz)
 FPGA pin 160 = /CS		<-- /LWR (local bus write strobe)
 FPGA pin 153 = D0		<-> LAD0 (local data bus)
 FPGA pin 146 = D1		<-> LAD1 (local data bus)
 FPGA pin 142 = D2		<-> LAD2 (local data bus)
 FPGA pin 135 = D3		<-> LAD3 (local data bus)
 FPGA pin 126 = D4		<-> LAD4 (local data bus)
 FPGA pin 119 = D5		<-> LAD5 (local data bus)
 FPGA pin 115 = D6		<-> LAD6 (local data bus)
 FPGA pin 108 = D7		<-> LAD7 (local data bus)

Programming sequence:

 set /PROGRAM low for 300nS minimum (resets chip and starts clearing memory)
 /INIT and DONE go low
 set /PROGRAM high
 wait for /INIT to go high (100uS max, when done clearing memory)
 set /WRITE low
 send data bytes (each byte strobes /CS low)
 the last few bytes in the file are dummies, which provide the clocks
 needed to let the device come out of config mode and begin running
 if a CRC error is detected, /INIT will go low
 DONE will go high during the dummy bytes at the end of the file
 set /WRITE high

*************************************************************************/

//#define _GNU_SOURCE /* getline() */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/types.h>
#include "upci.h"
#include "bitfile.h"

/************************************************************************/

/* I/O registers */
#define CTRL_STAT_OFFSET	0x0054	/* 9030 GPIO register (region 1) */

 /* bit number in GPIO register */
#define GPIO_3_MASK		(1<<11)	/* GPIO 3 */
#define DONE_MASK		(1<<11)	/* GPIO 3 */
#define _INIT_MASK		(1<<14)	/* GPIO 4 */
#define _LED_MASK		(1<<17)	/* GPIO 5 */
#define GPIO_6_MASK		(1<<20)	/* GPIO 6 */
#define _WRITE_MASK		(1<<23)	/* GPIO 7 */
#define _PROGRAM_MASK		(1<<26)	/* GPIO 8 */

/* Card I.D */
#define PLX9030_VENDOR 0x10B5 /* PCI vendor I.D. */
#define PLX9030_DEVICE 0x9030 /* PCI device I.D. */
#define M5I20_SSVENDOR 0x10B5 /* PCI subsystem vendor I.D. */
#define M5I20_SSDEVICE 0x3131 /* PCI subsystem device I.D. */

/* Exit codes */
#define EC_OK    0   /* Exit OK. */
#define EC_BADCL 100 /* Bad command line. */
#define EC_HDW   101 /* Some sort of hardware failure on the 5I20. */
#define EC_FILE  102 /* File error of some sort. */
#define EC_SYS   103 /* Beyond our scope. */

/************************************************************************/

static void errmsg(const char *funct, const char *fmt, ...);
static int parse_cmdline(unsigned argc, char *argv[]);
static int programfpga(int devnum, struct bitfile_chunk *ch);
static __u8 bit_reverse (__u8 data);
static int write5i20ram(int devnum, struct bitfile_chunk *ch);

/* globals to pass data from command line parser to main */
static char *config_file_name;
static int card_number;

/***********************************************************************/

int main(int argc, char *argv[])
{
    struct upci_dev_info info;
    int retval, devnum;
    struct bitfile *bf;
    struct bitfile_chunk *ch;

    /* if we are setuid, drop privs until needed */
    seteuid(getuid());
    if ( parse_cmdline(argc, argv) != 0 ) {
	errmsg(__func__,"command line error" );
	return EC_BADCL;
    }
    printf ( "Reading '%s'...\n", config_file_name);
    bf = bitfile_read(config_file_name);
    if ( bf == NULL ) {
	errmsg(__func__,"reading bitstream file '%s'", config_file_name );
	return EC_FILE;
    }
    retval = bitfile_validate_xilinx_info(bf);
    if ( retval != 0 ) {
	errmsg(__func__,"not a valid Xilinx bitfile" );
	return EC_FILE;
    }
    bitfile_print_xilinx_info(bf);
    /* chunk 'e' has the bitstream */
    ch = bitfile_find_chunk(bf, 'e', 0);
    /* now deal with the hardware */
    printf ( "Searching for 5i20 board...\n" );
    retval = upci_scan_bus();
    if ( retval < 0 ) {
	errmsg(__func__,"PCI bus data missing" );
	return EC_SYS;
    }
    info.vendor_id = PLX9030_VENDOR;
    info.device_id = PLX9030_DEVICE;
    info.ss_vendor_id = M5I20_SSVENDOR;
    info.ss_device_id = M5I20_SSDEVICE;
    info.instance = card_number;
    devnum = upci_find_device(&info);
    if ( devnum < 0 ) {
	errmsg(__func__, "5i20 board %d not found", info.instance );
	return EC_HDW;
    }
    upci_print_device_info(devnum);
    printf ( "Loading configuration into 5i20 board...\n" );
    retval = programfpga(devnum, ch);
    if ( retval != 0 ) {
	errmsg(__func__, "configuration did not load");
	return EC_HDW;
    }
    /* do we need to write data for the HAL driver to the board? */
    ch = bitfile_find_chunk(bf, 'w', 0);
    if ( ch != NULL ) {
	/* yes */
	printf ( "Writing data to FPGA RAM\n" );
	retval = write5i20ram(devnum, ch);
	if ( retval != 0 ) {
	    errmsg(__func__, "RAM data could not be loaded", info.instance );
	    return EC_HDW;
	}
    }
    upci_reset();
    printf( "Finished!\n" );
    return 0;
}

/************************************************************************/

static void errmsg(const char *funct, const char *fmt, ...)
{
    va_list vp;

    va_start(vp, fmt);
    fprintf(stderr, "ERROR in %s(): ", funct);
    vfprintf(stderr, fmt, vp);
    fprintf(stderr, "\n");
}

static int parse_cmdline(unsigned argc, char *argv[])
{
    if ((argc != 2) && (argc != 3)) {
	printf("\nbfload <filename> [<card>]\n\n");
	printf("    <filename> - name of bitfile\n");
	printf("    <cardnum>  - card number (default is 0)\n\n");
	printf("Loads an FPGA configuration from a bitfile into a\n");
	printf("Mesa 5i20 FPGA.  If the bitfile contains HAL driver\n");
	printf("config data, writes that data to the FPGA's RAM.\n\n");
	exit(EC_BADCL);
    }
    config_file_name = argv[1];
    card_number = 0;
    if ( argc == 3 ) {
	if (sscanf(argv[2], "%d", &card_number) != 1) {
	    errmsg(__func__,"bad card number: %s", argv[2]);
	    return -1;
	}
    }
    if (card_number > 15) {
	errmsg(__func__,"card number %d out of range (range is 0 to 15)", card_number);
	return -1;
    }
    return 0;
}

/* program FPGA on PCI board 'devnum', with data from bitfile chunk 'ch' */

static int programfpga(int devnum, struct bitfile_chunk *ch)
{
    int ctrl_region, data_region, count;
    __u32 status, control;
    __u8 *dp;

    printf("Opening PCI regions...\n");
    /* open regions for access */
    ctrl_region = upci_open_region(devnum, 1);
    if ( ctrl_region < 0 ) {
	errmsg(__func__, "could not open device %d, region %d (5i20 control port)", devnum, 1 );
	goto cleanup0;
    }
    data_region = upci_open_region(devnum, 2);
    if ( data_region < 0 ) {
	errmsg(__func__, "could not open device %d, region %d (5i20 data port)", devnum, 2 );
	goto cleanup1;
    }
    printf("Resetting FPGA...\n" );
    /* read current state of register */
    status = upci_read_u32(ctrl_region, CTRL_STAT_OFFSET);
    /* set /PROGRAM bit low  to reset device */
    control = status & ~_PROGRAM_MASK;
    /* set /WRITE and /LED high, (idle state) */
    control |= _WRITE_MASK | _LED_MASK;
    /* and write it back */
    upci_write_u32(ctrl_region, CTRL_STAT_OFFSET, control);
    /* verify that /INIT and DONE went low */
    status = upci_read_u32(ctrl_region, CTRL_STAT_OFFSET);
    if ( status & (DONE_MASK | _INIT_MASK) ) {
	errmsg(__func__, "FPGA did not reset: /INIT = %d, DONE = %d",
	    (status & _INIT_MASK ? 1 : 0), (status & DONE_MASK ? 1 : 0) );
	goto cleanup2;
    }
    /* set /PROGRAM high, let FPGA come out of reset */
    control = status | _PROGRAM_MASK;
    upci_write_u32(ctrl_region, CTRL_STAT_OFFSET, control);
    /* wait for /INIT to go high when it finishes clearing memory
	This should take no more than 100uS.  If we assume each PCI read
	takes 30nS (one PCI clock), that is 3300 reads.  Reads actually
	take several clocks, but even at a microsecond each, 3.3mS is not
	an excessive timeout value */
    count = 3300;
    do {
	status = upci_read_u32(ctrl_region, CTRL_STAT_OFFSET);
	if ( status & _INIT_MASK ) break;
    } while ( --count > 0 );
    if ( count == 0 ) {
	errmsg(__func__, "FPGA did not come out of /INIT" );
	goto cleanup3;
    }
    /* set /WRITE low for data transfer, and turn on LED */
    control = status & ~_WRITE_MASK & ~_LED_MASK;
    upci_write_u32(ctrl_region, CTRL_STAT_OFFSET, control);

    /* Program the card */
    count = ch->len;
    dp = ch->body;
    printf("Writing data to FPGA....\n" );
    while ( count-- > 0 ) {
	upci_write_u8(data_region, 0, bit_reverse(*(dp++)));
    }
    /* all bytes transferred, clear "bytes to go" indicator */
    printf("Data transfer complete...\n");
    status = upci_read_u32(ctrl_region, CTRL_STAT_OFFSET);
    if ( ! (status & _INIT_MASK) ) {
	/* /INIT goes low on CRC error */
	errmsg(__func__, "FPGA asserted /INIT: CRC error" );
	goto cleanup3;
    }
    if ( ! (status & DONE_MASK) ) {
	errmsg(__func__, "FPGA did not assert DONE" );
	goto cleanup3;
    }
    /* turn off write enable and LED */
    control = status | _WRITE_MASK | _LED_MASK;
    upci_write_u32(ctrl_region, CTRL_STAT_OFFSET, control);
    printf("Successfully programmed %u bytes\n", ch->len);
    return 0;
cleanup3:
    /* set /PROGRAM low (reset device), /WRITE high and LED off */
    status = upci_read_u32(ctrl_region, CTRL_STAT_OFFSET);
    control = status & ~_PROGRAM_MASK;
    control |= _WRITE_MASK | _LED_MASK;
    upci_write_u32(ctrl_region, CTRL_STAT_OFFSET, control);
cleanup2:
    upci_close_region(data_region);
cleanup1:
    upci_close_region(ctrl_region);
cleanup0:
    return -1;
}

/* the fpga was originally designed to be programmed serially... even
   though we are doing it using a parallel interface, the bit ordering
   is based on the serial interface, and the data needs to be reversed
*/

static __u8 bit_reverse (__u8 data)
{
    static const __u8 swaptab[256] = {
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF };

    return swaptab[data];
}

/* write data from bitfile chunk 'ch' to FPGA on board 'devnum' */

static int write5i20ram(int devnum, struct bitfile_chunk *ch)
{
    int mem_region, n;
    __u32 data;

    printf("Opening PCI region...\n");
    mem_region = upci_open_region(devnum, 5);
    if ( mem_region < 0 ) {
	errmsg(__func__, "could not open device %d, region %d (5i20 memory)",
	    devnum, 5 );
	return -1;
    }
    printf("Writing data to FPGA...\n");
    data = 0;
    for ( n = 0 ; n < ch->len ; n++ ) {
	data |= ch->body[n] << ((n & 3) * 8);
	if ( (n & 3) == 3 ) {
	    /* have 32 bits, write to the RAM */
	    upci_write_u32(mem_region, (n & ~3), data);
	    /* prep for next 32 */
	    data = 0;
	}
    }
    if ( (n & 3) != 0 ) {
	/* have residual bits, write to the RAM */
	upci_write_u32(mem_region, (n & ~3), data);
    }
    printf("Transferred %d bytes\n", n);
    upci_close_region(mem_region);
    return 0;
}
