/*
 * pasmpp.c
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
// File     : pasmpp.c
//
// Description:
//     Assembler pre-processor. This module's operation is independent
//     of the rest of the assembler. It contains no PRU specific
//     functionality.
//         - Preprocessor handles all source file opening and reading
//         - Initial source parsing
//         - Processes all '#' commands (#include and #define)
//         - Handles equate creation, matching, and expansion
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

/* Local Strcuture Types */

/* Equate Record */
typedef struct _EQUATE {
    struct _EQUATE  *pPrev;         /* Previous in EQUATE list */
    struct _EQUATE  *pNext;         /* Next in EQUATE list */
    int             Busy;           /* Is this record busy? */
    char            name[EQUATE_NAME_LEN];
    char            data[EQUATE_DATA_LEN];
} EQUATE;

/* Local Support Funtions */
static int ReadCharacter( SOURCEFILE *ps );
static int GetTextLine( SOURCEFILE *ps, char *Dst, int MaxLen, int *pLength, int *pEOF );
static int ParseSource( SOURCEFILE *ps, char *Src, char *Dst, int *pIdx, int MaxLen );
static int LoadInclude( SOURCEFILE *ps, char *Src );
static int EquateProcess( SOURCEFILE *ps, char *Src );
static int UndefProcess( SOURCEFILE *ps, char *Src );
static EQUATE *EquateFind( char *name );
static void EquateDestroy( EQUATE *peq );

static int IfDefProcess( SOURCEFILE *ps, char *Src, int fTrue );
static int ElseProcess( SOURCEFILE *ps, char *Src );
static int EndifProcess( SOURCEFILE *ps, char *Src );

int     OpenFiles=0;        /* Total number of open files */
EQUATE  *pEqList=0;         /* List of installed equates */

SOURCEFILE      sfArray[SOURCEFILE_MAX];
unsigned int    sfIndex = 0;

#define CC_MAX_DEPTH            8
uint    ccDepth = 0;
uint    ccStateFlags[CC_MAX_DEPTH];
#define CCSTATEFLG_TRUE         1       // Currently accepting code
#define CCSTATEFLG_ELSE         2       // Else has been used


/*===================================================================
//
// Public Functions
//
====================================================================*/

/*
// InitSourceFile
//
// Initializes all the fields in SOURCEFILE, and attempts to to open the
// file.
//
// Returns 1 on success, 0 on error
*/
SOURCEFILE *InitSourceFile( SOURCEFILE *pParent, char *filename )
{
    SOURCEFILE *ps;
    int i,j,k;
    char SourceName[SOURCE_NAME];
    char SourceBaseDir[SOURCE_BASE_DIR];

    /* Put a reasonable cap on #include depth */
    if( OpenFiles==15 )
    {
        Report(pParent,REP_FATAL,"Too many open files");
        return(0);
    }

    /*
    // Create a base directory for this file
    //
    // The base directory is used for any #include contained in the source
    */
    strcpy( SourceBaseDir, "./" );
    i=0;
    j=-1;
    k=0;
    while( filename[i] )
    {
        if( filename[i]==':' )
        {
            if(k)
            {
                Report(pParent,REP_FATAL,"Illegal source file name '%s'",filename);
                goto FILEOP_ERROR;
            }
            j=i;
            k=1;
        }
        if( filename[i]=='/' || filename[i]=='\\' )
            j=i;
        i++;
    }
    if( j>=(SOURCE_BASE_DIR-4) )
    {
        Report(pParent,REP_FATAL,"Pathname too long in '%s'",filename);
        goto FILEOP_ERROR;
    }
    if( j>0 )
    {
        if(k)
        {
            memcpy( SourceBaseDir, filename, j+1 );
            SourceBaseDir[j+1]='.';
            SourceBaseDir[j+2]='/';
            SourceBaseDir[j+3]=0;
        }
        else
        {
            if((filename[0]=='.' && filename[1]=='/') || filename[0]=='/' || filename[0]=='\\')
            {
                memcpy( SourceBaseDir, filename, j );
                SourceBaseDir[j]='/';
                SourceBaseDir[j+1]=0;
            }
            else
            {
                memcpy( SourceBaseDir+2, filename, j );
                SourceBaseDir[j+2]='/';
                SourceBaseDir[j+3]=0;
            }
        }
    }
    if( Options & OPTION_DEBUG )
        printf("Base source directory: '%s'\n",SourceBaseDir);

    /* Create the "friendly" filename for output messages */
    i=strlen(filename)-j;
    if( i>SOURCE_NAME )
    {
        Report(pParent,REP_FATAL,"Basename too long in '%s'",filename);
        goto FILEOP_ERROR;
    }
    memcpy( SourceName, filename+j+1, i );

    /*
    // See if this file was used before, or allocate a new record
    */
    for( i=0; i<(int)sfIndex; i++ )
    {
        if( !sfArray[i].InUse &&
                !strcmp(SourceName, sfArray[i].SourceName) &&
                !strcmp(SourceBaseDir, sfArray[i].SourceBaseDir) )
            break;
    }

    if( i<(int)sfIndex )
        ps = &sfArray[i];
    else
    {
        /* Allocate a new file */
        if( sfIndex==SOURCEFILE_MAX )
            { Report(pParent,REP_FATAL,"Max source files exceeded"); return(0); }

        ps = &sfArray[sfIndex];
        i = sfIndex++;
    }

    /*
    // Fill in file record
    */
    memset( ps, 0, sizeof(SOURCEFILE) );

    if( Options & OPTION_DEBUG )
        printf("New source file: '%s'\n",filename);

    /* Init the base fields */
    ps->pParent       = 0;
    ps->LastChar      = 0;
    ps->CurrentLine   = 1;
    ps->CurrentColumn = 1;
    ps->ccDepthIn     = ccDepth;
    ps->InUse         = 1;
    ps->FileIndex     = i;

    strcpy( ps->SourceName, SourceName );
    strcpy( ps->SourceBaseDir, SourceBaseDir );


    /* Open the file */
    ps->FilePtr = fopen(filename,"rb");
    if (!ps->FilePtr)
    {
        Report(pParent,REP_FATAL,"Can't open source file '%s'",filename);
        goto FILEOP_ERROR;
    }
    OpenFiles++;
    if( OpenFiles > 10 )
        Report(pParent,REP_WARN1,"%d open files - possible #include recursion",OpenFiles);
    return(ps);

FILEOP_ERROR:
    return(0);
}


/*
// CloseSourceFile
//
// Close the source file and free the block.
//
// void
*/
void CloseSourceFile( SOURCEFILE *ps )
{
    OpenFiles--;
    ps->InUse = 0;
    fclose( ps->FilePtr );
}


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
#define RAW_SOURCE_MAX  255
int GetSourceLine( SOURCEFILE *ps, char *Dst, int MaxLen )
{
    char c,Src[RAW_SOURCE_MAX],word[TOKEN_MAX_LEN];
    int  i,idx;
    int  len,eof;

NEXT_LINE:
    do
    {
        if( !GetTextLine(ps, Src, RAW_SOURCE_MAX, &len, &eof) )
            return(-1);
    } while( !len && !eof );

    if( !len && eof )
    {
        if( ps->ccDepthIn != ccDepth )
            { Report(ps,REP_ERROR,"#endif mismatch in file"); return(0); }
        return(0);
    }

    /*
    // Process any '#' directives
    */
    if( Src[0]=='#' )
    {
        idx = 1;
        c = Src[idx++];
        if( (c<'A'||c>'Z') && (c<'a'||c>'z') )
            { Report(ps,REP_ERROR,"Expected {a-z} after #"); return(-1); }
        i=0;
        word[i++]=c;
        while( i<(TOKEN_MAX_LEN-1) )
        {
            c = Src[idx++];
            if( c==' ' || c==0x9 || !c )
                break;
            word[i++]=c;
        }
        word[i]=0;

        /* Make sure the process functions see the final NULL */
        if( !c )
            idx--;

        if( !stricmp( word, "ifdef" ) )
        {
            if( !IfDefProcess( ps, Src+idx, 1 ) )
                return(-1);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "ifndef" ) )
        {
            if( !IfDefProcess( ps, Src+idx, 0 ) )
                return(-1);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "else" ) )
        {
            if( !ElseProcess( ps, Src+idx ) )
                return(-1);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "endif" ) )
        {
            if( !EndifProcess( ps, Src+idx ) )
                return(-1);
            goto NEXT_LINE;
        }

        if( ccDepth && !(ccStateFlags[ccDepth-1]&CCSTATEFLG_TRUE) )
            goto NEXT_LINE;

        if( !stricmp( word, "error" ) )
        {
            Report(ps,REP_ERROR,"%s",Src+idx);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "warn" ) )
        {
            Report(ps,REP_WARN1,"%s",Src+idx);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "note" ) )
        {
            Report(ps,REP_INFO,"%s",Src+idx);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "include" ) )
        {
            if( !LoadInclude( ps, Src+idx ) )
                return(-1);
            goto NEXT_LINE;
        }
        if( !stricmp( word, "define" ) )
        {
            EquateProcess( ps, Src+idx );
            goto NEXT_LINE;
        }
        if( !stricmp( word, "undef" ) )
        {
            UndefProcess( ps, Src+idx );
            goto NEXT_LINE;
        }
        Report(ps,REP_ERROR,"Unknown # directive");
        return(-1);
    }

    /*
    // Not '#' directive, process as string
    */

    if( ccDepth && !(ccStateFlags[ccDepth-1]&CCSTATEFLG_TRUE) )
        goto NEXT_LINE;

    idx = 0;
    if( !ParseSource( ps, Src, Dst, &idx, MaxLen ) )
        return(0);

    Dst[idx] = 0;
    return(idx);
}

/*
// ppCleanup
//
// Clean up the pre-processor environment
//
// void
*/
void ppCleanup()
{
    ccDepth = 0;
    while( pEqList )
        EquateDestroy( pEqList );
}


/*
// EquateCreate
//
// Creates an equate record
//
// Returns 0 on success, -1 on error
*/
int EquateCreate( SOURCEFILE *ps, char *Name, char *Value )
{
    EQUATE *pd;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,Name) )
        return(-1);

    /* Make sure its not a too long */
    if( strlen(Name)>=EQUATE_NAME_LEN )
        { Report(ps,REP_ERROR,"Equate name '%s' too long",Name); return(-1); }
    if( strlen(Value)>=EQUATE_DATA_LEN )
        { Report(ps,REP_ERROR,"Equate data '%s' too long",Value); return(-1); }

    /* Allocate a new record */
    pd = malloc(sizeof(EQUATE));
    if( !pd )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(-1); }

    /* Load in the name and data */
    strcpy( pd->name, Name );
    strcpy( pd->data, Value );

    /* Put this equate in the master list */
    pd->Busy  = 0;
    pd->pPrev = 0;
    pd->pNext = pEqList;
    if( pEqList )
        pEqList->pPrev = pd;
    pEqList   = pd;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DEFINE : '%s' = '%s'\n",
                            ps->SourceName,ps->CurrentLine,pd->name,pd->data);

    return(0);
}


/*
// CheckEquate
//
// Searches for an equate by name.
//
// Returns 1 on success, 0 on error
*/
int CheckEquate( char *name )
{
    if( EquateFind(name) )
        return(1);
    return(0);
}


/*===================================================================
//
// Private Functions
//
====================================================================*/

/*
// ReadCharacter
//
// Read a charater from the source file and track line and column
//
// Returns character or -1 on error
*/
static int ReadCharacter( SOURCEFILE *ps )
{
    int i;
    char c;

AGAIN:
    i = fread( &c, 1, 1, ps->FilePtr );
    if( i != 1 )
        return(-1);
    if( c == 0xd )
        goto AGAIN;
    if( ps->LastChar == 0xa )
    {
        ps->CurrentLine++;
        ps->CurrentColumn=1;
    }
    else
        ps->CurrentColumn++;
    ps->LastChar = c;

    return(c);
}


/*
// GetTextLine
//
// Gets a line of text from the source file without # directive
// processing, or processing past a EOL.
//
// Returns 1 on success, 0 on error
*/
static int GetTextLine( SOURCEFILE *ps, char *Dst, int MaxLen, int *pLength, int *pEOF )
{
    int c;
    int  idx;
    int  commentFlag,quoteFlag;

    /* Remove leading white space */
    do
    {
        c = ReadCharacter( ps );
    } while( c==' ' || c==0x9 || c==0xa );

    /*
    // Process line watching for comments and quotes
    */
    idx=0;
    commentFlag=0;
    quoteFlag=0;
    for(;;)
    {
        /* Process quotes and comments */
        if( c=='"' )
            quoteFlag^=1;

        if( quoteFlag )
            commentFlag=0;

        if( (commentFlag && c=='/') || (!quoteFlag && c==';') )
        {
            if( c=='/' && idx>0 )
                idx--;
            while( c!=0 && c!=-1 && c!=0xa )
                c = ReadCharacter( ps );
            break;
        }

        if( c=='/' )
            commentFlag=1;
        else
            commentFlag=0;

        /* If this character terminated the line, break now */
        if( c==0 || c==-1 || c==0xa )
            break;

        /* We didn't consume this charater */
        if( idx<(MaxLen-1) )
            Dst[idx++]=c;
        else
            { Report(ps,REP_ERROR,"Line too long"); return(0); }

        c = ReadCharacter( ps );
    }

    /* Back off white space */
    while( idx>0 && (Dst[idx-1]==' ' || Dst[idx-1]==0x9) )
        idx--;

    /* Null terminate the output */
    Dst[idx] = 0;

    if( quoteFlag )
        Report(ps,REP_ERROR,"Open Quotes");

    if( pLength )
        *pLength = idx;

    if( pEOF )
    {
        if( idx || c!=-1 )
            *pEOF=0;
        else
            *pEOF=1;
    }

    return(1);
}


/*
// ParseSource
//
// Parses the source string, expanding any equates
//
// Returns 1 on success, 0 on error
*/
#define WF_READY    0
#define WF_NONLABEL 1
#define WF_QUOTED   2
#define WF_LABEL    3
static int ParseSource( SOURCEFILE *ps, char *Src, char *Dst, int *pIdx, int MaxLen )
{
    char c,word[TOKEN_MAX_LEN];
    int  i,srcIdx,dstIdx,wordIdx;
    int  wordFlag;

    srcIdx = 0;
    dstIdx = *pIdx;
    wordIdx = 0;

    wordFlag=WF_READY;
    for(;;)
    {
        c = Src[srcIdx++];

        /* See if we go into label or non-label mode */
        if( wordFlag==WF_READY )
        {
            if( c=='"' )
                wordFlag=WF_QUOTED;
            else if( LabelChar(c,1) )
            {
                wordFlag=WF_LABEL;
                wordIdx=0;
                word[wordIdx++]=c;
                continue;
            }
            else if( LabelChar(c,0) )
                wordFlag=WF_NONLABEL;
        }
        /* See if we fall out of non-label mode */
        else if( wordFlag==WF_NONLABEL )
        {
            if( c=='"' )
                wordFlag=WF_QUOTED;
            else if( !LabelChar(c,0) )
                wordFlag=WF_READY;
        }
        /* See if we fall out of label mode */
        else if( wordFlag==WF_LABEL )
        {
            if( (wordIdx>=(TOKEN_MAX_LEN-1)) || !LabelChar(c,0) )
            {
                /* Here we are teminating the word and checking it */
                EQUATE *peq;
                if( c=='"' )
                    wordFlag=WF_QUOTED;
                else
                    wordFlag=WF_READY;
                word[wordIdx]=0;
                peq = EquateFind(word);
                /* See if equate exists and is free */
                if( peq && !peq->Busy )
                {
                    /* Mark as busy, process, then mark as free */
                    peq->Busy=1;
                    i = ParseSource( ps, peq->data, Dst, &dstIdx, MaxLen );
                    peq->Busy=0;

                    /* If there was an error, return now */
                    if( !i )
                        return(0);
                }
                else
                {
                    /* The word is not a EQUATE */
                    for(i=0;i<wordIdx;i++)
                    {
                        if( dstIdx<(MaxLen-1) )
                            Dst[dstIdx++]=word[i];
                        else
                            { Report(ps,REP_ERROR,"Line too long"); return(0); }
                    }
                }
            }
            else
            {
                /* Here we are building the word */
                word[wordIdx++]=c;
                continue;
            }
        }
        /* See if we fall out of quoted mode */
        else if( wordFlag==WF_QUOTED )
        {
            if( c=='"' )
                wordFlag=WF_READY;
        }

        /* If this character terminated the line, break now */
        if( !c )
        {
            if( wordFlag==WF_QUOTED )
                Report(ps,REP_ERROR,"Non-terminated string");
            break;
        }

        /* We didn't consume this charater */
        if( dstIdx<(MaxLen-1) )
            Dst[dstIdx++]=c;
        else
            { Report(ps,REP_ERROR,"Line too long"); return(0); }
    }

    *pIdx = dstIdx;
    return(1);
}


/*
// LoadInclude
//
// Processes a #include command
//
// Returns 1 on success, 0 on error
*/
static int LoadInclude( SOURCEFILE *ps, char *Src )
{
    char c,term;
    char NewFileName[SOURCE_BASE_DIR];
    int  rc,idx,oldidx,srcIdx;
    SOURCEFILE *psNew;

    srcIdx=0;

    /* Remove leading white space */
    do
    {
        c = Src[srcIdx++];
    } while( c==' ' || c==0x9 );

    /* Charater must be '"' or '<' */
    if( c=='"' )
    {
        term = '"';
        strcpy( NewFileName, ps->SourceBaseDir );
        idx = strlen(NewFileName);
    }
    else if( c=='<' )
    {
        term = '>';
        idx=0;
    }
    else
        { Report(ps,REP_ERROR,"Expected \" or < after #include"); return(0); }

    /* Read in the filename to the terminating character */
    // Check for include paths that start with "/" or "\"
    // (assume an absolute path)
    if( Src[srcIdx]=='/' || Src[srcIdx]=='\\' )
        idx = 0;
    oldidx = idx;
    for(;;)
    {
        c = Src[srcIdx++];
        // Check for include paths that include a ":"
        // (assume a driver letter preceeded)
        if( c==':' && idx>0 )
        {
            NewFileName[0]=NewFileName[idx-1];
            idx = 1;
            oldidx = idx;
        }
        if( c==term )
            break;
        if( !c )
            { Report(ps,REP_ERROR,"Bad filename in #include"); return(0); }
        if( idx >= (SOURCE_BASE_DIR-1) )
            { Report(ps,REP_ERROR,"Filename too long in #include"); return(0); }
        NewFileName[idx++]=c;
    }

    /* The line should be done now */
    if( Src[srcIdx] )
        { Report(ps,REP_ERROR,"Expected EOL after '%s'",NewFileName); return(0); }

    /* Null terminate the filename and make sure something got copied */
    NewFileName[idx]=0;
    if( idx == oldidx )
        { Report(ps,REP_ERROR,"Null filename in #include"); return(0); }

    /* Open the new file */
    if( !(psNew=InitSourceFile(ps, NewFileName)) )
        return(0);

    /* Process the new file */
    rc = ProcessSourceFile( psNew );

    /* Free the file block */
    CloseSourceFile( psNew );

    if( !rc && Pass==2 )
        return(0);
    else
        return(1);
}

/*
// EquateProcess
//
// Processes a #define command
//
// Returns 1 on success, 0 on error
*/
static int EquateProcess( SOURCEFILE *ps, char *Src )
{
    EQUATE *pd,*pdTmp;
    char c;
    int  idx,srcIdx;

    /* Allocate a new record */
    pd = malloc(sizeof(EQUATE));
    if( !pd )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(0); }

    srcIdx=0;

    /* Remove leading white space */
    do
    {
        c = Src[srcIdx++];
    } while( c==' ' || c==0x9 );

    /* Character must be a legal label */
    if( !LabelChar(c,1) )
        { Report(ps,REP_ERROR,"Illegal label"); free(pd); return(0); }

    /* The name can only be delimited by a white space */
    /* Note: We now allow a NULL for a #define with no value */
    idx=0;
    for(;;)
    {
        pd->name[idx++]=c;
        c = Src[srcIdx++];
        if( !c || c==' ' || c==0x9 )
            break;
        if( !LabelChar(c,0) )
            { Report(ps,REP_ERROR,"Illegal #define"); free(pd); return(0); }
        if( idx >= (EQUATE_NAME_LEN-1) )
            { Report(ps,REP_ERROR,"Label too long"); free(pd); return(0); }
    }
    pd->name[idx]=0;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,pd->name) )
    {
        free(pd);
        return(0);
    }

    /* Remove leading white space (unless we already hit EOL) */
    if( c )
        do
        {
            c = Src[srcIdx++];
        } while( c==' ' || c==0x9 );

    /* Load in the text part of the equate (defaul to "1" if no value) */
    if( !c )
        strcpy( pd->data, "1" );
    else
        strcpy( pd->data, Src+srcIdx-1 );

    /* Check for dedefinition, but ignore exact duplicates */
    if( (pdTmp = EquateFind(pd->name)) != 0 )
    {
        idx = strcmp( pd->data, pdTmp->data );
        if( !idx )
        {
            free(pd);
            return(1);
        }
        EquateDestroy(pdTmp);
        Report(ps,REP_WARN1,"Redefinition of equate '%s'",pd->name);
    }

    /* Put this equate in the master list */
    pd->Busy  = 0;
    pd->pPrev = 0;
    pd->pNext = pEqList;
    if( pEqList )
        pEqList->pPrev = pd;
    pEqList   = pd;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DEFINE : '%s' = '%s'\n",
                            ps->SourceName,ps->CurrentLine,pd->name,pd->data);

    return(1);
}


/*
// UndefProcess
//
// Processes a #undef command
//
// Returns 1 on success, 0 on error
*/
static int UndefProcess( SOURCEFILE *ps, char *Src )
{
    EQUATE *pdTmp;
    char c,name[EQUATE_NAME_LEN];
    int  idx,srcIdx;

    srcIdx=0;

    /* Remove leading white space */
    do
    {
        c = Src[srcIdx++];
    } while( c==' ' || c==0x9 );

    /* Character must be a legal label */
    if( !LabelChar(c,1) )
        { Report(ps,REP_ERROR,"Illegal label"); return(0); }

    /* The name is delimited by EOL */
    idx=0;
    for(;;)
    {
        name[idx++]=c;
        c = Src[srcIdx++];
        if( !c )
            break;
        if( c==' ' || c==0x9 )
            { Report(ps,REP_ERROR,"Unexpected additional characters on line"); return(0); }
        if( !LabelChar(c,0) )
            { Report(ps,REP_ERROR,"Illegal name"); return(0); }
        if( idx >= (EQUATE_NAME_LEN-1) )
            { Report(ps,REP_ERROR,"Label too long"); return(0); }
    }
    name[idx]=0;

    /* Check for dedefinition */
    if( !(pdTmp = EquateFind(name)) )
        { Report(ps,REP_WARN1,"Undef attempt on undfined '%s'",name); return(1); }

    EquateDestroy(pdTmp);

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : UNDEF  : '%s'\n",
                            ps->SourceName,ps->CurrentLine,name);

    return(1);
}


/*
// EquateFind
//
// Searches for an equate by name. If found, returns the record pointer.
//
// Returns EQUATE * on success, 0 on error
*/
static EQUATE *EquateFind( char *name )
{
    EQUATE *peq;

    peq = pEqList;
    while( peq )
    {
        if( !strcmp( name, peq->name ) )
            break;
        peq = peq->pNext;
    }
    return(peq);
}


/*
// EquateDestroy
//
// Frees an equate record.
//
// void
*/
static void EquateDestroy( EQUATE *peq )
{
    if( !peq->pPrev )
        pEqList = peq->pNext;
    else
        peq->pPrev->pNext = peq->pNext;

    if( peq->pNext )
        peq->pNext->pPrev = peq->pPrev;

    free(peq);
}


/*
// IfDefProcess
//
// Processes a #ifdef command
//
// Returns 1 on success, 0 on error
*/
static int IfDefProcess( SOURCEFILE *ps, char *Src, int fTrue )
{
    char c,name[EQUATE_NAME_LEN];
    int  idx,srcIdx;

    /* Check depth */
    if( ccDepth==CC_MAX_DEPTH )
        { Report(ps,REP_ERROR,"Conditional nesting limit exceeded"); return(0); }

    /* If we are already in a false if, just create another false here to track nesting */
    if( ccDepth && !(ccStateFlags[ccDepth-1]&CCSTATEFLG_TRUE) )
    {
        ccStateFlags[ccDepth++] = 0;

        if( Pass==1 && (Options & OPTION_DEBUG) )
            printf("%s(%5d) : IFDEF  : <skipped>\n",ps->SourceName,ps->CurrentLine);

        return(1);
    }

    srcIdx=0;

    /* Remove leading white space */
    do
    {
        c = Src[srcIdx++];
    } while( c==' ' || c==0x9 );

    /* Character must be a legal label */
    if( !LabelChar(c,1) )
        { Report(ps,REP_ERROR,"Illegal label"); return(0); }

    /* The name is delimited by EOL */
    idx=0;
    for(;;)
    {
        name[idx++]=c;
        c = Src[srcIdx++];
        if( !c )
            break;
        if( c==' ' || c==0x9 )
            { Report(ps,REP_ERROR,"Unexpected additional characters on line"); return(0); }
        if( !LabelChar(c,0) )
            { Report(ps,REP_ERROR,"Illegal name"); return(0); }
        if( idx >= (EQUATE_NAME_LEN-1) )
            { Report(ps,REP_ERROR,"Label too long"); return(0); }
    }
    name[idx]=0;

    ccStateFlags[ccDepth] = 0;

    /* Check for dedefinition */
    if( EquateFind(name) )
        ccStateFlags[ccDepth] = CCSTATEFLG_TRUE;

    /* Toggle the state for ifndef */
    if( !fTrue )
        ccStateFlags[ccDepth] ^= CCSTATEFLG_TRUE;

    ccDepth++;

    if( Pass==1 && (Options & OPTION_DEBUG) )
    {
        if( fTrue )
            printf("%s(%5d) : IFDEF  : '%s' (Result=%d)\n",
                                ps->SourceName,ps->CurrentLine,name,ccStateFlags[ccDepth-1]);
        else
            printf("%s(%5d) : IFNDEF : '%s' (Result=%d)\n",
                                ps->SourceName,ps->CurrentLine,name,ccStateFlags[ccDepth-1]);
    }

    return(1);
}


/*
// ElseProcess
//
// Processes a #else command
//
// Returns 1 on success, 0 on error
*/
static int ElseProcess( SOURCEFILE *ps, char *Src )
{
    int i;

    if( *Src )
        { Report(ps,REP_ERROR,"Unexpected additional characters on line"); return(0); }

    /* Make sure #else is legal here */
    if( !ccDepth || (ccStateFlags[ccDepth-1]&CCSTATEFLG_ELSE) )
        { Report(ps,REP_ERROR,"Multiple #else or use without corresponding #if"); return(0); }

    /* Mark it as used */
    ccStateFlags[ccDepth-1] |= CCSTATEFLG_ELSE;

    /* Toggle the TRUE state */
    ccStateFlags[ccDepth-1] ^= CCSTATEFLG_TRUE;

    /* If we are already in a nested false if, keep expession false */
    for( i=0; i<(int)(ccDepth-1); i++ )
        if( !(ccStateFlags[i]&CCSTATEFLG_TRUE) )
            ccStateFlags[ccDepth-1] &= ~CCSTATEFLG_TRUE;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : ELSE   : (Result=%d)\n",
                            ps->SourceName,ps->CurrentLine,ccStateFlags[ccDepth-1]&CCSTATEFLG_TRUE);

    return(1);
}


/*
// EndifProcess
//
// Processes a #endif command
//
// Returns 1 on success, 0 on error
*/
static int EndifProcess( SOURCEFILE *ps, char *Src )
{
    if( *Src )
        { Report(ps,REP_ERROR,"Unexpected additional characters on line"); return(0); }

    /* Make sure #else is legal here */
    if( !ccDepth  )
        { Report(ps,REP_ERROR,"#endif without corresponding #if"); return(0); }

    ccDepth--;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : ENDIF  :\n",
                            ps->SourceName,ps->CurrentLine);

    return(1);
}
