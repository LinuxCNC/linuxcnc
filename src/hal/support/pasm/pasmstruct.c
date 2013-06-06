/*
 * pasmstruct.c
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
// File     : pasmstruct.c
//
// Description:
//     Processes the scruct and scope commands
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

/* Struct Record */
#define STRUCT_NAME_LEN  TOKEN_MAX_LEN
#define STRUCT_MAX_ELEM  64
typedef struct _STRUCT {
    struct _STRUCT  *pPrev;         /* Previous in STRUCT list */
    struct _STRUCT  *pNext;         /* Next in STRUCT list */
    char            Name[STRUCT_NAME_LEN];
    int             Elements;       /* Element Count */
    uint            TotalSize;      /* Total Size */
    uint            Offset[STRUCT_MAX_ELEM];    /* Element Offset */
    uint            Size[STRUCT_MAX_ELEM];      /* Element Size */
    char            ElemName[STRUCT_MAX_ELEM][STRUCT_NAME_LEN];
} STRUCT;

/* Assignment Record */
typedef struct _ASSIGN {
    struct _ASSIGN  *pPrev;         /* Previous in ASSIGN list */
    struct _ASSIGN  *pNext;         /* Next in ASSIGN list */
    char            Name[STRUCT_NAME_LEN];
    char            BaseReg[STRUCT_NAME_LEN];
    int             Elements;       /* Element Count */
    uint            TotalSize;      /* Total Size */
    char            ElemName[STRUCT_MAX_ELEM][STRUCT_NAME_LEN];
    char            MappedReg[STRUCT_MAX_ELEM][STRUCT_NAME_LEN];
    uint            Offset[STRUCT_MAX_ELEM];    /* Element Offset */
    uint            Size[STRUCT_MAX_ELEM];      /* Element Size */
} ASSIGN;

/* Scope Record */
#define SCOPE_NAME_LEN TOKEN_MAX_LEN
typedef struct _SCOPE {
    struct _SCOPE   *pPrev;         /* Previous in SCOPE list */
    struct _SCOPE   *pNext;         /* Next in SCOPE list */
    uint            Flags;
#define SCOPE_FLG_OPEN      (1<<0)
    char            Name[SCOPE_NAME_LEN];
    struct _SCOPE   *pParent;        /* Current SCOPE when created */
    struct _ASSIGN  *pAssignList;    /* ASSIGN list */
} SCOPE;


/* Local Support Funtions */
static STRUCT *StructFind( char *Name );
static STRUCT *StructCreate( SOURCEFILE *ps, char *Name );
static void StructDestroy( STRUCT *pst );
static int GetRegname( SOURCEFILE *ps, uint element, char *str, uint off, uint size );
static ASSIGN *AssignFind( char *Name );
static ASSIGN *AssignCreate( SOURCEFILE *ps, ASSIGN **pList, char *Name );
static void AssignDestroy( ASSIGN **pList, ASSIGN *pas );
static char *StructNameCheck( char *source );
static int StructValueOperand( char *source, int CmdType, uint *pValue );
#define SVO_SIZEOF  0
#define SVO_OFFSET  1
static int GetFinalSize( char *ext, uint Size, uint *pValue );
static int GetFinalOffset( char *ext, uint Size, uint Offset, uint *pValue );
static SCOPE *ScopeCreate( SOURCEFILE *ps, char *Name );
static void ScopeDestroy( SCOPE *psc );
static void ScopeClose( SCOPE *psc );
static SCOPE *ScopeFind( char *Name );


/* Local structure lists */
STRUCT *pStructList=0;      /* List of declared structs */
STRUCT *pStructCurrent=0;

SCOPE  *pScopeList=0;       /* List of desclared scopes */
SCOPE  *pScopeCurrent=0;

/*===================================================================
//
// Public Functions
//
====================================================================*/

/*
// ScopeEnter
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeEnter( SOURCEFILE *ps, char *Name )
{
    if( Core == CORE_V0 )
        { Report(ps,REP_ERROR,".enter illegal with specified core version"); return(-1); }
    if( !ScopeCreate(ps, Name) )
        return(-1);
    return(0);
}


/*
// ScopeLeave
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeLeave( SOURCEFILE *ps, char *Name )
{
    SCOPE *psc;

    psc = ScopeFind(Name);
    if( !psc )
        { Report(ps,REP_ERROR,"Scope name undefined"); return(-1); }
    if( !(psc->Flags&SCOPE_FLG_OPEN) )
        { Report(ps,REP_ERROR,"Scope is not open"); return(-1); }
    ScopeClose(psc);
    return(0);
}


/*
// ScopeUsing
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int ScopeUsing( SOURCEFILE *ps, char *Name )
{
    SCOPE *psc;

    psc = ScopeFind(Name);
    if( !psc )
        { Report(ps,REP_ERROR,"Scope name undefined"); return(-1); }
    if( psc->Flags&SCOPE_FLG_OPEN )
        { Report(ps,REP_ERROR,"Scope is already open"); return(-1); }
    psc->Flags |= SCOPE_FLG_OPEN;
    pScopeCurrent = psc;
    return(0);
}


/*
// StructInit
//
// Returns: void
*/
void StructInit()
{
    ScopeCreate(0,"_ROOT_");
}


/*
// StructCleanup
//
// Returns: void
*/
void StructCleanup()
{
    while( pScopeList )
        ScopeDestroy( pScopeList );
    while( pStructList )
        StructDestroy( pStructList );
}


/*
// StructNew
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int StructNew( SOURCEFILE *ps, char *Name )
{
    if( pStructCurrent )
        { Report(ps,REP_ERROR,"Structure can not be nested"); return(-1); }
    if( Core == CORE_V0 )
        { Report(ps,REP_ERROR,".struct illegal with specified core version"); return(-1); }
    pStructCurrent=StructCreate(ps, Name);
    if( !pStructCurrent )
        return(-1);
    return(0);
}


/*
// StructEnd
//
// Returns:
//    0 - Success
//   -1 - Error
*/
int StructEnd(SOURCEFILE *ps)
{
    if( !pStructCurrent )
        { Report(ps,REP_ERROR,"Structure .struct/.ends mismatch"); return(-1); }
    if( !pStructCurrent->Elements )
        { Report(ps,REP_ERROR,"Structure must have at least 1 element"); pStructCurrent=0; return(-1); }
    pStructCurrent = 0;
    return(0);
}


/*
// StructAddElement
//
// Create a new structure record
//
// Returns 0 on success, -1 on error
*/
int StructAddElement( SOURCEFILE *ps, char *Name, uint size )
{
    STRUCT *pst = pStructCurrent;
    int i;

    if( !pst )
        { Report(ps,REP_ERROR,"Can not add element - missing .struct"); return(-1); }

    /* Make sure its not a too long */
    if( strlen(Name)>=STRUCT_NAME_LEN )
        { Report(ps,REP_ERROR,"Element name too long"); return(-1); }

    /* Make sure its not a reserved word */
    if( CheckTokenType(Name)!=TOKENTYPE_UNRESERVED )
        { Report(ps,REP_ERROR,"Illegal use of reserved word '%s'",Name); return(0); }

    /* Check for too many elements */
    if( pst->Elements==STRUCT_MAX_ELEM )
        { Report(ps,REP_ERROR,"Max element count (%d) exceeded",STRUCT_MAX_ELEM); return(-1); }

    /* Check for duplicate element name */
    for( i=0; i<pst->Elements; i++ )
        if( !strcmp(Name,pst->ElemName[i]) )
            break;
    if( i!= pst->Elements )
        { Report(ps,REP_ERROR,"Duplicate element name"); return(-1); }

    /* Add the element */
    strcpy( pst->ElemName[pst->Elements], Name );
    pst->Offset[pst->Elements] = pst->TotalSize;
    pst->Size[pst->Elements] = size;
    pst->TotalSize += size;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DOTCMD : Element '%s' declared, offset=%d, size=%d\n",
                            ps->SourceName,ps->CurrentLine,pst->ElemName[pst->Elements],
                            pst->Offset[pst->Elements],pst->Size[pst->Elements]);

    pst->Elements++;

    return(0);
}


/*
// StructAssign
//
// Assign a structure to an instance
//
// Returns 0 on success, -1 on error
*/
int StructAssign( SOURCEFILE *ps, char *structName, char *rsName,
                         char *reName, char *defName )
{
    STRUCT   *pst;
    ASSIGN   *pas;
    PRU_ARG rs, re;
    char     tmpData[EQUATE_DATA_LEN];
    uint     startOff,endOff,tmp,rangeCheck;
    int      i;

    if( pStructCurrent )
        { Report(ps,REP_ERROR,"Cannot assign while defining a structure"); return(-1); }
    if( !pScopeCurrent )
        { Report(ps,REP_ERROR,"Cannot assign outside of a scope"); return(-1); }

    if( strcmp( reName, "*" ) )
        rangeCheck = 1;
    else
        rangeCheck = 0;

    /* Get the register parameters */
    if( !GetRegister( ps, 2, rsName, &rs, 0, 0 ) )
        return -1;
    if( rangeCheck && !GetRegister( ps, 3, reName, &re, 0, 0 ) )
        return -1;

    /* Set the struct */
    pst = StructFind( structName );
    if( !pst )
        { Report(ps,REP_ERROR,"Structure '%s' not defined",structName); return(-1); }


    if( !(Options & OPTION_BIGENDIAN) )
    {
        /*
        // ** Little Endian Version **
        */

        /* See if the range is ok */
        startOff = rs.Value * 4;
        switch( rs.Field )
        {
        case FIELDTYPE_15_8:
        case FIELDTYPE_23_8:
            startOff += 1;
            break;
        case FIELDTYPE_23_16:
        case FIELDTYPE_31_16:
            startOff += 2;
            break;
        case FIELDTYPE_31_24:
            startOff += 3;
            break;
        }
        if( rangeCheck )
        {
            endOff = re.Value * 4;
            switch( re.Field )
            {
            case FIELDTYPE_7_0:
                endOff += 1;
                break;
            case FIELDTYPE_15_8:
            case FIELDTYPE_15_0:
                endOff += 2;
                break;
            case FIELDTYPE_23_16:
            case FIELDTYPE_23_8:
                endOff += 3;
                break;
            case FIELDTYPE_31_24:
            case FIELDTYPE_31_16:
            case FIELDTYPE_31_0:
                endOff += 4;
                break;
            }
            tmp = startOff + pst->TotalSize;

            if( tmp != endOff )
            {
                if( !(tmp&3) )
                    GetRegname( ps, 0, tmpData, tmp-4, 4 );
                else if( !(tmp&1) )
                    GetRegname( ps, 0, tmpData, tmp-2, 2 );
                else
                    GetRegname( ps, 0, tmpData, tmp-1, 1 );
                Report(ps,REP_ERROR,"Range error, parameter 3 should be '%s'",tmpData);
                return -1;
            }
        }
    }
    else
    {
        /*
        // ** Big Endian Version **
        */

        /* See if the range is ok */
        startOff = rs.Value * 4;
        switch( rs.Field )
        {
        case FIELDTYPE_23_8:
        case FIELDTYPE_23_16:
            startOff += 1;
            break;
        case FIELDTYPE_15_8:
        case FIELDTYPE_15_0:
            startOff += 2;
            break;
        case FIELDTYPE_7_0:
            startOff += 3;
            break;
        }
        if( rangeCheck )
        {
            endOff = re.Value * 4;
            switch( re.Field )
            {
            case FIELDTYPE_31_24:
                endOff += 1;
                break;
            case FIELDTYPE_23_16:
            case FIELDTYPE_31_16:
                endOff += 2;
                break;
            case FIELDTYPE_15_8:
            case FIELDTYPE_23_8:
                endOff += 3;
                break;
            case FIELDTYPE_7_0:
            case FIELDTYPE_15_0:
            case FIELDTYPE_31_0:
                endOff += 4;
                break;
            }
            tmp = startOff + pst->TotalSize;

            if( tmp != endOff )
            {
                if( !(tmp&3) )
                    GetRegname( ps, 0, tmpData, tmp-4, 4 );
                else if( !(tmp&1) )
                    GetRegname( ps, 0, tmpData, tmp-2, 2 );
                else
                    GetRegname( ps, 0, tmpData, tmp-1, 1 );
                Report(ps,REP_ERROR,"Range error, parameter 3 should be '%s'",tmpData);
                return -1;
            }
        }
    }

    /* Alignment check pass */
    tmp = startOff;
    for(i=0; i<pst->Elements; i++)
    {
        if( GetRegname( ps, i+1, tmpData, tmp, pst->Size[i] )<0 )
             return(-1);
        tmp += pst->Size[i];
    }

    if( !(pas = AssignCreate( ps, &pScopeCurrent->pAssignList, defName )) )
        return(-1);

    pas->Elements = pst->Elements;
    pas->TotalSize = pst->TotalSize;

    /* Equate create pass */
    for(i=0; i<pst->Elements; i++)
    {
        GetRegname( ps, i+1, tmpData, startOff, pst->Size[i] );
        if( !i )
        {
            strcpy(pas->Name,defName);
            strcpy(pas->BaseReg,tmpData);
        }
        pas->Offset[i] = pst->Offset[i];
        pas->Size[i] = pst->Size[i];
        strcpy(pas->ElemName[i],pst->ElemName[i]);
        strcpy(pas->MappedReg[i],tmpData);
        startOff += pst->Size[i];
    }

    return(0);
}


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
int StructParamProcess( SOURCEFILE *ps, int ParamIdx, char *source )
{
    char substr[TOKEN_MAX_LEN*2];
    int  subidx,srcidx;
    char tmpname[STRUCT_NAME_LEN],*pNewName;
    int i,j,onedot;

    srcidx=0;
    subidx=0;
    while(source[srcidx])
    {
        /* Scan past any number constants */
        if( source[srcidx]>='0' && source[srcidx]<='9' )
        {
            do
            {
                substr[subidx++] = source[srcidx++];
            } while( LabelChar(source[srcidx],0) );
        }
        /* Scan past any non-label charaters */
        else if( !LabelChar(source[srcidx],1) )
            substr[subidx++] = source[srcidx++];
        else
        {
            /* Scan in the candidate name */
            onedot = 0;
            tmpname[0] = source[srcidx];
            for(i=1; i<STRUCT_NAME_LEN; i++)
            {
                if( source[srcidx+i]=='.' )
                {
                    if( onedot )
                        break;
                    onedot = 1;
                }
                else if( !LabelChar(source[srcidx+i],0) )
                    break;

                tmpname[i] = source[srcidx+i];
            }

            if( i==STRUCT_NAME_LEN )
                { Report(ps,REP_ERROR,"Parameter %d: Name too long",ParamIdx); return(-1); }
            tmpname[i] = 0;

            /* Check for a structure command */
            if( !strcmp(tmpname,"SIZE") || !strcmp(tmpname,"OFFSET") )
            {
                int cmd;
                uint value;

                if( !strcmp(tmpname,"SIZE") )
                    cmd = SVO_SIZEOF;
                else
                    cmd = SVO_OFFSET;

                srcidx+=i;

                /* Remove WS */
                while( source[srcidx]==9 || source[srcidx]==' ' )
                    srcidx++;
                /* This char must be a '(' */
                if( source[srcidx]!='(' )
                    { Report(ps,REP_ERROR,"Missing '(' on SIZE/OFFSET"); return(-1); }
                srcidx++;
                /* Remove WS */
                while( source[srcidx]==9 || source[srcidx]==' ' )
                    srcidx++;

                /* Get the name */
                for(i=0; i<STRUCT_NAME_LEN; i++)
                {
                    if( !LabelChar(source[srcidx+i],0) && source[srcidx+i]!='.' )
                        break;
                    tmpname[i] = source[srcidx+i];
                }
                if( !i )
                    { Report(ps,REP_ERROR,"Malformed SIZE/OFFSET operand"); return(-1); }
                tmpname[i] = 0;
                srcidx+=i;

                /* Remove WS */
                while( source[srcidx]==9 || source[srcidx]==' ' )
                    srcidx++;
                /* This char must be a ')' */
                if( source[srcidx]!=')' )
                    { Report(ps,REP_ERROR,"Missing ')' on SIZE/OFFSET"); return(-1); }
                srcidx++;

                if( StructValueOperand(tmpname, cmd, &value)<0 )
                    { Report(ps,REP_ERROR,"Illegal SIZE/OFFSET operand '%s'",tmpname); return(-1); }

                sprintf(tmpname," %d ",value);
                for( i=0; tmpname[i]; i++ )
                    substr[subidx++] = tmpname[i];
            }
            else
            {
                /* Check for a replacement */
                pNewName = StructNameCheck(tmpname);

                /* Copy either the original or the replacement */
                if( !pNewName )
                {
                    for( j=0; j<i; j++ )
                        substr[subidx++] = source[srcidx++];
                }
                else
                {
                    srcidx += i;
                    while( *pNewName )
                        substr[subidx++] = *pNewName++;
                }

                /* Any more '.' are just register field modifiers */
                if( source[srcidx]=='.' )
                {
                    do
                    {
                        substr[subidx++] = source[srcidx++];
                    } while( LabelChar(source[srcidx],0) || source[srcidx]=='.' );
                }
            }
        }
    }
    substr[subidx++] = 0;
    if( subidx >= TOKEN_MAX_LEN )
        return -1;
    strcpy( source, substr );

    return(0);
}


/*
// CheckStruct
//
// Searches for struct template or struct by name.
//
// Returns 1 on success, 0 on error
*/
int CheckStruct( char *name )
{
    if( StructFind(name) || AssignFind(name) || ScopeFind(name) )
        return(1);
    return(0);
}


/*===================================================================
//
// Private Functions
//
====================================================================*/

/*
// StructFind
//
// Searches for a struct record by name. If found, returns the record pointer.
//
// Returns STRUCT * on success, 0 on error
*/
static STRUCT *StructFind( char *Name )
{
    STRUCT *pst;

    pst = pStructList;
    while( pst )
    {
        if( !strcmp( Name, pst->Name ) )
            break;
        pst = pst->pNext;
    }
    return(pst);
}


/*
// StructCreate
//
// Create a new structure record
//
// Returns STRUCT * on success, 0 on error
*/
static STRUCT *StructCreate( SOURCEFILE *ps, char *Name )
{
    STRUCT *pst;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,Name) )
        return(0);

    /* Make sure its not too long */
    if( strlen(Name)>=STRUCT_NAME_LEN )
        { Report(ps,REP_ERROR,"Structure name too long"); return(0); }

    /* Allocate a new record */
    pst = malloc(sizeof(STRUCT));
    if( !pst )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(0); }

    strcpy( pst->Name, Name );
    pst->Elements  = 0;
    pst->TotalSize = 0;

    /* Put this equate in the master list */
    pst->pPrev  = 0;
    pst->pNext  = pStructList;
    pStructList = pst;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DOTCMD : Structure '%s' declared\n",
                            ps->SourceName,ps->CurrentLine,pst->Name);

    return(pst);
}


/*
// StructDestroy
//
// Frees a structure record.
//
// void
*/
static void StructDestroy( STRUCT *pst )
{
    if( !pst->pPrev )
        pStructList = pst->pNext;
    else
        pst->pPrev->pNext = pst->pNext;

    if( pst->pNext )
        pst->pNext->pPrev = pst->pPrev;

    free(pst);
}



/*
// GetRegname
//
// Checks alignment and returns a string of the proper register
//
// Returns 0 on success, -1 on error
*/
static int GetRegname( SOURCEFILE *ps, uint element, char *str, uint off, uint size )
{
    if( ((off%4)+size) > 4 )
        { Report(ps,REP_ERROR,"Register alignment error on element %d",element); return(-1); }

    if( !(Options & OPTION_BIGENDIAN) )
    {
        /*
        // ** Little Endian Version **
        */
        if( size==4 )
            sprintf( str, "R%d", off/4 );
        else if( size==2 )
            sprintf( str, "R%d.w%d", off/4, off%4 );
        else
            sprintf( str, "R%d.b%d", off/4, off%4 );
    }
    else
    {
        /*
        // ** Big Endian Version **
        */
        if( size==4 )
            sprintf( str, "R%d", off/4 );
        else if( size==2 )
            sprintf( str, "R%d.w%d", off/4, 2-off%4 );
        else
            sprintf( str, "R%d.b%d", off/4, 3-off%4 );
    }

    return(0);
}

/*
// AssignFind
//
// Searches for an assignment record by name. If found, returns the record pointer.
//
// Returns STRUCT * on success, 0 on error
*/
static ASSIGN *AssignFind( char *Name )
{
    SCOPE  *psc;
    ASSIGN *pas;

    psc = pScopeList;
    while( psc )
    {
        if( psc->Flags&SCOPE_FLG_OPEN )
        {
            pas = psc->pAssignList;
            while( pas )
            {
                if( !strcmp( Name, pas->Name ) )
                    return(pas);
                pas = pas->pNext;
            }
        }
        psc = psc->pNext;
    }
    return(0);
}


/*
// AssignCreate
//
// Create a new assignment record
//
// Returns STRUCT * on success, 0 on error
*/
static ASSIGN *AssignCreate( SOURCEFILE *ps, ASSIGN **pList, char *Name )
{
    ASSIGN *pas;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,Name) )
        return(0);

    /* Make sure the name is not too long */
    if( strlen(Name)>=STRUCT_NAME_LEN )
        { Report(ps,REP_ERROR,"Structure name too long"); return(0); }

    /* Allocate a new record */
    pas = malloc(sizeof(ASSIGN));
    if( !pas )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(0); }

    strcpy( pas->Name, Name );

    /* Put this equate in the master list */
    pas->pPrev  = 0;
    pas->pNext  = *pList;
    *pList = pas;

    if( Pass==1 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : DOTCMD : Assignment '%s' declared\n",
                            ps->SourceName,ps->CurrentLine,pas->Name);

    return(pas);
}


/*
// AssignDestroy
//
// Frees an assignment record.
//
// void
*/
static void AssignDestroy( ASSIGN **pList, ASSIGN *pas )
{
    if( !pas->pPrev )
        *pList = pas->pNext;
    else
        pas->pPrev->pNext = pas->pNext;

    if( pas->pNext )
        pas->pNext->pPrev = pas->pPrev;

    free(pas);
}


/*
// Struct Name Check
//
// Returns NULL for not match, or replacement string ptr
//
*/
static char *StructNameCheck( char *source )
{
    ASSIGN *pas;
    char name[STRUCT_NAME_LEN];
    int i,off,more=0;

    for(i=0; i<STRUCT_NAME_LEN; i++)
    {
        name[i] = source[i];
        if( name[i]=='.' || name[i]==0 )
            break;
    }
    if( i==STRUCT_NAME_LEN )
        return(0);

    if( name[i]=='.' )
    {
        more = 1;
        name[i]=0;
    }

    pas = AssignFind(name);
    if( !pas )
        return(0);

    if(!more)
        return(pas->BaseReg);

    off = i+1;
    for(i=0; (i+off)<STRUCT_NAME_LEN; i++)
    {
        name[i] = source[off+i];
        if( name[i]=='.' || name[i]==0 )
            break;
    }

    if( (i+off)==STRUCT_NAME_LEN )
        return(0);

    name[i] = 0;
    for( i=0; i<pas->Elements; i++ )
    {
        if( !stricmp(name,pas->ElemName[i]) )
            return(pas->MappedReg[i]);
    }
    return(0);
}



/*
// Struct Value Operand
//
// Returns 0 for OK, or -1 for Fatal Error
*/
static int StructValueOperand( char *source, int CmdType, uint *pValue )
{
    ASSIGN *pas;
    STRUCT *pst;
    char name[STRUCT_NAME_LEN];
    char name2[STRUCT_NAME_LEN];
    char *ext = 0;
    int i;

    for(i=0; i<STRUCT_NAME_LEN; i++)
    {
        name[i] = source[i];
        if( name[i]=='.' || name[i]==0 )
            break;
    }
    if( i==STRUCT_NAME_LEN )
        return(-1);

    if( name[i]=='.' )
    {
        name[i]=0;
        strcpy(name2,source+i+1);

        /*
        // Look for a name extenion. For example, the "W2" on "MyStruct.MyField.W2"
        */
        for(i=0; i<STRUCT_NAME_LEN; i++)
        {
            if( name2[i]=='.' || name2[i]==0 )
                break;
        }
        if( i==STRUCT_NAME_LEN )
            return(-1);
        if( name2[i]=='.' )
        {
            name2[i]=0;
            ext = name2+i+1;
        }
    }
    else
        name2[0]=0;

    pas = AssignFind(name);
    if( pas )
    {
        if( !strlen(name2) )
        {
            if( CmdType==SVO_SIZEOF )
                *pValue = pas->TotalSize;
            else
                *pValue = 0;
            return(0);
        }

        for( i=0; i<pas->Elements; i++ )
        {
            if( !stricmp(name2,pas->ElemName[i]) )
                break;
        }

        if( i==pas->Elements )
            return(-1);

        if( CmdType==SVO_SIZEOF )
        {
            if(ext)
                return( GetFinalSize( ext, pas->Size[i], pValue ) );
            *pValue = pas->Size[i];
        }
        else
        {
            if(ext)
                return( GetFinalOffset( ext, pas->Size[i], pas->Offset[i], pValue ) );
            *pValue = pas->Offset[i];
        }
        return(0);
    }

    pst = StructFind(name);
    if( pst )
    {
        if( !strlen(name2) )
        {
            if( CmdType==SVO_SIZEOF )
                *pValue = pst->TotalSize;
            else
                *pValue = 0;
            return(0);
        }

        for( i=0; i<pst->Elements; i++ )
        {
            if( !stricmp(name2,pst->ElemName[i]) )
                break;
        }

        if( i==pst->Elements )
            return(-1);

        if( CmdType==SVO_SIZEOF )
        {
            if(ext)
                return( GetFinalSize( ext, pst->Size[i], pValue ) );
            *pValue = pst->Size[i];
        }
        else
        {
            if(ext)
                return( GetFinalOffset( ext, pst->Size[i], pst->Offset[i], pValue ) );
            *pValue = pst->Offset[i];
        }
        return(0);
    }

    return -1;
}


/*
// GetFinalSize - For subfield on struct operand
//
// Returns 0 for OK, or -1 for Fatal Error
*/
static int GetFinalSize( char *ext, uint Size, uint *pValue )
{
    char c;
    uint val;

    if( strlen(ext) > 2 )
        return -1;
    c = toupper(ext[0]);
    val = ext[1] - '0';
    switch(c)
    {
    case 'B':
        if( (val+1)>Size )
            return -1;
        *pValue = 1;
        return(0);
    case 'W':
        if( (val+2)>Size )
            return -1;
        *pValue = 2;
        return(0);
    }
    return(-1);
}

/*
// GetFinalOffset - For subfield on struct operand
//
// Returns 0 for OK, or -1 for Fatal Error
*/
static int GetFinalOffset( char *ext, uint Size, uint Offset, uint *pValue )
{
    char c;
    uint val;

    if( strlen(ext) > 2 )
        return -1;
    c = toupper(ext[0]);
    val = ext[1] - '0';
    switch(c)
    {
    case 'B':
        if( (val+1)>Size )
            return -1;
        if( Options & OPTION_BIGENDIAN )
            val = Size-val-1;
        *pValue = Offset+val;
        return(0);
    case 'W':
        if( (val+2)>Size )
            return -1;
        if( Options & OPTION_BIGENDIAN )
            val = Size-val-2;
        *pValue = Offset+val;
        return(0);
    }
    return(-1);
}


/*
// ScopeCreate
//
// Create a new scope record
//
// Returns STRUCT * on success, 0 on error
*/
static SCOPE *ScopeCreate( SOURCEFILE *ps, char *Name )
{
    SCOPE *psc;

    /* Make sure this name is OK to use */
    if( !CheckName(ps,Name) )
        return(0);

    /* Make sure its not too long */
    if( strlen(Name)>=SCOPE_NAME_LEN )
        { Report(ps,REP_ERROR,"Scope name too long"); return(0); }

    /* Allocate a new record */
    psc = malloc(sizeof(SCOPE));
    if( !psc )
        { Report(ps,REP_ERROR,"Memory allocation failed"); return(0); }

    strcpy( psc->Name, Name );
    psc->Flags = SCOPE_FLG_OPEN;
    psc->pParent = pScopeCurrent;
    psc->pAssignList = 0;

    /* Put this equate in the master list */
    psc->pPrev = 0;
    psc->pNext = pScopeList;
    pScopeList = psc;
    pScopeCurrent = psc;

    if( Pass==1 && (Options & OPTION_DEBUG) )
    {
        if(ps)
            printf("%s(%5d) : ",ps->SourceName,ps->CurrentLine);
        printf("DOTCMD : Scope '%s' declared\n",psc->Name);
     }

    return(psc);
}


/*
// ScopeDestroy
//
// Frees a scope record.
//
// void
*/
static void ScopeDestroy( SCOPE *psc )
{
    if( psc->Flags & SCOPE_FLG_OPEN )
        ScopeClose( psc );

    while( psc->pAssignList )
        AssignDestroy( &psc->pAssignList, psc->pAssignList );

    if( !psc->pPrev )
        pScopeList = psc->pNext;
    else
        psc->pPrev->pNext = psc->pNext;

    if( psc->pNext )
        psc->pNext->pPrev = psc->pPrev;

    free(psc);
}


/*
// ScopeClose
//
// Closes a scope record.
//
// void
*/
static void ScopeClose( SCOPE *psc )
{
    psc->Flags &= ~SCOPE_FLG_OPEN;

    while( pScopeCurrent && !(pScopeCurrent->Flags&SCOPE_FLG_OPEN) )
        pScopeCurrent = pScopeCurrent->pParent;
}


/*
// ScopeFind
//
// Finds a scope record.
//
// void
*/
static SCOPE *ScopeFind( char *Name )
{
    SCOPE *psc;

    psc = pScopeList;
    while( psc )
    {
        if( !strcmp( Name, psc->Name ) )
            break;
        psc = psc->pNext;
    }
    return(psc);
}
