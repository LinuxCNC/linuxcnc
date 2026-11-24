#!/bin/sh

# Run "apt-get --download-only ARGS" with a timeout, rotating
# /etc/apt/apt-mirrors.txt on each failure so a slow mirror goes last,
# then run "apt-get ARGS" from the filled cache. Only the download is
# killed on timeout, never dpkg. "update" is retried as is.

set -eu #Needed so CI fails when anything is wrong

list=/etc/apt/apt-mirrors.txt
attempts=4
n=1
dl=--download-only
[ "$1" = update ] && dl=
until sudo timeout 600 apt-get -o Acquire::Retries=3 $dl "$@"; do
    if [ $n -ge $attempts ]; then
        echo "error: apt-get download failed $attempts times" >&2
        exit 1
    fi
    n=$((n + 1))
    { tail -n +2 "$list"; head -n 1 "$list"; } | cut -f1 |
        awk '{ printf "%s\tpriority:%d\n", $1, NR }' > /tmp/apt-mirrors.txt
    sudo install -m 644 /tmp/apt-mirrors.txt "$list"
    echo "apt-get download attempt $n, mirrors now:" >&2
    cat "$list" >&2
done
[ -z "$dl" ] || sudo apt-get "$@"
