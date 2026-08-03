#!/bin/sh

# The azure.archive.ubuntu.com apt mirror on GitHub-hosted runners
# regularly degrades to trickle speeds for extended periods, which stalls
# package downloads until the job hits its timeout. apt retries do not
# help because the connection stays alive, only slow. Switch to the main
# Ubuntu archive, which is consistently fast from Azure.

set -eu #Needed so CI fails when anything is wrong
set -x

for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
    [ -e "$f" ] || continue
    sudo sed -i 's|http://azure\.archive\.ubuntu\.com/ubuntu|http://archive.ubuntu.com/ubuntu|g' "$f"
done
