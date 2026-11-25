#!/bin/bash
set -e

halcompile --install slowkins.comp >/dev/null
linuxcnc -r test.ini
