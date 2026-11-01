#!/bin/bash
set -e

halcompile --install hexfwdcheck.c >/dev/null
halrun -f hexfwdcheck.hal
