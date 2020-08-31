#!/bin/sh
set -e
${SUDO} halcompile --install test_define1.comp
${SUDO} halcompile --install test_use1.comp
halrun dotest.hal
