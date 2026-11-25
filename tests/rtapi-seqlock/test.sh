#!/bin/bash
set -e
halcompile --compile seqlock_check.comp >/dev/null
halrun -f check.hal
