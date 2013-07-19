/* Classic Ladder Project */
/* Copyright (C) 2001-2005 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* August 2005 */
/* ---------------------------------------- */
/* Modbus master protocol (Distributed I/O) */
/* ======================================== */
/* The outputs (coils and words) are always */
/* polled for writing (not only on change), */
/* so only this master should write them !  */
/* ---------------------------------------- */

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
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "classicladder.h"
#include "global.h"
#include "vars_access.h"
#include "protocol_modbus_master.h"
#include "socket_modbus_master.h"

StrModbusMasterReq ModbusMasterReq[ NBR_MODBUS_MASTER_REQ ];
// if '\0' => IP mode used for I/O modbus modules
char ModbusSerialPortNameUsed[ 30 ];
int ModbusSerialSpeed;
int ModbusSerialDataBits;
int ModbusSerialStopBits;
int ModbusSerialParity;
int ModbusSerialUseRtsToSend;
int ModbusTimeInterFrame;
int ModbusTimeOutReceipt;
int ModbusTimeAfterTransmit;

/* TEMP!!! put this variable in global config instead ? */
int ModbusEleOffset = 0;
int ModbusDebugLevel = 0; 
int MapInputs= 0;
int MapHolding= 0;
int MapRegisterRead= 0;
int MapRegisterWrite= 0;

int CurrentReq;
int InvoqIdentifHeaderIP;
unsigned char CurrentFuncCode;
int ErrorCnt;

void InitModbusMasterBeforeReadConf( void )
{ 
        ModbusSerialPortNameUsed[ 0 ] = '\0';
        ModbusSerialSpeed = 9600;
        ModbusSerialDataBits = 8;
        ModbusSerialStopBits = 1;
        ModbusSerialParity = 0; // no parity
        ModbusEleOffset = 0;
        ModbusSerialUseRtsToSend = 0;
        ModbusTimeInterFrame = 100;
        ModbusTimeOutReceipt = 500;
        ModbusTimeAfterTransmit = 0;
        ModbusDebugLevel= 0;
        MapCoilRead = 0;
        MapCoilWrite = 0;
        MapInputs = 0;
        MapHolding = 0;
        MapRegisterRead = 0;
        MapRegisterWrite = 0;

        InitModbusMasterParams ( );
}

void PrepareModbusMaster( void )
{
       if(modmaster) 
            { 
                CloseSocketModbusMaster( );
		InitSocketModbusMaster( );
            }
}

void InitModbusMasterParams( void )
{
	int ScanReq;
	for( ScanReq=0; ScanReq<NBR_MODBUS_MASTER_REQ; ScanReq++ )
	{
		ModbusMasterReq[ ScanReq ].SlaveAdr[ 0 ] = '\0'; 
		ModbusMasterReq[ ScanReq ].TypeReq = MODBUS_REQ_INPUTS_READ;
		ModbusMasterReq[ ScanReq ].FirstModbusElement = 1;
		ModbusMasterReq[ ScanReq ].NbrModbusElements = 1;
		ModbusMasterReq[ ScanReq ].LogicInverted = 0;
		ModbusMasterReq[ ScanReq ].OffsetVarMapped = 0;
	}
	
	
	CurrentReq = -1;
	InvoqIdentifHeaderIP = 0;
	CurrentFuncCode = 0;
	ErrorCnt = 0;
}

void FindNextReqFromTable( void )
{
	char Found = FALSE;
	int LoopSec = 0;
	int ReqSearched = CurrentReq;
	do
	{
		ReqSearched++;
		if ( ReqSearched>=NBR_MODBUS_MASTER_REQ )
			ReqSearched = 0;
		if ( ModbusMasterReq[ ReqSearched ].SlaveAdr[ 0 ]!='\0' )
			Found = TRUE;
		LoopSec++;
	}
	while( !Found  && LoopSec<NBR_MODBUS_MASTER_REQ+1 );
	if ( Found )
		CurrentReq = ReqSearched;
	else
		CurrentReq = -1;
}

/* Question prepared here start directly with function code 
  (no IP header or Slave number) */
int PrepPureModbusAskForCurrentReq( unsigned char * AskFrame )
{
	int FrameSize = 0;
	unsigned char FunctionCode = 0;
	int FirstEle = ModbusMasterReq[ CurrentReq ].FirstModbusElement-ModbusEleOffset; 
	int NbrEles = ModbusMasterReq[ CurrentReq ].NbrModbusElements;
	if ( FirstEle<0 )
		FirstEle = 0;
	switch( ModbusMasterReq[ CurrentReq ].TypeReq )
	{
		case MODBUS_REQ_INPUTS_READ:
			FunctionCode = MODBUS_FC_READ_INPUTS;
			break;
                case MODBUS_REQ_COILS_READ:
			FunctionCode = MODBUS_FC_READ_COILS;
			break;
		case MODBUS_REQ_COILS_WRITE:
			FunctionCode = MODBUS_FC_FORCE_COILS;
			if ( ModbusMasterReq[ CurrentReq ].NbrModbusElements==1 )
				FunctionCode = MODBUS_FC_FORCE_COIL; 
			break;
		case MODBUS_REQ_REGISTERS_READ:
			FunctionCode = MODBUS_FC_READ_INPUT_REGS;
			break;
		case MODBUS_REQ_REGISTERS_WRITE:
			FunctionCode = MODBUS_FC_WRITE_REGS;
			if ( ModbusMasterReq[ CurrentReq ].NbrModbusElements==1 )
				FunctionCode = MODBUS_FC_WRITE_REG; 
			break;
		case MODBUS_REQ_HOLD_READ:
			FunctionCode = MODBUS_FC_READ_HOLD_REGS;
			break;
		case MODBUS_REQ_DIAGNOSTICS:
			FunctionCode = MODBUS_FC_DIAGNOSTICS;
			break;
	}
	if ( FunctionCode>0 )
	{
		AskFrame[ FrameSize++ ] = FunctionCode;
		CurrentFuncCode = FunctionCode;
		switch( FunctionCode )
		{
			case MODBUS_FC_READ_INPUTS:    // 2				
			case MODBUS_FC_READ_INPUT_REGS:// 4 
                        case MODBUS_FC_READ_COILS:     // 1				
			case MODBUS_FC_READ_HOLD_REGS: // 3
                            AskFrame[ FrameSize++ ] = FirstEle >> 8;
                            AskFrame[ FrameSize++ ] = FirstEle & 0xff;
                            AskFrame[ FrameSize++ ] = NbrEles >> 8;
                            AskFrame[ FrameSize++ ] = NbrEles & 0xff;
                            break;
			case MODBUS_FC_FORCE_COIL:     // 5
			{
				int BitValue = GetVarForModbus( &ModbusMasterReq[ CurrentReq ], FirstEle );
				BitValue = (BitValue!=0)?MODBUS_BIT_ON:MODBUS_BIT_OFF;
				AskFrame[ FrameSize++ ] = FirstEle >> 8;
				AskFrame[ FrameSize++ ] = FirstEle & 0xff;
				AskFrame[ FrameSize++ ] = BitValue >> 8;
				AskFrame[ FrameSize++ ] = BitValue & 0xff;
				break;
			}
			case MODBUS_FC_FORCE_COILS:   // 15
			{
				int NbrRealBytes = (NbrEles+7)/8;
				int ScanEle = 0;
				int ScanByte, ScanBit;
				AskFrame[ FrameSize++ ] = FirstEle >> 8;
				AskFrame[ FrameSize++ ] = FirstEle & 0xff;
				AskFrame[ FrameSize++ ] = NbrEles >> 8;
				AskFrame[ FrameSize++ ] = NbrEles & 0xff;
				AskFrame[ FrameSize++ ] = NbrRealBytes & 0xff; /* this may get truncated */
				for( ScanByte=0; ScanByte<NbrRealBytes; ScanByte++ )
				{
					unsigned char Mask = 0x01;
					unsigned char ByteRes = 0;
					for( ScanBit=0; ScanBit<8; ScanBit++ )
					{
						int Value = GetVarForModbus( &ModbusMasterReq[ CurrentReq ], FirstEle+ScanEle );
						if ( Value && ScanEle<NbrEles )
							ByteRes = ByteRes | Mask;
						ScanEle++;
						Mask = Mask<<1;
					}
					AskFrame[ FrameSize++ ] = ByteRes;
				}
				break;
			}
			case MODBUS_FC_WRITE_REG:/*(function 6 */
			{
				int Value;
				AskFrame[ FrameSize++ ] = FirstEle >> 8;
				AskFrame[ FrameSize++ ] = FirstEle & 0xff;
				Value = GetVarForModbus( &ModbusMasterReq[ CurrentReq ], FirstEle );
//				printf("INFO MODBUS writing: WORD value =%d \n",Value);
				AskFrame[ FrameSize++ ] = Value >> 8;
				AskFrame[ FrameSize++ ] = Value & 0xff;
			}	
			break;

			case MODBUS_FC_WRITE_REGS: /*function 16 */
			{
				int i ;
				AskFrame[ FrameSize++ ] = FirstEle >> 8;
				AskFrame[ FrameSize++ ] = FirstEle & 0xff;
				AskFrame[ FrameSize++ ] = NbrEles >> 8;
				AskFrame[ FrameSize++ ] = NbrEles & 0xff;
				AskFrame[ FrameSize++ ] = (NbrEles*2) & 0xff; /* this may get truncated */
				for (i=0; i <NbrEles; i++)
				{
				int Value = GetVarForModbus( &ModbusMasterReq[ CurrentReq ], FirstEle +i );
//				printf("INFO MODBUS writing: WORD value =%d \n",Value);
				AskFrame[ FrameSize++ ] = Value >> 8;
				AskFrame[ FrameSize++ ] = Value & 0xff;
				}
				break;
			}
			case MODBUS_FC_DIAGNOSTICS://8
				AskFrame[ FrameSize++ ] = 0; //sub function number for echo
				AskFrame[ FrameSize++ ] = 0; //sub function number for echo
				AskFrame[ FrameSize++ ] = 1; // MSB of hardcoded 257
				AskFrame[ FrameSize++ ] = 1; // LSB of hardcoded 257
				break;
		}
	}
	return FrameSize;
}

/* Response given here start directly with function code 
  (no IP header or Slave number) */
int TreatPureModbusResponse( unsigned char * RespFrame, int SizeFrame )
{
	int cError = -1;

	if ( RespFrame[ 0 ]&MODBUS_FC_EXCEPTION_BIT )
	{
		printf("ERROR CLASSICLADDER-     MODBUS-EXCEPTION RECEIVED:%X (Excep=%X)\n", RespFrame[ 0 ], RespFrame[ 1 ]);
	}
	else
	{
		if ( RespFrame[0]!=CurrentFuncCode )
		{
			printf("ERROR CLASSICLADDER-    MODBUS -Function code received from slave was not the same as requested!\n");
		}
		else
		{
	
			int FirstEle = ModbusMasterReq[ CurrentReq ].FirstModbusElement-ModbusEleOffset;
			int NbrEles = ModbusMasterReq[ CurrentReq ].NbrModbusElements;

			if ( FirstEle<0 )
				FirstEle = 0;
			switch( RespFrame[ 0 ] )
			{
				case MODBUS_FC_READ_INPUTS://function code 2
                                case MODBUS_FC_READ_COILS: //function code 1
				{
					int NbrRealBytes = RespFrame[1];
					// validity request verify 
					if ( NbrRealBytes==(NbrEles+7)/8 && SizeFrame>=1+1+NbrRealBytes )
					{
						int ScanByte, ScanBit;
						int ScanEle = 0;
						// Bits values
						for( ScanByte=0; ScanByte<NbrRealBytes; ScanByte++ )
						{
							unsigned char BitsValues = RespFrame[ 2+ScanByte ];
							unsigned char Mask = 0x01;
							for( ScanBit=0; ScanBit<8; ScanBit++ )
							{
								int Value = 0;
								if ( BitsValues & Mask )
									Value = 1;
								if (  ModbusMasterReq[ CurrentReq ].LogicInverted )
									Value = (Value==0)?1:0;
								if ( ScanEle<NbrEles )
									SetVarFromModbus( &ModbusMasterReq[ CurrentReq ], FirstEle+ScanEle++, Value );
								Mask = Mask<<1;
							}
						}
						cError = 0;
					}
					break;
				}
				case MODBUS_FC_READ_INPUT_REGS://function 4
				case MODBUS_FC_READ_HOLD_REGS: //function 3
				{
					int i;
					int NbrBytes = RespFrame[1]/2;
					for (i=0; i <NbrBytes; i++) 
					{
						int hivalue=(RespFrame[2+(i*2)]<<8);
						int lovalue=( RespFrame[3+(i*2)]);
						int value=hivalue | lovalue;
						SetVarFromModbus( &ModbusMasterReq[ CurrentReq ], FirstEle+i, value );
					}
						cError = 0;
				
					break;
				}	
				case MODBUS_FC_FORCE_COIL://function 5
					if ( ((RespFrame[1]<<8) | RespFrame[2])==FirstEle && SizeFrame>=1+2 )
						cError = 0;
					break;
				case MODBUS_FC_FORCE_COILS://function 15
					if ( ((RespFrame[1]<<8) | RespFrame[2])==FirstEle && SizeFrame>=1+2+2 )
					{
						if ( ((RespFrame[3]<<8) | RespFrame[4])==NbrEles )
							cError = 0;
					}
					break;
				case MODBUS_FC_WRITE_REG://function 6
					if ( ((RespFrame[1]<<8) | RespFrame[2])==FirstEle && SizeFrame==1+2+2 )
					{	cError=0;	}
						
					break;
				case MODBUS_FC_WRITE_REGS: //function 16
					if ( ((RespFrame[1]<<8) | RespFrame[2])==FirstEle && SizeFrame>=1+2+2 )
					{
						if ( ((RespFrame[3]<<8) | RespFrame[4])==NbrEles )
							cError = 0;
					}
					break;
				case MODBUS_FC_DIAGNOSTICS://function 8
					if ( ((RespFrame[3]<<8) | RespFrame[4])== 257 )
					{	
					printf("INFO CLASSICLADDER-   MODBUS -Echo back from slave #%s is correct (data=257).\n",ModbusMasterReq[ CurrentReq ].SlaveAdr);
					cError = 0;
					}else{
					printf("ERROR CLASSICLADDER-    MODBUS -Echo back from slave #%s is WRONG.\n",ModbusMasterReq[ CurrentReq ].SlaveAdr);
					}
					break;
			}
		}
	}
	return cError;
}

/* Give number of bytes of the response we should receive for the current request */
int GetModbusResponseLenghtToReceive( void )
{
	int LgtResp = 0, NbrRealBytes;
	if ( CurrentReq!=-1 )
	{
		int NbrEles = ModbusMasterReq[ CurrentReq ].NbrModbusElements;
		switch( CurrentFuncCode )
		{
				case MODBUS_FC_READ_INPUTS:
                                case MODBUS_FC_READ_COILS:
                                // 1 byte for function code
                                // 1 byte for count of data bytes returned 
                                // 1 byte for 8 coils if number of coils is not evenly divisable by 8 add another byte
				        NbrRealBytes = (NbrEles+7)/8;
                                        LgtResp = 2+NbrRealBytes;
                                        break;
				case MODBUS_FC_READ_INPUT_REGS:
				case MODBUS_FC_READ_HOLD_REGS:
                                // 1 byte for function code
                                // 1 byte for count of data bytes returned
                                // 2 bytes per register for data returned and 1 byte for number of registers (max 125)
					LgtResp = 2+2*NbrEles;
					break;
				case MODBUS_FC_FORCE_COIL:
				case MODBUS_FC_FORCE_COILS:
				case MODBUS_FC_WRITE_REG:
				case MODBUS_FC_WRITE_REGS:
                                // 1 byte for function code
                                // 2 bytes for starting address
                                // 2 bytes for number of addresses
					LgtResp = 5;
					break;
				case MODBUS_FC_DIAGNOSTICS:
                                // assume hardcoded sub code: echo (0x0)
                                // 2 byte for function code
                                // 2 bytes for sub code 2 for data
					LgtResp = 5;
					break;
                                default:
                                        printf("INFO CLASSICLADDER-   MODBUS function code not reconized");
                                        break;
		}
	}
	if ( ModbusDebugLevel>=3 )
		printf("INFO CLASSICLADDER-   MODBUS Length we should receive=%d (+3)\n",LgtResp);
	return LgtResp;
}

/* Table of CRC values for high-order byte */
static unsigned char auchCRCHi[] = {
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
} ; 
/* Table of CRC values for low-order byte */
static char auchCRCLo[] = {
0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 
0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 
0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 
0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 
0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 
0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 
0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 
0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 
0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 
0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 
0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 
0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 
0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 
0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 
0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 
0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 
0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 
0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 
0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 
0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 
0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 
0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 
0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 
0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 
0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 
0x43, 0x83, 0x41, 0x81, 0x80, 0x40
} ;
/* CRC16 calc on frame pointed by puchMsg and usDataLen lenght */
/* Pre-calc routine taken from http://www.modicon.com/techpubs/crc7.html site */
unsigned short CRC16(unsigned char *puchMsg, unsigned short usDataLen)
{
	unsigned char uchCRCHi = 0xFF ;	/* high CRC byte initialized */
	unsigned char uchCRCLo = 0xFF ;	/* low CRC byte initialized  */
	unsigned uIndex ; /* will index into CRC lookup table */
	while (usDataLen--) /* pass through message buffer */
	{
		uIndex = uchCRCHi ^ *puchMsg++ ; /* calculate the CRC */
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		uchCRCLo = auchCRCLo[uIndex] ;
	}
	return (uchCRCHi << 8 | uchCRCLo) ;
}


/* Question generated here start directly with function code 
  (no IP header or Slave number) */
int ModbusMasterAsk( unsigned char * SlaveAddressIP, unsigned char * Question )
{
	int LgtAskFrame = 0;
	
	if ( CurrentReq==-1 )
		FindNextReqFromTable( );
	if ( CurrentReq!=-1 )
	{
		// start of the usefull frame depend if serial or IP
		int OffsetHeader = LGT_MODBUS_IP_HEADER;
		// Modbus/RTU on serial used ?
		if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
			OffsetHeader = 1; // slave address 
		LgtAskFrame = PrepPureModbusAskForCurrentReq( &Question[ OffsetHeader ] );
		if ( LgtAskFrame>0 )
		{
			if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
			{
				unsigned short CalcCRC;
				LgtAskFrame = LgtAskFrame+OffsetHeader;
				
				// slave address
				Question[ 0 ] = atoi( ModbusMasterReq[ CurrentReq ].SlaveAdr );
				// add CRC at the end of the frame
				CalcCRC = CRC16( &Question[ 0 ], LgtAskFrame ) ;
				Question[ LgtAskFrame++ ] = (unsigned char)(CalcCRC>>8); 
				Question[ LgtAskFrame++ ] = (unsigned char)CalcCRC;
			}
			else
			{
				// add IP specific header
				InvoqIdentifHeaderIP++;
				if ( InvoqIdentifHeaderIP>65535 )
					InvoqIdentifHeaderIP = 0;
				// invocation identifier
				Question[ 0 ] = (unsigned char)(InvoqIdentifHeaderIP>>8);
				Question[ 1 ] = (unsigned char)InvoqIdentifHeaderIP;
				// protocol identifier
				Question[ 2 ] = 0;
				Question[ 3 ] = 0;
				// length
				Question[ 4 ] = (unsigned char)((LgtAskFrame+1)>>8);
				Question[ 5 ] = (unsigned char)(LgtAskFrame+1);
				// unit identifier
				Question[ 6 ] = 1;
				strcpy( (char*) SlaveAddressIP, ModbusMasterReq[ CurrentReq ].SlaveAdr );
				LgtAskFrame = LgtAskFrame+OffsetHeader;
			}
			 
		}
		if ( ModbusDebugLevel>=1 )
		{
			int DebugFrame;
			printf("INFO CLASSICLADDER-   Modbus I/O module to send: Lgt=%d <-  ", LgtAskFrame );
			for( DebugFrame=0; DebugFrame<LgtAskFrame; DebugFrame++ )
			{
                           if (DebugFrame==0) {   printf("Slave address-%X  ", Question[ DebugFrame]);   
                           }else{   if (DebugFrame==1) {   printf("Function code-%X  Data-", Question[ DebugFrame]);   
				       }else{            printf("%X ", Question[ DebugFrame ] );     }}
			}
			printf("\n");
		}
	}
	return LgtAskFrame;
}

char TreatModbusMasterResponse( unsigned char * Response, int LgtResponse )
{
	int DebugFrame;
	char RepOk = FALSE;
	if ( ModbusDebugLevel>=1 )
	{
		printf("INFO CLASSICLADDER-   Modbus I/O module received: Lgt=%d -> ", LgtResponse );
		for( DebugFrame=0; DebugFrame<LgtResponse; DebugFrame++ )
		{
			if (DebugFrame==0) {   printf("(Slave address-%X - ", Response[ DebugFrame]);   
                        }else{   if (DebugFrame==1) {   printf("Function code-%X ) ", Response[ DebugFrame]);
				    }else{            printf("%X ", Response[ DebugFrame ] );     }}
		}
		printf("\n");
	}

	if ( CurrentReq!=-1 )
	{
		char FrameOk = FALSE;
		// start of the usefull frame depend if serial or IP
		int OffsetHeader = LGT_MODBUS_IP_HEADER;
		if ( LgtResponse > 0 )
		{
			// Modbus/RTU on serial used ?
			if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
			{
				unsigned short CalcCRC = CRC16( &Response[ 0 ], LgtResponse ) ;
				OffsetHeader = 1; // slave address
				// verify CRC
				if( CalcCRC==0 )
				{
					LgtResponse = LgtResponse-2;
					// verify number of slave which has responded
					if ( Response[ 0 ]==atoi( ModbusMasterReq[ CurrentReq ].SlaveAdr ) )
						FrameOk = TRUE;
				}
				else
				{
					printf("ERROR CLASSICLADDER-   MODBUS CRC error: calc=%x, frame=%x\n", CalcCRC, (Response[ LgtResponse-2 ]<<8)|Response[ LgtResponse-1 ] );
				}
			}
			else
			{
				// verify if transaction identifier correspond
				int TransId = (Response[ 0 ]<<8) | Response[ 1 ];
				if ( TransId==InvoqIdentifHeaderIP )
					FrameOk = TRUE;
			}
		} 
	
		if ( FrameOk )
		{
			// if valid frame => advance to next request
			if ( TreatPureModbusResponse( &Response[ OffsetHeader ], LgtResponse-OffsetHeader)>=0 )
			{
				ErrorCnt = 0;
				FindNextReqFromTable( );
				RepOk = TRUE;
			}
			else
			{
				printf("ERROR CLASSICLADDER-   MODBUS-INCORRECT RESPONSE RECEIVED FROM SLAVE!!!\n");
				ErrorCnt++;
			}
		}
		else
		{
			printf("ERROR CLASSICLADDER-   MODBUS-LOW LEVEL ERROR IN RESPONSE!!!\n");
                        //set error coil 0 on 'MODBUS ERROR'
                        
			ErrorCnt++;
		}
		if ( ErrorCnt>=3 )
		{
			ErrorCnt = 0;
			FindNextReqFromTable( );
		}

		
	}
        //set error coil (%E0) as apprioprate  
        if (RepOk==TRUE) {    WriteVar( VAR_ERROR_BIT, 0, FALSE);   }else{    WriteVar( VAR_ERROR_BIT, 0, TRUE);   }
	return RepOk;
}

/* Functions abstraction for project used */
// For EMC modbus coils/inputs must be mapped to memory bits (%B)
// because HAL will overwrite physical input variables (%I)
void SetVarFromModbus( StrModbusMasterReq * ModbusReq, int ModbusNum, int Value )
{
	int FirstEle = ModbusReq->FirstModbusElement-ModbusEleOffset;
	int VarNum;
        
	if ( FirstEle<0 )
		FirstEle = 0;
	VarNum = ModbusNum-FirstEle+ModbusReq->OffsetVarMapped;
	switch( ModbusReq->TypeReq )
	{
		case MODBUS_REQ_INPUTS_READ:
                case MODBUS_REQ_COILS_READ:
                       switch ( MapCoilRead )
                          {
                           case B_VAR:
			        WriteVar( VAR_MEM_BIT, VarNum, Value );
                                break;
                           case Q_VAR:
			        WriteVar( VAR_PHYS_OUTPUT, VarNum, Value );
                               break;
                           default:  
			       break;
                        }
                     break;
		case MODBUS_REQ_REGISTERS_READ:
		case MODBUS_REQ_HOLD_READ:
                        switch ( MapRegisterRead )
                           {
                            case W_VAR:
			         WriteVar( VAR_MEM_WORD, VarNum, Value );
                                 break;
                            case QW_VAR:
			         WriteVar( VAR_PHYS_WORD_OUTPUT, VarNum, Value );
                                 break;
                            default:  
			         break;
                           }
                      break;
                default:
                        printf("ERROR CLASSICLADDER--- Variable not defined for MODBUS READ REQUEST mapping ");
                      break;
                        
	}
}
int GetVarForModbus( StrModbusMasterReq * ModbusReq, int ModbusNum )
{
	int FirstEle = ModbusReq->FirstModbusElement-ModbusEleOffset;
	int VarNum;

	if ( FirstEle<0 )
		FirstEle = 0;
	VarNum = ModbusNum-FirstEle+ModbusReq->OffsetVarMapped;
	switch( ModbusReq->TypeReq )
	{
		case MODBUS_REQ_COILS_WRITE:
                        switch ( MapCoilWrite )
                           {
                            case B_VAR:
			         return ReadVar( VAR_MEM_BIT, VarNum );
                                 break;
                            case Q_VAR:
			         return ReadVar( VAR_PHYS_OUTPUT, VarNum );
                                 break;
                            case I_VAR:
			         return ReadVar( VAR_PHYS_INPUT, VarNum );
                                 break;
                            default:  
			         break;
                           }
			
		case MODBUS_REQ_REGISTERS_WRITE:
                        switch ( MapRegisterWrite )
                            {
                             case W_VAR:            
			          return ReadVar( VAR_MEM_WORD, VarNum );
                                  break;
                             case QW_VAR:
			          return ReadVar( VAR_PHYS_WORD_OUTPUT, VarNum );
                                  break;
                             case IW_VAR:
			          return ReadVar( VAR_PHYS_WORD_INPUT, VarNum );
                                  break;
                             default:  
			          break;
                            }
                default:
                        printf("ERROR CLASSICLADDER--- Variable not defined for MODBUS WRITE REQUEST mapping ");
                        
                      break;
			
	}
	return 0;
}


