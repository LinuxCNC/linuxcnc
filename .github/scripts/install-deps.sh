#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

.github/scripts/setup-apt-mirrors.sh
.github/scripts/apt-archive-cache.sh restore

.github/scripts/apt-get-retry.sh update --quiet
.github/scripts/apt-get-retry.sh install --yes --no-install-recommends devscripts equivs build-essential lintian clang
debian/configure
.github/scripts/apt-get-retry.sh --yes build-dep .
.github/scripts/apt-archive-cache.sh save
