// rtapi_print_hex, hex_dump_to_buffer
// based on Linux kernel lib/hexdump.c -  GPL2 only

/*
 * lib/hexdump.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#ifndef RTAPI_HEXDUMP_H
#define RTAPI_HEXDUMP_H

#include "rtapi.h"

RTAPI_BEGIN_DECLS

/***********************************************************************
*                      HEX DUMP FUNCTIONS                              *
************************************************************************/
#define RTAPI_DUMP_PREFIX_ADDRESS 1
#define RTAPI_DUMP_PREFIX_OFFSET 2

/**
 * rtapi_hex_dump_to_buffer - convert a blob of data to "hex ASCII" in memory
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 * @rowsize: number of bytes to print per line; must be 16 or 32
 * @groupsize: number of bytes to print at a time (1, 2, 4, 8; default = 1)
 * @linebuf: where to put the converted data
 * @linebuflen: total size of @linebuf, including space for terminating NUL
 * @ascii: include ASCII after the hex output
 *
 * rtapi_hex_dump_to_buffer() works on one "line" of output at a time, i.e.,
 * 16 or 32 bytes of input data converted to hex + ASCII output.
 *
 * Given a buffer of u8 data, rtapi_hex_dump_to_buffer() converts the input data
 * to a hex + ASCII dump at the supplied memory location.
 * The converted output is always NUL-terminated.
 *
 * E.g.:
 *   rtapi_hex_dump_to_buffer(frame->data, frame->len, 16, 1,
 *		 	      linebuf, sizeof(linebuf), true);
 *
 * example output buffer:
 * 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f  @ABCDEFGHIJKLMNO
 */
void rtapi_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
			      int groupsize, char *linebuf, size_t linebuflen,
			      int ascii);

/**
 * rtapi_print_hex_dump - print a text hex dump to syslog for a binary blob of data
 * @level: RTAPI log level (RTAPI_MSG_XXX)
 * @prefix_str: string to prefix each line with;
 *  caller supplies trailing spaces for alignment if desired
 * @prefix_type: controls whether prefix of an offset, address, or none
 *  is printed (%DUMP_PREFIX_OFFSET, %DUMP_PREFIX_ADDRESS, %DUMP_PREFIX_NONE)
 * @rowsize: number of bytes to print per line; must be 16 or 32
 * @groupsize: number of bytes to print at a time (1, 2, 4, 8; default = 1)
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 * @ascii: include ASCII after the hex output
 *
 * Given a buffer of u8 data, rtapi_print_hex_dump() prints a hex + ASCII dump
 * to the RTAPI log at the specified log level.
 *
 * rtapi_print_hex_dump() works on one "line" of output at a time, i.e.,
 * 16 or 32 bytes of input data converted to hex + ASCII output.
 * rtapi_print_hex_dump() iterates over the entire input @buf, breaking it into
 * "line size" chunks to format and print.
 *
 * E.g.:
 *    rtapi_print_hex_dump(RTAPI_MSG_ALL, RTAPI_DUMP_PREFIX_OFFSET,
 *                         16, 1, frame->data, frame->len, true,
 *                         "cant parse command from %s:", origin);
 * Example output using %RTAPI_DUMP_PREFIX_OFFSET and 1-byte mode:
 * 0009ab42: 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f  @ABCDEFGHIJKLMNO
 * Example output using %RTAPI_DUMP_PREFIX_ADDRESS and 4-byte mode:
 * ffffffff88089af0: 73727170 77767574 7b7a7978 7f7e7d7c  pqrstuvwxyz{|}~.
 */
void rtapi_print_hex_dump(int level, int prefix_type,
			  int rowsize, int groupsize,
			  const void *buf, size_t len, int ascii,
			  const char *fmt, ...)
    __attribute__((format(printf,8,9)));



RTAPI_END_DECLS

#endif /* RTAPI_HEXDUMP_H */
