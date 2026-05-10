#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

DIST=$1

echo "deb [arch=amd64,arm64 signed-by=/etc/apt/keyrings/linuxcnc.gpg] https://www.linuxcnc.org/ $DIST base" | sudo tee /etc/apt/sources.list.d/linuxcnc.list > /dev/null
case $DIST in
	'buster' | 'bullseye' | 'bookworm')
                gpg --homedir "${PWD}/gnupg" --export 3CB9FD148F374FEF | sudo tee /etc/apt/keyrings/linuxcnc.gpg > /dev/null
                ;;
        *)
                GPGTMP=$(mktemp -d /tmp/.gnupgXXXXXX)
                gpg --homedir "$GPGTMP" --keyserver hkp://keyserver.ubuntu.com --recv-key e43b5a8e78cc2927
                gpg --homedir "$GPGTMP" --export 'LinuxCNC Archive Signing Key' | sudo tee /etc/apt/keyrings/linuxcnc.gpg > /dev/null
                ;;
esac
sudo apt-get --quiet update
