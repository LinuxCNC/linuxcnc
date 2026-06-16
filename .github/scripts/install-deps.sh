#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

sudo apt-get --quiet update
sudo apt-get install --yes --no-install-recommends devscripts equivs build-essential lintian clang
debian/configure
sudo apt-get --yes build-dep .
