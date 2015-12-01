#!/bin/sh -ex

# this script is run inside a docker container

# build unsigned packages
DEBUILD_OPTS="-eDEB_BUILD_OPTIONS="parallel=${JOBS}" -us -uc -j${JOBS} -b"

PROOT_OPTS="-b /dev/shm -r ${CHROOT_PATH}"
if echo ${TAG} | grep -iq arm; then
    PROOT_OPTS="${PROOT_OPTS} -q qemu-arm-static"
fi

# build debs
export DEBUILD_OPTS
proot ${PROOT_OPTS} /bin/sh -exc 'cd ${MACHINEKIT_PATH}; \
    ./debian/configure -prx ; \
    debuild ${DEBUILD_OPTS}'
