#!/bin/sh -ex

# this script is run inside a docker container

# start required services
service rsyslog start
service dbus start
service avahi-daemon start

# run regressions
su mk -c '/bin/sh -exc "\
    . ${MACHINEKIT_PATH}/scripts/rip-environment; \
    runtests"'

# run nosetests
su mk -c '/bin/sh -exc "\
    . ${MACHINEKIT_PATH}/scripts/rip-environment; \
    cd ${MACHINEKIT_PATH}/src; \
    make nosetest"'
