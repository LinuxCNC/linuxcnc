#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

DIST=$1

if false; then
    #To install the RTAI deb's from linuxcnc base
    .github/scripts/add-linuxcnc-repository.sh "$DIST"
    sudo apt-get --yes install linux-headers-5.4.279-rtai-amd64 linux-image-5.4.279-rtai-amd64 rtai-modules-5.4.279
else
    #To install the RTAI deb's from NTULINUX git
    TMPDIR=$(mktemp -d)
    (
        cd "$TMPDIR"
        curl --no-progress-meter -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/linux-headers-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb
        curl --no-progress-meter -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/linux-image-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb
        curl --no-progress-meter -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/rtai-modules-5.4.302_5.3.4-linuxcnc_amd64.deb
    )
    sudo dpkg -i \
    "$TMPDIR/linux-headers-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb" \
    "$TMPDIR/linux-image-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb" \
    "$TMPDIR/rtai-modules-5.4.302_5.3.4-linuxcnc_amd64.deb"
    rm -rf "$TMPDIR"
fi