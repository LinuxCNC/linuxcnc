#!/bin/sh -e
sudo apt-get update -qq
sudo apt-get install -y devscripts equivs build-essential --no-install-recommends
debian/configure uspace
mk-build-deps
sudo dpkg -i linuxcnc-*.deb || true
sudo apt-get -f install -y --no-install-recommends
