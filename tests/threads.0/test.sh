#!/bin/bash
# Capture 3500 samples of the fast-thread counter (reset by the slow thread) and
# hand the file to checkresult.  See ../filestream-driver.sh for the mechanism.
. "$(dirname "$0")/../filestream-driver.sh"
fs_run threads.hal
