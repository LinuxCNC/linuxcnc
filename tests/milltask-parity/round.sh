#!/bin/bash
# round.sh — round every floating-point field on stdin to 4 decimals and write
# to stdout. %.4f is the parity tolerance knob: it lives here alone so moves.sh
# and normalize.sh cannot drift apart on the rounding precision.
set -u
perl -pe 's/(-?\d+\.\d+(?:e[+-]?\d+)?)/sprintf("%.4f",$1)/ge'
