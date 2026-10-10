#!/bin/sh
set -xe
halcompile --install test_define.comp
halcompile --install test_use.comp
! halrun dotest.hal
