/*
 * pru_ins.h
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
// File     : pru_ins.h
//
// Description:
//     Defines a data structre PRU_INST that can completely describe a
//     PRU opcode.
//
//---------------------------------------------------------------------------
// Revision:
//     15-Jun-12: 0.80 - Open source version
============================================================================*/

typedef struct _PRU_ARG {
    uint        Type;
    uint        Flags;              /* Flags for RegisterBit type */
#define PA_FLG_REGPOINTER           0x0001
#define PA_FLG_POSTINC              0x0002
#define PA_FLG_PREDEC               0x0004
    uint        Value;              /* Reg #, Imm Val, Count Val */
    uint        Field;              /* Field for Registers */
    uint        Bit;                /* Bit # for RegisterBit type */
} PRU_ARG;

#define ARGTYPE_REGISTER            1   /* Standard register and field */
#define ARGTYPE_IMMEDIATE           2   /* Immediate value */
#define ARGTYPE_COUNT               3   /* Count for burst */
#define ARGTYPE_R0BYTE              4   /* Byte from R0 */
#define ARGTYPE_CONSTANT            5   /* Constant Table Index */
#define ARGTYPE_OFFSET              6   /* 10 bit offset for jumps */
#define ARGTYPE_REGISTERBIT         7   /* Register in Rxx.Txx format Field=bitno */

#define FIELDTYPE_7_0               0   /* Bits 7:0 */
#define FIELDTYPE_15_8              1   /* Bits 15:8 */
#define FIELDTYPE_23_16             2   /* Bits 23:16 */
#define FIELDTYPE_31_24             3   /* Bits 31:24 */
#define FIELDTYPE_15_0              4   /* Bits 15:0 */
#define FIELDTYPE_23_8              5   /* Bits 23:8 */
#define FIELDTYPE_31_16             6   /* Bits 31:16 */
#define FIELDTYPE_31_0              7   /* Bits 31:0 */

#define FIELDTYPE_OFF_0             0   /* Offset bit 0 */
#define FIELDTYPE_OFF_8             1   /* Offset bit 8 */
#define FIELDTYPE_OFF_16            2   /* Offset bit 16 */
#define FIELDTYPE_OFF_24            3   /* Offset bit 24 */

extern char *FieldText[];

typedef struct _PRU_INST {
    uint        Op;                 /* Operation */
    uint        ArgCnt;             /* Argument Count */
    PRU_ARG    Arg[4];             /* Arguments */
} PRU_INST;

#define OP_ADD                      1
#define OP_ADC                      2
#define OP_SUB                      3
#define OP_SUC                      4
#define OP_LSL                      5
#define OP_LSR                      6
#define OP_RSB                      7
#define OP_RSC                      8
#define OP_AND                      9
#define OP_OR                       10
#define OP_XOR                      11
#define OP_NOT                      12
#define OP_MIN                      13
#define OP_MAX                      14
#define OP_CLR                      15
#define OP_SET                      16
#define OP_LDI                      17
#define OP_LBBO                     18
#define OP_LBCO                     19
#define OP_SBBO                     20
#define OP_SBCO                     21
#define OP_LFC                      22
#define OP_STC                      23
#define OP_JAL                      24
#define OP_JMP                      25
#define OP_QBGT                     26
#define OP_QBLT                     27
#define OP_QBEQ                     28
#define OP_QBGE                     29
#define OP_QBLE                     30
#define OP_QBNE                     31
#define OP_QBA                      32
#define OP_QBBS                     33
#define OP_QBBC                     34
#define OP_LMBD                     35
#define OP_CALL                     36
#define OP_WBC                      37
#define OP_WBS                      38
#define OP_MOV                      39
#define OP_MVIB                     40
#define OP_MVIW                     41
#define OP_MVID                     42
#define OP_SCAN                     43
#define OP_HALT                     44
#define OP_SLP                      45
#define OP_RET                      46
#define OP_ZERO                     47
#define OP_XIN                      48
#define OP_XOUT                     49
#define OP_XCHG                     50
#define OP_FILL                     51
#define OP_SXIN                     52
#define OP_SXOUT                    53
#define OP_SXCHG                    54
#define OP_LOOP		            55
#define OP_ILOOP	            56
#define OP_MAXIDX                   56

extern char *OpText[];
