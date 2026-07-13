#!/bin/bash
# Two multiclick instances driven via the filestream cmod (was WS streamer+sampler).
# filestream advances one input line per thread cycle — the deterministic pacing
# the timing-sensitive click detection needs.
. "$(dirname "$0")/../filestream-driver.sh"
fs_run multiclick.hal
