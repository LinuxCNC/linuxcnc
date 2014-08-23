#!/usr/bin/sed -f
/^\\providecommand{\\tabularnewline}/d
s/\\tabularnewline/\\\\/g
