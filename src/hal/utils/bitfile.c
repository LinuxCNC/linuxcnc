/*************************************************************************
*
* bitfile - a library reading and writing Xilinx bitfiles
*
* Copyright (C) 2007 John Kasunich (jmkasunich at fastmail dot fm)
*
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

************************************************************************/

#define _GNU_SOURCE /* getline() */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/types.h>
#include "bitfile.h"

#define XILINX_CHUNKS "abcde"

static const unsigned char header[BITFILE_HEADERLEN] = {
    0x00, 0x09,
    0x0f, 0xf0, 0x0f, 0xf0,
    0x0f, 0xf0, 0x0f, 0xf0,
    0x00, 0x00, 0x01
};

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

struct bitfile *bitfile_new(void)
{
    struct bitfile *bf;
    int n;

    bf = (struct bitfile *)malloc(sizeof(struct bitfile));
    if ( bf == NULL ) {
	errmsg(__func__,"malloc failure");
	return NULL;
    }
    bf->filename = NULL;
    /* standard header */
    for ( n = 0 ; n < BITFILE_HEADERLEN ; n++ ) {
	bf->header[n] = header[n];
    }
    for ( n = 0 ; n < BITFILE_MAXCHUNKS ; n++ ) {
	bf->chunks[n].tag = '\0';
	bf->chunks[n].len = 0;
	bf->chunks[n].body = NULL;
    }
    bf->num_chunks = 0;
    return bf;
}

void bitfile_free(struct bitfile *bf)
{
    int n;

    if (bf->filename != NULL) {
        free(bf->filename);
    }

    for ( n = 0 ; n < BITFILE_MAXCHUNKS ; n++ ) {
	if ( bf->chunks[n].body != NULL ) {
	    free(bf->chunks[n].body);
	}
    }
    free(bf);
}

static int read_chunk(int fd, struct bitfile_chunk *ch)
{
    int len_len, rv;
    __u8 lenbuf[4];

    /* read tag */
    rv = read(fd, &(ch->tag), 1);
    if ( rv != 1 ) {
	/* end of file is not an error */
	return 1;
    }
    if ( strchr(BITFILE_SMALLCHUNKS, ch->tag) != NULL ) {
	/* its a small chunk, 2 byte size */
	len_len = 2;
    } else {
	/* regular chunk, 4 byte size */
	len_len = 4;
    }
    /* read length */
    rv = read(fd, lenbuf, len_len);
    if ( rv != len_len ) {
	errmsg(__func__,"reading length: %s", strerror(errno));
	return -1;
    }
    /* compute size (note - the format uses big-endian layout */
    if ( len_len == 4 ) {
	ch->len = (int)( ((__u32)(lenbuf[0]) << 24 ) |
			 ((__u32)(lenbuf[1]) << 16 ) |
			 ((__u32)(lenbuf[2]) << 8 ) |
			  (__u32)(lenbuf[3]) );
    } else {
	ch->len = (int)( ((__u32)(lenbuf[0]) << 8 ) |
			  (__u32)(lenbuf[1]) );
    }
    /* allocate space for chunk content */
    ch->body = (__u8 *)malloc(ch->len);
    if ( ch->body == NULL ) {
	errmsg(__func__,"malloc failure");
	return -1;
    }
    /* read chunk content */
    rv = read(fd, ch->body, ch->len);
    if ( rv != ch->len ) {
	errmsg(__func__,"reading content: %s", strerror(errno));
	return -1;
    }
    return 0;
}

struct bitfile *bitfile_read(char *fname)
{
    struct bitfile *bf;
    int fd, rv, n;
    unsigned char c;

    /* create the struct */
    bf = bitfile_new();
    if ( bf == NULL ) {
	errmsg(__func__,"creating struct");
	return NULL;
    }
    /* open the file */
    fd = open(fname, O_RDONLY);
    if ( fd < 0 ) {
	errmsg(__func__,"opening file: %s", strerror(errno));
	goto cleanup0;
    }
    /* first BITFILE_HEADERLEN bytes are a header */
    rv = read(fd, bf->header, BITFILE_HEADERLEN);
    if ( rv < BITFILE_HEADERLEN ) {
	errmsg(__func__,"reading header: %s", strerror(errno));
	goto cleanup1;
    }
    /* test header */
    for ( n = 0 ; n < BITFILE_HEADERLEN ; n++ ) {
	if ( bf->header[n] != header[n] ) {
	    break;
	}
    }
    if ( n != BITFILE_HEADERLEN ) {
	errmsg(__func__,"header mismatch, '%s' is not a bitfile?", fname);
	goto cleanup1;
    }
    /* read chunks */
    while ( bf->num_chunks < BITFILE_MAXCHUNKS ) {
	rv = read_chunk(fd, &(bf->chunks[bf->num_chunks]));
	if ( rv < 0 ) {
	    errmsg(__func__,"reading chunk %d", bf->num_chunks);
	    goto cleanup1;
	}
	if ( rv > 0 ) {
	    /* end of file */
	    break;
	}
	bf->num_chunks++;
    }
    /* is anything left in the file? */
    rv = read(fd, &c, 1);
    if ( rv == 1 ) {
	errmsg(__func__,"more than %d chunks", BITFILE_MAXCHUNKS);
	goto cleanup1;
    }

    // save the filename
    bf->filename = strdup(fname);
    if (bf->filename == NULL) {
        errmsg(__func__, "out of memory\n");
        goto cleanup1;
    }

    /* done */
    close (fd);
    return bf;
cleanup1:
    close(fd);
cleanup0:
    bitfile_free(bf);
    return NULL;
}

static int write_chunk(int fd, struct bitfile_chunk *ch)
{
    int len_len, rv;
    __u8 hdrbuf[5];

    hdrbuf[0] = ch->tag;
    if ( strchr(BITFILE_SMALLCHUNKS, ch->tag) != NULL ) {
	/* its a small chunk, 2 byte size */
	len_len = 2;
    } else {
	/* regular chunk, 4 byte size */
	len_len = 4;
    }
    /* compute size (note - the format uses big-endian layout */
    if ( len_len == 4 ) {
	hdrbuf[1] = (ch->len >> 24) & 0xFF;
	hdrbuf[2] = (ch->len >> 16) & 0xFF;
	hdrbuf[3] = (ch->len >> 8) & 0xFF;
	hdrbuf[4] = ch->len & 0xFF;
    } else {
	hdrbuf[1] = (ch->len >> 8) & 0xFF;
	hdrbuf[2] = ch->len & 0xFF;
    }
    /* write tag and length */
    rv = write(fd, hdrbuf, len_len+1);
    if ( rv != len_len+1 ) {
	errmsg(__func__,"writing headerr: %s", ch->tag, strerror(errno));
	return -1;
    }
    /* write chunk content */
    rv = write(fd, ch->body, ch->len);
    if ( rv != ch->len ) {
	errmsg(__func__,"writing content: %s", strerror(errno));
	return -1;
    }
    return 0;
}

int bitfile_write(struct bitfile *bf, char *fname)
{
    int fd, rv, n;
    char *cp;
    struct bitfile_chunk *ch;

    /* open the file */
    fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
    if ( fd < 0 ) {
	errmsg(__func__,"opening file: %s", strerror(errno));
	return -1;
    }
    /* write the header */
    rv = write(fd, bf->header, BITFILE_HEADERLEN);
    if ( rv < BITFILE_HEADERLEN ) {
	errmsg(__func__,"writing header: %s", strerror(errno));
	goto cleanup0;
    }
    /* write xilinx chunks in preferred order */
    cp = XILINX_CHUNKS;
    while ( *cp != '\0' ) {
	ch = bitfile_find_chunk(bf, *cp, 0);
	if ( ch != NULL ) {
	    rv = write_chunk(fd, ch);
	    if ( rv < 0 ) {
		errmsg(__func__,"writing chunk '%c'", ch->tag);
		goto cleanup0;
	    }
	}
	cp++;
    }
    /* write non-xilinx chunks */
    for ( n = 0 ; n < bf->num_chunks ; n++ ) {
	ch = &(bf->chunks[n]);
	if ( strchr(XILINX_CHUNKS, ch->tag) == NULL ) {
	    /* not a xilinx chunk */
	    rv = write_chunk(fd, ch);
	    if ( rv < 0 ) {
		errmsg(__func__,"writing chunk '%c'", ch->tag);
		goto cleanup0;
	    }
	}
    }
    /* done */
    close(fd);
    return 0;
cleanup0:
    close(fd);
    return -1;
}

int bitfile_add_chunk(struct bitfile *bf, char tag, int len, unsigned char *data)
{
    int n;

    if (( strchr(BITFILE_SMALLCHUNKS, tag) != NULL ) &&
	( len > 0xFFFF )) {
	errmsg(__func__,"chunk is too large (%d bytes)", len);
	return -1;
    }
    n = bf->num_chunks;
    if ( n >= (BITFILE_MAXCHUNKS-1) ) {
	errmsg(__func__,"too many chunks");
	return -1;
    }
    bf->chunks[n].body = malloc(len);
    if ( bf->chunks[n].body == NULL ) {
	errmsg(__func__,"malloc failure");
	return -1;
    }
    bf->chunks[n].tag = tag;
    bf->chunks[n].len = len;
    memcpy(bf->chunks[n].body, data, len);
    bf->num_chunks++;
    return 0;
}

struct bitfile_chunk *bitfile_find_chunk(struct bitfile *bf, char tag, int n)
{
    int i;

    for ( i = 0 ; i < bf->num_chunks ; i++ ) {
	if ( bf->chunks[i].tag == tag ) {
	    if ( n == 0 ) {
		return &(bf->chunks[i]);
	    }
	    n--;
	}
    }
    return NULL;
}

void bitfile_print_chunk(struct bitfile *bf, char tag, char *title)
{
    struct bitfile_chunk *ch;

    ch = bitfile_find_chunk(bf, tag, 0);
    if ( ch != NULL ) {
	printf("%s%s\n", title, ch->body);
    }
}

int bitfile_validate_xilinx_info(struct bitfile *bf)
{
    char *cp;

    cp = XILINX_CHUNKS;
    while (*cp != '\0' ) {
	if ( bitfile_find_chunk(bf, *cp, 0) == NULL ) {
	    return -1;
	}
	cp++;
    }
    return 0;
}

void bitfile_print_xilinx_info(struct bitfile *bf)
{
    struct bitfile_chunk *ch;

    bitfile_print_chunk(bf, 'a', "Design name:     ");
    bitfile_print_chunk(bf, 'b', "Part ID:         ");
    bitfile_print_chunk(bf, 'c', "Design date:     ");
    bitfile_print_chunk(bf, 'd', "Design time:     ");
    ch = bitfile_find_chunk(bf, 'e', 0);
    if ( ch != NULL ) {
	printf ( "Bitstream size:  %d\n", ch->len );
    }
}

