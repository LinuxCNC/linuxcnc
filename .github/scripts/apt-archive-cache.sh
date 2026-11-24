#!/bin/sh

# Move downloaded .debs between apt's archive dir and ~/apt-archives,
# which the workflow keeps with actions/cache so most runs skip the
# mirrors. "restore" before apt-get, "save" after.

set -eu #Needed so CI fails when anything is wrong

dir=$HOME/apt-archives
mkdir -p "$dir"
case "$1" in
restore) find "$dir" -name '*.deb' -exec sudo cp -t /var/cache/apt/archives/ {} + ;;
save) find /var/cache/apt/archives -maxdepth 1 -name '*.deb' -exec cp -u -t "$dir" {} + ;;
*) echo "usage: $0 restore|save" >&2; exit 1 ;;
esac
du -sh "$dir"
