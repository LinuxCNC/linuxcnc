#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

sudo apt-get --quiet update
sudo apt-get install -y devscripts equivs build-essential lintian clang --no-install-recommends
debian/configure
sudo apt-get -y build-dep .
sudo apt install -y lintian
