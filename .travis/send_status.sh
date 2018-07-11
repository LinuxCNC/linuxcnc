#!/bin/bash -e
#
# send_status.sh
#
# Upload info about pass/fail status

cd ~/
if [ -f ~/no_sftp ]; then
    echo "Not sending status:  sftp unavailable" >&2
    exit 0
fi

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
