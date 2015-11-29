#!/bin/bash -ex
cd "$(dirname $0)/.."

CHROOT_PATH="/opt/rootfs"
MACHINEKIT_PATH="/usr/src/machinekit"
TRAVIS_PATH="$MACHINEKIT_PATH/.travis"
CONTAINER="kinsamanka/mkdocker"

cmd=${CMD}
if [ ${CMD} == "run_tests" ];
then
    cmd=build_rip
fi

# run build step
docker run \
    -v $(pwd):${CHROOT_PATH}${MACHINEKIT_PATH} \
    -e FLAV="${FLAV}" \
    -e JOBS=${JOBS} \
    -e TAG=${TAG} \
    -e CHROOT_PATH=${CHROOT_PATH} \
    -e MACHINEKIT_PATH=${MACHINEKIT_PATH} \
    -e TRAVIS_PATH=${TRAVIS_PATH} \
    ${CONTAINER}:${TAG} \
    ${CHROOT_PATH}${TRAVIS_PATH}/${cmd}.sh

# tests are run under a new container instead of chrooting
# this will allow us to run docker without using privileged mode
if [ ${CMD} == "run_tests" ];
then
    # create container using RIP rootfs
    docker build -t mk_runtest .travis/mk_runtests
    
    # run regressions
    docker run \
        -e MACHINEKIT_PATH=${MACHINEKIT_PATH} \
        --rm=true mk_runtest /run_tests.sh
fi
