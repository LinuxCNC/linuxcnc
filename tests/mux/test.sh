#!/bin/bash
# 18 type-converting mux_generic instances, driven via the filestream cmod (was
# WS streamer+sampler). filestream advances one input line per thread cycle — the
# classic pacing the live WS feed could not reproduce.
. "$(dirname "$0")/../filestream-driver.sh"
fs_run mux.hal
