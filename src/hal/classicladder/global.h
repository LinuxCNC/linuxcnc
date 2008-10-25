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

extern char CurrentProjectFileName[400];
extern char TmpDirectory[ 400 ];

extern StrGeneralParams GeneralParamsMirror;

#ifdef MODBUS_IO_MASTER
extern StrModbusMasterReq ModbusMasterReq[ NBR_MODBUS_MASTER_REQ ];
// if '\0' => IP mode used for I/O modbus modules
extern char ModbusSerialPortNameUsed[ 30 ];
extern int ModbusSerialSpeed;
extern int ModbusSerialUseRtsToSend;
extern int ModbusTimeInterFrame;
extern int ModbusTimeOutReceipt;
extern int ModbusTimeAfterTransmit;
extern int ModbusEleOffset;
extern int ModbusDebugLevel;
#endif

extern char * ErrorMessageVarParser;

