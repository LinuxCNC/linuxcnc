#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

debian/configure
git diff
apt-get --yes build-dep --indep-only .
debuild -us -uc --build=source,all
