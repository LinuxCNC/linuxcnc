#!/bin/bash -ex

# this script is run inside a docker container
cd ${ROOTFS}${MACHINEKIT_PATH}

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
rm -f debian/changelog
cat > debian/changelog <<EOF
machinekit (${VERSION}-${RELEASE}) ${DEBIAN_SUITE}; urgency=low

  * Travis CI rebuild for ${DISTRO}, ${PR_OR_BRANCH}, commit ${SHA1SHORT}
    - ${COMMIT_URL}

 -- ${COMMITTER_NAME} <${COMMITTER_EMAIL}>  $(date -R)

EOF
cat debian/changelog # debug output
cat debian/changelog.in >> debian/changelog

# Whilst using arceye/mk-builder docker image, need -d switch
# because new czmq4 libs were parachuted in and not installed onto the chroot fs

# build unsigned packages and sources on amd64
DEBUILD_OPTS+=" -eDEB_BUILD_OPTIONS=parallel=${JOBS} -us -uc -d -j${JOBS}"
if test ${MARCH} = 64; then
    # create upstream tarball only on amd64
    (
	cd ${ROOTFS}${MACHINEKIT_PATH}
	git archive HEAD | bzip2 -z > \
            ../machinekit_${VERSION}.orig.tar.bz2
    )
else
    # the rest will be binaries only
    DEBUILD_OPTS+=" -d -b"
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
proot-helper /bin/sh -exc 'cd ${MACHINEKIT_PATH}; \
    ./debian/configure ${FLAV_OPTS} ; \
    debuild ${DEBUILD_OPTS}'

# copy results
mkdir ${ROOTFS}/${MACHINEKIT_PATH}/deploy
chmod 0777 ${ROOTFS}/${MACHINEKIT_PATH}/deploy
cd ${ROOTFS}/${MACHINEKIT_PATH}/../
cp *deb *changes ${ROOTFS}/${MACHINEKIT_PATH}/deploy

# copy source
if test ${MARCH} = 64; then
    cp *bz2 *dsc ${ROOTFS}/${MACHINEKIT_PATH}/deploy
fi

chmod 0666 ${ROOTFS}/${MACHINEKIT_PATH}/deploy/*
