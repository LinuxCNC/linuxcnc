#!/bin/sh
set -xe
${SUDO} modcompile --install rtmath.comp
halrun dotest.hal
