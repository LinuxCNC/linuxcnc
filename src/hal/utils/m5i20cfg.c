/******************************************************************************
 *
 * Copyright (C) 2005 Peter C. Wallace <pcw AT mesanet DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 ******************************************************************************
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

/*---------------------------------------------------------------------------*/
/*
   sc5i20l.c

   Demonstration program for configuration of the Mesa Electronics 5I20 I/O
	 card/Linux.

   Version 0.2, Thursday February 24, 2005 -- 12:12:48.

   Note: This program must be run as root, as it accesses system I/O ports
	 directly.  (No device driver is used.)  Ugly.  Dangerous.  X86-only.
*/
/*---------------------------------------------------------------------------*/
/*
   Compiler: gcc

   Tabs: 4

   Compile with "gcc -O2 sc5i20l.c -o sc5i20"
*/
/*---------------------------------------------------------------------------*/
/*
   Revision history.

   1) Version 0.1, Monday February 21, 2005 -- 20:12:17.

	  Code frozen for version 0.1.


   2) Version 0.2, Thursday February 24, 2005 -- 12:12:48.

      Changes from version 0.1:
	  1) Uses of off_t replaced with unsigned long for consistency.
	  2) <sys/types.h> is no longer included.
*/
/*---------------------------------------------------------------------------*/
#define _GNU_SOURCE /* getline() */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/io.h>
#include <sys/stat.h>

/*---------------------------------------------------------------------------*/

/* Assorted #defines.
*/
#define PROGNAME      "SC5I20L"
#define SIGNONMESSAGE "\n"PROGNAME" version "PROGVERSION"\n"
#define PROGVERSION   "0.2"

/* I/O registers.
*/
#define R_5I20CONTROLNSTATUS 0x0054 /* 32-bit control/status register. */

/* PCI base register indices.
*/
#define BRNUM_5I20LCLCFGIO 1 /* Local configuration I/O base address register number. */
#define BRNUM_5I20IO16     2 /* 16-bit I/O window to FPGA. (Base address register 2.) */
#define BRNUM_5I20IO32     3 /* 32-bit I/O window to FPGA. (Base address register 3.) */
#define BRNUM_5I20CFGMEM   4 /* Configuration memory window. */

/* Masks for R_5I20CONTROLNSTATUS.
*/
#define P_5I20CFGPRGM         P_FPGAIOControlnStatus32
#define B_5I20CFGPRGM         26 /* Program enable. (Output.) */
#define M_5I20CFGPRGM         (0x1 << B_5I20CFGPRGM)
#define M_5I20CFGPRGMDEASSERT (0x0 << B_5I20CFGPRGM) /* Reset the FPGA. */
#define M_5I20CFGPRGMASSERT   (0x1 << B_5I20CFGPRGM) /* Begin programming. */

#define P_5I20CFGRW           P_FPGAIOControlnStatus32
#define B_5I20CFGRW           23 /* Data direction control. (Output.) */
#define M_5I20CFGRW           (0x1 << B_5I20CFGRW)
#define M_5I20CFGWRITEENABLE  (0x0 << B_5I20CFGRW) /* From CPU. */
#define M_5I20CFGWRITEDISABLE (0x1 << B_5I20CFGRW) /* To CPU. */

#define P_5I20LEDONOFF        P_FPGAIOControlnStatus32
#define B_5I20LEDONOFF        17 /* Red LED control. (Output.) */
#define M_5I20LEDONOFF        (0x1 << B_5I20LEDONOFF)
#define M_5I20LEDON           (0x0 << B_5I20LEDONOFF)
#define M_5I20LEDOFF          (0x1 << B_5I20LEDONOFF)

#define P_5I20PRGMDUN         P_FPGAIOControlnStatus32
#define B_5I20PRGMDUN         11 /* Programming-done flag. (Input.) */
#define M_5I20PRGMDUN         (0x1 << B_5I20PRGMDUN)
#define M_5I20PRGMDUNNOW      (0x1 << B_5I20PRGMDUN)

/* Paths.
*/
#define PATH_PROC_BUS_PCI "/proc/bus/pci"

/* Card I.D.
*/
#define VENDORID5I20 0x10B5 /* PCI vendor I.D. */
#define DEVICEID5I20 0x9030 /* PCI device I.D. */

/* Exit codes.
*/
#define EC_OK    0   /* Exit OK. */
#define EC_BADCL 100 /* Bad command line. */
#define EC_USER  101 /* Invalid input. */
#define EC_HDW   102 /* Some sort of hardware failure on the 5I20. */
#define EC_FILE  103 /* File error of some sort. */
#define EC_SYS   104 /* Beyond our scope. */
#define EC_NTRNL 105 /* Internal error. */


/*---------------------------------------------------------------------------*/

/* Assorted typedefs.
*/

typedef unsigned char  unsigned8,  ubyte ;
typedef unsigned short unsigned16, uword ;
typedef unsigned long unsigned32, udword ;

/*---------------------------------------------------------------------------*/

/* Local external variables.
*/
static unsigned long ImageLen ; /* Number of bytes in device image portion of file. */

static uword P_FPGAData, P_FPGAIOControlnStatus32 ;

static void (*DataOutFuncPtr)(unsigned thebyte) ;

static FILE *ConfigFile ;

static int PrintConfig = 0;

static const char ProgNameMsg[] = { PROGNAME } ;

static FILE *InfoStr;
static FILE *ErrStr;
static FILE *OutStr;


/*---------------------------------------------------------------------------*/

/* Function: cleanup
   Purpose: Release external resources, etc.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void cleanup(void)

{
	iopl(0) ;
}

/*---------------------------------------------------------------------------*/

/* Function: fatalerror
   Purpose: Display a fatal error message and exit.
   Used by: Local functions.
   Returns: Never.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void fatalerror(int exitcode, const char *fmt, ...)

{
	va_list vp ;


	cleanup() ; /* Maybe free up allocated system resources. */

	fflush(InfoStr) ; /* Make sure we don't get normal informational messages mixed in with error messages. */
	fflush(OutStr) ;
	va_start(vp, fmt) ;
	fprintf(ErrStr, "\n\n\a%s:\n", ProgNameMsg) ;
	vfprintf(ErrStr, fmt, vp) ;
	putc('\n', ErrStr) ;
	fflush(ErrStr) ;

	exit(exitcode) ;
}

/*---------------------------------------------------------------------------*/

/* Function: ledon
   Purpose: Turn on the 5I20's programming LED.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void ledon(void)

{
	outl(((inl(P_5I20LEDONOFF) & ~M_5I20LEDONOFF) | M_5I20LEDON), P_5I20LEDONOFF) ;
}

/*---------------------------------------------------------------------------*/

/* Function: ledoff
   Purpose: Turn off the 5I20's programming LED.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void ledoff(void)

{
	outl(((inl(P_5I20LEDONOFF) & ~M_5I20LEDONOFF) | M_5I20LEDOFF), P_5I20LEDONOFF) ;
}

/*---------------------------------------------------------------------------*/

/* Function: writeenable
   Purpose: Enable configuration data transfer from the CPU to the 5I20.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void writeenable(void)

{
	outl(((inl(P_5I20CFGRW) & ~M_5I20CFGRW) | M_5I20CFGWRITEENABLE), P_5I20CFGRW) ;
	ledon() ;
}

/*---------------------------------------------------------------------------*/

/* Function: writedisable
   Purpose: Disable configuration data transfer from the CPU to the 5I20.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void writedisable(void)

{
	outl(((inl(P_5I20CFGRW) & ~M_5I20CFGRW) | M_5I20CFGWRITEDISABLE), P_5I20CFGRW) ;
	ledoff() ;
}

/*---------------------------------------------------------------------------*/

/* Function: programenable
   Purpose: Enable 5I20 programming.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void programenable(void)

{
	outl(((inl(P_5I20CFGPRGM) & ~M_5I20CFGPRGM) | M_5I20CFGPRGMASSERT), P_5I20CFGPRGM) ;
	writeenable() ;
}

/*---------------------------------------------------------------------------*/

/* Function: programdisable
   Purpose: Disable 5I20 programming/reset the FPGA.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void programdisable(void)

{
	outl(((inl(P_5I20CFGPRGM) & ~M_5I20CFGPRGM) | M_5I20CFGPRGMDEASSERT), P_5I20CFGPRGM) ;
	writedisable() ;
}

/*---------------------------------------------------------------------------*/

/* Function: outdata8wswap
   Purpose: Send a byte to the 5I20 data port with bits swapped.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void outdata8wswap(unsigned thebyte)

{
	static const unsigned char swaptab[256] =
	{
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
		0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
	} ;


	if(PrintConfig)
	{
	    fprintf(OutStr, "0x%02x, ", swaptab[thebyte]); 
	}
	else
	{
	    outb(swaptab[thebyte], P_FPGAData) ;
	}
}

/*---------------------------------------------------------------------------*/

/* Function: outdata8noswap
   Purpose: Send a byte to the 5I20 data port with no bit swapping.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void outdata8noswap(unsigned thebyte)

{
    if(PrintConfig)
    {
	fprintf(OutStr, "0x%02x, ", thebyte); 
    }
    else
    {
	outb(thebyte, P_FPGAData) ;
    }
}

/*---------------------------------------------------------------------------*/

/* Function: findpcicard
   Purpose: Try to find the specified PCI card.  If found set parameters.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void findpcicard(unsigned cardnum)

{
	udword vendordev, busdevfunc ;
	udword bases[6] ;

	int numfields ;

	unsigned
		i, numlines ;

	size_t linelen ;

	FILE *f ;

	char namebuf[256 + 1] ;
	char *lineptr ;


	if(snprintf(namebuf, sizeof(namebuf), "%s/devices", PATH_PROC_BUS_PCI) >= sizeof(namebuf))
	{
		fatalerror(EC_NTRNL, "File name too long: %s...", PATH_PROC_BUS_PCI) ;
	}
	if((f = fopen(namebuf, "r")) == 0)
	{
		/* (Shouldn't happen.)
		*/
		fatalerror(EC_SYS, "File not found: %s", namebuf) ;
	}

	lineptr = 0 ;
	linelen = 0 ;
	numlines = 0 ;
	i = cardnum ;
	for( ; ; )
	{
		if(getline(&lineptr, &linelen, f) < 0)
		{
			free(lineptr) ;
			if(numlines == 0)
			{
				fatalerror(EC_SYS, "Unexpected end of file in %s", namebuf) ;
			}
			else
			{
				break ;
			}
		}
		++numlines ;

		/* Bus/dev/func vendor/device IRQ base0 base1 base2 base3 base4 base5...
		*/
		numfields = sscanf(lineptr, "%lx %lx %*x  %lx %lx %lx %lx %lx %lx",
						   &busdevfunc, &vendordev,
						   &bases[0], &bases[1], &bases[2], &bases[3], &bases[4], &bases[5]) ;
		if(numfields < 8)
		{
			free(lineptr) ;
			fatalerror(EC_SYS, "Incomplete PCI device information found: %s", namebuf) ;
		}
		if(vendordev == ((VENDORID5I20 << 16) | DEVICEID5I20))
		{
			if(i++ != cardnum)
			{
				continue ;
			}

			fclose(f) ;
			free(lineptr) ;

			fprintf(InfoStr, "\nUsing PCI device at bus %lu/device %lu/function %lu.",
				   (busdevfunc >> 8), ((busdevfunc >> 3) & 0x1F), (busdevfunc & 0x07)) ;
			P_FPGAIOControlnStatus32 = ((uword)bases[BRNUM_5I20LCLCFGIO] & ~0x3) ;
			fprintf(InfoStr, "\n\nPLX 9030 configuration I/O base port: 0x%04hX", P_FPGAIOControlnStatus32) ;
			P_FPGAIOControlnStatus32 += R_5I20CONTROLNSTATUS ;
			fprintf(InfoStr, "\nPLX 9030 control/status port:         0x%04hX", P_FPGAIOControlnStatus32) ;
			P_FPGAData = ((uword)bases[BRNUM_5I20IO16] & ~0x3) ;
			fprintf(InfoStr, "\nPLX 9030 16-bit I/O base:             0x%04hX", P_FPGAData) ;
			fprintf(InfoStr, "\nPLX 9030 32-bit I/O base:             0x%04hX", ((uword)bases[BRNUM_5I20IO32] & ~0x3)) ;
			fputc('\n', InfoStr) ;
			fflush(InfoStr) ;

			/* Enable direct access to the I/O port range.

			   Don't try this without adult supervision!
			*/
			if(iopl(3) < 0) /* Danger, Will Robinson!  Danger!  Danger! */
			{
				fatalerror(EC_SYS, "Can't enable direct port access.") ;
			}

			return ;
		}
	}

	fatalerror(EC_USER, "Can't find 5I20 card number %u.", cardnum) ;
}

/*---------------------------------------------------------------------------*/

/* Function: sendcompletionclocks
   Purpose: Send completion clocks to finish up programming.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void sendcompletionclocks(void)

{
	unsigned count ;


	writedisable() ;

	/* Send configuration completion clocks.  (6 should be enough, but we send
		 lots for good measure.)
	*/
	for(count = 24 ; count != 0 ; --count)
	{
		outb(-1, P_FPGAData) ;
	}
}

/*---------------------------------------------------------------------------*/

/* Function: programdunq
   Purpose: Return whether or not the FPGA is done programming.
   Used by: Local functions.
   Returns: Yes/no status.
   Notes:
	 -Failure to become DONE may be caused by an attempt to load an invalid
		FPGA code image, or an I/O address conflict.
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static int programdunq(void)

{
	return (inl(P_5I20PRGMDUN) & M_5I20PRGMDUN) == M_5I20PRGMDUNNOW ;
}

/*---------------------------------------------------------------------------*/

/* Function: wait4dun
   Purpose: Wait for the FPGA to finish programming.
   Used by: Local functions.
   Returns: Done/timeout status.
   Notes:
	 -Failure to become DONE may be caused by an attempt to load an invalid
		FPGA code image, or an I/O address conflict.
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static signed wait4dun(void)

{
	clock_t then ;

	unsigned waitcount ;


	/* Wait at least long enough for the FPGA to finish its post-programming
		 tomfoolery.  This time is generally masked by software overhead, but the
		 programmer needs to be aware of it.
	*/
	then = (clock() + ((CLOCKS_PER_SEC / 10000) + 1)) ; /* 100 uS. should be enough. */
	while(clock() < then)
	{
		;
	}

	/* The DONE bit should now indicate end of programming.
	*/
	if(programdunq())
	{
		/* Programming seems to be complete; make sure the state sticks and we're not
			 seeing some sort of I/O conflict-related foo.
		*/
		for(waitcount = 100 ; waitcount != 0 ; --waitcount)
		{
			if(!programdunq())
			{
				return -1 ; /* (Shouldn't.) */
			}
		}

		return 0 ; /* Success. */
	}
	else
	{
		return -1 ; /* Failure -- didn't become DONE. */
	}
}

/*---------------------------------------------------------------------------*/

/* Function: delay100us
   Purpose: Wait for 100 uS.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void delay100us(void)

{
	clock_t then ;

	then = (clock() + ((CLOCKS_PER_SEC / 10000) + 1)) ;
	while(clock() < then)
	{
		;
	}
}

/*---------------------------------------------------------------------------*/

/* Function: filelengthq
   Purpose: Determine the length of the specified file.
   Used by: Local functions.
   Returns: The file length, or -1 on failure.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
	 2) Thursday February 24, 2005 -- 12:12:48.
	    -We now return unsigned long instead of off_t.
*/

static unsigned long filelengthq(FILE *f)

{
	struct stat fileinfo ;


	fstat(fileno(f), &fileinfo) ;
	return fileinfo.st_size ;
}

/*---------------------------------------------------------------------------*/

/* Function: saybytesfromfile
   Purpose: Send the bytes from the specified offset in the specified open
     file to standard out.
   Used by: Local functions.
   Returns: The number of bytes read.
   Notes:
     -It is assumed that numbytes includes the trailing '\0'.
     -The file position is not restored to its original value.
     -Don't try to output 0 bytes.
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static size_t saybytesfromfile(FILE *f, long fromoffset, size_t numbytes, FILE *str)

{
	int c ;

	size_t len = 0 ;


	if(fseek(f, fromoffset, SEEK_SET) != 0)
	{
		return 0 ;
	}
	while(numbytes-- != 0)
	{
		if((c = getc(f)) == EOF)
		{
			break ;
		}
		++len ;
		if(c == '\0')
		{
			break ;
		}
		fputc(c, str) ;
	}
	return len ;
}

/*---------------------------------------------------------------------------*/

/* Function: readbytesfromfile
   Purpose: Read the specified number of bytes from the specified offset in
	 the specified open file to the specified address.
   Used by: Local functions.
   Returns: The number of bytes read.
   Notes:
     -The file position is not restored to its original value.
     -Don't try to read 0 bytes.
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static size_t readbytesfromfile(FILE *f, long fromoffset, size_t numbytes, ubyte *bufptr)

{
	if(fseek(f, fromoffset, SEEK_SET) != 0)
	{
		return 0 ;
	}
	return fread(bufptr, 1, numbytes, f) ;
}

/*---------------------------------------------------------------------------*/

/* Function: sayfileinfo
   Purpose: Display some bytes from our configuration file on standard out.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
     -It is assumed that numbytes includes the trailing '\0'.
     -The file position is not restored to its original value.
     -Don't try to output 0 bytes.
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void sayfileinfo(signed long *infobaseptr, FILE *str)

{
	size_t len ;

	ubyte b[2] ;


	if(readbytesfromfile(ConfigFile, *infobaseptr, sizeof(b), b) != sizeof(b))
	{
		ioerror:
		fatalerror(EC_FILE, "File I/O error.  Unexpected end of file?") ;
	}
	len = ((b[0] << 8) | b[1]) ; /* (Bigendian.) */
	if(saybytesfromfile(ConfigFile, (*infobaseptr + sizeof(b)), len, str) != len)
	{
		goto ioerror ;
	}
	*infobaseptr += (sizeof(b) + (signed long)len + 1) ; /* Skip over length field, text field, and tag. */
}

/*---------------------------------------------------------------------------*/

/* Function: selectfiletype
   Purpose: Set up for processing the input file.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void selectfiletype(void)

{
	ubyte b[14] ;
	FILE *bitstr;

	bitstr = (PrintConfig)? OutStr: InfoStr;

	/* Read in the interesting parts of the file header.
	*/
	if(readbytesfromfile(ConfigFile, 0, sizeof(b), b) != sizeof(b))
	{
		ioerror:
		fatalerror(EC_FILE, "File I/O error.  Unexpected end of file?") ;
	}

	/* Figure out what kind of file we have.
	*/
	if((b[0] == 0x00) && (b[1] == 0x09) && (b[11] == 0x00) && (b[12] == 0x01) && (b[13] == 'a'))
	{
		/* Looks like a .BIT file.
		*/
		signed long base ;


		fprintf(InfoStr, "\nLooks like a .BIT file:") ;
		base = 14 ; /* Offset of design name length field. */

		/* Display file particulars.
		*/
		if(PrintConfig) fprintf(bitstr, "/*");
		fprintf(bitstr, "\n  Design name:          ") ; sayfileinfo(&base, bitstr) ;
		fprintf(bitstr, "\n  Part I.D.:            ") ; sayfileinfo(&base, bitstr) ;
		fprintf(bitstr, "\n  Design date:          ") ; sayfileinfo(&base, bitstr) ;
		fprintf(bitstr, "\n  Design time:          ") ; sayfileinfo(&base, bitstr) ;

		if(readbytesfromfile(ConfigFile, base, 4, b) != 4)
		{
			goto ioerror ;
		}
		ImageLen = (((unsigned long)b[0] << 24) |
					((unsigned long)b[1] << 16) |
					((unsigned long)b[2] << 8) |
					(unsigned long)b[3]) ;
		fprintf(bitstr, "\n  Configuration length: %lu bytes", ImageLen) ;
		if(PrintConfig) fprintf(bitstr, "\n*/\n");

		DataOutFuncPtr = outdata8wswap ; /* We have to swap bits in .BIT files. */

		/* We leave the file position set to the next byte in the file, which should
		     be the first byte of the body of the data image.
		*/
	}
	else if((b[0] == 0xFF) && (b[4] == 0x55) && (b[5] == 0x99) && (b[6] == 0xAA) && (b[7] == 0x66))
	{
		/* Looks like a PROM file.
		*/
		fprintf(InfoStr, "\nLooks like a PROM file:") ;
		DataOutFuncPtr = outdata8noswap ; /* No bit swap in PROM files. */

		ImageLen = filelengthq(ConfigFile); ;
		if(ImageLen < 0)
		{
			seekerror:
			fatalerror(EC_SYS, "File seek error.") ;
		}
		fprintf(InfoStr, "\n  Configuration length: %lu bytes", ImageLen) ;

		/* PROM file data starts at offset 0, so we have to back up.
		*/
		if(fseek(ConfigFile, 0, SEEK_SET) != 0)
		{
			goto seekerror ;
		}
	}
	else
	{
		/* It isn't something we know about.
		*/
		fatalerror(EC_FILE, "Unknown file type.") ;
	}

	fputc('\n', InfoStr) ;
	fflush(InfoStr) ; /* (Make sure the user can see all output to date.) */
}

/*---------------------------------------------------------------------------*/

/* Function: signon
   Purpose: Display the signon message.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static void signon(void)

{
	fprintf(InfoStr, "%s", SIGNONMESSAGE) ;
	fprintf(InfoStr, "Mesa Electronics 5I20 configuration utility for Linux.\n") ;
	fflush(InfoStr) ;
}

/*---------------------------------------------------------------------------*/

/* Function: pcl
   Purpose: Collect parameters from the command line.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

static const char FileNameInfo[] = "\n"
"filename.ext specifies the name of the configuration file to be loaded." ;

static const char CardInfo[] = "\n"
"card is the number of the PCI card.  (For use with multiple 5I20 cards.)" ;

static const char PurposeInfo[] = "\n"
"Purpose: 5I20 configuration upload." ;


static void pcl(unsigned argc, char *argv[])

{
	unsigned cardnum = 0 ;


	if((argc != 2) && (argc != 3))
	{
		fprintf(InfoStr, "\n%s program usage:\n%s filename.ext [<card>]", ProgNameMsg, ProgNameMsg) ;
		fprintf(InfoStr, "\nWhere:") ;
		fprintf(InfoStr, "%s", FileNameInfo) ;
		fprintf(InfoStr, "%s", CardInfo) ;
		fprintf(InfoStr, "%s", PurposeInfo) ;
		fputc('\n', InfoStr) ;
		fflush(InfoStr) ;

		exit(EC_BADCL) ;
	}

	if((ConfigFile = fopen(argv[1], "rb")) == 0) /* (Binary mode.) */
	{
		fatalerror(EC_USER, "File not found: %s", argv[1]) ;
	}

	if(argc == 2)
	{
	    PrintConfig = 1;
	}
	else
	{
	    if(sscanf(argv[2], "%u", &cardnum) != 1)
	    {
		    badcardnum:
		    fatalerror(EC_USER, "Invalid card number: %s", argv[2]) ;
	    }
	    if(cardnum > 15)
	    {
		    goto badcardnum ;
	    }

	    findpcicard(cardnum) ;
	}

	selectfiletype() ;
}

/*---------------------------------------------------------------------------*/

/* Function: programfpga
   Purpose: Initialize the FPGA with the contents of file ConfigFile.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

#define FORMATSTRING "%-6lu"

static void programfpga(void)

{
	signed long bytesleft ;
	unsigned long numbytes ;

	size_t count ;

	static ubyte filebuffer[16384] ;
	ubyte *bptr ;


	/* Enable programming.
	*/
	fprintf(InfoStr, "\nProgramming...\n") ;
	fflush(InfoStr) ;

	if(PrintConfig)
	{
	    fprintf(OutStr, "\nstatic unsigned char\n");
	    fprintf(OutStr, "fpgaConfig[] = {");
	}
	else
	{
	    programdisable() ; /* Reset the FPGA. */
	    if(programdunq())
	    {
		    /* Note that if we see DONE at the start of programming, it's most likely due
			     to an attempt to access the FPGA at the wrong I/O location.
		    */
		    fatalerror(EC_HDW, "<DONE> status bit indicates busy at start of programming.") ;
	    }
	    programenable() ;

	    /* Delay for at least 100 uS. to allow the FPGA to finish its reset
		     sequencing.  (In reality, the time taken for the initial file access
		     makes this this delay redundant.)
	    */
	    delay100us() ;
	}

	/* Program the card.
	*/
	numbytes = 0 ;
	bytesleft = ImageLen ;
	for( ; ; )
	{
		/* Write the file to the FPGA.
		*/
		errno = 0 ;
		count = fread(filebuffer, 1, sizeof(filebuffer), ConfigFile) ;
		if(count == 0)
		{
			/* 0 bytes could be end of file or error; must check.
			*/
			if(errno != 0)
			{
				if(!PrintConfig) programdisable() ;

				fatalerror(EC_SYS, "File I/O error.") ;
			}
			else
			{
				fprintf(InfoStr, FORMATSTRING, 0L) ; fflush(InfoStr) ;

				break ; /* Done. */
			}
		}
		else
		{
			/* Write the block to the FPGA data port.
			*/
			fprintf(InfoStr, FORMATSTRING "\r", bytesleft) ; fflush(InfoStr) ;
			bytesleft -= (signed long)count ;

			bptr = filebuffer ;
			do
			{
				if(PrintConfig && ((numbytes & 0x7) == 0)) fprintf(OutStr, "\n    ");

				DataOutFuncPtr(*bptr++) ;

				numbytes++;
			} while(--count != 0) ;
		}
	}

	fclose(ConfigFile) ;

	/* Wait for completion of programming.
	*/
	if(!PrintConfig && (wait4dun() < 0))
	{
		fatalerror(EC_HDW, "Error: Not <DONE>; programming not completed.") ;
	}

	if(PrintConfig)
	{
	    fprintf(OutStr, "\n};\n");
	    fflush(OutStr);
	}
	else
	{
	    /* Send configuration completion clocks.
	    */
	    sendcompletionclocks() ;
	}

	fprintf(InfoStr, "\nSuccessfully programmed %lu bytes.", numbytes) ;
	fflush(InfoStr) ;
}

/*---------------------------------------------------------------------------*/

/* Function: main
   Purpose: Program entry point.
   Used by: Program launcher.
   Returns: Exit status.
   Notes:
   Revision history:
	 1) Monday February 21, 2005 -- 20:12:17.
*/

int main(int argc, char *argv[])

{
	InfoStr = stderr;
	ErrStr = stderr;
	OutStr = stdout;

	signon() ;
	pcl(argc, argv) ;
	programfpga() ;
	cleanup() ;
	fprintf(InfoStr, "\n\nFunction complete.\n") ; fflush(InfoStr) ;

	return 0 ;
}

/*---------------------------------------------------------------------------*/
