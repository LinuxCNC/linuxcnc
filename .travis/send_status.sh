#!/bin/bash -e

cd ~/
if [ ! -f ~/no_sftp ]; then
    FILE="${TRAVIS_REPO_SLUG//\//.}_${TRAVIS_BRANCH}_${TRAVIS_JOB_NUMBER}_"
    if [ ${TRAVIS_TEST_RESULT} ]; then
        FILE+="passed"
    else
        FILE+="failed"
    fi

    touch ${FILE}
    cat >sftp_cmds <<EOF
cd shared/info
put ${FILE}
bye
EOF

    sshpass -p ${SFTP_PASSWD} sftp -P ${SFTP_PORT} -o StrictHostKeyChecking=no \
        -oBatchMode=no -b sftp_cmds ${SFTP_USER}@${SFTP_ADDR}

fi
