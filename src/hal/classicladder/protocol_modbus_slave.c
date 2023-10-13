/* Classic Ladder Project */
/* Copyright (C) 2001-2004 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* December 2004 */
/* --------------------- */
/* Modbus slave protocol */
/* --------------------- */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "protocol_modbus_defines.h"
#include "protocol_modbus_slave.h"
//////#include "protocol_modbus_master.h" // some Modbus defines shared



/* TEMP!!! put this variable in global config instead ? */
int OffsetForVars = 1;


/* Question here start directly with function code 
  (no IP header or Slave number) */
int ModbusRequestToRespond( unsigned char * Question, int LgtQuestion, unsigned char * Response )
{
	int LgtResponse = 0;
	int ErrorCode = 0;
	int ScanEle;
	unsigned char FunctionCode = Question[ 0 ];
printf("FUNCTION CODE=%d\n", FunctionCode );
	switch( FunctionCode )
	{
		// Read n bits (read or write bits)
		case MODBUS_FC_READ_COILS:
		case MODBUS_FC_READ_INPUTS:
			if ( LgtQuestion>=5 )
			{
				int FirstBit = (Question[1]<<8) | Question[2];
				int NbrBits = (Question[3]<<8) | Question[4];
				int NbrRealBytes = (NbrBits+7)/8;
				int ScanByte, ScanBit;
				// validity request verify
				if ( FirstBit+1+NbrRealBytes*8>GetMobdusSlaveNbrVars( FunctionCode ) )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( NbrBits>2000 )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
					
				if ( ErrorCode==0 )
				{ 
					// Code Function for response
					Response[ LgtResponse++ ] = FunctionCode;
					// Length in bytes
					Response[ LgtResponse++ ] = NbrRealBytes;
					// Bits values
					ScanEle = 0;
					for( ScanByte=0; ScanByte<NbrRealBytes; ScanByte++ )
					{
						unsigned char BitsValues = 0;
						unsigned char Mask = 0x01;
						for( ScanBit=0; ScanBit<8; ScanBit++ )
						{
							if( GetVarForModbusSlave( FunctionCode, FirstBit+ScanEle++ ) )
								BitsValues = BitsValues|Mask;
							Mask = Mask<<1;
						}
						Response[ LgtResponse++ ] = BitsValues;
					}
				}
			}
			else
			{
				ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
			}
			break;
		// Read n words (read or write words)
		case MODBUS_FC_READ_HOLD_REGS:
		case MODBUS_FC_READ_INPUT_REGS:
			if ( LgtQuestion>=5 )
			{
				int FirstWord = (Question[1]<<8) | Question[2];
				int NbrWords = (Question[3]<<8) | Question[4];
				// validity request verify
				if ( FirstWord+1+NbrWords>GetMobdusSlaveNbrVars( FunctionCode ) )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( NbrWords>200 )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
					
				if ( ErrorCode==0 )
				{ 
					// Code Function for response
					Response[ LgtResponse++ ] = FunctionCode;
					// Length in bytes
					Response[ LgtResponse++ ] = NbrWords*2;
					// Words values
					for( ScanEle=0; ScanEle<NbrWords; ScanEle++ )
					{
						int ValueWord = GetVarForModbusSlave( FunctionCode, FirstWord+ScanEle );
						Response[ LgtResponse++ ] = (unsigned char)(ValueWord>>8);
						Response[ LgtResponse++ ] = (unsigned char)ValueWord;
					}
				}
			}
			else
			{
				ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
			}
			break;
		// Write one bit (write bit)
		case MODBUS_FC_FORCE_COIL:
			if ( LgtQuestion>=5 )
			{
				int FirstBit = (Question[1]<<8) | Question[2];
				int OffsetQuest = 3;
				int ValueBit;
				ValueBit = (Question[ OffsetQuest++ ]<<8 );
				ValueBit = ValueBit | Question[ OffsetQuest++ ];
			
				// validity request verify
				if ( FirstBit+1>GetMobdusSlaveNbrVars( FunctionCode ) )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( ValueBit!=MODBUS_BIT_ON && ValueBit!=MODBUS_BIT_OFF )
					ErrorCode = MODBUS_ILLEGAL_DATA_VALUE;
					
				if ( ErrorCode==0 )
				{
					SetVarFromModbusSlave( FunctionCode, FirstBit, ValueBit?1:0 );
					// Code Function for response
					Response[ LgtResponse++ ] = FunctionCode;
					// First Bit
					Response[ LgtResponse++ ] = (unsigned char)(FirstBit>>8);
					Response[ LgtResponse++ ] = (unsigned char)(FirstBit);
					// Bit value
					Response[ LgtResponse++ ] = (unsigned char)(ValueBit>>8);
					Response[ LgtResponse++ ] = (unsigned char)(ValueBit);
				}
			}
			break;
		// Write n bits (write bits)
		case MODBUS_FC_FORCE_COILS:
			if ( LgtQuestion>=7 )
			{
				int FirstBit = (Question[1]<<8) | Question[2];
				int NbrBits = (Question[3]<<8) | Question[4];
				// validity request verify
				if ( FirstBit+1+NbrBits>GetMobdusSlaveNbrVars( FunctionCode ) )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( NbrBits>2000 )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( (NbrBits+7)/8>Question[5] )
					ErrorCode = MODBUS_ILLEGAL_DATA_VALUE;
					
				if ( ErrorCode==0 )
				{ 
					int ScanByte = 6;
					int ScanBit = 0;
					int CurrentBit = FirstBit;
					unsigned char Mask = 0x01;
					do
					{
						int Value = 0;
						if ( Question[ ScanByte ]&Mask )
							Value = 1;
						SetVarFromModbusSlave( FunctionCode, CurrentBit, Value );
						ScanBit++;
						Mask = Mask<<1; 
						if ( ScanBit>=8 )
						{
							ScanBit = 0;
							ScanByte++;
							Mask = 0x01;
						}
						CurrentBit++;
					}
					while( CurrentBit<FirstBit+NbrBits );
					
					// Code Function for response
					Response[ LgtResponse++ ] = FunctionCode;
					// First bit
					Response[ LgtResponse++ ] = (unsigned char)(FirstBit>>8);
					Response[ LgtResponse++ ] = (unsigned char)(FirstBit);
					// Nbr bits
					Response[ LgtResponse++ ] = (unsigned char)(NbrBits>>8);
					Response[ LgtResponse++ ] = (unsigned char)(NbrBits);
				}
			}
			break;
		// Write 1 or n words (write words)
		case MODBUS_FC_WRITE_HOLD_REG:
		case MODBUS_FC_WRITE_HOLD_REGS:
			if ( ( LgtQuestion>=5 && Question[0]==6 ) || ( LgtQuestion>=8 && Question[0]==16 ) )
			{
				int OffsetQuest = 3;
				int FirstWord = (Question[1]<<8) | Question[2];
				int NbrWords = 1;
				int ValueWord = 0;
				// is type n words ?
				if ( Question[0]==16 )
				{
					NbrWords = (Question[3]<<8) | Question[4];
					OffsetQuest = 6;
					if ( NbrWords*2>Question[5] )
						ErrorCode = MODBUS_ILLEGAL_DATA_VALUE;
				}
				// request verify
				if ( FirstWord+1+NbrWords>GetMobdusSlaveNbrVars( FunctionCode ) )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
				if ( NbrWords>200 )
					ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
					
				if ( ErrorCode==0 )
				{ 
					// Words values
					for( ScanEle=0; ScanEle<NbrWords; ScanEle++ )
					{
						ValueWord = (Question[ OffsetQuest++ ]<<8 );
						ValueWord = ValueWord | Question[ OffsetQuest++ ];
						SetVarFromModbusSlave( FunctionCode, FirstWord+ScanEle, ValueWord );
					}
					// Code Function for response
					Response[ LgtResponse++ ] = FunctionCode;
					// First word
					Response[ LgtResponse++ ] = (unsigned char)(FirstWord>>8);
					Response[ LgtResponse++ ] = (unsigned char)(FirstWord);
					// is type n words ?
					if ( Question[0]==16 )
					{
						// Nbr Words
						Response[ LgtResponse++ ] = (unsigned char)(NbrWords>>8);
						Response[ LgtResponse++ ] = (unsigned char)(NbrWords);
					}
					else
					{
						// Word value
						Response[ LgtResponse++ ] = (unsigned char)(ValueWord>>8);
						Response[ LgtResponse++ ] = (unsigned char)(ValueWord);
					}
				}
			}
			else
			{
				ErrorCode = MODBUS_ILLEGAL_DATA_ADDRESS;
			}
			break;
		default:
			Response[ LgtResponse++ ] = 0x80 | FunctionCode;
			Response[ LgtResponse++ ] = MODBUS_ILLEGAL_FUNCTION;
			break;
	}
	if ( ErrorCode>0 )
	{
		LgtResponse = 0;
		Response[ LgtResponse++ ] = 0x80 | FunctionCode;
		Response[ LgtResponse++ ] = ErrorCode;
	}
	return LgtResponse;
}


int GetMobdusSlaveNbrVars( unsigned char FunctCode )
{
	switch( FunctCode )
	{
		case MODBUS_FC_READ_COILS:
		case MODBUS_FC_READ_INPUTS:
		case MODBUS_FC_FORCE_COIL:
		case MODBUS_FC_FORCE_COILS:
			return InfosGene->GeneralParams.SizesInfos.nbr_bits;
		case MODBUS_FC_READ_HOLD_REGS:
		case MODBUS_FC_READ_INPUT_REGS:
		case MODBUS_FC_WRITE_HOLD_REG:
		case MODBUS_FC_WRITE_HOLD_REGS:
			return InfosGene->GeneralParams.SizesInfos.nbr_words;
	}
	return 0;
}

void SetVarFromModbusSlave( unsigned char FunctCode, int ModbusNum, int Value )
{
	switch( FunctCode )
	{
		case MODBUS_FC_READ_COILS:
		case MODBUS_FC_READ_INPUTS:
		case MODBUS_FC_FORCE_COIL:
		case MODBUS_FC_FORCE_COILS:
			WriteVar( VAR_MEM_BIT, ModbusNum+OffsetForVars, Value );
		case MODBUS_FC_READ_HOLD_REGS:
		case MODBUS_FC_READ_INPUT_REGS:
		case MODBUS_FC_WRITE_HOLD_REG:
		case MODBUS_FC_WRITE_HOLD_REGS:
			WriteVar( VAR_MEM_WORD, ModbusNum+OffsetForVars, Value );
	}
}
int GetVarForModbusSlave( unsigned char FunctCode, int ModbusNum )
{
	switch( FunctCode )
	{
		case MODBUS_FC_READ_COILS:
		case MODBUS_FC_READ_INPUTS:
		case MODBUS_FC_FORCE_COIL:
		case MODBUS_FC_FORCE_COILS:
			return ReadVar( VAR_MEM_BIT, ModbusNum+OffsetForVars );
		case MODBUS_FC_READ_HOLD_REGS:
		case MODBUS_FC_READ_INPUT_REGS:
		case MODBUS_FC_WRITE_HOLD_REG:
		case MODBUS_FC_WRITE_HOLD_REGS:
			return ReadVar( VAR_MEM_WORD, ModbusNum+OffsetForVars );
	}
	return 0;
}

