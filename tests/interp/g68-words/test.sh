#!/bin/bash
# Each G68.2 form takes only the words it reads; any other is refused
# instead of being ignored.  One case per program, the message or ok.
case_() {
    printf '%s\nM2\n' "$2" > case.ngc
    msg=$(rs274 -g case.ngc 2>&1 | grep -v -e '^ *[0-9]* N\.\.\.\.\.' -e '^executing' | head -1)
    echo "$1: ${msg:-ok}"
}
case_ p2-ijk    $'G68.2 P2 Q1 X0 Y0 Z0 I1\nG68.2 P2 Q2 X1 Y0 Z0\nG68.2 P2 Q3 X0 Y1 Z0'
case_ p2-r-q1   $'G68.2 P2 Q1 X0 Y0 Z0 R10\nG68.2 P2 Q2 X1 Y0 Z0\nG68.2 P2 Q3 X0 Y1 Z0'
case_ p2-r-q3   $'G68.2 P2 Q1 X0 Y0 Z0\nG68.2 P2 Q2 X1 Y0 Z0\nG68.2 P2 Q3 X0 Y1 Z0 R10'
case_ p2-r-twice $'G68.2 P2 Q0 X0 Y0 Z0 R10\nG68.2 P2 Q1 X0 Y0 Z0\nG68.2 P2 Q2 X1 Y0 Z0 R5\nG68.2 P2 Q3 X0 Y1 Z0'
case_ p2-r-q0   $'G68.2 P2 Q0 X0 Y0 Z0 R10\nG68.2 P2 Q1 X0 Y0 Z0\nG68.2 P2 Q2 X1 Y0 Z0\nG68.2 P2 Q3 X0 Y1 Z0'
case_ p2-no-q0   $'G68.2 P2 Q1 X0 Y0 Z0\nG68.2 P2 Q2 X1 Y0 Z0\nG68.2 P2 Q3 X0 Y1 Z0'
case_ p2-q0-late $'G68.2 P2 Q1 X0 Y0 Z0\nG68.2 P2 Q0 X0 Y0 Z0'
case_ p3-q2-xyz $'G68.2 P3 Q1 X0 Y0 Z0 I1 J0 K0\nG68.2 P3 Q2 X1 I0 J0 K1'
case_ p3-q2-r   $'G68.2 P3 Q1 X0 Y0 Z0 I1 J0 K0\nG68.2 P3 Q2 I0 J0 K1 R5'
case_ p3-q1-r   $'G68.2 P3 Q1 X0 Y0 Z0 I1 J0 K0 R5\nG68.2 P3 Q2 I0 J0 K1'
case_ p3-4deg   $'G68.2 P3 Q1 X0 Y0 Z0 I1 J0 K0\nG68.2 P3 Q2 I0.07 J0 K1'
case_ p3-6deg   $'G68.2 P3 Q1 X0 Y0 Z0 I1 J0 K0\nG68.2 P3 Q2 I-0.106 J0 K1'
case_ p3-zero   $'G68.2 P3 Q1 X0 Y0 Z0 I0 J0 K0\nG68.2 P3 Q2 I0 J0 K1'
case_ p1-a      $'G68.2 P1 X1 J30 A10'
case_ g684-b    $'G68.2 X1 J30\nG68.4 B2 I10'
case_ g684-none $'G68.4 I10'
case_ g52-a      $'G68.2 X1 J30\nG52 X1 A2'
case_ g92-plane  $'G68.2 X1 J30\nG92 X1'
case_ g682-g52   $'G68.2 X1 J30\nG52 X1\nG68.2 X2 J30'
case_ g684-g52   $'G68.2 X1 J30\nG52 X1\nG68.4 I10'
case_ g682-g52-0 $'G68.2 X1 J30\nG52 X1\nG52 X0\nG68.2 X2 J30'
rm -f case.ngc
