#!/bin/sh

#set -x

# see redis.conf
DB=/tmp/redis-test.rdb

# run on non-standard port so as not to interfere accidentially
ARG='-p 6579'

redis-server redis.conf

# wait a bit top come up or next command fails
sleep 1

# expect PONG
redis-cli $ARG PING

#expect OK
cat /etc/passwd | redis-cli $ARG -x set mypasswd

# exercise Python bindings
python pytest.py |diff /etc/passwd -

# shutdown the server
redis-cli $ARG shutdown save

# wait a bit until redis-server goes away
sleep 1

# make sure it's gone away, expect
#Could not connect to Redis at 127.0.0.1:6379: Connection refused
redis-cli $ARG PING 2>/dev/null

if test $? -eq 0
then
    echo "why?"
else
    echo "succeeded to fail"
fi

# make sure DB exists
if test -f $DB
then
    echo "$DB doesnt exist!"
else
    echo $DB does exist, removing
    rm -f $DB
fi
