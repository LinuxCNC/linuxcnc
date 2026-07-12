#!/bin/sh
set -xe
${SUDO} modcompile --install test_define.comp
${SUDO} modcompile --install test_use.comp
! halrun dotest.hal
