_FILES_CLASSICLADDER
_FILE-rung_1.csv
#VER=2.0
#LABEL=Rung 1
#COMMENT=Stepper Estop Chain
#PREVRUNG=0
#NEXTRUNG=2
0-0-50/0 , 1-0-50/0 , 9-0-50/0 , 9-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-60/0 , 9-0-0/0 , 9-0-0/0 , 50-0-60/0
0-0-0/0 , 2-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-50/2 , 9-0-0/0 , 9-0-0/0 , 0-1-60/1
0-0-50/0 , 1-0-50/0 , 2-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/1 , 9-0-0/0 , 0-1-60/1
9-0-0/0 , 1-0-50/0 , 2-0-50/1 , 1-0-60/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 0-1-0/0
0-0-60/1 , 0-0-0/0 , 0-0-0/0 , 1-1-50/2 , 0-1-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-60/6
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_1.csv
_FILE-symbols.csv
#VER=1.0
%I0,GUI-E,
%I1,EXT-E,
%I2,E-RESET,
%I3,Lube_on,
%I4,sensor,
%I5,I5,
%I6,I6,
%I7,I7,
%I8,I8,
%I9, ,
%Q0,EMC-E,
%Q1,toggle,
%Q2,Lube_low,
%Q3,Q3,
%Q4,Q4,
%Q5,Q5,
%Q6,Q6,
%Q7,Q7,
%Q8,Q8,
%Q9,Q9,
%T0,Time-on,Time lube pump is on
%T1,Time-off,Time lube pump is off
%T2,Time-warn,Time before low lube warning
%B0,reset,reset off timer
_/FILE-symbols.csv
_FILE-rung_2.csv
#VER=2.0
#LABEL=
#COMMENT=Intermittent Lube
#PREVRUNG=1
#NEXTRUNG=-1
9-0-0/0 , 1-0-50/3 , 2-0-0/0 , 99-0-0/0 , 10-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 53-0-60/1
0-0-0/0 , 0-0-0/0 , 0-1-0/0 , 99-1-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 52-0-60/1
0-0-0/0 , 0-0-0/0 , 2-1-60/1 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 10-0-0/1 , 0-0-60/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-1-0/0 , 0-0-0/0 , 0-0-0/0 , 99-1-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-0/0
0-0-0/0 , 0-0-0/0 , 1-1-50/4 , 99-0-0/0 , 10-0-0/2 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-60/9
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 99-1-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-60/2
_/FILE-rung_2.csv
_FILE-modbusioconf.csv
#VER=1.0
_/FILE-modbusioconf.csv
_FILE-com_params.txt
MODBUS_MASTER_SERIAL_PORT=
MODBUS_MASTER_SERIAL_SPEED=9600
MODBUS_MASTER_SERIAL_DATABITS=8
MODBUS_MASTER_SERIAL_STOPBITS=1
MODBUS_MASTER_SERIAL_PARITY=0
MODBUS_ELEMENT_OFFSET=0
MODBUS_MASTER_SERIAL_USE_RTS_TO_SEND=0
MODBUS_MASTER_TIME_INTER_FRAME=100
MODBUS_MASTER_TIME_OUT_RECEIPT=500
MODBUS_MASTER_TIME_AFTER_TRANSMIT=0
MODBUS_DEBUG_LEVEL=0
MODBUS_MAP_COIL_READ=0
MODBUS_MAP_COIL_WRITE=0
MODBUS_MAP_INPUT=0
MODBUS_MAP_HOLDING=0
MODBUS_MAP_REGISTER_READ=0
MODBUS_MAP_REGISTER_WRITE=0
_/FILE-com_params.txt
_FILE-timers_iec.csv
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
1,0,0
_/FILE-timers_iec.csv
_FILE-timers.csv
1,10
1,20
0,20
2,1
2,5
1,0
1,0
1,0
1,0
1,0
_/FILE-timers.csv
_FILE-counters.csv
0
0
0
0
0
0
0
0
0
0
_/FILE-counters.csv
_FILE-sections.csv
#VER=1.0
#NAME000=Prog1
000,0,-1,1,2,0
_/FILE-sections.csv
_FILE-arithmetic_expressions.csv
#VER=2.0
_/FILE-arithmetic_expressions.csv
_FILE-rung_0.csv
#VER=2.0
#LABEL=
#COMMENT=
#PREVRUNG=0
#NEXTRUNG=1
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_0.csv
_FILE-ioconf.csv
#VER=1.0
_/FILE-ioconf.csv
_FILE-monostables.csv
1,10
1,20
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
_/FILE-monostables.csv
_FILE-sequential.csv
#VER=1.0
_/FILE-sequential.csv
_FILE-general.txt
PERIODIC_REFRESH=100
SIZE_NBR_RUNGS=100
SIZE_NBR_BITS=500
SIZE_NBR_WORDS=100
SIZE_NBR_TIMERS=10
SIZE_NBR_MONOSTABLES=10
SIZE_NBR_COUNTERS=10
SIZE_NBR_TIMERS_IEC=10
SIZE_NBR_PHYS_INPUTS=15
SIZE_NBR_PHYS_OUTPUTS=25
SIZE_NBR_ARITHM_EXPR=100
SIZE_NBR_SECTIONS=10
SIZE_NBR_SYMBOLS=100
_/FILE-general.txt
_/FILES_CLASSICLADDER
