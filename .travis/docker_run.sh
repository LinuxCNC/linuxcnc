#!/bin/bash -ex
cd "$(dirname $0)/.."

CHROOT_PATH="/opt/rootfs"
MACHINEKIT_PATH="/usr/src/machinekit"
TRAVIS_PATH="$MACHINEKIT_PATH/.travis"
CONTAINER="kinsamanka/mkdocker"
# Verbose RIP build output:  "true" or "false"
MK_BUILD_VERBOSE=${MK_BUILD_VERBOSE:-"false"}
# Verbose package build output:  "true" or "false"
MK_PACKAGE_VERBOSE=${MK_PACKAGE_VERBOSE:-"false"}
# Verbose regression test debug output:  "true" or "false"
MK_DEBUG_TESTS=${MK_DEBUG_TESTS:-"false"}

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
    -e MK_BUILD_VERBOSE \
    -e MK_PACKAGE_VERBOSE \
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
        -e MK_DEBUG_TESTS=${MK_DEBUG_TESTS} \
        --rm=true mk_runtest /run_tests.sh
fi
