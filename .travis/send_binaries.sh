#!/bin/bash -e

FILE="${TRAVIS_REPO_SLUG//\//.}_${TRAVIS_BRANCH}_${TRAVIS_JOB_NUMBER}.tgz"

if [ "${CMD}" = "run_tests" ]; then
    echo "Skipping upload on regression test run" >&2
    exit 0
fi

if [ "${TRAVIS_TEST_RESULT}" -ne 0 ]; then
    echo "Skipping upload on failure" >&2
    exit 0
fi

if [ ! -f ~/no_sftp ]; then
    echo "Skipping upload when sftp unavailable" >&2
    exit 0
fi

if [ -z "${SFTP_PASSWD}" ]; then
    echo "Not sending status:  sftp parameters unconfigured" >&2
    exit 0
fi

if [ ! -d ${ROOTFS}/${MACHINEKIT_PATH}/deploy ]; then
    echo "Skipping upload when deploy directory absent" >&2
    exit 1
fi


# Tar up results and ship them out
cd ${TRAVIS_BUILD_DIR}
tar cvzf ${FILE} -C ${ROOTFS}/${MACHINEKIT_PATH}/deploy .

cat >sftp_cmds <<EOF
cd shared/incoming
put ${FILE}
bye
EOF

sshpass -p ${SFTP_PASSWD} sftp -P ${SFTP_PORT} -o StrictHostKeyChecking=no \
        -oBatchMode=no -b sftp_cmds ${SFTP_USER}@${SFTP_ADDR}
