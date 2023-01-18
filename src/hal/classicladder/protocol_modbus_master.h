//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef _PROTOCOL_MODBUS_MASTER_H
#define _PROTOCOL_MODBUS_MASTER_H

/* Modbus requests list available for the user */
#define MODBUS_REQ_INPUTS_READ 0
#define MODBUS_REQ_COILS_WRITE 1
#define MODBUS_REQ_INPUT_REGS_READ 2
#define MODBUS_REQ_HOLD_REGS_WRITE 3
#define MODBUS_REQ_COILS_READ 4
#define MODBUS_REQ_HOLD_REGS_READ 5
#define MODBUS_REQ_READ_STATUS 6
#define MODBUS_REQ_DIAGNOSTICS 7

// mapping variable type
#define B_VAR   0
#define Q_VAR   1
#define I_VAR   2
#define W_VAR   0
#define QW_VAR  1
#define IW_VAR  2

#define LGT_SLAVE_ADR 25

#define NBR_MODBUS_MASTER_REQ 20 /*50: problem with GTK config window: adding vertical scroll else required*/

typedef struct StrModbusMasterReq
{
	/* IP address or IP:port or slave number for serial */
	/* if '\0' => req not defined */
	char SlaveAdr[ LGT_SLAVE_ADR ];
	char TypeReq; /* see MODBUS_REQ_ list */
	int FirstModbusElement;
	int NbrModbusElements;
	char LogicInverted;
	int OffsetVarMapped;
}StrModbusMasterReq;

typedef struct StrModbusConfig
{
	// if '\0' => IP mode used for I/O Modbus modules
	char ModbusSerialPortNameUsed[ 30 ];
	int ModbusSerialSpeed;
	int ModbusSerialDataBits;   // Number of data bits (5, 6, 7, 8)
	int ModbusSerialParity;     // Parity (00 = None, 01 = Odd, 02 = Even, 03 = Mark, 04 = Space)
	int ModbusSerialStopBits;   // Number of stop bits (1 or 2)
	int ModbusSerialUseRtsToSend;
	int ModbusTimeInterFrame;
	int ModbusTimeOutReceipt;
	int ModbusTimeAfterTransmit;
	// classic Modbus offset (0 in frames = 1 in doc, or 0 everywhere: often too much simple to be used...)
	int ModbusEleOffset; 
	int ModbusDebugLevel;
	// types of vars to map for each Modbus request
	int MapTypeForReadInputs;
	int MapTypeForReadCoils;
	int MapTypeForWriteCoils;
	int MapTypeForReadInputRegs;
	int MapTypeForReadHoldRegs;
	int MapTypeForWriteHoldRegs;
}StrModbusConfig;

void InitModbusMasterBeforeReadConf( void );
void PrepareModbusMaster( void );
void InitModbusMasterParams( void );
int GetModbusResponseLenghtToReceive( void );
int ModbusMasterAsk( unsigned char * SlaveAddressIP, unsigned char * Question );
char TreatModbusMasterResponse( unsigned char * Response, int LgtResponse );

void SetVarFromModbus( StrModbusMasterReq * ModbusReq, int ModbusNum, int Value );
int GetVarForModbus( StrModbusMasterReq * ModbusReq, int ModbusNum );
#endif

