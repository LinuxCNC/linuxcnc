/*
 * pasmdot.c
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
// File     : pasmdot.c
//
// Description:
//     Processes the "dot" commands (.ret, .origin, .main, etc.)
//
//---------------------------------------------------------------------------
// Revision:
//     15-Jun-12: 0.80 - Open source version
============================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if !defined(__APPLE__)
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include <ctype.h>
#include "pasm.h"

#define DOTCMD_MAIN         0
#define DOTCMD_END          1
#define DOTCMD_PROC         2
#define DOTCMD_RET          3
#define DOTCMD_ORIGIN       4
#define DOTCMD_ENTRYPOINT   5
#define DOTCMD_STRUCT       6
#define DOTCMD_ENDS         7
#define DOTCMD_U32          8
#define DOTCMD_U16          9
#define DOTCMD_U8           10
#define DOTCMD_ASSIGN       11
#define DOTCMD_SETCALLREG   12
#define DOTCMD_ENTER        13
#define DOTCMD_LEAVE        14
#define DOTCMD_USING        15
#define DOTCMD_MACRO        16
#define DOTCMD_MPARAM       17
#define DOTCMD_ENDM         18
#define DOTCMD_CODEWORD     19
#define DOTCMD_MAX          19
char *DotCmds[] = { ".main",".end",".proc",".ret",".origin",".entrypoint",
                    ".struct",".ends",".u32",".u16",".u8",".assign",
                    ".setcallreg", ".enter", ".leave", ".using",
                    ".macro", ".mparam", ".endm", ".codeword" };

/*===================================================================
//
// Public Functions
//
====================================================================*/

/*
// CheckDotCommand
//
// Check to see if supplied word is a dot command
//
// Returns 1 if the word is a command, else zero
*/
int CheckDotCommand( char *word )
{
    int i;

    /* Commands are reserved */
    for(i=0; i<=DOTCMD_MAX; i++)
    {
        if( !stricmp( word, DotCmds[i] ) )
            return(1);
    }
    return(0);
}

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
int DotCommand( SOURCEFILE *ps, int TermCnt, char **pTerms, char *Src, int MaxSrc )
{
    int i;

    for(i=0; i<=DOTCMD_MAX; i++)
    {
        if( !stricmp( pTerms[0], DotCmds[i] ) )
            break;
    }
    if( i>DOTCMD_MAX )
    {
        Report(ps,REP_ERROR,"Unrecognized dot command");
        return(-1);
    }

    if( i==DOTCMD_MAIN )
    {
        char c,cs;
        int  quote=0;
        int  idx=0,nameidx=0;

        /*
        // .main command
        //
        // Just print a warning - its only here for compatibility
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }

        /* If the string is in quotes, skip the first charater */
        if( pTerms[1][0]=='"' )
        {
            quote++;
            idx++;
        }
        c = pTerms[1][idx++];
        cs = ps->SourceName[nameidx++];
        while( c && c!='"' )
        {
            if( toupper(c) != toupper(cs) )
            {
NO_MATCH:
                Report(ps,REP_WARN1,".main name '%s' doesn't match '%s'",
                      pTerms[1],ps->SourceName);
                return(0);
            }
            c = pTerms[1][idx++];
            cs = ps->SourceName[nameidx++];
        }
        if( cs && cs!='.' )
            goto NO_MATCH;
        if( c=='"' )
        {
            quote--;
            c = pTerms[1][idx++];
        }
        if( c )
            { Report(ps,REP_ERROR,"Trailing characters on name"); return(-1); }
        if( quote )
            { Report(ps,REP_ERROR,"Unbalanced quotes on name"); return(-1); }
        return(0);
    }
    else if( i==DOTCMD_END )
    {
        /*
        // .end command
        //
        // Do nothing - its only here for compatibility
        */
        if( TermCnt != 1 )
            { Report(ps,REP_ERROR,"Expected no operands"); return(-1); }
        return(0);
    }
    else if( i==DOTCMD_PROC )
    {
        /*
        // .proc command
        //
        // Create a label from the proc name, with a leading '.'
        // (this is for compatibility)
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        sprintf( Src, ".%s:", pTerms[1] );
        return(strlen(Src));
    }
    else if( i==DOTCMD_RET )
    {
        /*
        // .ret command
        //
        // Generate the line "jmp r30.w0"
        // This makes us compatible with "CALL", although inexplicably,
        // the CALL command is not a "dot" command.
        //
        */
        if( TermCnt != 1 )
            { Report(ps,REP_ERROR,"Expected no operands"); return(-1); }
        if( Options & OPTION_RETREGSET )
            { Report(ps,REP_ERROR,".ret incompatible with .setcallreg, use ret"); return(-1); }
        if( Core > CORE_V1 )
            { Report(ps,REP_ERROR,".ret illegal with specified core version, use ret"); return(-1); }
        strcpy( Src, "jmp     r30.w0" );
        return(strlen(Src));
    }
    else if( i==DOTCMD_ORIGIN )
    {
        int val,tmp;
        char tstr[TOKEN_MAX_LEN];

        /*
        // .origin command
        //
        // Alter the origin for writing code
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }

        strcpy( tstr, pTerms[1] );
        if( Expression(ps, tstr, (uint *)&val, &tmp)<0 )
            { Report(ps,REP_ERROR,"Error in processing .origin value"); return(-1); }
        if( Core == CORE_V0 )
            { Report(ps,REP_ERROR,".origin illegal with specified core version"); return(-1); }
        if( val<CodeOffset )
            { Report(ps,REP_ERROR,".origin value is less than current offset"); return(-1); }
        if( CodeOffset>=0 )
            Report(ps,REP_WARN1,"Resetting .origin value after use");
        if( EntryPoint<0 )
            EntryPoint = val;

        CodeOffset = val;
        return(0);
    }
    else if( i==DOTCMD_ENTRYPOINT )
    {
        int val,tmp;
        char tstr[TOKEN_MAX_LEN];

        /*
        // .entrypoint command
        //
        // Alter the origin for writing code
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }

        strcpy( tstr, pTerms[1] );
        if( Expression(ps, tstr, (uint *)&val, &tmp)<0 )
            { Report(ps,REP_ERROR,"Error in processing .entrypoint value"); return(-1); }

        if( Core == CORE_V0 )
            { Report(ps,REP_ERROR,".entrypoint illegal with specified core version"); return(-1); }

        if( HaveEntry )
            { Report(ps,REP_ERROR,"Multiple .entrypoint declarations"); return(-1); }

        EntryPoint = val;
        HaveEntry  = 1;
        return(0);
    }
    else if( i==DOTCMD_STRUCT )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( StructNew(ps,pTerms[1]) );

    }
    else if( i==DOTCMD_ENDS )
    {
        if( TermCnt != 1 )
            { Report(ps,REP_ERROR,"Expected no operands"); return(-1); }
        return( StructEnd(ps) );
    }
    else if( i==DOTCMD_U32 )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( StructAddElement( ps, pTerms[1], 4 ) );
    }
    else if( i==DOTCMD_U16 )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( StructAddElement( ps, pTerms[1], 2 ) );
    }
    else if( i==DOTCMD_U8 )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( StructAddElement( ps, pTerms[1], 1 ) );
    }
    else if( i==DOTCMD_ASSIGN )
    {
        if( TermCnt != 5 )
            { Report(ps,REP_ERROR,"Expected 4 operands"); return(-1); }
        return( StructAssign(ps, pTerms[1], pTerms[2], pTerms[3], pTerms[4]) );
    }
    else if( i==DOTCMD_SETCALLREG )
    {
        PRU_ARG r;

        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        if( Core == CORE_V0 )
            { Report(ps,REP_ERROR,".setcallreg illegal with specified core version"); return(-1); }
        if( Pass==1 && (Options & OPTION_RETREGSET) )
            { Report(ps,REP_ERROR,".setcallreg redefinition"); return(-1); }
        if( CodeOffset>=0 )
            { Report(ps,REP_ERROR,"Can not use .setcallreg after code generation"); return(-1); }
        if( !GetRegister( ps, 1, pTerms[1], &r, 0, 0 ) )
            return -1;

        switch( r.Field )
        {
        case FIELDTYPE_15_0:
        case FIELDTYPE_23_8:
        case FIELDTYPE_31_16:
            if( r.Value<31 )
            {
                RetRegValue = r.Value;
                RetRegField = r.Field;
                Options |= OPTION_RETREGSET;
                return 0;
            }
        }

        Report(ps,REP_ERROR,"Register field must be r0 to r30 and 16 bits wide");
        return(-1);
    }
    else if( i==DOTCMD_ENTER )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( ScopeEnter(ps, pTerms[1]) );
    }
    else if( i==DOTCMD_LEAVE )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( ScopeLeave(ps, pTerms[1]) );
    }
    else if( i==DOTCMD_USING )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( ScopeUsing(ps, pTerms[1]) );
    }
    else if( i==DOTCMD_MACRO )
    {
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }
        return( MacroEnter(ps, pTerms[1]) );
    }
    else if( i==DOTCMD_MPARAM || i==DOTCMD_ENDM )
        { Report(ps,REP_ERROR,"%s can not be used outside of macro",pTerms[0]); return(-1); }
    else if( i==DOTCMD_CODEWORD )
    {
        uint opcode;
        int  tmp;
        char tstr[TOKEN_MAX_LEN];

        /*
        // .codeword command
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(-1); }

        strcpy( tstr, pTerms[1] );
        if( Expression(ps, tstr, &opcode, &tmp)<0 )
            { Report(ps,REP_ERROR,"Error in processing .codeword value"); return(-1); }

        GenOp( ps, TermCnt, pTerms, opcode );
        return(0);
    }

    Report(ps,REP_ERROR,"Dot command - Internal Error");
    return(-1);
}


/*
// DotInitialize
//
// Open the dot-command environment
//
// void
*/
void DotInitialize(int pass)
{
    StructInit();
}



/*
// DotCleanup
//
// Clean up the dot-command environment
//
// void
*/
void DotCleanup(int pass)
{
    StructCleanup();
    MacroCleanup();
}
