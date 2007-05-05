/*************************************************************************
*
* bfmerge - merges info for HAL driver into FPGA bitfie
*
* Copyright (C) 2007 John Kasunich (jmkasunich at fastmail dot fm)
*
**************************************************************************

'bfmerge' reads a file containing data that is intended for the hal_5i2x
driver, and merges that data into an FPGA bitfile.  The data describes
the configuration that is in the FPGA, in a way that allows the driver
to export the appropriate HAL objects (pins, parameters, functions, etc.)
'bfmerge' adds that data to a section of an FPGA bitfile.  Later, when
the bitfile is loaded into the FPGA, the driver data is loaded into the
FPGA's RAM, where the driver can read it.

'bfmerge' accepts a file containing the driver data ('ramfile'), a source
bitfile name, and an optional destination bitfile name.  The source bit-
file contains the FPGA configuration data.  The driver data from the
ramfile is appended to the FPGA config, and is written to the destination
bitfile.  If no destination file was supplied, the results are written
to the original bitfile.

The format of the ram file is as follows:

Comments start with '#' (anywhere on a line) and run to the end of line.
Addresses are specified by '@' followed by a number (hex or decimal).
Data is specified by hex or decimal numbers, one per byte, and the address
increments automatically after each byte.  Sixteen bit data values can be
entered by preceding the number with 'w', and 32 bit by preceding it with
'l'.  Multi-byte values are stored most significant byte first.  The
memory buffer is zeroed at startup, so large chunks of zeros don't need
to be specified in the file.

This program automatically calculates a checksum and writes it to the
last two bytes of the FPGA config RAM.

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

This code was written as part of the EMC HAL project.  For more
information, go to www.linuxcnc.org.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <linux/types.h>
#include <errno.h>
#include <ctype.h>
#include "bitfile.h"

#define RAMLEN 1024

static void errmsg(const char *funct, const char *fmt, ...)
{
    va_list vp;

    va_start(vp, fmt);
    fprintf(stderr, "ERROR in %s(): ", funct);
    vfprintf(stderr, fmt, vp);
    fprintf(stderr, "\n");
}

#define BUFLEN 1000

int main ( int argc, char *argv[] )
{
    char *ramfile_name;
    FILE *ramfile;
    int linenum;
    unsigned int addr;
    char *cp, *cp1, buf[BUFLEN];
    struct bitfile *bf;
    struct bitfile_chunk *ch;
    char *dst;
    int n, rv;
    __u32 temp;
    __u16 checksum1, checksum2;
    __u8 data[RAMLEN];
    __u8 *dp8;

    if (( argc != 3 ) && ( argc != 4 )) {
	printf ( "usage: bfmerge <ramfile> <src-bitfile> [<dst-bitfile>]\n" );
	return 1;
    }
    ramfile_name = argv[1];
    printf("reading ram file '%s'\n", ramfile_name );
    ramfile = fopen(argv[1], "r" );
    if ( ramfile == NULL ) {
	errmsg(__func__,"opening ram file '%s': %s", ramfile_name, strerror(errno) );
	return 1;
    }
    // init to all zeros
    for ( addr = 0 ; addr < RAMLEN ; addr++ ) {
	data[addr] = 0;
    }
    // read ram file
    linenum = 0;
    addr = 0;
    while ( fgets(buf, BUFLEN-1, ramfile) != NULL ) {
	linenum++;
	cp = buf;
	while ( 1 ) {
	    /* check address - last 2 bytes are reserved for checksum and
	       we allow enough room for 32 bit data, so we need 6 free */
	    if ( addr >= RAMLEN-6 ) {
		errmsg(__func__,"address out of range: %d (0x%x) at %s:%d",
		    addr, addr, ramfile_name, linenum);
		goto cleanup1;
	    }
	    // skip whitespace
	    while ( isspace(*cp) ) cp++;
	    // check for end of line, or start of comment
	    if (( *cp == '\0' ) || ( *cp == '#' )) {
		break;
	    }
	    if ( *cp == '@' ) {
		// read address
		cp++;
		while ( isspace(*cp) ) cp++;
		addr = strtoul(cp, &cp1, 0);
		if ( cp1 == cp ) {
		    errmsg(__func__,"bad char '%c' in address at %s:%d",
			*cp, ramfile_name, linenum);
		    goto cleanup1;
		}
		cp = cp1;
	    } else if ( *cp == 'l' ) {
		// read long (4 byte) data value
		cp++;
		while ( isspace(*cp) ) cp++;
		temp = strtoul(cp, &cp1, 0);
		if ( cp1 == cp ) {
		    errmsg(__func__,"bad char '%c' in long data at %s:%d",
			*cp, ramfile_name, linenum);
		    goto cleanup1;
		}
		cp = cp1;
		data[addr++] = temp >> 24;
		data[addr++] = temp >> 16;
		data[addr++] = temp >> 8;
		data[addr++] = temp;
	    } else if ( *cp == 'w' ) {
		// read word (2 byte) data value
		cp++;
		while ( isspace(*cp) ) cp++;
		temp = strtoul(cp, &cp1, 0);
		if ( cp1 == cp ) {
		    errmsg(__func__,"bad char '%c' in word data at %s:%d",
			*cp, ramfile_name, linenum);
		    goto cleanup1;
		}
		if (temp > 0xFFFF ) {
		    errmsg(__func__,"word value %ud (0x%ux) out of range at %s:%d",
			temp, temp, ramfile_name, linenum);
		    goto cleanup1;
		}
		cp = cp1;
		data[addr++] = temp >> 8;
		data[addr++] = temp;
	    } else {
		// read byte data value
		temp = strtoul(cp, &cp1, 0);
		if ( cp1 == cp ) {
		    errmsg(__func__,"bad char '%c' in byte data at %s:%d",
			*cp, ramfile_name, linenum);
		    goto cleanup1;
		}
		if (temp > 0xFF ) {
		    errmsg(__func__,"byte value %ud (0x%ux) out of range at %s:%d",
			temp, temp, ramfile_name, linenum);
		    goto cleanup1;
		}
		cp = cp1;
		data[addr++] = temp;
	    }
	}
    }
    fclose (ramfile);
    printf("ram file '%s' read successfully\n", argv[1] );
    /* calculate a checksum */
    /* we use a 16 bit variant on Adler32, it is more robust than a
	simple checksum, and simpler to compute than a real CRC */
    checksum1 = 0;
    checksum2 = 0;
    for ( n = 0 ; n < RAMLEN-2 ; n++ ) {
	checksum1 += data[n];
	while ( checksum1 > 251 ) checksum1 -= 251;
	checksum2 += checksum1;
	while ( checksum2 > 251 ) checksum2 -= 251;
    }
    data[RAMLEN-2] = checksum1;
    data[RAMLEN-1] = checksum2;
    printf ( "checksums = %02X %02X\n", checksum1, checksum2 );
#if 1
    /* print the data */
    dp8 = (__u8 *)data;
    n = 0;
    while ( n < RAMLEN ) {
	if (( n & 15 ) == 0 ) {
	    printf ( "%04X: ", n );
	}
	printf ( "%02X ", *(dp8++) );
	n++;
	if (( n & 15 ) == 0 ) {
	    printf ( "\n" );
	}
    }
#endif
    printf("reading bitfile '%s'\n", argv[2] );
    bf = bitfile_read(argv[2]);
    if ( bf == NULL ) {
	printf( "read failed\n" );
	return 1;
    }
    ch = bitfile_find_chunk(bf, 'w', 0);
    if ( ch != NULL ) {
	printf ( "bitfile has %d byte 'w' chunk\n", ch->len );
	if ( ch->len != RAMLEN ) {
	    printf ( "reallocating\n" );
	    dp8 = malloc(RAMLEN);
	    if ( dp8 == NULL ) {
		errmsg(__func__,"malloc failure" );
		goto cleanup2;
	    }
	    free ( ch->body );
	    ch->body = dp8;
	    ch->len = RAMLEN;
	}
	printf ( "copying\n" );
	for ( n = 0 ; n < RAMLEN ; n++ ) {
	    ch->body[n] = data[n];
	}
    } else {
	printf ( "adding 'w' chunk\n" );
	rv = bitfile_add_chunk(bf, 'w', RAMLEN, data);
	if ( rv != 0 ) {
	    errmsg(__func__,"adding 'w' chunk" );
	    goto cleanup2;
	}
    }
    if ( argc == 4 ) {
	dst = argv[3];
    } else {
	dst = argv[2];
    }
    printf("writing bitfile '%s'\n", dst );
    rv = bitfile_write(bf, dst);
    if ( rv != 0 ) {
	printf( "write failed\n" );
	return 1;
    }
    printf ( "done\n" );
    return 0;

cleanup1:
    fclose(ramfile);
    return 1;
cleanup2:
    bitfile_free(bf);
    return 1;
}
