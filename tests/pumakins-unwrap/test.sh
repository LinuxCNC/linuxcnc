#!/bin/bash
set -e

halcompile --install unwrapcheck.c >/dev/null
halrun -f unwrapcheck.hal
