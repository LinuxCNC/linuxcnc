#!/bin/bash -e

FILE="${TRAVIS_REPO_SLUG//\//.}_${TRAVIS_BRANCH}_${TRAVIS_JOB_NUMBER}.tgz"

if [ "${CMD}" = "run_tests" ]; then
    exit 0
fi

# skip upload on failure
if [ "${TRAVIS_TEST_RESULT}" -eq 0 ] && [ ! -f ~/no_sftp ]; then
    cd ${TRAVIS_BUILD_DIR}
    tar cvzf ${FILE} -C deploy .

cat >sftp_cmds <<EOF
cd shared/incoming
put ${FILE}
bye
EOF

    sshpass -p ${SFTP_PASSWD} sftp -P ${SFTP_PORT} -o StrictHostKeyChecking=no \
        -oBatchMode=no -b sftp_cmds ${SFTP_USER}@${SFTP_ADDR}

fi
