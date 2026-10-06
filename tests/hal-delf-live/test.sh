#!/bin/bash
set -e
halcompile --install delfvictim.comp
timeout -k 5 120 halrun -f delftest.hal
