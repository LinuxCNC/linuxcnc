/*
 * pasm.h
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
// File     : pasm.h
//
// Description:
//     Main include file
//         - Global data structures and assembler state
//         - Declarations of all public functions
//
//---------------------------------------------------------------------------
// Revision:
//     15-Jun-12: 0.80 - Open source version
============================================================================*/
typedef unsigned int uint;

#include "pru_ins.h"

#define TOKEN_MAX_LEN   128

/* Label Record */
#define LABEL_NAME_LEN  TOKEN_MAX_LEN
typedef struct _LABEL {
    struct _LABEL   *pPrev;         /* Previous in LABEL list */
    struct _LABEL   *pNext;         /* Next in LABEL list */
    int             Offset;         /* Offset Value */
    char            Name[LABEL_NAME_LEN];
} LABEL;

/* Source File Record */
#define SOURCE_NAME     64
#define SOURCE_BASE_DIR 256
typedef struct _SOURCEFILE {
    struct _SOURCE  *pParent;       /* The file that included this file */
    FILE            *FilePtr;       /* Open file handle */
    unsigned int    InUse;          /* Set to '1' if file is active */
    unsigned int    FileIndex;      /* Index of this source file in CODEGEN */
    unsigned int    CurrentLine;    /* The current line being read */
    unsigned int    CurrentColumn;  /* The current column being read */
    unsigned int    ccDepthIn;      /* Original condition code depth */
    char            LastChar;       /* Last character read from file */
    char            SourceName[SOURCE_NAME];
    char            SourceBaseDir[SOURCE_BASE_DIR];
} SOURCEFILE;

/* Source Line Record */
#define MAX_TOKENS      12
#define SRC_FLG_LABEL       (1<<0)
#define SRC_FLG_DOTCMD1     (1<<1)
#define SRC_FLG_DOTCMD2     (1<<2)
typedef struct _SRCLINE {
    uint    Flags;
    uint    Terms;
    char    Label[TOKEN_MAX_LEN];
    char    Term[MAX_TOKENS][TOKEN_MAX_LEN];
} SRCLINE;

/* CodeGen Record */
typedef struct _CODEGEN {
    unsigned char   Flags;          /* Record flags */
#define CODEGEN_FLG_FILEINFO 0x01
#define CODEGEN_FLG_CANMAP   0x02
    unsigned char   Resv8;          /* Reserved */
    unsigned short  FileIndex;      /* Source file index */
    unsigned int    Line;           /* The line number */
    unsigned int    AddrOffset;     /* Code address offset */
    unsigned int    CodeWord;       /* Code */
} CODEGEN;

/* User Options */
extern unsigned int Options;
#define OPTION_BINARY               (1<<0)
#define OPTION_BINARYBIG            (1<<1)
#define OPTION_CARRAY               (1<<2)
#define OPTION_IMGFILE              (1<<3)
#define OPTION_DBGFILE              (1<<4)
#define OPTION_LISTING              (1<<5)
#define OPTION_DEBUG                (1<<6)
#define OPTION_BIGENDIAN            (1<<7)
#define OPTION_RETREGSET            (1<<8)
#define OPTION_SOURCELISTING        (1<<9)
extern unsigned int Core;
#define CORE_NONE                   0
#define CORE_V0                     1
#define CORE_V1                     2
#define CORE_V2                     3
#define CORE_V3                     4
extern FILE         *CArrayFile;
extern FILE         *ListingFile;

/* Assembler Engine */
extern int  Pass;                   /* Pass 1 or 2 of parser */
extern int  HaveEntry;              /* Entrypont flag (init to 0) */
extern int  EntryPoint;             /* Entrypont (init to -1) */
extern int  CodeOffset;             /* Current instruction "word" offset (zero based) */
extern int  Errors;                 /* Total number or errors */
extern int  FatalError;             /* Set on fatal error */
extern int  Warnings;               /* Total number of warnings */
extern uint RetRegValue;            /* Return register index */
extern uint RetRegField;            /* Return register field */

#define DEFAULT_RETREGVAL   30
#define DEFAULT_RETREGFLD   FIELDTYPE_15_0

#define SOURCEFILE_MAX 32
extern SOURCEFILE sfArray[SOURCEFILE_MAX];
extern unsigned int sfIndex;

/* Use platform appropriate function for case-insensitive string compare */
#ifdef _MSC_VER
  #define stricmp _stricmp
#elif defined(__GNUC__)
  #define stricmp strcasecmp
#endif


/*=====================================================================
//
// Functions Implemented by the Opcode Module
//
//====================================================================*/

/*
// CheckOpcode
//
// Called to see if the supplied token is a reserved word.
//
// Returns index of opcode, 0 if not an opcode
*/
int CheckOpcode( char *word );


/*
// CheckTokenType
//
// Called to see if the supplied token is reserved word.
//
// Returns token type flags
*/
uint CheckTokenType( char *word );
#define TOKENTYPE_UNRESERVED        0
#define TOKENTYPE_FLG_OPCODE        0x0001
#define TOKENTYPE_FLG_DIRECTIVE     0x0002
#define TOKENTYPE_FLG_REG_BASE      0x0004
#define TOKENTYPE_FLG_REG_ADDR      0x0008
#define TOKENTYPE_FLG_REG_PTR       0x0010
#define TOKENTYPE_FLG_REG_POSTINC   0x0020
#define TOKENTYPE_FLG_REG_PREDEC    0x0040

/*
// ProcessOp
//
// Opcode processor
//
// This is the function that assembles opcode statements
//
// ps      - Pointer to source file record
// TermCnt - Number of terms (including the command)
// pTerms  - Pointer to the terms
//
// Returns:
//      1 : Success
//      0 : Error
*/
int ProcessOp( SOURCEFILE *ps, int TermCnt, char **pTerms );

/*
// GetRegister
//
// Get Register Argument
//
// Parses the source string for a register and field
//
// ps      - Pointer to source file record
// num     - operand number (1 based)
// src     - source string
// pa      - Pointer to register structure
// fBitOk  - Can accept Rxx.Txx
// termC   - Set to non-zero if special terminating character ('+')
//
// Returns:
//      1 : Success
//      0 : Error
*/
int GetRegister( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa, int fBitOk, char termC );


/*=====================================================================
//
// Functions Implemented by the DotCommand Module
//
//====================================================================*/

/*
// CheckDotCommand
//
// Check to see if supplied word is a dot command
//
// Returns 1 if the word is a command, else zero
*/
int CheckDotCommand( char *word );

/*
// DotCommand
//
// Dot command processor
//
// This is the function where users add their assembler commands
//
// ps      - Pointer to source file record
// TermCnt - Number of terms (including the command)
// pTerms  - Pointer to the terms
// Src     - Buffer to write any resulting assembly line
// MaxSrc  - Size of assembly line buffer
//
// Returns:
//    >=0 : Success - Length of assemby line (0 to MaxSrc)
//     <0 : Illegal command
*/
int DotCommand( SOURCEFILE *ps, int TermCnt, char **pTerms, char *Src, int MaxSrc );

/*
// DotInitialize
//
// Open the dot-command environment
//
// void
*/
void DotInitialize(int pass);

/*
// DotCleanup
//
// Clean up the dot environment
//
// void
*/
void DotCleanup(int pass);



/*=====================================================================
//
// Functions Implemented by the Structure/Scope Module
//
//====================================================================*/

/*
// ScopeEnter
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeEnter( SOURCEFILE *ps, char *Name );


/*
// ScopeLeave
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeLeave( SOURCEFILE *ps, char *Name );


/*
// ScopeUsing
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeUsing( SOURCEFILE *ps, char *Name );


/*
// StructInit
//
// Returns: void
*/
void StructInit();


/*
// StructCleanup
//
// Returns: void
*/
void StructCleanup();


/*
// StructNew
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int StructNew( SOURCEFILE *ps, char *Name );


/*
// StructEnd
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int StructEnd( SOURCEFILE *ps );


/*
// StructAddElement
//
// Create a new structure record
//
// Returns 0 on success, -1 on error
*/
int StructAddElement( SOURCEFILE *ps, char *Name, uint size );


/*
// StructAssign
//
// Assign a structure to an instance
//
// Returns 0 on success, -1 on error
*/
int StructAssign( SOURCEFILE *ps, char *structName, char *rsName,
                         char *reName, char *defName );


/*
// Struct Param Process
//
// Processes the supplied argument for stucture references or SIZE/OFFSET
// operations. When found, the structure definition is used to substitute
// in the proper register or numeric value.
//
// The string 'source' is assumed to be be able to hold a length of
// at least 'TOKEN_MAX_LEN' bytes.
//
// Returns 0 for OK, or -1 for Fatal Error
//
*/
int StructParamProcess( SOURCEFILE *ps, int ParamIdx, char *source );


/*
// CheckStruct
//
// Searches for struct template or struct by name.
//
// Returns 1 on success, 0 on error
*/
int CheckStruct( char *name );



/*=====================================================================
//
// Main Assembler Functions
//
//====================================================================*/

/*
// ProcessSourceFile
//
// New source file to assemble.
//
// Returns 1 on success, 0 on error
*/
int ProcessSourceFile( SOURCEFILE *ps );

/*
// ProcessSourceLine
//
// New source line to assemble.
//
// Returns 1 on success, 0 on error
*/
int ProcessSourceLine( SOURCEFILE *ps, int length, char *src );

/*
// ParseSourceLine
//
// New source line to parse.
//
// Returns 1 on success, 0 on error
*/
int ParseSourceLine( SOURCEFILE *ps, int length, char *src, SRCLINE *pa );

/*
// Report
//
// Report an abnormal condition
*/
#define REP_INFO    0   /* Information only */
#define REP_WARN1   1   /* Warn on pass1 */
#define REP_WARN2   2   /* Warn on pass2 */
#define REP_ERROR   3
#define REP_FATAL   4
void Report( SOURCEFILE *ps, int Level, char *fmt, ... );

/*
// LabelChar
//
// Return whether the character is legal for a label.
// Numbers are not allowed when FlagFirstChar is set.
//
// Returns 1 on success, 0 on error
*/
int LabelChar( char c, int FlagFirstChar );


/*
// LabelCreate
//
// Create a label with the supplied offset value
//
// Returns 1 on success, 0 on error
*/
int LabelCreate( SOURCEFILE *ps, char *label, int value );


/*
// LabelFind
//
// Searches for an equate by name. If found, returns the record pointer.
//
// Returns LABEL * on success, 0 on error
*/
LABEL *LabelFind( char *name );


/*
// LabelDestroy
//
// Frees and label record.
//
// void
*/
void LabelDestroy( LABEL *pl );


/*
// GenOp
//
// Generate an opcode to the ouput file
//
// ps      - Pointer to source file record
// TermCnt - Number of terms (including the command)
// pTerms  - Pointer to the terms
// opcode  - Generated Opcode
*/
void GenOp( SOURCEFILE *ps, int TermCnt, char **pTerms, uint opcode );


/*
// Check Name
//
// Returns 1 if the name is free, or 0 if it is in use
*/
int CheckName( SOURCEFILE *ps, char *name );


/*=======================================================================
//
// Expression Analyzer
//
=======================================================================*/

/*
// Expression - Math Expression Parser
//
// Returns 0 on success, <0 on error
*/
int Expression( SOURCEFILE *ps, char *s, uint *pResult, int *pIndex );



/*=======================================================================
//
// Pre-processor Functions
//
=======================================================================*/

/*
// InitSourceFile
//
// Initializes all the fields in SOURCEFILE, and attempts to to open the
// file.
//
// Returns 1 on success, 0 on error
*/
SOURCEFILE *InitSourceFile( SOURCEFILE *pParent, char *filename );

/*
// CloseSourceFile
//
// Close the source file and free the block.
//
// void
*/
void CloseSourceFile( SOURCEFILE *ps );

/*
// GetSourceLine
//
// Get a new line of source code.
//
// This module also processes:
//    '#' directives
//    #define expansion
//    Comments
//
// Returns length of line, 0 on EOF, -1 on Error
*/
int GetSourceLine( SOURCEFILE *ps, char *Dst, int MaxLen );

/*
// ppCleanup
//
// Clean up the pre-processor environment
//
// void
*/
void ppCleanup();


/*
// EquateCreate
//
// Creates an equate record
//
// Returns 0 on success, -1 on error
*/
#define EQUATE_NAME_LEN TOKEN_MAX_LEN
#define EQUATE_DATA_LEN 256
int EquateCreate( SOURCEFILE *ps, char *Name, char *Value );


/*
// CheckEquate
//
// Searches for an equate by name.
//
// Returns 1 on success, 0 on error
*/
int CheckEquate( char *name );


/*=======================================================================
//
// Macro Functions
//
=======================================================================*/

/*
// MacroEnter
// Returns:
//    0 - Success
//   -1 - Error
*/
int MacroEnter( SOURCEFILE *ps, char *Name );


/*
// ProcessMacro
//
// ps      - Pointer to source file record
// TermCnt - Number of terms (including the command)
// pTerms  - Pointer to the terms
//
// Returns:
//      1 : Success
//      0 : Error
*/
int ProcessMacro( SOURCEFILE *ps, int TermCnt, char **pTerms );


/*
// MacroCleanup
//
// Returns: void
*/
void MacroCleanup();


/*
// CheckMacro
//
// Searches for an macro by name.
//
// Returns 1 on success, 0 on error
*/
int CheckMacro( char *name );
