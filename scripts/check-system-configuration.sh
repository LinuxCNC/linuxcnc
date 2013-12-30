#!/bin/bash

# check-system-configuration.sh: see if config and log files exist and
# log entries actually appear

#
# Check rsyslog configuration
#
# This assumes rsyslogd.
#
# Rsyslogd can drop debug messages from LinuxCNC when coming in a too
# high a rate.  This is controlled by the SystemLogRateLimitBurst
# parameter.
check-rsyslog() {
    local logfile=/var/log/linuxcnc.log
    local logconf=/etc/rsyslog.d/linuxcnc.conf

    local res=0
    local nologfile=false
    if test ! -f $logfile
    then
	echo "Warning:  Logfile '$logfile' does not exist."
	echo "          Hint:  Be sure the (possibly empty) logfile exists;"
	echo "          and restart rsyslogd; rsyslogd will not create"
	echo "          missing files."
	echo
	res=1
	nologfile=true
    fi

    if test ! -f $logconf || ! grep -q SystemLogRateLimitBurst $logconf; then
	res=1
	echo "Warning:  No rate limit in rsyslogd is set."
	echo "          The rsyslogd drops logs when incoming at higher than"
	echo "          the rate configured by 'SystemLogRateLimitBurst'.  The"
	echo "          default rate is lower than LinuxCNC requires when"
	echo "          running in debug mode."
	echo "          Hint:  Put src/rtapi/rsyslogd-linuxcnc.conf into"
	echo "          /etc/rsyslog.d/linuxcnc.conf for a reasonable default,"
	echo "          and restart rsyslogd."
	echo "          "
	echo
    fi
    test $res -ne 0 && return 1

    tag=logtest:`echo $$|md5sum|tr -d '-'`

    logger -p local1.debug $tag
    sleep 0.5
    if grep -q $tag $logfile >/dev/null 2>/dev/null
    then
    # logging works
	return 0
    else
	echo "Warning:  Logging appears to be not working:"
	echo "          Both $logconf and"
	echo "          $logfile exist, but a test"
	echo "          did not appear in the log.  Please investigate."
	echo
	return 1
    fi
}

#
# Check /etc/security/limits.d configuration
#
# Xenomai needs higher memlock ulimit than the default 64k
#
check-ulimits() {
    local ulimit_conf=/etc/security/limits.d/linuxcnc.conf
    # a guess at what a reasonable memlock value should be
    local reasonable_memlock=32767

    # If $ulimit_conf exists, assume that the contents are correct
    if test -f $ulimit_conf; then
	return 0
    fi

    # Otherwise, look for a reasonable setting for memlock.
    #
    # Assumption: the 'memlock' setting comes from the last entry in
    # these files, and <domain> and <type> fields are correct
    export LC_COLLATE=C
    memlock=0
    for f in /etc/security/limits.conf /etc/security/limits.d/*.conf; do
	mtmp=$(awk '$3=="memlock" {m=$4} END {print m}' $f)
	test -n "$mtmp" && memlock=$mtmp
    done
    
    if ! test $memlock = unlimited -o \
	$(($memlock>$reasonable_memlock)) = 1; then
	echo "Warning:  $ulimit_conf does not exist, and a reasonable"
	echo "          'memlock' value not found in configuration."
	echo "          Please check the system configuration and correct."
	echo "          Hint:  src/rtapi/shmdrv/limits.d-linuxcnc.conf may"
	echo "          be a reasonable example to install in"
	echo "          $ulimit_conf."
	echo
	return 1
    fi
}


#
# Check shmdrv udev configuration
#
# The shmdrv converged shared memory driver for kthreads flavors must
# be accessible.  Look for a 'shmdrv.rules' file.
check-shmdrv() {
    local udev_conf=/etc/udev/rules.d/50-LINUXCNC-shmdrv.rules
    
    # If $udev_conf exists, assume the contents are correct
    if test -f $udev_conf; then
	return 0
    fi

    # Otherwise, assume any KERNEL=="shmdrv" setting is correct
    for f in /etc/udev/rules.d/*.rules; do
	if grep -q 'KERNEL=="shmdrv"' $f; then
	    return 0
	fi
    done
    
    # If we're here, we found no sign of udev configuration.
    echo "Warning:  No udev configuration for shmdrv was found."
    echo "          The user running LinuxCNC must have write access to"
    echo "          /dev/shmdrv when running kernel threads.  This may"
    echo "          be configured in /etc/udev/rules.d."
    echo "          Hint:  see src/rtapi/shmdrv/shmdrv.rules for a"
    echo "          reasonable default."
    echo
    return 1
}


res=0
check-rsyslog || res=1
check-ulimits || res=1
check-shmdrv || res=1

exit $res
