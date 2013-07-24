#!/bin/sh
set -e
comp --install test_define1.comp
comp --install test_use1.comp
halrun dotest.hal
