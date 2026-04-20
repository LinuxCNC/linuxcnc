#!/bin/bash

# We have many special echo statements with carefully planned good and bad
# escapes that are flagged. This directive disables the message and applies to
# the entire script.
# shellcheck disable=2028
true

INIVALUE=inivalue
[ -x ./inivalue ] && INIVALUE=./inivalue

command -v "$INIVALUE" > /dev/null 2>&1 || { r "*** Missing inivalue executable"; exit 1; }

r() { echo "$@" >> result; }
f() { echo "***" "$@" "failed" >> result; }
t() { echo "***" "$@" "did not fail" >> result; }
tst() { "$INIVALUE" "$@" xtest.ini >> result 2>&1; }

true > result
r "+++ Running test program for 'inivalue' +++"

r "--- test ini-file does not exist"
"$INIVALUE" --var=dontcare this_file_does_not_exist_and_will_never_be_here.ini >> result 2>&1 && t "Missing ini-file"

r "--- test missing include filename (without blank)"
echo "#INCLUDE" > xtest.ini
tst --var=dontcare && t "Missing include"

r "--- test missing include filename (with blank)"
echo "#INCLUDE  " > xtest.ini
tst --var=dontcare && t "Missing include"

r "--- test include file not found"
echo "#INCLUDE this_file_does_not_exist.inc" > xtest.ini
tst --var=dontcare && t "Nonexistent include file"

r "--- test include file recursion"
echo "#INCLUDE xtest.inc" > xtest.ini
echo "#INCLUDE xtest.ini" > xtest.inc
tst --var=dontcare && t "Include recursion"
# we /could/ test max include depth, but that is a pain...

r "--- test invalid section, missing ']'"
echo "[SECTION" > xtest.ini
tst --var=VAR && t "Invalid section"

r "--- test invalid section, no content"
echo "[]" > xtest.ini
tst --var=VAR && t "Invalid section"
echo "[ ]" > xtest.ini
tst --var=VAR && t "Invalid section"

r "--- test invalid section, invalid identifier"
echo "[0SECTION]" > xtest.ini
tst --var=VAR && t "Invalid section"
echo "[xæøåz]" > xtest.ini
tst --var=VAR && t "Invalid section"

r "--- test duplicate section merge warning"
( echo "[SECTION]"
  echo "[SECTION]"
  echo "VAR=val" ) > xtest.ini
tst --var=VAR || f "Merging duplicate section"

r "--- test section trailing junk warning"
( echo "[SECTION] This is junk"
  echo "VAR=val" ) > xtest.ini
tst --var=VAR || f "Trailing section junk"

r "--- test invalid variable name missing '='"
( echo "[SECTION]"
  echo "VAR" ) > xtest.ini
tst --var=VAR && t "Invalid variable missing '='"

r "--- test invalid variable name missing identifier"
( echo "[SECTION]"
  echo " = value" ) > xtest.ini
tst --var=VAR && t "Invalid variable missing identifier"

r "--- test invalid variable name missing '='"
( echo "[SECTION]"
  echo "VAR x" ) > xtest.ini
tst --var=VAR && t "Invalid variable missing '='"

r "--- test invalid variable name identifier"
( echo "[SECTION]"
  echo "0VAR=val" ) > xtest.ini
tst --var=0VAR && t "Invalid variable name"
( echo "[SECTION]"
  echo "VÅR=val" ) > xtest.ini
tst --var=VAR && t "Invalid variable name"
( echo "[SECTION]"
  echo "VAR x=val" ) > xtest.ini
tst --var=VAR && t "Invalid variable name"


r "--- test invalid variable outside section"
echo "VAR=val" > xtest.ini
tst --var=VAR && t "Invalid variable outside section"

r "--- test continuation"
( echo "[SECTION]"
  echo "VAR=val \\"
  echo "  more val" ) > xtest.ini
tst --var=VAR || f "Variable/line continuation"

r "--- test last line has continuation without newline"
( echo "[SECTION]"
  echo -n "VAR=val \\" ) > xtest.ini
tst --var=VAR && t "Variable/line continuation"

r "--- test last line has continuation with newline"
( echo "[SECTION]"
  echo "VAR=val \\" ) > xtest.ini
tst --var=VAR && t "Variable/line continuation"

r "--- test getting all sections"
( echo "[S1]"
  echo "VAR=s1"
  echo "[S2]"
  echo "VAR=s2a"
  echo "VAR=s2b"
  echo "VAR=s2c"
  echo "[S3]"
  echo "VAR=s3" ) > xtest.ini
tst --sections || f "Getting sections"

r "--- test get all variable of a name"
tst --var=VAR --all || f "Getting [*]VAR[1,-]"
r "--- test get all variable of a name from the 3'rd"
tst --var=VAR --all --num=3 || f "Getting [*]VAR[3,-]"
r "--- test get all variable of a name from unexisting number"
tst --var=VAR --all --num=42 && t "Getting [*]VAR[42,-]"

r "--- test get the 2'nd variable of global name VAR"
tst --var=VAR --num=2 || f "Getting [*]VAR[2]"
r "--- test get the 2'nd variable of name VAR in section"
tst --var=VAR --num=2 --sec=S2 || f "Getting [S2]VAR[2]"
r "--- test get the unexisting number variable of name VAR in section"
tst --var=VAR --num=42 --sec=S2 && t "Getting [S2]VAR[42]"

( echo "[SECTION]"
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
	tst --var="VAR${i}p" --type=i || f "Integer VAR${i}p variable"
	tst --var="VAR${i}m" --type=i || f "Integer VAR${i}m variable"
	tst --var="VAR${i}p" --type=u || f "Unsigned VAR${i}p variable"
	tst --var="VAR${i}m" --type=u || f "Unsigned VAR${i}m variable"
done
r "--- test real value"
tst --var=VARrp --type=r || f "Real variable"
tst --var=VARrm --type=r || f "Real variable"
r "--- test boolean value"
for i in 1 2 3 4 5 6 7; do
	tst --var="VARBt${i}" --type=b || f "Boolean variable VARBt${i}"
	tst --var="VARBf${i}" --type=b || f "Boolean variable VARBf${i}"
	tst --var="VARBt${i}" --type=b -b || f "Boolean variable VARBt${i}"
	tst --var="VARBf${i}" --type=b -b || f "Boolean variable VARBf${i}"
	tst --var="VARBt${i}" --type=b -B || f "Boolean variable VARBt${i}"
	tst --var="VARBf${i}" --type=b -B || f "Boolean variable VARBf${i}"
done

r "--- test bad integer/unsigned value"
tst --var="VARBadNum" --type=i && t "Bad integer VARBadNum"
tst --var="VARBadNum" --type=u && t "Bad unsigned VARBadNum"
tst --var="VARBadNum" --type=r && t "Bad real VARBadNum"
r "--- test warn integer/unsigned trailing context"
tst --var="VARWarnNum" --type=i || f "Bad integer VARWarnNum"
tst --var="VARWarnNum" --type=u || f "Bad unsigned VARWarnNum"
tst --var="VARWarnNum" --type=r || f "Bad real VARWarnNum"
r "--- test bad boolean value"
tst --var="VARBadBool" --type=b && t "Bad boolean VARBadBool"

r "--- test bad range"
tst --var=VARdm --type=i -m 0 -M -1 && t "Integer range"
tst --var=VARdm --type=u -m 1 -M  0 && t "Integer range"
tst --var=VARdm --type=r -m 0 -M -1 && t "Integer range"

r "--- test range tested value"
tst --var=VARdm --type=i -m -1 -M 0 && t "Integer range"
tst --var=VARdm --type=i -m -50 -M -40 || f "Integer range"
tst --var=VARdm --type=u -m 0 -M -0 && t "Unsigned range"
tst --var=VARdp --type=u -m 42 -M 42 || f "Unsigned range"
tst --var=VARdm --type=u -m -50 -M -40 || f "Unsigned range"
tst --var=VARrp --type=r -m 8.0 -M 9.999 && t "Real range"
tst --var=VARrp --type=r -m 3.14 -M 45.0 || f "Real range"
tst --var=VARrm --type=r -m -1.21 -M 0.0 && t "Real range"
tst --var=VARrm --type=r -m -50.1 -M -40 || f "Real range"

r "--- test tilde expansion without HOME"
XHOME="$HOME"
export -n HOME
( echo "[SECTION]"
  echo "VAR=~/xyz" ) > xtest.ini
tst --var=VAR --sec=SECTION --tildeexpand && t "Missing HOME"

r "--- test tilde expansion with HOME"
HOME="/we/control/home"
export HOME
tst --var=VAR --sec=SECTION --tildeexpand || f "Using HOME"
HOME="$XHOME"
export HOME
unset XHOME

r "--- reproduce ini-file content"
( echo "[QUINE]"
  echo "VAR=42"
  echo "VAR=0x2a"
  echo "VAR=0o52"
  echo "VAR=0b101010"
  echo "[QUOTED]"
  echo "S='\"single surrounding \\\\ double\"'"
  echo "D=\"'double surrounding \\\\ single'\""
  echo "E=embedded \"double\" and 'single'"
  echo "C=\" spaces and \\x1f newline\\n\""
  echo "U=\\x20 start spaces"
  echo "V=end spaces \\x20"
  echo "W=\\x20 all spaces \\x20"
  echo "[NOTQUINE]"
  echo "This=-42"
  echo "iS=-0x2a"
  echo "a=-0o52"
  echo "sTrAnGe=-0b101010"
  echo "Variable=Variable" ) > xtest.ini
true > quine.ini
for s in $("$INIVALUE" --sections xtest.ini); do
	echo "[$s]" >> quine.ini
	"$INIVALUE" --variables --content --sec "$s" xtest.ini >> quine.ini
done
if diff -u xtest.ini quine.ini >> result 2>&1; then
	r "ini-file reproduction success"
else
	f "quine diff"
fi

r "--- comment removal processing"
( echo " ; comment ignored"
  echo " # comment ignored"
  echo "[MUSTFAILHERE" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Must fail @line 3"
( echo " [SECTION] ; comment ignored"
  echo " VARa=blabla # comment not ignored"
  echo " VARb=blabla ; comment not ignored"
) > xtest.ini
tst --var=VARa --sec=SECTION || f "Comment VARa removal"
tst --var=VARb --sec=SECTION || f "Comment VARb removal"

r "--- string processing single/double quote"
( echo "[SECTION]"
  echo "VARs='foo'"
  echo "VARq=\"bar\"" ) > xtest.ini
tst --var=VARs --sec=SECTION || f "Single quote string"
tst --var=VARq --sec=SECTION || f "Double quote string"

r "--- string processing single/double in normal text"
( echo "[SECTION]"
  echo "VARs=foo 'quote' bar"
  echo "VARq=bar \"quote\" foo" ) > xtest.ini
tst --var=VARs --sec=SECTION || f "Embedded single quote string"
tst --var=VARq --sec=SECTION || f "Embedded double quote string"

r "--- string continuations escaped newlines"
( echo "[SECTION]"
  echo "VAR=\"some\n \\"
  echo "    string\n \\"
  echo "    separated on\n \\"
  echo "    lines\"" ) > xtest.ini
tst --var=VAR --sec=SECTION || f "String collection and continuation"

r "--- embedded nul in values"
( echo "[SECTION]"
  echo -e "VAR=val\0val" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded NUL in value"
( echo "[SECTION]"
  echo -e "VAR=val\0val" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded literal NUL"
( echo "[SECTION]"
  echo "VAR=\\0" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded octal NUL"
( echo "[SECTION]"
  echo "VAR=\\x00" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded hex NUL"
( echo "[SECTION]"
  echo "VAR=\\u0000" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded UTF-16 NUL"
( echo "[SECTION]"
  echo "VAR=\\U00000000" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Embedded UTF-32 NUL"

r "--- escape invalid and improper hex"
( echo "[SECTION]"
  echo "VAR=\\xYZ" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid hex escape"
( echo "[SECTION]"
  echo "VAR=\\xY" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Improper short hex escape"
( echo "[SECTION]"
  echo "VAR=\"\\xY \"" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid short hex escape"
( echo "[SECTION]"
  echo "VAR=\\uVWZY" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid hex UTF-16"
( echo "[SECTION]"
  echo "VAR=\\uVWZ" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Improper short hex UTF-16"
( echo "[SECTION]"
  echo "VAR=\"\\uVWZ \"" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid short hex UTF-16"
( echo "[SECTION]"
  echo "VAR=\\Uvwxyijkl" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid hex UTF-32"
( echo "[SECTION]"
  echo "VAR=\\Uvwxyijk" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Improper short hex UTF-32"
( echo "[SECTION]"
  echo "VAR=\"\\Uvwxyijk \"" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid short hex UTF-32"

r "--- UTF-16 surrogates, sigh"
( echo "[SECTION]"
  echo "VAR=\\ud800" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Missing UTF-16 low surrogate"
( echo "[SECTION]"
  echo "VAR=\\udc00" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-16 high surrogate"
( echo "[SECTION]"
  echo "VAR=\\ud800\\ud800" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-16 low surrogate"
( echo "[SECTION]"
  echo "VAR=\"\\ud800\\udc0 \"" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-16 low surrogate"
( echo "[SECTION]"
  # &#1f600; smiley
  echo "VAR=\\ud83d\\ude00" ) > xtest.ini
tst --var=VAR --sec=SECTION || f "Valid UTF-16"

r "--- invalid code points"
( echo "[SECTION]"
  echo "VAR=\\U00110000" ) > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid code point UTF-32"

r "--- valid code points"
( echo "[SECTION]"
  # AAAA
  echo "VAR=\\101\\x41\\u0041\\U00000041" ) > xtest.ini
tst --var=VAR --sec=SECTION || f "Valid code points"

r "--- invalid UTF-8"
echo -e '[SECTION]\nVAR="\\xc0 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 c0 overlap"
echo -e '[SECTION]\nVAR="\\xc1 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 c1 overlap"
echo -e '[SECTION]\nVAR="\\xc2"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 c2 short seq"
echo -e '[SECTION]\nVAR="\\xe0\\x8f"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 e08f overlap"
echo -e '[SECTION]\nVAR="\\xed\\xa0"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 surrogate"
echo -e '[SECTION]\nVAR="\\xed\\x9f"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 ed9f short seq"
echo -e '[SECTION]\nVAR="\\xf0\\x8f"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f08f overlap"
echo -e '[SECTION]\nVAR="\\xf0\\x90\\x80"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f09080 short seq"
echo -e '[SECTION]\nVAR="\\xf4\\x90"' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f490 code point"
echo -e '[SECTION]\nVAR="\\xf5 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f5 code point"
echo -e '[SECTION]\nVAR="\\xf6 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f6 code point"
echo -e '[SECTION]\nVAR="\\xf7 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f7 code point"
echo -e '[SECTION]\nVAR="\\xf8 "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 f8 code point"
echo -e '[SECTION]\nVAR="\\xff "' > xtest.ini
tst --var=VAR --sec=SECTION && t "Invalid UTF-8 ff code point"
# &#1f600; smiley: 11110 000  10 011111  10 011000  10 000000
echo -e '[SECTION]\nVAR=\\xf0\\x9f\\x98\\x80' > xtest.ini
tst --var=VAR --sec=SECTION || f "UTF-8 Smiley"

r "+++ all done +++"

rm -f xtest.ini xtest.inc quine.ini
# vim: ts=4 sw=4
