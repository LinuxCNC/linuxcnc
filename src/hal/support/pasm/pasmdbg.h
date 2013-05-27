/*
 * pasmdbg.h
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/


/*===========================================================================
 * Copyright (c) Texas Instruments Inc 2010-12
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */


/*===========================================================================
// PASM - PRU Assembler
//---------------------------------------------------------------------------
//
// File     : pasmdbg.h
//
// Description:
//     File format for pView debugger debug file
//
//---------------------------------------------------------------------------
// Revision:
//     15-Jun-12: 0.80 - Open source version
============================================================================*/

#define DBGFILE_NAMELEN_SHORT   64

typedef struct _DBGFILE_HEADER {
    unsigned int    FileID;
#define DBGFILE_FILEID_VER3     (0x10150000 | 0x03)
    unsigned int    LabelCount;     /* Number of label records */
    unsigned int    LabelOffset;    /* File offset to label records */
    unsigned int    FileCount;      /* Number of file records */
    unsigned int    FileOffset;     /* File offset to file records */
    unsigned int    CodeCount;      /* Number of code records */
    unsigned int    CodeOffset;     /* File offset to code records */
    unsigned int    EntryPoint;     /* Program entrypoint */
    unsigned int    Flags;          /* File format flags */
#define DBGHDR_FLAGS_BIGENDIAN      0x00000001
} DBGFILE_HEADER;

typedef struct _DBGFILE_LABEL {
    unsigned int    AddrOffset;
    char            Name[DBGFILE_NAMELEN_SHORT];
} DBGFILE_LABEL;

typedef struct _DBGFILE_FILE {
    char            SourceName[DBGFILE_NAMELEN_SHORT];
} DBGFILE_FILE;

typedef struct _DBGFILE_CODE {
    unsigned char   Flags;          /* Record flags */
#define DBGFILE_CODE_FLG_FILEINFO 0x01
#define DBGFILE_CODE_FLG_CANMAP   0x02
    unsigned char   Resv8;          /* Reserved */
    unsigned short  FileIndex;      /* Source file index */
    unsigned int    Line;           /* The line number */
    unsigned int    AddrOffset;     /* Code address offset */
    unsigned int    CodeWord;       /* Code */
} DBGFILE_CODE;
