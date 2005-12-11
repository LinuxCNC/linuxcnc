_FILES_CLASSICLADDER
_FILE-sections.csv
; Sections
#VER=1.0
#NAME000=Prog1
000,0,-1,1,2,0
_/FILE-sections.csv
_FILE-timers.csv
; Timers :
; Base(see classicladder.h),Preset
1,10
1,20
0,20
2,1
2,5
2,10
2,2
1,1
1,0
1,2
_/FILE-timers.csv
_FILE-rung_0.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
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
_FILE-rung_1.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=Rung 1
#COMMENT=Stepper Estop Chain
#PREVRUNG=0
#NEXTRUNG=2
9-0-50/0 , 1-0-50/0 , 9-0-50/0 , 9-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-60/0 , 9-0-0/0 , 9-0-0/0 , 50-0-60/0
0-0-0/0 , 2-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-50/2 , 9-0-0/0 , 9-0-0/0 , 0-1-60/1
0-0-50/0 , 1-0-50/0 , 2-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/1 , 9-0-0/0 , 0-1-60/1
0-0-0/0 , 1-0-50/0 , 2-0-50/1 , 1-0-60/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 0-1-0/0
0-0-60/1 , 0-0-0/0 , 0-0-0/0 , 1-1-50/2 , 0-1-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-60/6
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_1.csv
_FILE-arithmetic_expressions.csv
; Arithmetic expressions :
; Compare or Operate ones




_/FILE-arithmetic_expressions.csv
_FILE-monostables.csv
; Monostables :
; Base(see classicladder.h),Preset
1,10
1,20
0,20
1,0
1,0
1,0
1,0
1,0
1,0
1,0
_/FILE-monostables.csv
_FILE-rung_2.csv
; Rung :
; all the blocks with the following format :
; type (see classicladder.h) - ConnectedWithTop - VarType (see classicladder.h) / VarOffset
#VER=2.0
#LABEL=
#COMMENT=Intermittent Lube
#PREVRUNG=1
#NEXTRUNG=-1
9-0-0/0 , 1-0-50/3 , 2-0-0/0 , 99-0-0/0 , 10-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 53-0-60/1
0-0-0/0 , 0-0-0/0 , 0-1-0/0 , 99-0-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 52-0-60/1
0-0-0/0 , 0-0-0/0 , 2-1-60/1 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 10-0-0/1 , 0-0-60/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-1-0/0 , 0-0-0/0 , 0-0-0/0 , 99-0-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-0/0
0-0-0/0 , 0-0-0/0 , 1-1-50/4 , 99-0-0/0 , 10-0-0/2 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-60/2
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 99-0-0/0 , 99-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 50-0-60/2
_/FILE-rung_2.csv
_/FILES_CLASSICLADDER
