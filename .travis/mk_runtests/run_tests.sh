#!/bin/sh -ex

# this script is run inside a docker container

touch /var/log/linuxcnc.log
cp ${MACHINEKIT_PATH}/src/rtapi/rsyslogd-linuxcnc.conf \
    /etc/rsyslog.d/linuxcnc.conf

# start required services
service rsyslog start
service dbus start
service avahi-daemon start

# run tests
su mk -c /run_tests_helper.sh

