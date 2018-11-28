#!/bin/bash

# check-system-configuration.sh: see if config and log files exist and
# log entries actually appear

#
# Check rsyslog configuration
#
# This assumes rsyslogd.
#
# Rsyslogd can drop debug messages from Machinekit when coming in a too
# high a rate.  This is controlled by the SystemLogRateLimitBurst
# parameter.
get-conf-files() {
    conf_files="$*"
    local conflist=""
    for i in $conf_files; do
    conflist+=" $(readlink -e $i)"
    done
    echo $conflist
}

check-rsyslog() {
    local logfile=/var/log/linuxcnc.log
    local logconfigs="$(get-conf-files /etc/rsyslog.conf /etc/rsyslog.d/*.conf)"

    local res=0
    local nologfile=false
    if test ! -f $logfile
    then
    echo "Warning:  Logfile '$logfile' does not exist."
    echo
    echo "          rsyslogd does not create non-existent log files; be"
    echo "          sure the (possibly empty) logfile exists and"
    echo "          restart rsyslogd"
    echo
    echo "          Hint:"
    echo "            $ sudo touch /var/log/linuxcnc.log"
    echo "            $ sudo service rsyslog restart"
    echo
    echo "          Gurus:  If you intentionally log to another file,"
    echo "          please be aware of where Machinekit logs go for"
    echo "          debugging purposes"
    echo
    res=1
    nologfile=true
    fi

    if test -z "$logconfigs"; then
    echo "Warning:  No rsyslog.conf found; system log daemon not rsyslogd?"
    echo
    echo "          Please check your syslog configuration for"
    echo "          rate limiting; an example for rsyslogd can be found in"
    echo "          src/rtapi/rsyslogd-linuxcnc.conf"
    echo
    elif ! grep -q SystemLogRateLimitBurst $logconfigs; then
    res=1
    echo "Warning:  No rate limit in rsyslogd is set."
    echo
    echo "          The 'rsyslogd' daemon drops logs when incoming at"
    echo "          higher than the rate configured by"
    echo "          'SystemLogRateLimitBurst'.  The default rate is lower"
    echo "          than Machinekit requires when running in debug mode."
    echo
    echo "          Hint:"
    echo "            $ sudo cp rtapi/rsyslogd-linuxcnc.conf" \
	         "/etc/rsyslog.d/linuxcnc.conf"
    echo "            $ sudo service rsyslog restart"
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
    echo "Warning:  Logging appears to be not working"
    echo
    echo "          Found rsyslogd configuration and $logfile"
    echo "          exists, but a test did not appear in the log."
    echo "          Please investigate."
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
    local ulimit_configs="$(get-conf-files \
    /etc/security/limits.conf /etc/security/limits.d/*.conf)"
    # a guess at what a reasonable memlock value should be
    local reasonable_memlock=32767

    # Look for a reasonable setting for memlock
    #
    # Assumption: the 'memlock' setting comes from the last entry in
    # these files, and <domain> and <type> fields are correct

    # First, look for 'memlock' entries in the configuration
    local memlock=0
    local last_ulimit_config
    local memlock_found=false
    for f in $ulimit_configs; do
    mtmp=$(awk '/^\*[ \t]+(-|soft)[ \t]+memlock/ {m=$4} END {print m}' $f)
    if test -n "$mtmp"; then
        if test $memlock != 0; then
	echo "Warning:  Multiple configurations for 'memlock' setting"
	echo
	echo "          Please check configuration in these files:"
	echo "            $last_ulimit_config"
	echo "            $f"
	echo
	return 1
        fi
        memlock=$mtmp
        memlock_found=true
        last_ulimit_config=$f
    fi
    done
    if ! $memlock_found; then
    echo "Warning:  no configuration for 'memlock' found"
    echo
    echo "          Hint:"
    echo "            $ sudo cp rtapi/shmdrv/limits.d-machinekit.conf /etc/security/limits.d/machinekit.conf"
    return 1
    fi
    
    # Next, check the 'memlock' value looks sane
    if test $memlock != unlimited && test $memlock -lt $reasonable_memlock; then
    echo "Warning:  Config 'memlock' value $memlock too small"
    echo
    echo "          Value for 'memlock' in file $last_ulimit_config"
    echo "          should be raised to $reasonable_memlock or greater"
    echo ""
    return 1
    fi
}

HAVE_KERNEL_THREADS=false

res=0
check-rsyslog || res=1
check-ulimits || res=1

exit $res
