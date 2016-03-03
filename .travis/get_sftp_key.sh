#!/bin/bash -e
# do not enable verbosity as the decryption keys will be visible on the logs

die () {
    echo $1
    touch ~/no_sftp
    exit 0
}

# make sure SFTP_DEPLOY_ADDR is defined and not empty
if [ "${SFTP_DEPLOY_ADDR}" != "empty" ] && \
   [ "${TRAVIS_SECURE_ENV_VARS}" = "true" ]; 
then
    curl http://${SFTP_DEPLOY_ADDR}/travis/${TRAVIS_REPO_SLUG//\//.}/access_key.enc \
        -o ~/access_key.enc || die "Cannot download access_key!"
    openssl aes-256-cbc -K $encrypted_sftp_key -iv $encrypted_sftp_iv \
        -in ~/access_key.enc -out ~/.ssh/id_rsa -d || \
            die "Cannot decrypt accesxs_key!" 
    chmod 0600 ~/.ssh/id_rsa
    
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
FLAV=${FLAV}
EOF

    cat >${TRAVIS_BUILD_DIR}/../sftp_cmds <<EOF
cd shared/info
put ${TRAVIS_BUILD_DIR}/../${FILE}
bye
EOF

    err=0
    sftp -P ${SFTP_DEPLOY_PORT} -o StrictHostKeyChecking=no -oBatchMode=no \
        -b ${TRAVIS_BUILD_DIR}/../sftp_cmds \
        ${SFTP_DEPLOY_USER}@${SFTP_DEPLOY_ADDR} || err=$?

    if [ $err -ne 0 ]; then
        die "Error connecting with sftp. Exit code: ${err}"
    fi

    exit 0
fi

# cannot use sftp
die "SFTP not available"
