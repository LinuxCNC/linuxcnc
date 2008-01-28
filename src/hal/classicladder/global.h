#ifdef MAT_CONNECTION
#include "../../lib/plc.h"
#define TYPE_FOR_BOOL_VAR plc_pt_t
#else
#define TYPE_FOR_BOOL_VAR char
#endif


extern StrRung * RungArray;
extern TYPE_FOR_BOOL_VAR * VarArray;
extern int * VarWordArray;
extern StrTimer * TimerArray;
extern StrMonostable * MonostableArray;
extern StrCounter * CounterArray;
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

#ifdef __GTK_H__
extern GdkPixmap *pixmap;
extern GtkWidget *drawing_area;
//extern GtkWidget *chkvar[40];
extern GtkWidget *EditWindow;
#endif

extern char LadderDirectory[400];
extern char TmpDirectory[ 400 ];

#ifdef DYNAMIC_PLCSIZE
extern plc_sizeinfo_s sinfo;
//extern plc_sizeinfo_s *plc_sizeinfo;
#endif

#ifdef USE_MODBUS
#include "protocol_modbus_master.h"
extern StrModbusMasterReq ModbusMasterReq[ NBR_MODBUS_MASTER_REQ ];
// if '\0' => IP mode used for I/O modbus modules
extern char ModbusSerialPortNameUsed[ 30 ];
extern int ModbusSerialSpeed;
extern int ModbusSerialUseRtsToSend;
extern int ModbusTimeInterFrame;
extern int ModbusTimeOutReceipt;
extern int ModbusTimeAfterTransmit;
extern int ModbusDebugLevel;
#endif

extern char * ErrorMessageVarParser;

