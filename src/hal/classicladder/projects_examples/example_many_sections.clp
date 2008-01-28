_FILES_CLASSICLADDER
_FILE-general.txt
PERIODIC_REFRESH=100
SIZE_NBR_RUNGS=100
SIZE_NBR_BITS=500
SIZE_NBR_WORDS=100
SIZE_NBR_TIMERS=10
SIZE_NBR_MONOSTABLES=10
SIZE_NBR_COUNTERS=10
SIZE_NBR_TIMERS_IEC=10
SIZE_NBR_PHYS_INPUTS=50
SIZE_NBR_PHYS_OUTPUTS=50
SIZE_NBR_ARITHM_EXPR=100
SIZE_NBR_SECTIONS=10
SIZE_NBR_SYMBOLS=100
_/FILE-general.txt
_FILE-timers.csv
; Timers :
; Base(see classicladder.h),Preset
0,2
1,5
2,5
1,10
1,10
1,10
1,10
1,10
1,10
1,10
_/FILE-timers.csv
_FILE-monostables.csv
; Monostables :
; Base(see classicladder.h),Preset
1,3
1,10
1,10
1,10
1,10
1,10
1,10
1,10
1,10
1,10
_/FILE-monostables.csv
_FILE-counters.csv
; Counters :
; Preset
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
_FILE-timers_iec.csv
; Timers IEC :
; Base(see classicladder.h),Preset,TimerMode(see classicladder.h)
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
_FILE-arithmetic_expressions.csv
; Arithmetic expressions :
; Compare or Operate ones
@200/0@:=@200/0@+1
@200/2@:=@200/2@+1
@200/1@:=@200/1@+1
@200/1@:=@200/1@+1
@200/0@:=0
@200/1@>255
@200/1@:=0











@200/1@&15<=7

















































































_/FILE-arithmetic_expressions.csv
_FILE-rung_0.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=Set B1/B2 to call SRs
#PREVRUNG=0
#NEXTRUNG=1
0-0-0/1 , 0-0-0/2 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/20
0-0-0/5 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/25
0-0-0/12 , 0-0-0/13 , 0-0-0/14 , 0-0-0/15 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/22
0-0-0/0 , 0-0-0/0 , 0-0-0/6 , 0-0-0/5 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/22
2-0-20/0 , 9-0-0/0 , 99-0-0/0 , 10-0-0/1 , 9-0-0/0 , 99-0-0/0 , 11-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-0/20
0-0-0/0 , 0-0-0/0 , 99-1-0/0 , 99-0-0/0 , 0-0-0/0 , 99-0-0/0 , 99-0-0/0 , 0-0-0/0 , 0-0-0/0 , 54-1-0/5
_/FILE-rung_0.csv
_FILE-rung_1.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=LBL4
#COMMENT=Inc W2 if not (J)umping
#PREVRUNG=0
#NEXTRUNG=5
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/1
0-0-0/25 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/31
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_1.csv
_FILE-rung_5.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=LBL6
#COMMENT=
#PREVRUNG=1
#NEXTRUNG=0
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/3
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/4
99-0-0/0 , 99-0-0/0 , 20-0-0/5 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/6
0-0-0/0 , 0-0-0/0 , 0-0-0/10 , 0-0-0/0 , 0-0-0/10 , 0-0-0/11 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/37
0-0-0/0 , 0-0-0/0 , 0-0-0/7 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/8
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/9
_/FILE-rung_5.csv
_FILE-rung_7.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=B35 unset in Sub-Routine4
#PREVRUNG=-1
#NEXTRUNG=8
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 52-0-0/35
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
1-0-0/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 55-0-0/4
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_7.csv
_FILE-rung_8.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=B36 set if SR4 not called
#PREVRUNG=7
#NEXTRUNG=-1
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/0
2-0-0/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/4
1-0-0/35 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-0/36
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 53-1-0/35
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_8.csv
_FILE-rung_9.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=Call SR6 to speed-up B30
#PREVRUNG=-1
#NEXTRUNG=-1
99-0-0/0 , 99-0-0/0 , 20-0-0/18 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-0/30
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 53-0-0/35
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
1-0-0/2 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 55-0-0/6
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_9.csv
_FILE-rung_10.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=
#PREVRUNG=-1
#NEXTRUNG=-1
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/2
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_10.csv
_FILE-sections.csv
; Sections
#VER=1.0
#NAME000=Prog1
#NAME001=Prog2
#NAME002=SubRoutine4
#NAME003=SubRoutine6
000,0,-1,0,5,0
001,0,-1,7,8,0
002,0,4,9,9,0
003,0,6,10,10,0
_/FILE-sections.csv
_FILE-sequential.csv
; Sequential
#VER=1.0
_/FILE-sequential.csv
_FILE-ioconf.csv
; I/O Configuration
_/FILE-ioconf.csv
_FILE-modbusioconf.csv
; Modbus Distributed I/O Configuration
_/FILE-modbusioconf.csv
_FILE-symbols.csv
; Symbols
#VER=1.0
_/FILE-symbols.csv
_/FILES_CLASSICLADDER
