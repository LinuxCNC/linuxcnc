#!/bin/bash

set -eu #Needed so CI fails when anything is wrong
set -x

DIST=$1

if false; then
    #To install the Xenomai deb's from linuxcnc base (not available right now)
    .github/scripts/add-linuxcnc-repository.sh "$DIST"
    sudo apt-get --yes install libxenomai1 libxenomai-dev libevl libevl-dev linux-libc-evl-dev
else
    CURLOPTS=( --no-progress-meter --retry-all-errors --retry 5 --retry-delay 2 -fLO )
    XENOMAI3_DLD="https://github.com/hdiethelm/xenomai3-linuxcnc/releases/download"
    XENOMAI4_DLD="https://github.com/hdiethelm/xenomai4-linuxcnc/releases/download"
    #To install the RTAI deb's from NTULINUX git
    TMPDIR=$(mktemp -d)
    (
        cd "$TMPDIR"
        curl "${CURLOPTS[@]}" "${XENOMAI3_DLD}/xenomai-3.3-6/libxenomai1_3.3-6_amd64.deb"
        curl "${CURLOPTS[@]}" "${XENOMAI3_DLD}/xenomai-3.3-6/libxenomai-dev_3.3-6_amd64.deb"
        curl "${CURLOPTS[@]}" "${XENOMAI4_DLD}/libevl-0.59-3/libevl_0.59-3_amd64.deb"
        curl "${CURLOPTS[@]}" "${XENOMAI4_DLD}/libevl-0.59-3/libevl-dev_0.59-3_amd64.deb"
        curl "${CURLOPTS[@]}" "${XENOMAI4_DLD}/kernel-6.12.90-cip24-xenomai4-0.59_6.12.90-4/linux-libc-evl-dev_6.12.90-4_amd64.deb"
    )
    #dependency
    sudo apt-get --quiet update
    sudo apt-get --yes install libbpf1 adduser
    #packages
    sudo dpkg -i \
    "$TMPDIR/libxenomai1_3.3-6_amd64.deb" \
    "$TMPDIR/libxenomai-dev_3.3-6_amd64.deb" \
    "$TMPDIR/libevl_0.59-3_amd64.deb" \
    "$TMPDIR/libevl-dev_0.59-3_amd64.deb" \
    "$TMPDIR/linux-libc-evl-dev_6.12.90-4_amd64.deb"
    rm -rf "$TMPDIR"
fi
