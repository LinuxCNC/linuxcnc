#!/bin/sh
set -xe
halcompile --install rtmath.comp
halrun dotest.hal
