#!/bin/bash -e
# a crash in the test script prints where it was
export PYTHONFAULTHANDLER=1 PYTHONUNBUFFERED=1
rm -f preview.var preview.var.bak
halrun -f preview.hal
rm -f preview.var preview.var.bak
