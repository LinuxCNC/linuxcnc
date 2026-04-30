#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

echo "deb [arch=amd64,arm64 signed-by=/etc/apt/keyrings/linuxcnc.gpg] https://www.linuxcnc.org/ trixie base" | sudo tee /etc/apt/sources.list.d/linuxcnc.list > /dev/null
GPGTMP=$(mktemp -d /tmp/.gnupgXXXXXX)
gpg --homedir $GPGTMP --keyserver hkp://keyserver.ubuntu.com --recv-key e43b5a8e78cc2927
gpg --homedir $GPGTMP --export 'LinuxCNC Archive Signing Key' | sudo tee /etc/apt/keyrings/linuxcnc.gpg > /dev/null
sudo apt-get --quiet update
