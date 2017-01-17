#!/bin/sh -e
sudo apt-get update -qq
sudo apt-get install -y devscripts equivs build-essential --no-install-recommends
debian/configure uspace
mk-build-deps -i -r -s sudo -t 'apt-get --no-install-recommends --no-install-suggests'
