/*
 * pasmexp.c
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
// File     : pasmexp.c
//
// Description:
//     Expression analyzer. This module is a "drop in", and thus does't
//     have much knowledge of the rest of the assembler.
//         - Handles expression processing
//
//     Note that the expression analyzer will only report errors on pass 2
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

#define MAXTERM         32

#define EOP_MULTIPLY     1
#define EOP_DIVIDE       2
#define EOP_MOD          3
#define EOP_ADD          4
#define EOP_SUBTRACT     5
#define EOP_LEFTSHIFT    6
#define EOP_RIGHTSHIFT   7
#define EOP_AND          8
#define EOP_XOR          9
#define EOP_OR           10

uint prec[] = { 999,
                1,  /* EOP_MULTIPLY   */
                1,  /* EOP_DIVIDE     */
                1,  /* EOP_MOD        */
                2,  /* EOP_ADD        */
                2,  /* EOP_SUBTRACT   */
                3,  /* EOP_LEFTSHIFT  */
                3,  /* EOP_RIGHTSHIFT */
                4,  /* EOP_AND        */
                5,  /* EOP_XOR        */
                6 };/* EOP_OR         */


int EXP_getValue( SOURCEFILE *ps, char *s, int *pIdx, uint *pValue );
int EXP_getOperation( SOURCEFILE *ps, char *s, int *pIdx, uint *pValue );
static int GetRegisterOffset( char *src, uint *pValue );

/*
// Expression - Math Expression Parser
//
// Returns 0 on success, <0 on error
*/
int Expression( SOURCEFILE *ps, char *s, uint *pResult, int *pIndex )
{
    uint    values[MAXTERM];
    uint    ops[MAXTERM];
    int     maxprec;
    int     i;
    int     validx,opidx,stridx;

    validx=0;
    opidx=0;
    stridx=0;

    while( validx<MAXTERM )
    {
        i= EXP_getValue(ps, s, &stridx, &values[validx]);
        if( !i )
            break;
        if( i<0 )
        {
            if( pIndex )
                *pIndex = stridx;
            return(-1);
        }
        validx++;

        i= EXP_getOperation(ps, s, &stridx, &ops[opidx]);
        if( !i )
            break;
        if( i<0 )
        {
            if( pIndex )
                *pIndex = stridx;
            return(-1);
        }
        opidx++;
    }

    if( i )
    {
        Report(ps,REP_ERROR,"Max term count exceeded");
        return(-1);
    }

    if( pIndex )
        *pIndex = stridx;

    /* If values and ops unbalanced, quit now */
    if( opidx >= validx || !validx )
        return(-1);

    while( opidx )
    {
        /* Find the highest prec op */
        maxprec = 0;
        for( i=1; i<opidx; i++)
            if( prec[ops[i]] < prec[ops[maxprec]] )
                maxprec = i;

        switch( ops[maxprec] )
        {
        case EOP_MULTIPLY:
            values[maxprec] = values[maxprec] * values[maxprec+1];
            break;
        case EOP_DIVIDE:
            if( !values[maxprec+1] )
            {
                if( Pass==2 )
                {
                    Report(ps,REP_ERROR,"Divide by zero");
                    return(-1);
                }
                else
                    values[maxprec] = 0;
            }
            else
                values[maxprec] = values[maxprec] / values[maxprec+1];
            break;
        case EOP_MOD:
            if( !values[maxprec+1] )
            {
                if( Pass==2 )
                {
                    Report(ps,REP_ERROR,"Mod by zero");
                    return(-1);
                }
                else
                    values[maxprec] = 0;
            }
            else
                values[maxprec] = values[maxprec] % values[maxprec+1];
            break;
        case EOP_ADD:
            values[maxprec] = values[maxprec] + values[maxprec+1];
            break;
        case EOP_SUBTRACT:
            values[maxprec] = values[maxprec] - values[maxprec+1];
            break;
        case EOP_LEFTSHIFT:
            values[maxprec] = values[maxprec] << values[maxprec+1];
            break;
        case EOP_RIGHTSHIFT:
            values[maxprec] = values[maxprec] >> values[maxprec+1];
            break;
        case EOP_AND:
            values[maxprec] = values[maxprec] & values[maxprec+1];
            break;
        case EOP_XOR:
            values[maxprec] = values[maxprec] ^ values[maxprec+1];
            break;
        case EOP_OR:
            values[maxprec] = values[maxprec] | values[maxprec+1];
            break;
        }

        // Remove this op and 2nd value term from the list
        i = MAXTERM-2-maxprec;
        if( i>0 )
        {
            memcpy( &values[maxprec+1], &values[maxprec+2], i*sizeof(uint));
            memcpy( &ops[maxprec], &ops[maxprec+1], i*sizeof(uint));
        }

        opidx--;
        validx--;
    }

    if( validx != 1 )
        { Report(ps,REP_ERROR,"Exp internal error"); return(-1); }

    *pResult = values[0];
    return(0);
}


/*
// EXP_getValue - Get a value from the supplied string
//
// Returns 0 no value, 1 on success, <0 on error
*/
int EXP_getValue( SOURCEFILE *ps, char *s, int *pIdx, uint *pValue )
{
    int     base = 10,index,i,j,k;
    int     rc = 1;
    uint    tval = 0;
    char    c;

    index = *pIdx;

    c = s[index];
    while( c==' ' || c==9 )
    {
        index++;
        c = s[index];
    }

    if( !c )
        return(0);

    /* Look for a label */
    if( LabelChar(c,1) || c=='.' || c=='&' )
    {
        LABEL *pl;
        char lblstr[LABEL_NAME_LEN];
        int  lblidx = 0;

        for(;;)
        {
            lblstr[lblidx++]=c;
            index++;
            c = s[index];
            if( !LabelChar(c,0) && c!='.' )
                break;
        }
        lblstr[lblidx]=0;
        *pIdx = index;

        if( CheckTokenType(lblstr) & TOKENTYPE_FLG_REG_ADDR )
        {
            if( GetRegisterOffset(lblstr+1,&tval) )
            {
                *pValue = tval;
                return(1);
            }
        }
        pl = LabelFind(lblstr);
        if(!pl && Pass==1)
            *pValue = 0;
        else if( !pl )
            { Report(ps,REP_ERROR,"Not found: '%s'",lblstr); return(0); }
        else
            *pValue = pl->Offset;
        return(1);
    }

    if( c=='-' )
    {
        index++;
        i = EXP_getValue( ps, s, &index, &tval );
        if( i<0 )
            rc = i;
        else
            tval = (uint)(-(int)tval);
        goto EGV_EXIT;
    }
    if( c=='~' )
    {
        index++;
        i = EXP_getValue( ps, s, &index, &tval );
        if( i<0 )
            rc = i;
        else
            tval = ~tval;
        goto EGV_EXIT;
    }
    if( c=='(' )
    {
        /* Scan to the far ')' */
        index++;
        j = index;
        i=1;
        for(;;)
        {
            c = *(s+j);
            if( !c )
            {
                rc = -1;
                goto EGV_EXIT;
            }
            if( c=='(' )
                i++;
            if( c==')' )
            {
                i--;
                if(!i)
                {
                    /* Terminate the string and eval the () */
                    *(s+j) = 0;
                    i = Expression( 0, s+index, &tval, &k );
                    if( i<0 )
                    {
                        index+=k;
                        rc=i;
                    }
                    else
                        index=j+1;
                    goto EGV_EXIT;
                }
            }
            j++;
        }
    }

    /* This character must be a number */
    if( c<'0' || c>'9' )
    {
        rc = -1;
        goto EGV_EXIT;
    }
    index++;
    tval = c-'0';
    if( tval==0 )
    {
        c = s[index];
        if( c=='x' )
        {
            base=16;
            index++;
        }
        else if( c=='b' )
        {
            base=2;
            index++;
        }
        else
            base=8;
    }

    for(;;)
    {
        c = s[index];
        if( c>='0' && c<='9' )
            i = c-'0';
        else if( c>='a' && c<='f' )
            i = c-'a'+10;
        else if( c>='A' && c<='F' )
            i = c-'A'+10;
        else
            break;

        if( i>=base )
        {
            rc = -1;
            goto EGV_EXIT;
        }
        tval *= base;
        tval += i;
        index++;
    }

EGV_EXIT:
    *pValue = tval;
    *pIdx = index;
    return(rc);
}


/*
// EXP_getOperation - Get an operation from the supplied string
//
// Returns 0 no value, 1 on success, <0 on error
*/
int EXP_getOperation( SOURCEFILE *ps, char *s, int *pIdx, uint *pValue )
{
    int     index;
    char    c;
    int     rc = 1;

    index = *pIdx;

    c = s[index];
    while( c==' ' || c==9 )
    {
        index++;
        c = s[index];
    }

    if( !c )
        return(0);
    else if( c=='*' )
        *pValue = EOP_MULTIPLY;
    else if( c=='/' )
        *pValue = EOP_DIVIDE;
    else if( c=='%' )
        *pValue = EOP_MOD;
    else if( c=='+' )
        *pValue = EOP_ADD;
    else if( c=='-' )
        *pValue = EOP_SUBTRACT;
    else if( c=='<' )
    {
        index++;
        c = s[index];
        if( c != '<' )
            rc=-1;
        else
            *pValue = EOP_LEFTSHIFT;
    }
    else if( c=='>' )
    {
        index++;
        c = s[index];
        if( c != '>' )
            rc=-1;
        else
            *pValue = EOP_RIGHTSHIFT;
    }
    else if( c=='&' )
        *pValue = EOP_AND;
    else if( c=='^' )
        *pValue = EOP_XOR;
    else if( c=='|' )
        *pValue = EOP_OR;
    else
        rc = -1;

    if( rc == 1)
        index++;

    *pIdx = index;
    return(rc);
}


/*
// GetRegisterOffset
//
// Get Register Offset
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int GetRegisterOffset( char *src, uint *pValue )
{
    uint idx;
    char c,field;
    int  val,reg,width,offset;

    if( Core == CORE_V0 )
        return(0);

    /*
    // The following register syntaxes are valid:
    //   Raa            aa=(0-31)
    //   Raa.Wb         aa=(0-31) b=(0-2)
    //   Raa.Bc         aa=(0-31) c=(0-3)
    //   Raa.Tdd        aa=(0-31) dd=(0-31)
    //   Raa.Wb.Be      aa=(0-31) b=(0-2) e=(0-1)
    */

    idx=0;
    c = src[idx++];
    /* Get initial 'R##' */
    if( toupper(c) != 'R' )
        return(0);
    c = src[idx++];
    if( !isdigit(c) )
        return(0);
    val = 0;
    while( isdigit(c) )
    {
        val *= 10;
        val += c-'0';
        c = src[idx++];
    }
    if( val>31 )
        return(0);

    reg    = val;
    width  = 32;
    offset = 0;

    for(;;)
    {
        /* This char must be '.', or terminator */

        /* If terminated, we're done */
        if( !c )
            break;
        if( c != '.' )
            return(0);

        c = src[idx++];

        /* This char must be 'W', or 'B' */
        c = toupper(c);
        if( c!='W' && c!='B' )
            return(0);
        field=c;
        c = src[idx++];
        if( !isdigit(c) )
            return(0);
        val = 0;
        while( isdigit(c) )
        {
            val *= 10;
            val += c-'0';
            c = src[idx++];
        }
        if( field=='W' )
        {
            if( ((val*8)+16)>width )
                return(0);
            width = 16;
            offset += val;
        }
        if( field=='B' )
        {
            if( ((val*8)+8)>width )
                return(0);
            width = 8;
            offset += val;
        }
    }

    if( !(Options & OPTION_BIGENDIAN) )
        *pValue = reg * 4 + offset;
    else
    {
        width /= 8;
        offset = 4 - offset - width;
        *pValue = reg * 4 + offset;
    }

    return(1);
}
