/***************************************************************************
 *            pci_read.c
 *
 *  Sun Sep  9 15:25:18 2007
 *  Copyright  2007  Stephen Wille Padnos
 *  swpadnos @ users.sourceforge.net
 *  loosely based on a work by John Kasunich: bfload.c
 *
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as published 
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/types.h>
#include "upci.h"
#include "bitfile.h"

/* define DEBUG_PRINTS to print info about command line parameters,
   the detected board, etc. */
#undef DEBUG_PRINTS

#define array_size(x) (sizeof(x)/sizeof(x[0]))

struct board_info {
    char *board_type;
    char *chip_type;
    unsigned short vendor_id;
    unsigned short device_id;
    unsigned short ss_vendor_id;
    unsigned short ss_device_id;
    int fpga_pci_region;
    int upci_devnum;
};


struct board_info board_info_table[] =
    {
	{ "5i20", "2s200pq208", 0x10B5, 0x9030, 0x10B5, 0x3131, 5, 0},
	{ "5i22-1M", "3s1000fg320", 0x10B5, 0x9054, 0x10B5, 0x3132, 3, 0},
	{ "5i22-1.5M", "3s1500fg320", 0x10B5, 0x9054, 0x10B5, 0x3131, 3, 0}
    };

static void errmsg(const char *funct, const char *fmt, ...);
static int parse_cmdline(unsigned argc, char *argv[]);

/* globals to pass data from command line parser to main */
struct read_var {
	__u32 *val;		/* value */
	__u32 minval, maxval;	/* allowable range, set them equal for no limit */
	char *shortname;		/* name of var */
	char *longname;			/* more descriptive var name for error messages */
};

/* these could have been stored in the struct above, but it's easier to access
   them this way */
static __u32 cardnum, cardtype, pci_region, pci_offset, value;

static struct read_var params[] =
	{
	{&cardnum, 0, 15, "cardnum", "Card Number"},
	{&cardtype, 0, array_size(board_info_table), "cardtype", "Card Type"},
	{&pci_region, 0, 3, "region", "PCI Region"},
	{&pci_offset, 0, 65532, "offset", "Region Offset"},
	};


/* Exit codes */
#define EC_OK    0   /* Exit OK. */
#define EC_BADCL 100 /* Bad command line. */
#define EC_HDW   101 /* Some sort of hardware failure on the 5I20. */
#define EC_FILE  102 /* File error of some sort. */
#define EC_SYS   103 /* Beyond our scope. */


/***********************************************************************/

int main(int argc, char *argv[])
{
	int data_region, retval;
#ifdef DEBUG_PRINTS
	int dbg;
#endif
	struct upci_dev_info info;
    struct board_info board;

    /* if we are setuid, drop privs until needed */
    retval = seteuid(getuid());
    if (retval != 0) {
        fprintf(stderr, "failed to set euid to uid %d: %s\n", getuid(), strerror(errno));
        return EC_SYS;
    }

    if ( parse_cmdline(argc, argv) != EC_OK ) {
		return EC_BADCL;
    }

#ifdef DEBUG_PRINTS
	for (dbg=0;dbg<array_size(params);dbg++) {
		printf("Parameter %s = %u\n", params[dbg].longname, *(params[dbg].val));
	}
#endif
    /* set up local pointer to the correct board info table entry */
    board = board_info_table[cardtype];
#ifdef DEBUG_PRINTS
    printf ( "Board type:      %s\n", board.board_type );
#endif
    /* now deal with the hardware */
    retval = upci_scan_bus();
    if ( retval < 0 ) {
		errmsg(__func__,"PCI bus data missing" );
		return EC_SYS;
    }
    info.vendor_id = board.vendor_id;
    info.device_id = board.device_id;
    info.ss_vendor_id = board.ss_vendor_id;
    info.ss_device_id = board.ss_device_id;
    info.instance = cardnum;
    /* find the matching device */
    board.upci_devnum = upci_find_device(&info);
    if ( board.upci_devnum < 0 ) {
		errmsg(__func__, "%s board #%d not found",
	    	board.board_type, info.instance );
		return EC_HDW;
    }
#ifdef DEBUG_PRINTS
    upci_print_device_info(board.upci_devnum);
#else
    data_region = upci_open_region(board.upci_devnum, pci_region);
    value = upci_read_u32(data_region, pci_offset);
#endif
    printf("0x%08X\n", value);
    return EC_OK;
}

/************************************************************************/

static void errmsg(const char *funct, const char *fmt, ...)
{
    va_list vp;

    va_start(vp, fmt);
    fprintf(stderr, "ERROR in %s(): ", funct);
    vfprintf(stderr, fmt, vp);
    fprintf(stderr, "\n");
    va_end(vp);
}

void usage(void) {
	int i;
	fprintf(stderr, "\npci_read <card> <cardtype> <region> <offset>\n\n");
	fprintf(stderr, "All parameters are required.\n");
	fprintf(stderr, "    <card>      - card number (0-15)\n\n");
	fprintf(stderr, "    <cardtype>  - card type.  valid types are:\n");
	for (i=0;i<array_size(board_info_table);i++) {
		fprintf(stderr, "                  %d - %s\n", i, board_info_table[i].board_type);
	}
	fprintf(stderr, "\n    <region>    - which PCI region to read from\n\n");
	fprintf(stderr, "    <offset>    - offset into the region\n\n");
}

static int parse_cmdline(unsigned argc, char *argv[])
{
	int i;
	__u32 temp;
	char * eptr;

    if (argc != array_size(params)+1) {
		usage();
		return EC_BADCL;
    }

	/* loop through the command line parameters and the list of vars */
	for (i=0;i<array_size(params);i++)
	{
		temp = strtoul(argv[i+1], &eptr, 0);
		if (*eptr!='\0')
		{
			errmsg(__func__,"invalid %s: %s", params[i].longname, argv[i+1]);
			return EC_BADCL;
		}
		if (params[i].minval != params[i].maxval) {
				if ((temp < params[i].minval) || (temp > params[i].maxval)) {
					errmsg(__func__,"Parameter %s out of range: %u",
						params[i].longname, temp);
					return EC_BADCL;
				}
		}
		*(params[i].val)=temp;
	}
    return EC_OK;
}
