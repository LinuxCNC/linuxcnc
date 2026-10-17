#!/bin/sh
set -e
halcompile --install test_define1.comp
halcompile --install test_use1.comp
halrun dotest.hal
