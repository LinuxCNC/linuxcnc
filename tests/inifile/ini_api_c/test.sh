#!/bin/sh
FLGS=( -O2 -Wall -Wextra -Werror -I"${HEADERS}" -DULAPI -L"${LIBDIR}" )
gcc "${FLGS[@]}" -o initest  initest.c -llinuxcncini || { echo "Failed compile"; exit 1; }

[ -x ./initest ] || { echo "initest program not present or executable"; exit 1; }

r() { echo "$@" >> result; }
f() { echo "***" "$@" "failed" >> result; }
t() { echo "***" "$@" "did not fail" >> result; }
tst() {	./initest "$@" xtest.ini >> result 2>&1; }

true > result
r "+++ Running test program for 'initest' +++"

( echo "[SECTION]"
  echo "VAR=value to check the rest is missing"
  echo "VARdp=42"
  echo "VARxp=0x2a"
  echo "VARop=0o52"
  echo "VARbp=0b101010"
  echo "VARdm=-42"
  echo "VARxm=-0x2a"
  echo "VARom=-0o52"
  echo "VARbm=-0b101010"
  echo "VARrp=+4.2e1"
  echo "VARrm=-4.2e1"
  echo "VARBadBool=BadBool"
  echo "VARBadNum=BadInt42"
  echo "VARWarnNum=42Warn"
  echo "VARBt1=TRUE"
  echo "VARBt2=true"
  echo "VARBt3=True"
  echo "VARBt4=yes"
  echo "VARBt5=YeS"
  echo "VARBt6=1"
  echo "VARBt7=oN"
  echo "VARBf1=FALSE"
  echo "VARBf2=false"
  echo "VARBf3=FalSe"
  echo "VARBf4=no"
  echo "VARBf5=nO"
  echo "VARBf6=0"
  echo "VARBf7=oFf" ) > xtest.ini
r "--- test integer/unsigned value"
for i in d x o b; do
	tst -v"VAR${i}p" -ti || f "Integer VAR${i}p variable"
	tst -v"VAR${i}m" -ti || f "Integer VAR${i}m variable"
	tst -v"VAR${i}p" -tu || f "Unsigned VAR${i}p variable"
	tst -v"VAR${i}m" -tu || f "Unsigned VAR${i}m variable"
	tst -v"VAR${i}p" -tn || f "Integer VAR${i}p variable"
	tst -v"VAR${i}m" -tn || f "Integer VAR${i}m variable"
done
r "--- test real value"
tst -vVARrp -tr || f "Real variable"
tst -vVARrm -tr || f "Real variable"
r "--- test boolean value"
for i in 1 2 3 4 5 6 7; do
	tst -v"VARBt${i}" -tb || f "Boolean variable VARBt${i}"
	tst -v"VARBf${i}" -tb || f "Boolean variable VARBf${i}"
done

r "--- test bad integer/unsigned value"
tst -v"VARBadNum" -ti && t "Bad integer VARBadNum"
tst -v"VARBadNum" -tu && t "Bad unsigned VARBadNum"
tst -v"VARBadNum" -tr && t "Bad real VARBadNum"
r "--- test warn integer/unsigned trailing context"
tst -v"VARWarnNum" -ti || f "Bad integer VARWarnNum"
tst -v"VARWarnNum" -tu || f "Bad unsigned VARWarnNum"
tst -v"VARWarnNum" -tr || f "Bad real VARWarnNum"
r "--- test bad boolean value"
tst -v"VARBadBool" -tb && t "Bad boolean VARBadBool"

r "--- test string value"
tst -v"VARBadNum" -ts || f "String VARBadNum"
tst -v"VARWarnNum" -ts || f "String VARWarnNum"

r "--- test missing value"
tst -v"MissingVAR" -ts && t "MissingVAR string"
tst -v"MissingVAR" -ti && t "MissingVAR sint"
tst -v"MissingVAR" -tu && t "MissingVAR uint"
tst -v"MissingVAR" -tb && t "MissingVAR bool"
tst -v"MissingVAR" -tn && t "MissingVAR int"

r "--- test missing section"
tst -v"VAR" -s"SECTION" -ts || f "Section string"
tst -v"VAR" -s"MISSING" -ts && t "Missing section string"
tst -v"VAR" -s"MISSING" -ti && t "Missing section sint"
tst -v"VAR" -s"MISSING" -tu && t "Missing section uint"
tst -v"VAR" -s"MISSING" -tb && t "Missing section bool"
tst -v"VAR" -s"MISSING" -tn && t "Missing section int"

r "+++ all done +++"

rm -f initest xtest.ini
exit 0
