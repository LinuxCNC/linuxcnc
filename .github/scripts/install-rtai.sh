#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

#To install the RTAI deb's from linuxcnc base
#.github/scripts/add-linuxcnc-repository.sh
#sudo apt-get --yes install linux-headers-5.4.279-rtai-amd64 linux-image-5.4.279-rtai-amd64 rtai-modules-5.4.279
#-----
#To install the RTAI deb's from NTULINUX git
curl -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/linux-headers-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb
curl -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/linux-image-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb
curl -fLO https://github.com/NTULINUX/RTAI/releases/download/v5.3.4/rtai-modules-5.4.302_5.3.4-linuxcnc_amd64.deb
sudo dpkg -i linux-headers-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb linux-image-5.4.302-rtai-amd64_5.4.302-rtai-amd64-1_amd64.deb rtai-modules-5.4.302_5.3.4-linuxcnc_amd64.deb
#-----
