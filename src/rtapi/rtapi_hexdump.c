// rtapi_print_hex, rtapi_hex_dump_to_buffer
// based on Linux kernel lib/hexdump.c -  GPL2 only

/*
 * lib/hexdump.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

#include "config.h"
#include "rtapi.h"
#include "rtapi_hexdump.h"

#define scnprintf snprintf // should carry over scnprintf, vscnprintf too

#include <stdarg.h>		/* va_start and va_end macros */

#ifdef MODULE
#include "rtapi_app.h"

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/export.h>

#else  /* user land */


#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#define hex_asc_lo(x)   hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)   hex_asc[((x) & 0xf0) >> 4]
#define min(x, y) ({                            \
	    typeof(x) _min1 = (x);		\
	    typeof(y) _min2 = (y);		\
	    (void) (&_min1 == &_min2);		\
	    _min1 < _min2 ? _min1 : _min2; })
#endif


// API doc moved to rtapi_hexdump.h

#ifndef MODULE
const char hex_asc[] = "0123456789abcdef";
#endif

void rtapi_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
			      int groupsize, char *linebuf, size_t linebuflen,
			      int ascii)
{
    const __u8 *ptr = buf;
    __u8 ch;
    int j, lx = 0;
    int ascii_column;

    if (rowsize != 16 && rowsize != 32)
	rowsize = 16;

    if (!len)
	goto nil;
    if (len > rowsize)		/* limit to one line at a time */
	len = rowsize;
    if ((len % groupsize) != 0)	/* no mixed size output */
	groupsize = 1;

    switch (groupsize) {
    case 8: {
	const __u64 *ptr8 = buf;
	int ngroups = len / groupsize;

	for (j = 0; j < ngroups; j++)
	    lx += scnprintf(linebuf + lx, linebuflen - lx,
			    "%s%16.16llx", j ? " " : "",
			    (unsigned long long)*(ptr8 + j));
	ascii_column = 17 * ngroups + 2;
	break;
    }

    case 4: {
	const __u32 *ptr4 = buf;
	int ngroups = len / groupsize;

	for (j = 0; j < ngroups; j++)
	    lx += scnprintf(linebuf + lx, linebuflen - lx,
			    "%s%8.8x", j ? " " : "", *(ptr4 + j));
	ascii_column = 9 * ngroups + 2;
	break;
    }

    case 2: {
	const __u16 *ptr2 = buf;
	int ngroups = len / groupsize;

	for (j = 0; j < ngroups; j++)
	    lx += scnprintf(linebuf + lx, linebuflen - lx,
			    "%s%4.4x", j ? " " : "", *(ptr2 + j));
	ascii_column = 5 * ngroups + 2;
	break;
    }

    default:
	for (j = 0; (j < len) && (lx + 3) <= linebuflen; j++) {
	    ch = ptr[j];
	    linebuf[lx++] = hex_asc_hi(ch);
	    linebuf[lx++] = hex_asc_lo(ch);
	    linebuf[lx++] = ' ';
	}
	if (j)
	    lx--;

	ascii_column = 3 * rowsize + 2;
	break;
    }
    if (!ascii)
	goto nil;

    while (lx < (linebuflen - 1) && lx < (ascii_column - 1))
	linebuf[lx++] = ' ';
    for (j = 0; (j < len) && (lx + 2) < linebuflen; j++) {
	ch = ptr[j];
	linebuf[lx++] = (isascii(ch) && isprint(ch)) ? ch : '.';
    }
 nil:
    linebuf[lx++] = '\0';
}


void rtapi_print_hex_dump(int level, int prefix_type,
			  int rowsize, int groupsize,
			  const void *buf, size_t len, int ascii,
			  const char *fmt, ...)
{
    const __u8 *ptr = buf;
    int i, linelen, remaining = len;
    unsigned char linebuf[32 * 3 + 2 + 32 + 1];
    unsigned char prefix_str[100];
    va_list args;

    va_start(args, fmt);
    rtapi_vsnprintf((char *)prefix_str, sizeof(prefix_str), fmt, args);
    va_end(args);

    if (rowsize != 16 && rowsize != 32)
	rowsize = 16;

    for (i = 0; i < len; i += rowsize) {
	linelen = min(remaining, rowsize);
	remaining -= rowsize;

	rtapi_hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				 (char *) linebuf, sizeof(linebuf), ascii);

	switch (prefix_type) {
	case RTAPI_DUMP_PREFIX_ADDRESS:
	    rtapi_print_msg(level, "%s%p: %s\n",
			    prefix_str, ptr + i, linebuf);
	    break;
	case RTAPI_DUMP_PREFIX_OFFSET:
	    rtapi_print_msg(level,"%s%.8x: %s\n", prefix_str, i, linebuf);
	    break;
	default:
	    rtapi_print_msg(level, "%s%s\n", prefix_str, linebuf);
	    break;
	}
    }
}


#ifdef RTAPI
EXPORT_SYMBOL(rtapi_print_hex_dump);
EXPORT_SYMBOL(rtapi_hex_dump_to_buffer);
#endif
