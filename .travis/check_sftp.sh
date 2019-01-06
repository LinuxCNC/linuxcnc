#!/bin/bash -e
# do not enable verbosity as the decryption keys will be visible on the logs
#
# check_sftp.sh
#
# Check if SFTP is available
# - Yes:  upload an info file with information about the build
# - No:  touch ~/no_sftp to signal SFTP unavailable to send_binaries/send_status

die () {
    echo $1
    touch ~/no_sftp
    exit 0
}

# Ensure SFTP is possible
if [ -z "${SFTP_ADDR}" -o "${SFTP_ADDR}" = "empty"]; then
    die "SFTP not available:  \$SFTP_ADDR undefined"
elif [ "${TRAVIS_SECURE_ENV_VARS}" = "false" ]; then
    die "SFTP not available:  TRAVIS_SECURE_ENV_VARS=${TRAVIS_SECURE_ENV_VARS}"
fi

# test conection
FILE="${TRAVIS_REPO_SLUG//\//.}_${TRAVIS_BRANCH}_${TRAVIS_JOB_NUMBER}"

cat >${TRAVIS_BUILD_DIR}/../${FILE} << EOF
TRAVIS_BRANCH=${TRAVIS_BRANCH}
TRAVIS_BUILD_DIR=${TRAVIS_BUILD_DIR}
TRAVIS_BUILD_ID=${TRAVIS_BUILD_ID}
TRAVIS_BUILD_NUMBER=${TRAVIS_BUILD_NUMBER}
TRAVIS_COMMIT=${TRAVIS_COMMIT}
TRAVIS_COMMIT_RANGE=${TRAVIS_COMMIT_RANGE}
TRAVIS_JOB_ID=${TRAVIS_JOB_ID}
TRAVIS_PULL_REQUEST=${TRAVIS_PULL_REQUEST}
TAG=${TAG}
CMD=${CMD}
EOF

cat >${TRAVIS_BUILD_DIR}/../sftp_cmds <<EOF
cd shared/info
put ${TRAVIS_BUILD_DIR}/../${FILE}
bye
EOF

err=0
sshpass -p ${SFTP_PASSWD} sftp -P ${SFTP_PORT} -o StrictHostKeyChecking=no \
    -oBatchMode=no -b ${TRAVIS_BUILD_DIR}/../sftp_cmds \
    ${SFTP_USER}@${SFTP_ADDR} || err=$?

if [ $err -ne 0 ]; then
    die "Error connecting with sftp. Exit code: ${err}"
fi

echo "Successfully configured sftp"
