#!/bin/sh -ex

# this script is run inside a docker container

# start required services
service rsyslog start
service dbus start
service avahi-daemon start

# regression test debugging
if ${MK_DEBUG_TESTS}; then
    DEBUG=5
    RUNTESTS_VERBOSE=-v
fi

# run regressions
su mk -c '/bin/sh -exc "\
    . ${MACHINEKIT_PATH}/scripts/rip-environment; \
    env MSGD_OPTS=-s DEBUG='${DEBUG}' runtests '${RUNTESTS_VERBOSE}'"'

# run nosetests
su mk -c '/bin/sh -exc "\
    . ${MACHINEKIT_PATH}/scripts/rip-environment; \
    cd ${MACHINEKIT_PATH}/src; \
    make nosetest"'
