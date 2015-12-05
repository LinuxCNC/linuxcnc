#!/bin/sh -ex

# this script is run inside a docker container

PROOT_OPTS="-b /dev/shm -r ${CHROOT_PATH}"
if test ${MARCH} = armhf; then
    PROOT_OPTS="${PROOT_OPTS} -q qemu-arm-static"
fi

# rip build
proot ${PROOT_OPTS} ${TRAVIS_PATH}/build_rip_helper.sh

# tar the chroot directory
tar czf /tmp/rootfs.tgz -C ${CHROOT_PATH} .
cp /tmp/rootfs.tgz ${CHROOT_PATH}${TRAVIS_PATH}/mk_runtests
