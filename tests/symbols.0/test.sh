#!/bin/sh
set -xe
comp --install test_define.comp
comp --install test_use.comp
! halrun dotest.hal
