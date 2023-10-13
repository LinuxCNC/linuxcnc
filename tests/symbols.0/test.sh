#!/bin/sh
set -xe
${SUDO} halcompile --install test_define.comp
${SUDO} halcompile --install test_use.comp
! halrun dotest.hal
