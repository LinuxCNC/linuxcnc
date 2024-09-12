#!/bin/sh -ex
sudo apt-get update -qq
sudo apt-get install -y devscripts equivs build-essential --no-install-recommends
debian/configure
# This gives an error that is hidden from us
#mk-build-deps -i -r -s sudo -t 'apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --no-install-suggests'
if ! sudo apt-get -y build-dep . ; then
    echo "E: could not install dependencies, maybe inspect d/control for hints:"
    cat debian/control
    echo
    echo "E: apt-get -y build-dep . failed"
    exit 1
fi
sudo apt install -y lintian
