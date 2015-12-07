#!/bin/bash -x

DISTRO=${TAG%-*}
MARCH=${TAG#*-}

if [ "${CMD}" = "run_tests" ]; then
    exit 0
fi

# skip upload on failure
if [ "${TRAVIS_TEST_RESULT}" -eq 0 ] && [ "${TRAVIS_PULL_REQUEST}" = "false" ] \
        && [ "${CMD}" = "build_deb" ]; then
    PACKAGECLOUD_REPO=${PACKAGECLOUD_REPO:-machinekit}
    repo=${PACKAGECLOUD_USER}/${PACKAGECLOUD_REPO}/debian/${DISTRO}

    # delete extra debs as packagecloud fails if the same files
    # have already been uploaded
    if [ "${FLAV}" == "rt_preempt" ] || [ "${FLAV}" == "xenomai" ]; then
        rm -f ${TRAVIS_BUILD_DIR}/deploy/machinekit_*
        rm -f ${TRAVIS_BUILD_DIR}/deploy/machinekit-dev*
    fi

    package_cloud push ${repo} ${TRAVIS_BUILD_DIR}/deploy/*deb
    if [ "${MARCH}" = "64" ]; then
        package_cloud push ${repo} ${TRAVIS_BUILD_DIR}/deploy/*dsc
    fi
fi    

