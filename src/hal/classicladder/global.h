/* Classic Ladder Project */
/* Copyright (C) 2001-2009 Marc Le Douarain */
/* http://membres.lycos.fr/mavati/classicladder/ */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */

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

#ifdef MAT_CONNECTION
#include "../../lib/plc.h"
#define TYPE_FOR_BOOL_VAR plc_pt_t
#else
#define TYPE_FOR_BOOL_VAR char
#endif

#ifdef MODBUS_IO_MASTER
#include "protocol_modbus_master.h"
#endif

extern StrRung * RungArray;
extern TYPE_FOR_BOOL_VAR * VarArray;
//extern unsigned char * LogVarArray;
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
extern GtkWidget *MainSectionWindow;
//Cairo extern GdkPixmap *pixmap;
extern GtkWidget *drawing_area;
extern GtkWidget *EditWindow;
extern int PrintRightMarginPosiX;
extern int PrintRightMarginWidth;
#endif

extern char TmpDirectory[ 400 ];

extern StrGeneralParams GeneralParamsMirror;

#ifdef MODBUS_IO_MASTER
extern StrModbusMasterReq ModbusMasterReq[ NBR_MODBUS_MASTER_REQ ];
extern StrModbusConfig ModbusConfig;
#endif

extern char * ErrorMessageVarParser;

//XXX log functionality not implemented.
//extern StrLog Log;
//extern StrConfigEventLog ConfigEventLog[ NBR_CONFIG_EVENTS_LOG ]; 
//extern unsigned char ListCurrentDefType[ NBR_CURRENT_DEFS_MAX ];
//extern int ListCurrentDefParam[ NBR_CURRENT_DEFS_MAX ];


