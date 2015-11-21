#!/bin/bash -ex
cd "$(dirname $0)/.."

CHROOT_PATH="/opt/rootfs"
MACHINEKIT_PATH="/usr/src/machinekit"
TRAVIS_PATH="$MACHINEKIT_PATH/.travis"

docker run \
    --privileged=true \
    -v $(pwd):${CHROOT_PATH}${MACHINEKIT_PATH} \
    -e FLAV="${FLAV}" \
    -e MK_DIR=${TAG}_${CMD} \
    -e JOBS=${JOBS} \
    -e TAG=${TAG} \
    -e ROOT=${CHROOT_PATH} \
    kinsamanka/mkdocker:${TAG} \
    ${CHROOT_PATH}${TRAVIS_PATH}/${CMD}.sh
