#!/bin/bash -e
#
# send_binaries.sh
#
# After a successful package build, upload build result to an SFTP
# server if available

if [ "${CMD}" != "deb" ]; then
    echo "Skipping package upload for command '${CMD}'" >&2
    exit 0
elif [ "${TRAVIS_TEST_RESULT}" -ne 0 ]; then
    echo "Skipping package upload after build failure" >&2
    exit 0
elif [ -f ~/no_sftp ]; then
    echo "Skipping package upload:  disabled with '~/no_sftp' file" >&2
    exit 0
fi

FILE="${TRAVIS_REPO_SLUG//\//.}_${TRAVIS_BRANCH}_${TRAVIS_JOB_NUMBER}.tgz"

cd ${TRAVIS_BUILD_DIR}
tar cvzf ${FILE} -C deploy .

cat >sftp_cmds <<EOF
cd shared/incoming
put ${FILE}
bye
EOF

sshpass -p ${SFTP_PASSWD} sftp -P ${SFTP_PORT} -o StrictHostKeyChecking=no \
        -oBatchMode=no -b sftp_cmds ${SFTP_USER}@${SFTP_ADDR}
