#ifndef BITFILE_H
#define BITFILE_H

/*************************************************************************
*
* bitfile - a library for reading and writing Xilinx bitfiles
*
* Copyright (C) 2007 John Kasunich (jmkasunich at fastmail dot fm)
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

**************************************************************************

Info about Xilinx bitfiles:

The format consists of several variable length chunks, preceded by what
seems to be a constant header.

Some details about the header hint that it might actually be a variable
length and content field as well, but I haven't found any files that
had different content there (I googled up some nearly ten year old files,
and I also examined ones made with the latest toolset).  So these
functions treat that header as a constant, and report any header that
doesn't match the standard as "not a bitfile".  The code would allow
that rule to be relaxed if needed later.

After the header, each "chunk" consists of a one byte "tag", a two or
four byte length, and "length" bytes of data (the body).

In some chunks, the body is a zero terminated printable string.  In others
it is a blob of binary data.  The file format doesn't care.

Standard Xilinx files use 5 chunks: 'a' through 'd' are zero-terminated
strings with information about the file.  'e' is a large binary blob
with the actual bitstream in it.  Xilinx uses 2 byte lengths for chunks
'a' thru 'd', and 4 bytes for chunk 'e'.  This library allows other
chunks, and assume that all have 4 byte lengths, except 'a' thru 'd'.

This library allows additional chunks to be added to the file.

*************************************************************************/

struct bitfile_chunk {
    char tag;
    int len;
    unsigned char *body;
};

#define BITFILE_MAXCHUNKS 50
/* list of chunks that use 2 byte length values, all others are 4 byte */
#define BITFILE_SMALLCHUNKS "abcd"
#define BITFILE_HEADERLEN 13

struct bitfile {
    char *filename;
    unsigned char header[BITFILE_HEADERLEN];
    int num_chunks;
    struct bitfile_chunk chunks[BITFILE_MAXCHUNKS];
};

/************************************************************************/

/* 'bitfile_new' allocates a struct bitfile, initializes its header field
   with the defaule xilinx header, and initializes its chunks as empty.
   It returns a pointer to the resulting struct, or NULL on error.
*/
struct bitfile *bitfile_new(void);


/* 'bitfile_free' frees all memory associated with a struct bitfile,
   including the chunk bodies and the struct itself.  It assumes that
   the struct was create by a call to 'bitfile_new' or 'bitfile_read'.
*/
void bitfile_free(struct bitfile *bf);


/* 'bitfile_read' reads a bitfile from disk, parses the individual chunks,
   and stores them in a struct bitfile in memory.  The struct and the chunk
   bodies are malloc'ed, and can be conveniently freed by `bitfile_free`.
   It returns a pointer to the new struct bitfile, or NULL on error.
*/
struct bitfile *bitfile_read(char *fname);


/* 'bitfile_write' writes the contents of a caller supplied struct bitfile
   to a specified file in standard bitfile format.  It returns zero on
   success, or -1 on failure.  It will write the standard xilinx 'a'
   through 'e' chunks first, and in order, even if they are not that way
   in the structure, followed by any extra chunks in the order in which
   they are encounterd.  (This is to ensure compatibility with other 
   programs that read bitfiles - this library doesn't care about chunk
   ordering, but other programs might.)
*/
int bitfile_write(struct bitfile *bf, char *fname);


/* 'bitfile_add_chunk` adds a new chunk to a caller supplied struct bitfile.
   The data is copied into a newly allocated block of memory, so the original
   can be dynamic memory, static memory, or anything else.  Returns zero on
   success, or -1 on error.
*/
int bitfile_add_chunk(struct bitfile *bf, char tag, int len, unsigned char *data);


/* 'bitfile_find_chunk` searches a caller supplied struct bitfile for a
   chunk whose tag matches 'tag', and returns a pointer to that chunk, or
   NULL if no match is found.  If 'n' is zero, it will return the first
   matching chunk, if 'n' is 1 it will return the second, etc.  Note that
   normal bitfiles never have more than one chunk with any given tag, so
   'n' should usually be zero.
*/
struct bitfile_chunk *bitfile_find_chunk(struct bitfile *bf, char tag, int n);


/* 'bitfile_print_chunk' searches a caller supplied struct bitfile for a
   chunk whose tag matches 'tag', and prints the body of that chunk as a
   string, preceded by a caller supplied title.  If no match is found
   it prints nothing.  This is handy for printing out the contents of
   the standard Xilinx 'a' through 'd' fields, but will print binary
   junk if invoked on the 'e' chunk (the actual bitstream), or any chunk
   not containing a zero-terminated printable string.
*/
void bitfile_print_chunk(struct bitfile *bf, char tag, char *title);


/* 'bitfile_validate_xilinx_info` verifies that the caller supplied
   struct bitfile contains the minimum chunks to be a valid Xilinx
   bitstream - 'a' through 'e'.  It returns 0 for a valid bitstream,
   and -1 if any required chunk is missing.
*/
int bitfile_validate_xilinx_info(struct bitfile *bf);


/* 'bitfile_print_xilinx_info` invokes `bitfile_print_chunk` to print
   the standard Xilinx 'a' through 'd' chunks (design name, targeted part,
   date, and time) in a easily readable format.  It also prints the
   length of the 'e' chunk (the bitstream).
*/
void bitfile_print_xilinx_info(struct bitfile *bf);

#endif /* BITFILE_H */
