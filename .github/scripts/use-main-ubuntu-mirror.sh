#!/bin/sh

# The azure.archive.ubuntu.com apt mirror on GitHub-hosted runners
# regularly degrades to trickle speeds, stalling jobs until they time
# out. Switch to archive.ubuntu.com, which is consistently fast.

set -eu #Needed so CI fails when anything is wrong
set -x

# apt-mirrors.txt must be included: the runner image points apt at it
# via mirror+file:// in ubuntu.sources, so the azure URL lives there.
for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources /etc/apt/apt-mirrors.txt; do
    [ -e "$f" ] || continue
    sudo sed -i 's|http://azure\.archive\.ubuntu\.com/ubuntu|http://archive.ubuntu.com/ubuntu|g' "$f"
done

# Fail if azure is still referenced, so an image layout change cannot
# turn this script into a silent no-op.
if grep -rsq 'azure\.archive\.ubuntu\.com' /etc/apt; then
    echo "error: azure.archive.ubuntu.com still referenced under /etc/apt after mirror switch" >&2
    exit 1
fi
