#!/bin/sh -ex

# this script is run inside a docker container

# rip build
proot-helper ${TRAVIS_PATH}/build_rip_helper.sh

# tar the chroot directory
tar czf /tmp/rootfs.tgz -C ${ROOTFS} .
cp /tmp/rootfs.tgz ${ROOTFS}${TRAVIS_PATH}/mk_runtests
