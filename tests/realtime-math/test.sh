#!/bin/sh
set -xe
${SUDO} halcompile --install rtmath.comp
halrun dotest.hal
