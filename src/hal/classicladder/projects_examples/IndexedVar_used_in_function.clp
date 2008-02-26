_FILES_CLASSICLADDER
_FILE-general.txt
PERIODIC_REFRESH=100
SIZE_NBR_RUNGS=100
SIZE_NBR_BITS=240
SIZE_NBR_WORDS=120
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
_FILE-ioconf.csv
#VER=1.0
_/FILE-ioconf.csv
_FILE-rung_1.csv
#VER=2.0
#LABEL=
#COMMENT=Adding 10 to the indexed var
#PREVRUNG=-1
#NEXTRUNG=-1
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/1
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0
_/FILE-rung_1.csv
_FILE-symbols.csv
#VER=1.0
%W9,Index,
_/FILE-symbols.csv
_FILE-modbusioconf.csv
#VER=1.0
_/FILE-modbusioconf.csv
_FILE-arithmetic_expressions.csv
#VER=2.0
0000,@200/9@:=4
0001,@200/0[200/9]@:=@200/0[200/9]@+10
0002,@200/9@:=6
0003,@200/9@:=7
_/FILE-arithmetic_expressions.csv
_FILE-rung_0.csv
#VER=2.0
#LABEL=
#COMMENT=Index used for %Wxxx param
#PREVRUNG=0
#NEXTRUNG=1
1-0-50/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/0
103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 9-1-0/0 , 9-0-0/0 , 55-0-0/0
2-0-50/1 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/2
103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 103-0-0/0 , 9-1-0/0 , 9-0-0/0 , 55-0-0/0
9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 9-0-0/0 , 99-0-0/0 , 99-0-0/0 , 60-0-0/3
0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 0-0-0/0 , 9-1-0/0 , 9-0-0/0 , 55-0-0/0
_/FILE-rung_0.csv
_FILE-timers.csv
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
_/FILE-timers.csv
_FILE-sequential.csv
#VER=1.0
_/FILE-sequential.csv
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
_FILE-monostables.csv
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
1,0
_/FILE-monostables.csv
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
_FILE-sections.csv
#VER=1.0
#NAME000=Prog1
#NAME001=Function
000,0,-1,0,0,0
001,0,0,1,1,0
_/FILE-sections.csv
_/FILES_CLASSICLADDER
