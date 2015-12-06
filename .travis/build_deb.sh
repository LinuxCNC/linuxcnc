#!/bin/bash -ex

# this script is run inside a docker container
cd ${CHROOT_PATH}${MACHINEKIT_PATH}

# Verbose build
if ${MK_PACKAGE_VERBOSE}; then
    DEBUILD_OPTS+=" -eDH_VERBOSE=1"
fi

# Supplied variables for package configuration
MAJOR_MINOR_VERSION="${MAJOR_MINOR_VERSION:-0.1}"
PKGSOURCE="${PKGSOURCE:-travis.${TRAVIS_REPO_SLUG/\//.}}"
DEBIAN_SUITE="${DEBIAN_SUITE:-experimental}"
REPO_URL="${REPO_URL:-https://github.com/machinekit/machinekit}"

# Compute version
if ${IS_PR}; then
    # Use build timestamp (now) as pkg version patchlevel
    TIMESTAMP="$(date +%s)"
    PR_OR_BRANCH="pr${TRAVIS_PULL_REQUEST}"
    COMMIT_URL="${REPO_URL}/pull/${TRAVIS_PULL_REQUEST}"
else
    # Use merge commit timestamp as pkg version patchlevel
    TIMESTAMP="$COMMIT_TIMESTAMP"
    PR_OR_BRANCH="${TRAVIS_BRANCH}"
    COMMIT_URL="${REPO_URL}/commit/${TRAVIS_COMMIT:0:8}"
fi

# sanitize upstream version
UPSTREAM=${PKGSOURCE}.${PR_OR_BRANCH}
# remove dash
UPSTREAM=${UPSTREAM//-/}
# remove underscore
UPSTREAM=${UPSTREAM//_/}

VERSION="${MAJOR_MINOR_VERSION}.${TIMESTAMP}"

# Compute release
SHA1SHORT="${TRAVIS_COMMIT:0:8}"
RELEASE="1${UPSTREAM}.git${SHA1SHORT}~1${DISTRO}"

# Generate debian/changelog entry
#
# https://www.debian.org/doc/debian-policy/ch-source.html#s-dpkgchangelog
mv debian/changelog debian/changelog.old
cat > debian/changelog <<EOF
machinekit (${VERSION}-${RELEASE}) ${DEBIAN_SUITE}; urgency=low

  * Travis CI rebuild for ${DISTRO}, ${PR_OR_BRANCH}, commit ${SHA1SHORT}
    - ${COMMIT_URL}

 -- ${COMMITTER_NAME} <${COMMITTER_EMAIL}>  $(date -R)

EOF
cat debian/changelog # debug output
cat debian/changelog.old >> debian/changelog

# build unsigned packages and sources on amd64
DEBUILD_OPTS+=" -eDEB_BUILD_OPTIONS=parallel=${JOBS} -us -uc -j${JOBS}"
if test ${MARCH} = 64; then
    # create upstream tarball only on amd64
    (
	cd ${CHROOT_PATH}${MACHINEKIT_PATH}
	git archive HEAD | bzip2 -z > \
            ../machinekit_${VERSION}.orig.tar.bz2
    )
else
    # the rest will be binaries only
    DEBUILD_OPTS+=" -b"
fi

PROOT_OPTS="-b /dev/shm -r ${CHROOT_PATH}"
if test ${MARCH} = armhf; then
    PROOT_OPTS="${PROOT_OPTS} -q qemu-arm-static"
fi

case "${FLAV}" in
   "posix") FLAV_OPTS="-p"
   ;;
   "rt_preempt") FLAV_OPTS="-r"
   ;;
   "xenomai") FLAV_OPTS="-x"
   ;;
   *) FLAV_OPTS="-prx"
   ;;
esac
export FLAV_OPTS

# build debs
export DEBUILD_OPTS
proot ${PROOT_OPTS} /bin/sh -exc 'cd ${MACHINEKIT_PATH}; \
    ./debian/configure ${FLAV_OPTS} ; \
    debuild ${DEBUILD_OPTS}'

# copy results
mkdir ${CHROOT_PATH}/${MACHINEKIT_PATH}/deploy
cp ${CHROOT_PATH}/${MACHINEKIT_PATH}/../*deb \
    ${CHROOT_PATH}/${MACHINEKIT_PATH}/deploy

# copy source
if test ${MARCH} = 64; then
(
    cd ${CHROOT_PATH}/${MACHINEKIT_PATH}/../
    cp *bz2 *dsc *changes ${CHROOT_PATH}/${MACHINEKIT_PATH}/deploy
)
fi

# delete extra debs as packagecloud fails if files have the same name
if [ "${FLAV}" == "rt_preempt" ] || [ "${FLAV}" == "xenomai" ]; then
    rm ${CHROOT_PATH}/${MACHINEKIT_PATH}/deploy/machinekit_*
    rm ${CHROOT_PATH}/${MACHINEKIT_PATH}/deploy/machinekit-dev*
fi

