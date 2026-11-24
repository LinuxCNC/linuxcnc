#!/bin/sh

# The default apt mirror on GitHub-hosted runners (azure, now
# us-east-2.ec2.archive.ubuntu.com) regularly drops to trickle speeds,
# stalling jobs until timeout-minutes cancels them. The runner image
# points ubuntu.sources at /etc/apt/apt-mirrors.txt via mirror+file:,
# so time 1 MB from each candidate and list them fastest first. apt
# moves down the list on errors; apt-get-retry.sh rotates it when a
# mirror slows down later.

set -eu #Needed so CI fails when anything is wrong
set -x

list=/etc/apt/apt-mirrors.txt

# Fail if the image layout changed, so this cannot become a silent no-op.
if ! grep -sq "mirror+file:$list" /etc/apt/sources.list.d/ubuntu.sources; then
    echo "error: ubuntu.sources does not use mirror+file:$list" >&2
    exit 1
fi
if grep -sE 'https?://[a-z0-9.-]*archive\.ubuntu\.com' /etc/apt/sources.list /etc/apt/sources.list.d/*; then
    echo "error: an Ubuntu archive host is set outside $list" >&2
    exit 1
fi

mirrors=$({
    echo http://archive.ubuntu.com/ubuntu
    echo http://azure.archive.ubuntu.com/ubuntu
    echo http://mirrors.edge.kernel.org/ubuntu
    cut -f1 "$list"
} | awk 'NF && !seen[$1]++')

# Probe in parallel; a failed or timed out probe scores 0.
dir=$(mktemp -d)
i=0
for m in $mirrors; do
    i=$((i + 1))
    ( speed=$(curl -s -o /dev/null -r 0-1048575 -m 15 -w '%{speed_download}' \
            "$m/dists/noble/main/binary-amd64/Packages.gz") || speed=0
      echo "$speed $m" > "$dir/$i" ) &
done
wait
sort -rn "$dir"/* | awk '{ printf "%8.0f kB/s  %s\n", $1 / 1000, $2 }' >&2

# apt tries the lowest priority first and shuffles entries without one,
# so number every entry. The image tags its own priority:1.
sort -rn "$dir"/* | awk '{ printf "%s\tpriority:%d\n", $2, NR }' > "$dir/list"
sudo install -m 644 "$dir/list" "$list"
rm -rf "$dir"
cat "$list"
