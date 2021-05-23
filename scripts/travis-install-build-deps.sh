#!/bin/sh -ex
sudo sh -c 'echo deb-src http://us.archive.ubuntu.com/ubuntu/ xenial main universe >> /etc/apt/sources.list'
grep . /etc/apt/sources.list /etc/apt/sources.list.d/* || true
sudo apt-get update -qq
sudo apt-get install -y devscripts equivs build-essential --no-install-recommends
sudo apt-get remove -f libreadline6-dev || true
sudo apt-get remove -f libreadline-dev || true
debian/configure uspace noauto
mk-build-deps -i -r -s sudo -t 'apt-get --no-install-recommends --no-install-suggests'
sudo apt install -y lintian
