#!/bin/bash

# check logging - see if config and log files exist and
# log entries actually appear
# this assumes rsyslogd

logfile=/var/log/linuxcnc.log
logconf=/etc/rsyslog.d/linuxcnc.conf

if test ! -f $logfile
then
    echo "$logfile does not exist - consider running 'sudo make log'"
    exit 1
fi

if test ! -f $logconf
then
    echo "$logfile does not exist - consider running 'sudo make log'"
    exit 1
fi

grep -q SystemLogRateLimitBurst $logconf >/dev/null 2>/dev/null

if [ $? -ne 0 ]
then
    echo "the rsyslogd rate limit is not set - consider running 'sudo make log'"
    exit 1
fi

tag=`echo $$|md5sum|tr -d '-'`

logger -p local1.debug logtest:$tag
sleep 0.5
if grep logtest:$tag $logfile >/dev/null 2>/dev/null
then
    # logging works
    exit 0
else
    echo strange - $logconf and $logfile exist, but logging does not work
    echo please fix manually
    exit 1
fi

