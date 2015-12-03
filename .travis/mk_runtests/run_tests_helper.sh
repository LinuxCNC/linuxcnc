#!/bin/sh -ex

. ${MACHINEKIT_PATH}/scripts/rip-environment

# regression test debugging
if ${MK_DEBUG_TESTS}; then
    DEBUG=5
    RUNTESTS_VERBOSE=-v
fi

# run regressions and dump log on error
env MSGD_OPTS=-s DEBUG=${DEBUG} runtests ${RUNTESTS_VERBOSE} || \
    cat /var/log/linuxcnc.log

# run nosetests
cd ${MACHINEKIT_PATH}/src
make nosetest || cat /var/log/linuxcnc.log
