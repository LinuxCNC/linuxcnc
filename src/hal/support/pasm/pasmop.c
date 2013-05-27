/*
 * pasmop.c
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
// File     : pasmop.c
//
// Description:
//     Handles the processing of PRU opcodes.
//         - Provides a function to test for reserved words
//         - Processes assembly lines and generates opcodes
//         - Contains private functions to process operand fields
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

char *OpText[] = {
    "$ERROR$","ADD","ADC","SUB","SUC","LSL","LSR","RSB","RSC","AND","OR",
    "XOR","NOT","MIN","MAX","CLR","SET","LDI","LBBO","LBCO","SBBO",
    "SBCO","LFC","STC","JAL","JMP","QBGT","QBLT","QBEQ","QBGE","QBLE",
    "QBNE","QBA","QBBS","QBBC","LMBD","CALL","WBC","WBS","MOV","MVIB",
    "MVIW","MVID","SCAN","HALT","SLP", "RET", "ZERO", "XIN", "XOUT", "XCHG",
    "FILL", "SXIN", "SXOUT", "SXCHG", "LOOP", "ILOOP" };

/* Local Support Funtions */
static int GetImValue( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa, uint low, uint high );
static int GetConstant( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa );
static int GetR0offset( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa );
static int GetJmpOffset( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa );
static int Offset2Reg( SOURCEFILE *ps, int num, PRU_ARG *pa, uint addr, uint size );

/*
// CheckOpcode
//
// Called to see if the supplied token is an opcode.
//
// Returns index of opcode, 0 if not an opcode
*/
int CheckOpcode( char *word )
{
    int i;

    for(i=1; i<=OP_MAXIDX; i++)
    {
        if( !stricmp( word, OpText[i] ) )
            return(i);
    }
    return(0);
}


/*
// CheckTokenType
//
// Called to see if the supplied token is reserved word.
//
// Returns 1 if reserved, 0 if not
*/
uint CheckTokenType( char *word )
{
    int i,parse_state;
    uint flags;
    char c;

    /* Opcodes are reserved */
    if( CheckOpcode(word) )
        return(TOKENTYPE_FLG_OPCODE);

    /* Check for command */
    if( CheckDotCommand( word ) )
        return(TOKENTYPE_FLG_DIRECTIVE);

    if( !strcmp(word,"SIZE") || !strcmp(word,"OFFSET") )
        return(TOKENTYPE_FLG_DIRECTIVE);

    /*
    // [&,*][--](B,C,R,T,W)#[#][.(B,C,R,T,W)#[#]][++ ] is reserved
    //
    // For example:
    //   R       - not reserved
    //   R23     - reserved
    //   R100    - not reserved
    //   C34.XYZ - not reserved
    //   C34.T34 - reserved
    */
    i=0;
    parse_state=0;

    if( word[i]=='*' )
    {
        /* Mark as a pointer */
        flags = TOKENTYPE_FLG_REG_PTR;
        i++;

        /* Check if its an immediate address */
        if( word[i]=='&' )
        {
            flags |= TOKENTYPE_FLG_REG_ADDR;
            i++;
        }
        /* Else its a register base */
        else
        {
            flags |= TOKENTYPE_FLG_REG_BASE;

            /* Registers can also be pre-dec */
            if( word[i]=='-' )
            {
                if( word[i+1]!='-' )
                    return(TOKENTYPE_UNRESERVED);
                i+=2;
                flags |= TOKENTYPE_FLG_REG_PREDEC;
            }
        }
    }
    else if( word[i]=='&' )
    {
        flags = TOKENTYPE_FLG_REG_ADDR;
        i++;
    }
    else
        flags = TOKENTYPE_FLG_REG_BASE;

    for(;;)
    {
        c = word[i++];
        c = toupper(c);
        switch( parse_state )
        {
        case 0:
            if( c=='B' || c=='C' || c=='R' || c=='T' || c=='W' )
                parse_state=1;
            else
                return(TOKENTYPE_UNRESERVED);
            break;

        case 1:
            if( isdigit(c) )
                parse_state=2;
            else
                return(TOKENTYPE_UNRESERVED);
            break;

        case 2:
        case 3:
            if( isdigit(c) )
            {
                parse_state++;
                if( parse_state>3 )
                    return(TOKENTYPE_UNRESERVED);
            }
            else if( c=='.' )
                parse_state=0;
            else if( !c || c==' ' || c==0x9 )
                return(flags);
            else if( c=='+' )
            {
                /* Can not be an address or predec */
                if( flags & (TOKENTYPE_FLG_REG_ADDR|TOKENTYPE_FLG_REG_PREDEC) )
                    return(TOKENTYPE_UNRESERVED);

                /* Rest of type must match */
                if( word[i]!=c || word[i+1]!=0 )
                    return(TOKENTYPE_UNRESERVED);

                flags |= TOKENTYPE_FLG_REG_POSTINC;
                return(flags);
            }
            else
                return(TOKENTYPE_UNRESERVED);
            break;
        }
    }
}

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
int ProcessOp( SOURCEFILE *ps, int TermCnt, char **pTerms )
{
    PRU_INST inst;
    unsigned int opcode;
    unsigned int utmp;

    /* Get opcode */
    inst.Op = CheckOpcode(pTerms[0]);

    if( !inst.Op )
        { Report(ps,REP_ERROR,"Invalid opcode"); return(0); }

    switch( inst.Op )
    {
    case OP_NOT:
        /*
        // Instruction in the form of:
        //     NOT     Rdst, Rsrc, OP(255)
        //     NOT     Rdst, Rsrc
        */
        if( TermCnt!=3 && TermCnt!=4 )
            { Report(ps,REP_ERROR,"Expected 2 or 3 operands on NOT"); return(0); }
        goto PARSE_ARITHMETIC;
    case OP_ADD:
    case OP_ADC:
    case OP_SUB:
    case OP_SUC:
    case OP_LSL:
    case OP_LSR:
    case OP_RSB:
    case OP_RSC:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
    case OP_MIN:
    case OP_MAX:
    case OP_LMBD:
        /*
        // Instruction in the form of:
        //     OPCODE  Rdst, Rsrc, OP(255)
        */
        if( inst.Op==OP_LMBD && Core<CORE_V1 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt!=4  )
            { Report(ps,REP_ERROR,"Expected 3 operands"); return(0); }
PARSE_ARITHMETIC:
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
            return(0);
        if( TermCnt==4 )
        {
            if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
            {
                if( !GetRegister( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 0 ) )
                    return(0);
            }
            else
            {
                if( !GetImValue( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 255 ) )
                    return(0);
            }
        }
        else
        {
            inst.Arg[2].Type = ARGTYPE_IMMEDIATE;
            inst.Arg[2].Value = 0;
        }
CODE_ARITHMETIC:
        if( inst.Op == OP_LMBD )
            opcode = 0x13 << 25;
        else if( inst.Op == OP_SCAN )
            opcode = 0x14 << 25;
        else
            opcode = (inst.Op-OP_ADD) << 25;
        opcode |= inst.Arg[0].Value;
        opcode |= inst.Arg[0].Field << 5;
        opcode |= inst.Arg[1].Value << 8;
        opcode |= inst.Arg[1].Field << 13;
        if( inst.Arg[2].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= inst.Arg[2].Field << 21;
        }
        else
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= 1<<24;
        }
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);


    case OP_CLR:
    case OP_SET:
        /*
        // Instruction in the form of:
        //     OPCODE  Rdst, Rsrc, OP(255) -or-
        //     OPCODE  Rdst, Rsrc.Tnn      -or-
        //     OPCODE  Rdst, OP(255)       -or-
        //     OPCODE  Rdst.Tnn
        */
        if( TermCnt<2 || TermCnt>4 )
            { Report(ps,REP_ERROR,"Expected 1 to 3 operands"); return(0); }

        /* The 3 argument (4 term) variant can be parsed as arithmetic */
        if( TermCnt==4 )
            goto PARSE_ARITHMETIC;
        else if( TermCnt==2 )
        {
            /*
            // Hande "OPCODE  Rdst.Tnn"
            */
            if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 1, 0 ) )
                return(0);
            if( inst.Arg[0].Type != ARGTYPE_REGISTERBIT )
                { Report(ps,REP_ERROR,"Single operand mode must specify .T field"); return(0); }
            /* 2nd arg = 1st arg, move bit# to 3rd arg */
            inst.Arg[0].Type = ARGTYPE_REGISTER;
            inst.Arg[1] = inst.Arg[0];
            inst.Arg[2].Type = ARGTYPE_IMMEDIATE;
            inst.Arg[2].Value = inst.Arg[0].Bit;
        }
        else
        {
            /*
            // Handle "OPCODE  Rdst, Rsrc.Tnn", and
            //        "OPCODE  Rdst, OP(255)"
            */
            if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
                return(0);
            if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
            {
                if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 1, 0 ) )
                    return(0);
            }
            else
            {
                if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 31 ) )
                    return(0);
            }

            /* Patch up the args to the standard "arithmetic" form */
            if( inst.Arg[1].Type == ARGTYPE_REGISTERBIT )
            {
                /* Here the 2nd arg is Rsrc.Tnn, move bit# to 3rd arg */
                inst.Arg[1].Type  = ARGTYPE_REGISTER;
                inst.Arg[2].Type  = ARGTYPE_IMMEDIATE;
                inst.Arg[2].Value = inst.Arg[1].Bit;
            }
            else
            {
                /* Here the 2nd arg is OP(255), 3rd arg = 2nd arg, 2nd=1st */
                inst.Arg[2] = inst.Arg[1];
                inst.Arg[1] = inst.Arg[0];
            }
        }
        goto CODE_ARITHMETIC;


    case OP_LDI:
        /*
        // Instruction in the form of:
        //     LDI  Rdst, &Rsrc
        //     LDI  Rdst, #Im65535
        */
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
        {
            return(0);
        }
        if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 65535 ) )
        {
            return(0);
        }
        opcode = 0x24 << 24;
        opcode |= inst.Arg[0].Value;
        opcode |= inst.Arg[0].Field << 5;
        opcode |= inst.Arg[1].Value << 8;
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);


    case OP_MOV:
        /*
        // Instruction in the form of:
        //     MOV  Rdst, Rsrc          -or-
        //     MOV  Rdst, &Rsrc         -or-
        //     MOV  Rdst, #Imm
        */
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        /* If the second term is not reg, treat similar to LDI */
        if( !(CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE) )
        {
            // Unlike LDI, we will auto-select the best opcodes to implement the move
            if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            {
                return(0);
            }

            switch( inst.Arg[0].Field )
            {
            case FIELDTYPE_7_0:
            case FIELDTYPE_15_8:
            case FIELDTYPE_23_16:
            case FIELDTYPE_31_24:
                if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 255 ) )
                    return(0);
                break;
            case FIELDTYPE_15_0:
            case FIELDTYPE_23_8:
            case FIELDTYPE_31_16:
                if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 65535 ) )
                    return(0);
                break;
            case FIELDTYPE_31_0:
                if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0xFFFFFFFF ) )
                    return(0);
                // If the value is greater than 0xFFFF, code the upper half here
                if( inst.Arg[1].Value > 0xFFFF )
                {
                    opcode = 0x24 << 24;
                    opcode |= inst.Arg[0].Value;
                    opcode |= FIELDTYPE_31_16 << 5;
                    opcode |= (inst.Arg[1].Value>>16) << 8;
                    GenOp( ps, TermCnt, pTerms, opcode );
                    inst.Arg[0].Field = FIELDTYPE_15_0;
                    inst.Arg[1].Value &= 0xFFFF;
                }
                break;
            default:
                return(0);
            }
            opcode = 0x24 << 24;
            opcode |= inst.Arg[0].Value;
            opcode |= inst.Arg[0].Field << 5;
            opcode |= inst.Arg[1].Value << 8;
            GenOp( ps, TermCnt, pTerms, opcode );
            return(1);
        }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
            return(0);
        /* Code as an AND */
        inst.Op = OP_AND;
        inst.Arg[2] = inst.Arg[1];
        goto CODE_ARITHMETIC;


    case OP_SCAN:
        /*
        // Instruction in the form of:
        //     SCAN  Rdst, OP(255)
        */
        if( Core!=CORE_V1 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt!=3  )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( inst.Arg[0].Field != FIELDTYPE_31_0 )
            { Report(ps,REP_ERROR,"Register fields not allowed on operand 1"); return(0); }
        inst.Arg[1] = inst.Arg[0];
        if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[2]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[2]), 0, 255 ) )
                return(0);
        }
        goto CODE_ARITHMETIC;

    case OP_MVIB:
        /*
        // Instruction in the form of:
        //     MVIB  [*][&][--]Rdst[++], [*][&][--]Rsrc[++] [, bn]
        */
        utmp = 1;
        goto CODE_MVI;
    case OP_MVIW:
        /*
        // Instruction in the form of:
        //     MVIW  [*][&][--]Rdst[++], [*][&][--]Rsrc[++] [, bn]
        */
        utmp = 2;
        goto CODE_MVI;
    case OP_MVID:
        /*
        // Instruction in the form of:
        //     MVID  [*][&][--]Rdst[++], [*][&][--]Rsrc[++] [, bn]
        */
        utmp = 4;
CODE_MVI:
        if( Core<CORE_V1 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt < 3 || TermCnt > 4)
            { Report(ps,REP_ERROR,"Expected 2 or 3 operands"); return(0); }
        else
        {
            uint argtype;
            int i;

            for(i=1;i<=2;i++)
            {
                /* Params must be a register in this version */
                argtype = CheckTokenType(pTerms[i]);
                if( argtype == (TOKENTYPE_FLG_REG_PTR|TOKENTYPE_FLG_REG_ADDR) )
                {
                    if( !GetImValue( ps, i, pTerms[i]+1, &(inst.Arg[i-1]), 0, 127 ) )
                        return(0);
                    if( !Offset2Reg( ps, i, &(inst.Arg[i-1]), inst.Arg[i-1].Value, utmp ) )
                        return(0);
                }
                else if( argtype == TOKENTYPE_FLG_REG_BASE )
                {
                    if( !GetRegister( ps, i, pTerms[i], &(inst.Arg[i-1]), 0, 0 ) )
                        return(0);
                }
                else
                    goto CODE_MVI_PLUS;
            }

            /* Arg[1] must not be larger than the move size */
            if( utmp == 1 && inst.Arg[1].Type == ARGTYPE_REGISTER )
            {
                switch(inst.Arg[1].Field)
                {
                case FIELDTYPE_15_0:
                case FIELDTYPE_31_0:
                    inst.Arg[1].Field = FIELDTYPE_7_0;
                    break;
                case FIELDTYPE_31_16:
                    inst.Arg[1].Field = FIELDTYPE_23_16;
                    break;
                }
            }
            else if( utmp == 2 && inst.Arg[1].Type == ARGTYPE_REGISTER )
            {
                switch(inst.Arg[1].Field)
                {
                case FIELDTYPE_31_0:
                    inst.Arg[1].Field = FIELDTYPE_15_0;
                    break;
                }
            }

            /* Code as an AND */
            inst.Op = OP_AND;
            inst.Arg[2] = inst.Arg[1];
            goto CODE_ARITHMETIC;
        }

CODE_MVI_PLUS:
        if( Core<CORE_V2 )
            { Report(ps,REP_ERROR,"This form of MVIx illegal with specified core version"); return(0); }
        else
        {
        }
        return(1);

    case OP_HALT:
        if( Core<CORE_V1 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt != 1 )
            { Report(ps,REP_ERROR,"Expected no operands"); return(0); }
        opcode = 0x15 << 25;
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_SLP:
        if( Core<CORE_V1 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(0); }
        opcode = 0x1F << 25;
        if( !GetImValue( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 1 ) )
            return(0);
        opcode |= inst.Arg[0].Value << 23;
        GenOp( ps, TermCnt, pTerms, opcode );
        if( Core<CORE_V3 )
        {
            opcode = 0x8 << 25;
            GenOp( ps, TermCnt, pTerms, opcode );
        }
        return(1);

    case OP_SBBO:
        opcode = 0xE << 28;
        goto BURST_OPCODE;
    case OP_LBBO:
        opcode = 0xF << 28;
        goto BURST_OPCODE;
    case OP_SBCO:
        opcode = 0x8 << 28;
        goto BURST_OPCODE;
    case OP_LBCO:
        opcode = 0x9 << 28;

BURST_OPCODE:
        /*
        // Instruction in the form of:
        //     OPCODE Rdst, Rnn/Cnn, OP(255), n    -or-
        //     OPCODE Rdst, Rnn/Cnn, OP(255), bn
        */
        if( TermCnt != 5 )
            { Report(ps,REP_ERROR,"Expected 4 operands"); return(0); }
        if( CheckTokenType(pTerms[1]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 127 ) )
                return(0);
        }
        if( inst.Op==OP_SBBO || inst.Op==OP_LBBO )
        {
            if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
                return(0);
            if( inst.Arg[1].Field != FIELDTYPE_31_0 )
                { Report(ps,REP_ERROR,"Register fields not allowed on operand 2"); return(0); }
        }
        else
        {
            if( !GetConstant( ps, 2, pTerms[2], &(inst.Arg[1]) ) )
                return(0);
        }
        if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue(ps, 3, pTerms[3], &(inst.Arg[2]), 0, 255) )
                return(0);
        }
        if( CheckTokenType(pTerms[4]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetR0offset( ps, 4, pTerms[4], &(inst.Arg[3]) ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 4, pTerms[4], &(inst.Arg[3]), 1, 124 ) )
                return(0);
        }

        if( inst.Arg[0].Type == ARGTYPE_IMMEDIATE )
        {
            opcode |= (inst.Arg[0].Value>>2) & 0x1F;
            opcode |= (inst.Arg[0].Value&0x3) << 5;
        }
        else
        {
            opcode |= inst.Arg[0].Value;

            if( !(Options & OPTION_BIGENDIAN) )
            {
                /*
                // ** Little Endian Version **
                */
                switch( inst.Arg[0].Field )
                {
                case FIELDTYPE_15_8:
                case FIELDTYPE_23_8:
                    opcode |= 1<<5;
                    break;
                case FIELDTYPE_23_16:
                case FIELDTYPE_31_16:
                    opcode |= 2<<5;
                    break;
                case FIELDTYPE_31_24:
                    opcode |= 3<<5;
                    break;
                }
            }
            else
            {
                /*
                // ** Big Endian Version **
                */
                switch( inst.Arg[0].Field )
                {
                case FIELDTYPE_23_16:
                case FIELDTYPE_23_8:
                    opcode |= 1<<5;
                    break;

                case FIELDTYPE_15_0:
                case FIELDTYPE_15_8:
                    opcode |= 2<<5;
                    break;

                    case FIELDTYPE_7_0:
                    opcode |= 3<<5;
                    break;
                }
            }
        }

        opcode |= inst.Arg[1].Value << 8;
        if( inst.Arg[2].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= inst.Arg[2].Field << 21;
        }
        else
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= 1<<24;
        }
        if( inst.Arg[3].Type == ARGTYPE_R0BYTE )
            utmp = inst.Arg[3].Value + 124;
        else
            utmp = inst.Arg[3].Value - 1;
        opcode |= (utmp&0x70)<<(25-4);
        opcode |= (utmp&0x0E)<<(13-1);
        opcode |= (utmp&0x01)<<7;
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_LFC:
        /*
        // Instruction in the form of:
        //     LFC Rdst, #Im255
        */
        if( Core!=CORE_V0 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 255 ) )
            return(0);

        opcode = 0xb << 28;
        opcode |= inst.Arg[0].Value;
        opcode |= inst.Arg[0].Field << 5;
        opcode |= inst.Arg[1].Value << 8;
        opcode |= 1 << 24;
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);


    case OP_STC:
        /*
        // Instruction in the form of:
        //     STC Rsrc, #Im255
        //     STC Rsrc, #Im255, OP(255)
        */
        if( Core!=CORE_V0 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt!=3 && TermCnt!=4 )
            { Report(ps,REP_ERROR,"Expected 2 or 3 operands"); return(0); }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 255 ) )
            return(0);
        if( TermCnt==4 )
        {
            if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
            {
                if( !GetRegister( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 0 ) )
                    return(0);
            }
            else
            {
                if( !GetImValue( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 255 ) )
                    return(0);
            }
        }

        opcode = 0xa << 28;
        opcode |= inst.Arg[0].Value;
        opcode |= inst.Arg[0].Field << 5;
        opcode |= inst.Arg[1].Value << 8;
        if( TermCnt==4 )
        {
            if( inst.Arg[2].Type == ARGTYPE_REGISTER )
            {
                opcode |= inst.Arg[2].Value << 16;
                opcode |= inst.Arg[2].Field << 21;
            }
            else
            {
                opcode |= inst.Arg[2].Value << 16;
                opcode |= 1<<24;
            }
        }
        else
        {
            /* When a 32 bit reg is used, we need the 8 MS bits from that reg */
            if( inst.Arg[0].Field==FIELDTYPE_31_0 )
            {
                opcode |= inst.Arg[0].Value << 16;
                opcode |= FIELDTYPE_31_24 << 21;
            }
            /* Less than 32 bits were used, so clear the 8 MS bits via #0 */
            else
                opcode |= 1<<24;
        }

        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);


    case OP_JAL:
        /*
        // Instruction in the form of:
        //     JAL Rdst, Rjmp
        //     JAL Rdst, #Im65535
        */
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 0 ) )
            return(0);
        if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 65535 ) )
                return(0);
        }

CODE_JAL:
        opcode = 0x11 << 25;
        opcode |= inst.Arg[0].Value;
        opcode |= inst.Arg[0].Field << 5;
        if( inst.Arg[1].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[1].Value << 16;
            opcode |= inst.Arg[1].Field << 21;
        }
        else
        {
            opcode |= inst.Arg[1].Value << 8;
            opcode |= 1<<24;
        }
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_RET:
        if( TermCnt != 1 )
            { Report(ps,REP_ERROR,"Expected no operands"); return(0); }
        inst.Arg[1].Type  = ARGTYPE_REGISTER;
        inst.Arg[1].Value = RetRegValue;
        inst.Arg[1].Field = RetRegField;
        goto CODE_JMP;

    case OP_JMP:
    case OP_CALL:
        /*
        // Instruction in the form of:
        //     OPCODE Rjmp
        //     OPCODE #Im65535
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(0); }
        if( CheckTokenType(pTerms[1]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[1]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 1, pTerms[1], &(inst.Arg[1]), 0, 65535 ) )
                return(0);
        }

        if( inst.Op==OP_CALL )
        {
            inst.Arg[0].Type  = ARGTYPE_REGISTER;
            inst.Arg[0].Value = RetRegValue;
            inst.Arg[0].Field = RetRegField;
            goto CODE_JAL;
        }

CODE_JMP:
        opcode = 0x10 << 25;
        if( inst.Arg[1].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[1].Value << 16;
            opcode |= inst.Arg[1].Field << 21;
        }
        else
        {
            opcode |= inst.Arg[1].Value << 8;
            opcode |= 1<<24;
        }
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_QBGT:
        opcode = 0xC << 27;
        goto QB_OPCODE;
    case OP_QBLT:
        opcode = 0x9 << 27;
        goto QB_OPCODE;
    case OP_QBEQ:
        opcode = 0xA << 27;
        goto QB_OPCODE;
    case OP_QBGE:
        opcode = 0xE << 27;
        goto QB_OPCODE;
    case OP_QBLE:
        opcode = 0xB << 27;
        goto QB_OPCODE;
    case OP_QBNE:
        opcode = 0xD << 27;
QB_OPCODE:
        /*
        // Instruction in the form of:
        //     OPCODE JmpDest, Reg1, OP(255)
        */
        if( TermCnt != 4 )
            { Report(ps,REP_ERROR,"Expected 3 operands"); return(0); }
        if( !GetJmpOffset( ps, 1, pTerms[1], &(inst.Arg[0]) ) )
            return(0);
        if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
            return(0);
        if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 255 ) )
                return(0);
        }

CODE_QB:
        opcode |= inst.Arg[0].Value & 0xFF;
        opcode |= inst.Arg[1].Value << 8;
        opcode |= inst.Arg[1].Field << 13;
        if( inst.Arg[2].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= inst.Arg[2].Field << 21;
        }
        else
        {
            opcode |= inst.Arg[2].Value << 16;
            opcode |= 1<<24;
        }
        opcode |= (inst.Arg[0].Value & 0x300) << (25-8);
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_QBA:
        /*
        // Instruction in the form of:
        //     QBA JmpDest
        */
        if( TermCnt != 2 )
            { Report(ps,REP_ERROR,"Expected 1 operand"); return(0); }
        if( !GetJmpOffset( ps, 1, pTerms[1], &(inst.Arg[0]) ) )
            return(0);
        opcode = 0xF << 27;
        opcode |= inst.Arg[0].Value & 0xFF;
        opcode |= (inst.Arg[0].Value & 0x300) << (25-8);
        opcode |= 1<<24;
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);


    case OP_QBBS:
        opcode = 0x1a << 27;
        goto BITTEST_OPCODE;
    case OP_QBBC:
        opcode = 0x19 << 27;
BITTEST_OPCODE:
        /*
        // Instruction in the form of:
        //     OPCODE  JmpDest, Rtest, OP(255) -or-
        //     OPCODE  JmpDest, Rtest.Tnn
        */
        if( TermCnt!=3 && TermCnt!=4 )
            { Report(ps,REP_ERROR,"Expected 2 to 3 operands"); return(0); }
        if( !GetJmpOffset( ps, 1, pTerms[1], &(inst.Arg[0]) ) )
            return(0);
        if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 1, 0 ) )
            return(0);
        if( TermCnt==3 )
        {
            /*
            // Handle "OPCODE  JmpDest, Rtest.Tnn"
            */
            if( inst.Arg[1].Type != ARGTYPE_REGISTERBIT )
                { Report(ps,REP_ERROR,"Two operand mode must specify .T field"); return(0); }
            /* Move bit# to 3rd arg */
            inst.Arg[1].Type  = ARGTYPE_REGISTER;
            inst.Arg[2].Type  = ARGTYPE_IMMEDIATE;
            inst.Arg[2].Value = inst.Arg[1].Bit;
        }
        else
        {
            /*
            // Handle "OPCODE  JmpDest, Rtest, OP(255)"
            */
            if( inst.Arg[1].Type != ARGTYPE_REGISTER )
                { Report(ps,REP_ERROR,"Three operand mode may not use .T field"); return(0); }
            if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
            {
                if( !GetRegister( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 0 ) )
                    return(0);
            }
            else
            {
                if( !GetImValue( ps, 3, pTerms[3], &(inst.Arg[2]), 0, 31 ) )
                    return(0);
            }
        }

        goto CODE_QB;


    case OP_WBC:
        opcode = 0x1a << 27;
        goto WAITBIT_OPCODE;
    case OP_WBS:
        opcode = 0x19 << 27;
WAITBIT_OPCODE:
        /*
        // Instruction in the form of:
        //     OPCODE  Rtest, OP(255) -or-
        //     OPCODE  Rtest.Tnn
        */
        if( TermCnt<2 || TermCnt>3 )
            { Report(ps,REP_ERROR,"Expected 1 to 2 operands"); return(0); }

        inst.Arg[0].Type  = ARGTYPE_OFFSET;
        inst.Arg[0].Value = 0;
        inst.Arg[0].Field = 0;

        if( !GetRegister( ps, 1, pTerms[1], &(inst.Arg[1]), 1, 0 ) )
            return(0);
        if( TermCnt==2 )
        {
            /*
            // Handle "OPCODE  JmpDest, Rtest.Tnn"
            */
            if( inst.Arg[1].Type != ARGTYPE_REGISTERBIT )
                { Report(ps,REP_ERROR,"Two operand mode must specify .T field"); return(0); }
            /* Move bit# to 3rd arg */
            inst.Arg[1].Type  = ARGTYPE_REGISTER;
            inst.Arg[2].Type  = ARGTYPE_IMMEDIATE;
            inst.Arg[2].Value = inst.Arg[1].Bit;
        }
        else
        {
            /*
            // Handle "OPCODE  JmpDest, Rtest, OP(255)"
            */
            if( inst.Arg[1].Type != ARGTYPE_REGISTER )
                { Report(ps,REP_ERROR,"Three operand mode may not use .T field"); return(0); }
            if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
            {
                if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[2]), 0, 0 ) )
                    return(0);
            }
            else
            {
                if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[2]), 0, 31 ) )
                    return(0);
            }
        }

        goto CODE_QB;


    case OP_ZERO:
        /*
        // Instruction in the form of:
        //     ZERO  &Rdst,  #Im124
        //     ZERO  &Rdst,  bn
        //     ZERO  #Im123, #Im124
        //     ZERO  #Im123, bn
        */
    case OP_FILL:
        /*
        // Instruction in the form of:
        //     FILL  &Rdst,  #Im124
        //     FILL  &Rdst,  bn
        //     FILL  #Im123, #Im124
        //     FILL  #Im123, bn
        */
        if( Core<CORE_V2 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
        // First argument
        if( CheckTokenType(pTerms[1]) == TOKENTYPE_FLG_REG_ADDR )
        {
            if( !GetRegister( ps, 1, pTerms[1]+1, &(inst.Arg[0]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 123 ) )
                return(0);
        }
        // Second argument
        if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetR0offset( ps, 2, pTerms[2], &(inst.Arg[1]) ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 1, 124 ) )
                return(0);
            if( (inst.Arg[0].Value + inst.Arg[1].Value)>124 )
                { Report(ps,REP_ERROR,"Operation length exceeds register file length"); return(0); }
            if( inst.Arg[1].Type != TOKENTYPE_FLG_REG_BASE && inst.Arg[1].Value == 0 )
                { Report(ps,REP_ERROR,"Zero length operation"); return(0); }
        }

        if ( inst.Op == OP_FILL )
            opcode = 254 << 15;        // use /dev/fill
        else
            opcode = 255 << 15;        // use /dev/zero

        utmp = 0;
        goto CODE_XFR;

    case OP_XIN:
    case OP_XOUT:
    case OP_XCHG:
    case OP_SXIN:
    case OP_SXOUT:
    case OP_SXCHG:
        /*
        // Instruction in the form of:
        //     XIN  #Im253, Rdst, #Im124
        //     XIN  #Im253, Rdst, bn
        //     XOUT #Im253, Rsrc, #Im124
        //     XOUT #Im253, Rsrc, bn
        //     XCHG #Im253, Rsrc, #Im124
        //     XCHG #Im253, Rsrc, bn
        // for reasons of consistency with FILL and ZERO, the second argument can also have a '&' !
        */
        if( Core<CORE_V2 )
            { Report(ps,REP_ERROR,"Instruction illegal with specified core version"); return(0); }
        if( TermCnt != 4 )
            { Report(ps,REP_ERROR,"Expected 3 operands"); return(0); }
        // First argument: #Im253
        if( !GetImValue( ps, 1, pTerms[1], &(inst.Arg[0]), 0, 253 ) )
            return(0);
        // Second argument: Rsrc or Rdst
        if( !(CheckTokenType(pTerms[2]) & (TOKENTYPE_FLG_REG_BASE|TOKENTYPE_FLG_REG_ADDR)) )
            return(0);
        if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
               return(0);
        }
        else if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_ADDR )
        {
            if( !GetRegister( ps, 2, pTerms[2]+1, &(inst.Arg[1]), 0, 0 ) )
               return(0);
        }
        else
            return(0);
        // Third argument: #Im124 or bn
        if( CheckTokenType(pTerms[3]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetR0offset( ps, 3, pTerms[3], &(inst.Arg[2]) ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 3, pTerms[3], &(inst.Arg[2]), 1, 124 ) )
                return(0);
        }
        /* XFR */
        opcode = inst.Arg[0].Value << 15;       // device id
        utmp = 1;
CODE_XFR:
        switch( inst.Op )
        {
        case OP_XCHG:  opcode |=  0x5f << 23;              break;
        case OP_SXCHG: opcode |= (0x5f << 23) | (1 << 14); break;
        case OP_XOUT:  opcode |=  0x5e << 23;              break;
        case OP_SXOUT: opcode |= (0x5e << 23) | (1 << 14); break;
        case OP_FILL:
        case OP_ZERO:
        case OP_XIN:   opcode |=  0x5d << 23;              break;
        case OP_SXIN:  opcode |= (0x5d << 23) | (1 << 14); break;
        default:       opcode |=  0x5c << 23;
        }
        // merge base register
	if( inst.Arg[utmp].Type == ARGTYPE_IMMEDIATE )
        {
            if( !Offset2Reg( ps, utmp+1, &(inst.Arg[utmp]), inst.Arg[utmp].Value, 1 ) )
                return(0);
        }
        opcode |= inst.Arg[utmp].Value << 0;      // register nr
        // merge register offset field
        switch (inst.Arg[utmp].Field)
        {
        case FIELDTYPE_7_0:
        case FIELDTYPE_31_0:
        case FIELDTYPE_15_0:	opcode |= FIELDTYPE_OFF_0  << 5; break;
        case FIELDTYPE_15_8:
        case FIELDTYPE_23_8:	opcode |= FIELDTYPE_OFF_8  << 5; break;
        case FIELDTYPE_23_16:
        case FIELDTYPE_31_16:	opcode |= FIELDTYPE_OFF_16 << 5; break;
        case FIELDTYPE_31_24:	opcode |= FIELDTYPE_OFF_24 << 5; break;
        }
        // merge count
        if( inst.Arg[utmp+1].Type == ARGTYPE_IMMEDIATE )
            opcode |= (inst.Arg[utmp+1].Value - 1) << 7;      // count - 1
        else if( inst.Arg[utmp+1].Type == ARGTYPE_R0BYTE)
            opcode |= (124 + inst.Arg[utmp+1].Value) << 7;    // count in R0 byte
        else
            return(0);
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);

    case OP_LOOP:
    case OP_ILOOP:
        /*
        // Instruction in the form of:
        //     LOOP  #Im65535, Rcnt
        //     LOOP  #Im65535, #Im255
        //     ILOOP #Im65535, Rcnt
        //     ILOOP #Im65535, #Im255
        */
        if( TermCnt != 3 )
            { Report(ps,REP_ERROR,"Expected 2 operands"); return(0); }
	// First argument
        if( !GetJmpOffset( ps, 1, pTerms[1], &(inst.Arg[0]) ) )
            return(0);
        if( Pass==2 && (inst.Arg[0].Value > 255) )
            { Report(ps,REP_ERROR,"Operand 1 relative jump out of range"); return(0); }

	// Second argument
        if( CheckTokenType(pTerms[2]) & TOKENTYPE_FLG_REG_BASE )
        {
            if( !GetRegister( ps, 2, pTerms[2], &(inst.Arg[1]), 0, 0 ) )
                return(0);
        }
        else
        {
            if( !GetImValue( ps, 2, pTerms[2], &(inst.Arg[1]), 1, 32 ) )
                return(0);
        }
        opcode = 0x18 << 25;
	if( inst.Op == OP_ILOOP)
            opcode |= 1 << 15;
        opcode |= inst.Arg[0].Value & 0xFF;
        if( inst.Arg[1].Type == ARGTYPE_REGISTER )
        {
            opcode |= inst.Arg[1].Value << 16;
            opcode |= inst.Arg[1].Field << 21;
        }
        else
        {
            opcode |= ((inst.Arg[1].Value - 1) & 31) << 16;
            opcode |= 1 << 24;
        }
        GenOp( ps, TermCnt, pTerms, opcode );
        return(1);
    }

    Report(ps,REP_ERROR,"Invalid opcode");
    return(0);
}

/*===================================================================
//
// Private Functions
//
====================================================================*/

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
int GetRegister( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa, int fBitOk, char termC )
{
    uint idx;
    char c,field;
    int  val,reg,width,offset,bit;

    /*
    // The following register syntaxes are valid:
    //   Raa            aa=(0-31)
    //   Raa.Wb         aa=(0-31) b=(0-2)
    //   Raa.Bc         aa=(0-31) c=(0-3)
    //   Raa.Tdd        aa=(0-31) dd=(0-31)
    //   Raa.Wb.Be      aa=(0-31) b=(0-2) e=(0-1)
    //   Raa.Wb.Tff     aa=(0-31) b=(0-2) ff=(0-15)
    //   Raa.Bc.Tgg     aa=(0-31) c=(0-3) gg=(0-7)
    //   Raa.Wb.Be.Tgg  aa=(0-31) b=(0-2) e=(0-1) gg=(0-7)
    */

    idx=0;
    c = src[idx++];

    /* Get initial 'R##' */
    if( toupper(c) != 'R' )
        goto INVALID_REG;
    c = src[idx++];
    if( !isdigit(c) )
        goto INVALID_REG;
    val = 0;
    while( isdigit(c) )
    {
        val *= 10;
        val += c-'0';
        c = src[idx++];
    }
    if( val>31 )
        goto INVALID_REG;

    reg    = val;
    width  = 32;
    offset = 0;
    bit    = -1;

    for(;;)
    {
        /* This char must be '.', or terminator */

        /* If terminated, we're done */
        if( c==termC )
            break;
        if( c != '.' )
            goto INVALID_REG;

        c = src[idx++];

        /* This char must be 'W', 'B', or 'T' */
        c = toupper(c);
        if( c!='T' && c!='W' && c!='B' )
            goto INVALID_REG;
        field=c;
        c = src[idx++];
        if( !isdigit(c) )
            goto INVALID_REG;
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
                goto INVALID_REG;
            width = 16;
            offset += val;
        }
        if( field=='B' )
        {
            if( ((val*8)+8)>width )
                goto INVALID_REG;
            width = 8;
            offset += val;
        }
        if( field=='T' )
        {
            if( !fBitOk )
                { Report(ps,REP_ERROR,"Operand %d use of .T field not allowed here",num); return(0); }
            if( val>=width )
                goto INVALID_REG;
            if(c!=termC)
                goto INVALID_REG;
            bit = val;
        }

        if( c!=termC && (Core==CORE_V0) )
            goto INVALID_REG;
    }

    /* Build the record */
    pa->Type  = ARGTYPE_REGISTER;
    pa->Value = reg;

    if( width==32 )
        pa->Field = FIELDTYPE_31_0;
    if( width==16 )
    {
        if( !offset )
            pa->Field = FIELDTYPE_15_0;
        else if( offset==1 )
            pa->Field = FIELDTYPE_23_8;
        else
            pa->Field = FIELDTYPE_31_16;
    }
    if( width==8 )
    {
        if( !offset )
            pa->Field = FIELDTYPE_7_0;
        else if( offset==1 )
            pa->Field = FIELDTYPE_15_8;
        else if( offset==2 )
            pa->Field = FIELDTYPE_23_16;
        else
            pa->Field = FIELDTYPE_31_24;
    }
    if( bit>=0 )
    {
        pa->Type = ARGTYPE_REGISTERBIT;
        pa->Bit = bit;
    }

    return(1);

INVALID_REG:
    Report(ps,REP_ERROR,"Operand %d '%s' invalid register or register mode",num,src);
    return(0);
}

/*
// GetImValue
//
// Get Register Argument
//
// Parses the source string for an immediate value
//
// ps      - Pointer to source file record
// num     - operand number (1 based)
// src     - source string
// pa      - Pointer to register structure
// min     - Lowest legal value
// max     - Highest legal value
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int GetImValue( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa, uint min, uint max )
{
    char tstr[TOKEN_MAX_LEN];
    uint val;
    int  idx;

    /* Handler register file offsets as immediates */
    val = CheckTokenType(src);
    if( val!=TOKENTYPE_UNRESERVED && !(val&TOKENTYPE_FLG_REG_ADDR) )
    {
        Report(ps,REP_ERROR,"Operand %d reserved word '%s' not legal here",num,src);
        return(0);
    }

    // Get our value from a an expression
    if( *src=='#' )
        src++;
    strcpy( tstr, src );
    if( Expression(ps, tstr, &val, &idx)<0 )
    {
        Report(ps,REP_ERROR,"Operand %d error in expression",num);
        return(0);
    }

    if( Pass==2 && (Options & OPTION_DEBUG) )
        printf("%s(%5d) : EXP    : '%s' = %d\n", ps->SourceName,ps->CurrentLine,src,val);

    /* Setup the record */
    pa->Type  = ARGTYPE_IMMEDIATE;
    pa->Value = val;
    pa->Field = 0;

    if( val<min || val>max )
    {
        Report(ps,REP_ERROR,"Operand %d immediate value out of range (%d-%d)",num,min,max);
        return(0);
    }

    return(1);
}

/*
// GetConstant
//
// Get Constant Table Argument
//
// Parses the source string for a constant register
//
// ps      - Pointer to source file record
// num     - source line number
// src     - source string
// pa      - Pointer to register structure
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int GetConstant( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa )
{
    uint idx;
    char c;
    int  val;

    idx=0;
    c = src[idx++];

    /* Get C## */
    if( toupper(c) != 'C' )
        goto INVALID_CONST;
    c = src[idx++];
    if( !isdigit(c) )
        goto INVALID_CONST;
    val = 0;
    while( isdigit(c) )
    {
        val *= 10;
        val += c-'0';
        c = src[idx++];
    }
    if( c || val>31 )
        goto INVALID_CONST;

    /* Setup the record */
    pa->Type  = ARGTYPE_CONSTANT;
    pa->Value = val;
    pa->Field = 0;

    return(1);

INVALID_CONST:
    Report(ps,REP_ERROR,"Operand %d invalid constant table entry '%s'",num,src);
    return(0);
}


/*
// GetR0offset
//
// Get "R0-offset" Argument for Burst Opcodes
//
// Parses the source string for a register and field
//
// ps      - Pointer to source file record
// num     - source line number
// src     - source string
// pa      - Pointer to register structure
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int GetR0offset( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa )
{
    uint idx;
    char c;
    int  val;

    idx=0;
    c = src[idx++];

    /* Get B## */
    if( toupper(c) != 'B' )
        goto INVALID_R0BYTE;
    c = src[idx++];
    if( !isdigit(c) )
        goto INVALID_R0BYTE;
    val = 0;
    while( isdigit(c) )
    {
        val *= 10;
        val += c-'0';
        c = src[idx++];
    }
    if( c || val>3 )
        goto INVALID_R0BYTE;

    /* Setup the record */
    pa->Type  = ARGTYPE_R0BYTE;
    pa->Value = val;
    pa->Field = 0;

    return(1);

INVALID_R0BYTE:
    Report(ps,REP_ERROR,"Operand %d invalid byte count '%s'",num,src);
    return(0);
}


/*
// GetJmpOffset
//
// Get 10 bit Jump Offset for Quick Branch
//
// Parses the source string for a register and field
//
// ps      - Pointer to source file record
// num     - operand number (1 based)
// src     - source string
// pa      - Pointer to register structure
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int GetJmpOffset( SOURCEFILE *ps, int num, char *src, PRU_ARG *pa )
{
    char tstr[TOKEN_MAX_LEN];
    uint val;
    int  idx;
    int  jmpoff;

    strcpy( tstr, src );
    if( Expression(ps, tstr, &val, &idx)<0 )
    {
        Report(ps,REP_ERROR,"Operand %d error in expression",num);
        return(0);
    }

    if( Pass==2 )
    {
        if( Options & OPTION_DEBUG )
            printf("%s(%5d) : EXP    : '%s' = %d\n", ps->SourceName,ps->CurrentLine,src,val);
        jmpoff = ((int)val) - CodeOffset;
        if( jmpoff < -512 || jmpoff > 511 )
            { Report(ps,REP_ERROR,"Operand %d relative jump out of range",num); return(0); }
    }
    else
        jmpoff = 0x3FF;
    /* Setup the record */
    pa->Type  = ARGTYPE_OFFSET;
    pa->Value = (uint)jmpoff;
    pa->Field = 0;

    return(1);
}

/*
// Offset2Reg
//
// Convert Register Offset to Register
//
// ps      - Pointer to source file record
// num     - operand number (1 based)
// pa      - Pointer to register structure
// addr    - Field address (0 - 127)
// size    - Field size (1, 2, or 4)
//
// Returns:
//      1 : Success
//      0 : Error
*/
static int Offset2Reg( SOURCEFILE *ps, int num, PRU_ARG *pa, uint addr, uint size )
{
    uint offset;

    offset = addr&3;

    if( addr+size > 128 )
        { Report(ps,REP_ERROR,"Operand %d, field extends past register file", num); return(0); }

    if( (size==2 && offset==3) ||
        (size==4 && offset!=0) )
            { Report(ps,REP_ERROR,"Operand %d, this field alignment not supported", num); return(0); }

    /* Build the record */
    pa->Type  = ARGTYPE_REGISTER;
    pa->Value = addr/4;

    /* Get field type */
    if( !(Options & OPTION_BIGENDIAN) )
    {
        /*
        // Little Endian Version
        */
        if( size==4 )
            pa->Field = FIELDTYPE_31_0;
        else if( size==2 )
        {
            if( !offset )
                pa->Field = FIELDTYPE_15_0;
            else if( offset==1 )
                pa->Field = FIELDTYPE_23_8;
            else
                pa->Field = FIELDTYPE_31_16;
        }
        else
        {
            if( !offset )
                pa->Field = FIELDTYPE_7_0;
            else if( offset==1 )
                pa->Field = FIELDTYPE_15_8;
            else if( offset==2 )
                pa->Field = FIELDTYPE_23_16;
            else
                pa->Field = FIELDTYPE_31_24;
        }
    }
    else
    {
        /*
        // Big Endian Version
        */
        if( size==4 )
            pa->Field = FIELDTYPE_31_0;
        else if( size==2 )
        {
            if( !offset )
                pa->Field = FIELDTYPE_31_16;
            else if( offset==1 )
                pa->Field = FIELDTYPE_23_8;
            else
                pa->Field = FIELDTYPE_15_0;
        }
        else
        {
            if( !offset )
                pa->Field = FIELDTYPE_31_24;
            else if( offset==1 )
                pa->Field = FIELDTYPE_23_16;
            else if( offset==2 )
                pa->Field = FIELDTYPE_15_8;
            else
                pa->Field = FIELDTYPE_7_0;
        }
    }

    return(1);
}
