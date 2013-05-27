/*
 * pasmmacro.c
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
// File     : pasmmacro.c
//
// Description:
//     Processes the macro commands
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

/* Local Macro Definitions */

#define MACRO_NAME_LEN      TOKEN_MAX_LEN
#define MACRO_MAX_ARGS      8
#define MACRO_MAX_LINES     128
#define MACRO_LINE_LENGTH   256
#define MACRO_MAX_LABELS    32
#define MAX_SOURCE_LINE     256

/* Macro Struct Record */
typedef struct _MACRO {
    struct _MACRO   *pPrev;         /* Previous in MACRO list */
    struct _MACRO   *pNext;         /* Next in MACRO list */
    char            Name[MACRO_NAME_LEN];
    int             InUse;          /* Macro is in use */
    int             Id;             /* Macro ID */
    int             Arguments;      /* Number of arguments */
    int             Required;       /* Number of required arguments */
    int             Labels;         /* Number of labels */
    int             Expands;        /* Number of label expansions */
    int             CodeLines;      /* Number of code lines */
    char            ArgName[MACRO_MAX_ARGS][TOKEN_MAX_LEN];
    char            ArgDefault[MACRO_MAX_ARGS][TOKEN_MAX_LEN];
    char            LableName[MACRO_MAX_LABELS][TOKEN_MAX_LEN];
    char            Code[MACRO_MAX_LINES][MACRO_LINE_LENGTH];
} MACRO;


/* Local Support Funtions */
static int _strncat( char *dst, int len, char *src );
static MACRO *MacroFind( char *Name );
static MACRO *MacroCreate( SOURCEFILE *ps, char *Name );
int MacroAddArg( SOURCEFILE *ps, MACRO *pm, char *ArgText );
static void MacroDestroy( MACRO *pm );

/* Local macro list */
int   MacroId=0;
MACRO *pMacroList=0;      /* List of declared structs */
MACRO *pMacroCurrent=0;


/*===================================================================
//
// Public Functions
//
====================================================================*/

/*
// MacroEnter
// Returns:
//    0 - Success
//   -1 - Error
*/
int MacroEnter( SOURCEFILE *ps, char *Name )
{
    SRCLINE sl;
    MACRO   *pm;
    char    src[MAX_SOURCE_LINE];
    int     i;

    if( Core == CORE_V0 )
        { Report(ps,REP_ERROR,".macro illegal with specified core version"); return(-1); }

    /* Create the macro */
    pm = MacroCreate( ps, Name );
    if( !pm )
        return(-1);

    /* Scan source lines until we see .endm */
    for(;;)
    {
        /* Abort on a total disaster */
        if( FatalError || Errors >= 25 )
            return(-1);

        /* Get a line of source code */
        i = GetSourceLine( ps, src, MAX_SOURCE_LINE );
        if( !i )
            { Report(ps,REP_ERROR,"Missing .endm on macro"); return(-1); }
        if( i<0 )
            continue;

        if( !ParseSourceLine(ps,i,src,&sl) )
            continue;

        /* Check for a label */
        if( sl.Flags & SRC_FLG_LABEL )
        {
            if( pm->Labels==MACRO_MAX_LABELS )
                Report(ps,REP_ERROR,"Macro contains too many labels");
            else
            {
                strcpy( pm->LableName[pm->Labels], sl.Label );
                pm->Labels++;
            }
        }

        /* Check for a macro related dot command */
        if( sl.Terms && (sl.Flags & SRC_FLG_DOTCMD1) )
        {
            if( !stricmp( sl.Term[0], ".mparam" ) )
            {
                if( sl.Terms==1 )
                    { Report(ps,REP_ERROR,"Expected at least 1 parameter on .mparam"); continue; }
                for( i=1; i<(int)sl.Terms; i++)
                    MacroAddArg(ps,pm,sl.Term[i]);
                continue;
            }
            else if( !stricmp( sl.Term[0], ".macro" ) )
                { Report(ps,REP_ERROR,"Macro definitions may not be nested"); continue; }
            else if( !stricmp( sl.Term[0], ".endm" ) )
            {
                pm->InUse = 0;
                return(0);
            }
        }
        /* Else store the line as part of the macro */
        else
        {
            if( pm->CodeLines == MACRO_MAX_LINES )
                { Report(ps,REP_ERROR,"Macro line count exceeded"); continue; }
            strcpy(pm->Code[pm->CodeLines],src);
            pm->CodeLines++;
        }
    }
}


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
int ProcessMacro( SOURCEFILE *ps, int TermCnt, char **pTerms )
{
    MACRO   *pm;
    int     cidx,sidx,nidx,i;
    char    src[MAX_SOURCE_LINE];
    char    namebuf[MACRO_NAME_LEN];
    char    c;

    pm = MacroFind(pTerms[0]);
    if( !pm )
        return(0);

    if( pm->InUse )
        { Report(ps,REP_ERROR,"Illegal recursive use of macro '%s'",pTerms[0]); return(0); }

    if( pm->Required >= TermCnt )
        { Report(ps,REP_ERROR,"Expected at least %d arguments on '%s'",pm->Required,pTerms[0]); return(0); }

    if( pm->Arguments < (TermCnt-1) )
        { Report(ps,REP_ERROR,"Expected no more than %d arguments on '%s'",pm->Arguments,pTerms[0]); return(0); }

    /* Bump expansion count */
    pm->Expands++;
    pm->InUse = 1;

    for( cidx=0; cidx<pm->CodeLines; cidx++ )
    {
        /* Build the assembly statement */
        sidx=0;
        nidx=0;
        src[0] = 0;
        for(;;)
        {
            c=pm->Code[cidx][sidx++];
            /* Check for start of name */
            if( !nidx )
            {
                if(LabelChar(c,1))
                {
                    namebuf[nidx++]=c;
                    continue;
                }
            }
            /* Else continue a previously started name */
            else
            {
                if(LabelChar(c,0))
                {
                    /* Check for name too long */
                    if( nidx==(MACRO_NAME_LEN-1) )
                        { Report(ps,REP_ERROR,"Term too long in macro assembly text"); pm->InUse=0; return(0); }
                    namebuf[nidx++]=c;
                    continue;
                }

                /* This name is done */
                namebuf[nidx]=0;

                /* Look for an argument match */
                for(i=0;i<pm->Arguments;i++)
                {
                    if(!strcmp(namebuf,pm->ArgName[i]))
                    {
                        /* Match! */
                        if( (i+1)>=TermCnt )
                            _strncat( src, MAX_SOURCE_LINE, pm->ArgDefault[i] );
                        else
                            _strncat( src, MAX_SOURCE_LINE, pTerms[i+1] );
                        goto SUBTEXTDONE;
                    }
                }

                /* Look for a label match */
                for(i=0;i<pm->Labels;i++)
                {
                    if(!strcmp(namebuf,pm->LableName[i]))
                    {
                        char labeltext[TOKEN_MAX_LEN+32];

                        /* Match! */
                        sprintf(labeltext,"_%s_%d_%d_", pm->LableName[i],pm->Id,pm->Expands);
                        _strncat( src, MAX_SOURCE_LINE, labeltext );
                        goto SUBTEXTDONE;
                    }
                }

                /* Sub in the original text */
                _strncat( src, MAX_SOURCE_LINE, namebuf );
SUBTEXTDONE:
                nidx = 0;
            }
            /* Check for text too long */
            i=strlen(src);
            if( i==(MAX_SOURCE_LINE-1) )
                { Report(ps,REP_ERROR,"Macro expansion too long"); pm->InUse=0; return(0); }
            src[i++]=c;
            src[i]=0;
            if( !c )
                break;
        }

        i=strlen(src);
        if(i)
        {
            if( !ProcessSourceLine(ps, i, src) )
            {
                Report(ps,REP_ERROR,"(While expanding code line %d of macro '%s')",(cidx+1),pm->Name);
                pm->InUse=0;
                return(0);
            }
        }
    }
    pm->InUse = 0;
    return(1);
}


/*
// MacroCleanup
//
// Returns: void
*/
void MacroCleanup()
{
    while( pMacroList )
        MacroDestroy( pMacroList );
    MacroId = 0;
}

/*
// CheckMacro
//
// Searches for an macro by name.
//
// Returns 1 on success, 0 on error
*/
int CheckMacro( char *name )
{
    if( MacroFind(name) )
        return(1);
    return(0);
}



/*===================================================================
//
// Private Functions
//
====================================================================*/

static int _strncat( char *dst, int len, char *src )
{
    int sidx,didx;

    didx = 0;
    while( didx<len && dst[didx] )
        didx++;
    if( didx>(len-1) )
        return(-1);
    sidx = 0;
    while( src[sidx] )
    {
        if( didx>(len-1) )
        {
            dst[didx] = 0;
            return(-1);
        }
        dst[didx++] = src[sidx++];
    }
    dst[didx] = 0;
    return(didx);
}


/*
// MacroFind
//
// Searches for a macro record by name. If found, returns the record pointer.
//
// Returns MACRO * on success, 0 on error
*/
static MACRO *MacroFind( char *Name )
{
    MACRO *pm;

    pm = pMacroList;
    while( pm )
    {
        if( !strcmp( Name, pm->Name ) )
            break;
        pm = pm->pNext;
    }
    return(pm);
}


/*
// MacroCreate
//
// Create a new macro record
//
// Returns MACRO * on success, 0 on error
*/
static MACRO *MacroCreate( SOURCEFILE *ps, char *Name )
{
    MACRO *pm;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,Name) )
        return(0);

    /* Make sure its not too long */
    if( strlen(Name)>=MACRO_NAME_LEN )
        { Report(ps,REP_ERROR,"Macro name too long"); return(0); }

    /* Allocate a new record */
    pm = malloc(sizeof(MACRO));
    if( !pm )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(0); }

    strcpy( pm->Name, Name );
    pm->InUse     = 1;
    pm->Id        = MacroId++;
    pm->Arguments = 0;
    pm->Required  = 0;
    pm->CodeLines = 0;
    pm->Labels    = 0;
    pm->Expands   = 0;

    /* Put this equate in the master list */
    pm->pPrev  = 0;
    pm->pNext  = pMacroList;
    pMacroList = pm;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DOTCMD : Macro '%s' declared\n",
                            ps->SourceName,ps->CurrentLine,pm->Name);

    return(pm);
}


/*
// MacroAddArg
//
// Add an argument to a macro record
//
// Returns 0 on success, -1 on error
*/
int MacroAddArg( SOURCEFILE *ps, MACRO *pm, char *ArgText )
{
    int  i,sidx;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DOTCMD : Macro Parameter '%s' declared\n",
                         ps->SourceName,ps->CurrentLine,ArgText);

    if( pm->Arguments == MACRO_MAX_ARGS )
        { Report(ps,REP_ERROR,"Too many macro arguments"); return(-1); }

    /* Scan in the argument */
    sidx=0;
    while( ArgText[sidx]==' ' || ArgText[sidx]==9 )
        sidx++;
    i=0;
    while( ArgText[sidx]!=' ' && ArgText[sidx]!=9 && ArgText[sidx]!='=' && ArgText[sidx]!=0 )
    {
        if(i==TOKEN_MAX_LEN)
            { Report(ps,REP_ERROR,"Macro argument name too long"); return(-1); }
        if( (i==0 && !LabelChar(ArgText[sidx],1)) ||
            (i!=0 && !LabelChar(ArgText[sidx],0)) )
            { Report(ps,REP_ERROR,"Illegal character in macro argument name"); return(-1); }
        pm->ArgName[pm->Arguments][i++] = ArgText[sidx++];
    }
    pm->ArgName[pm->Arguments][i] = 0;
    if( !i )
        goto MARG_SYNTAX;

    /* Verify no duplicate naming */
    for(i=0; i<pm->Arguments; i++)
    {
        if( !strcmp(pm->ArgName[i],pm->ArgName[pm->Arguments]) )
        {
            Report(ps,REP_ERROR,"Duplicate macro argument name '%s'",pm->ArgName[i]);
            return(-1);
        }
    }

    /* Scan in the default value (if any) */
    while( ArgText[sidx]==' ' || ArgText[sidx]==9 )
        sidx++;

    if( ArgText[sidx]=='=' )
    {
        sidx++;
        while( ArgText[sidx]==' ' || ArgText[sidx]==9 )
            sidx++;
        i=0;
        while( ArgText[sidx]!=' ' && ArgText[sidx]!=9 && ArgText[sidx]!='=' && ArgText[sidx]!=0 )
        {
            if(i==TOKEN_MAX_LEN)
                { Report(ps,REP_ERROR,"Macro argument value too long"); return(-1); }
            if( !LabelChar(ArgText[sidx],0) && ArgText[sidx]!='.' )
                goto MARG_SYNTAX;
            pm->ArgDefault[pm->Arguments][i++] = ArgText[sidx++];
        }
        pm->ArgDefault[pm->Arguments][i] = 0;
        if( !i )
            goto MARG_SYNTAX;
        pm->Arguments++;
    }
    else
    {
        pm->ArgDefault[pm->Arguments][0] = 0;
        if(pm->Arguments > pm->Required)
            { Report(ps,REP_ERROR,"Optional macro arguments must be listed last"); return(-1); }
        pm->Arguments++;
        pm->Required++;
    }

    if( ArgText[sidx]!=0 )
    {
MARG_SYNTAX:
        Report(ps,REP_ERROR,"Syntax error in macro argument '%s' around character %d",ArgText,sidx+1);
        return(-1);
    }

    return(0);
}


/*
// MacroDestroy
//
// Frees a macro record.
//
// void
*/
static void MacroDestroy( MACRO *pm )
{
    if( !pm->pPrev )
        pMacroList = pm->pNext;
    else
        pm->pPrev->pNext = pm->pNext;

    if( pm->pNext )
        pm->pNext->pPrev = pm->pPrev;

    free(pm);
}
