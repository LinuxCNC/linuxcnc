#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

# Minimal dependency set for a --disable-gui build: no X11, Tk, GTK or
# documentation tooling.  If a future change makes the headless build
# silently depend on a GUI library again, this job fails at configure
# or build time instead of papering over it with full build-deps.

.github/scripts/use-main-ubuntu-mirror.sh

sudo apt-get --quiet update
sudo apt-get install --yes --no-install-recommends \
    build-essential \
    autoconf \
    automake \
    pkg-config \
    python3 \
    python3-dev \
    yapps2 \
    intltool \
    gettext \
    libreadline-dev \
    libtirpc-dev \
    libudev-dev \
    libglib2.0-dev \
    libmodbus-dev \
    libusb-1.0-0-dev \
    libboost-python-dev \
    libfmt-dev \
    python3-pybind11 \
    python3-numpy \
    libcap-dev \
    libedit-dev \
    procps \
    psmisc \
    sysvinit-utils \
    util-linux \
    kmod \
    netcat-openbsd
