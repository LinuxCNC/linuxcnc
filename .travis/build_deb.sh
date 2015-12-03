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
DISTRO="${DISTRO:-${TAG%-*}}"
DEBIAN_SUITE="${DEBIAN_SUITE:-experimental}"
REPO_URL="${REPO_URL:-https://github.com/machinekit/machinekit}"

# Compute version
if test "$TRAVIS_PULL_REQUEST" = "false"; then
    # Use build timestamp (now) as pkg version patchlevel
    TIMESTAMP="$(date +%s)"
    PR_OR_BRANCH="${TRAVIS_BRANCH}"
    COMMIT_URL="${REPO_URL}/commit/${TRAVIS_COMMIT:0:8}"
else
    # Use merge commit timestamp as pkg version patchlevel
    TIMESTAMP="$COMMIT_TIMESTAMP"
    PR_OR_BRANCH="pr${TRAVIS_PULL_REQUEST}"
    COMMIT_URL="${REPO_URL}/pull/${TRAVIS_PULL_REQUEST}"
fi
VERSION="${MAJOR_MINOR_VERSION}.${TIMESTAMP}"

# Compute release
SHA1SHORT="${TRAVIS_COMMIT:0:8}"
RELEASE="1${PKGSOURCE}.${PR_OR_BRANCH}.git${SHA1SHORT}~1${DISTRO}"

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

# build unsigned packages
DEBUILD_OPTS+=" -eDEB_BUILD_OPTIONS=parallel=${JOBS} -us -uc -j${JOBS} -b"

PROOT_OPTS="-b /dev/shm -r ${CHROOT_PATH}"
if echo ${TAG} | grep -iq arm; then
    PROOT_OPTS="${PROOT_OPTS} -q qemu-arm-static"
fi

# build debs
export DEBUILD_OPTS
proot ${PROOT_OPTS} /bin/sh -exc 'cd ${MACHINEKIT_PATH}; \
    ./debian/configure -prx ; \
    debuild ${DEBUILD_OPTS}'
