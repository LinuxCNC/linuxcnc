#!/bin/sh -e
sudo apt-get update -qq
sudo apt-get install -y devscripts equivs build-essential --no-install-recommends
sudo apt-get remove -f libreadline6-dev || true
sudo apt-get remove -f libreadline-dev || true
debian/configure uspace
mk-build-deps -i -r -s sudo -t 'apt-get --no-install-recommends --no-install-suggests'
