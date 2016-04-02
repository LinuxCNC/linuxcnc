#!/bin/sh
import sys

print r"""\
.ds TOC Table of Contents
.de XS
.da toc*div
.ev h
.ie \\n[.$] .XA "\\$1"
.el .XA
..
.de @div-end!toc*div
.XE
..
.de XA
.ie '\\n(.z'toc*div' \{\
.       if d toc*num .toc*end-entry
.       ie \\n[.$] \{\
.               ie '\\$1'no' .ds toc*num
.               el .ds toc*num "\\$1
.       \}
.       el .ds toc*num \\n[%]
.       br
.       par@reset
.       na
.       in (n;0\\$2)
.\}
.el .@error XA without XS
..
.de XE
.ie '\\n(.z'toc*div' \{\
.       if d toc*num .toc*end-entry
.       ev
.       di
.\}
.el .@error XE without XS
..
.de toc*end-entry
\\a\\t\\*[toc*num]
.br
.rm toc*num
..
.de PX
.if !'\\$1'no' \{\
.       ce 1
.       ie (\\n[PS] >= 1000) \
.               ps ((\\n[PS]z / 1000u) + 2z)
.       el \
.               ps \\n[PS]+2
.       ft B
\\*[TOC]
.       ft
.       ps
.\}
.nf
.       ll -8n
.char \[toc*leader-char] .\h'1m'
.lc \[toc*leader-char]
.ta (u;\\n[.l]-\\n[.i]-\w'000') (u;\\n[.l]-\\n[.i])R
.sp 2
.toc*div
.par@reset
.."""

head = None
for fn in sys.argv[1:]:
    with open(fn, "rU") as f:
        for line in f.readlines():
            line = line.rstrip('\n')
            if not head and line.startswith(".SH NAME"):
                head = line
            elif head and line:
                print head
                print line
                print ".XS"
                print line
                print ".XE"
                head = None
            else:
                print line
print '.TH LinuxCNC "TOC" "" "LinuxCNC Documentation" "The Enhanced Machine Controller"'
print ".PX"
