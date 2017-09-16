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
#ifdef MAT_CONNECTION
#include "../../lib/plc.h"
#define TYPE_FOR_BOOL_VAR plc_pt_t
#else
#define TYPE_FOR_BOOL_VAR char
#endif

#include "protocol_modbus_master.h"

extern StrRung * RungArray;
extern TYPE_FOR_BOOL_VAR * VarArray;
extern int * VarWordArray;
extern double * VarFloatArray;
#ifdef OLD_TIMERS_MONOS_SUPPORT
extern StrTimer * TimerArray;
extern StrMonostable * MonostableArray;
#endif
extern StrCounter * CounterArray;
extern StrTimerIEC * NewTimerArray;
extern StrArithmExpr * ArithmExpr;
extern StrInfosGene * InfosGene;
extern StrSection * SectionArray;
#ifdef SEQUENTIAL_SUPPORT
extern StrSequential *Sequential;
extern StrSequential EditSeqDatas;
#endif
extern StrSymbol * SymbolArray;

extern StrEditRung EditDatas;
extern StrArithmExpr * EditArithmExpr;

extern StrDatasForBase CorresDatasForBase[3];
extern char * TimersModesStrings[ NBR_TIMERSMODES ];

#ifdef __GTK_H__
extern GdkPixmap *pixmap;
extern GtkWidget *drawing_area;
extern GtkWidget *EditWindow;
#endif

extern char TmpDirectory[ 400 ];

extern StrGeneralParams GeneralParamsMirror;

#ifdef MODBUS_IO_MASTER
extern StrModbusMasterReq ModbusMasterReq[ NBR_MODBUS_MASTER_REQ ];
// if '\0' => IP mode used for I/O modbus modules
extern char ModbusSerialPortNameUsed[ 30 ];
extern int ModbusSerialSpeed;
extern int ModbusSerialDataBits;
extern int ModbusSerialStopBits;
extern int ModbusSerialParity;
extern int ModbusSerialUseRtsToSend;
extern int ModbusTimeInterFrame;
extern int ModbusTimeOutReceipt;
extern int ModbusTimeAfterTransmit;
extern int ModbusEleOffset;
extern int ModbusDebugLevel;
// Variables for Mapping MODBUS
extern int MapCoilRead;
extern int MapCoilWrite;
extern int MapInputs;
extern int MapHolding;
extern int MapRegisterRead;
extern int MapRegisterWrite;
#endif

extern char * ErrorMessageVarParser;

