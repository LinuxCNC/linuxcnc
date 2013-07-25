#!/bin/sh
set -xe
comp --install test_define.comp
comp --install test_use.comp
! MSGD_OPTS="--stderr" halrun dotest.hal
