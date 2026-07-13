#!/bin/bash
. "$(dirname "$0")/../../filestream-driver.sh"
: > in.txt
fs_run runaway.hal
