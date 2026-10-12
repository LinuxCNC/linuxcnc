#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

# Build and test the architecture-specific Debian packages inside a plain
# Debian container started with "docker run", for architectures where the
# GitHub "container:" job key cannot be used (the runner injects its own
# 64-bit node into the container to run actions).
# Same steps as the package-arch job, run from the repository root.

echo "Kernel: $(uname -m), userland: $(dpkg --print-architecture), CPUs: $(nproc)"

apt-get -q update
apt-get --yes upgrade
# Install stuff needed to turn the repo into a debian source package and run tests.
apt-get --yes --no-install-recommends install curl git lsb-release python3 devscripts sudo adduser xauth

git config --global --add safe.directory "${PWD}"

DEBEMAIL=emc-developers@lists.sourceforge.net \
DEBFULLNAME="LinuxCNC Github CI Robot" \
    .github/scripts/build-package-arch.sh

.github/scripts/verify-clean-repo.sh ':(exclude)VERSION' ':(exclude)debian/changelog'

apt-get --yes --no-install-recommends install ../*.deb

adduser --disabled-password --gecos "" testrunner
passwd -d testrunner
adduser testrunner sudo
find tests/ -type d -exec chmod 0777 {} + # make test dirs world-writable for the testrunner
su -c "./scripts/runtests -p -d ./tests" testrunner

.github/scripts/verify-clean-repo.sh ':(exclude)VERSION' ':(exclude)debian/changelog'
