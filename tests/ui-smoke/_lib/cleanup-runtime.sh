#!/bin/bash
# Reset linuxcnc-related runtime state so the next ui-smoke test starts
# from a clean environment. Used as both a pre-launch belt-and-braces
# cleanup and as a post-shutdown last-resort if scripts/linuxcnc's own
# SIGTERM trap could not reap everything in time.
#
# SHM_KEYS mirrors SHMEM_KEY in scripts/runtests:157. If a ui-smoke
# crash leaks any of these, the next runtests invocation aborts in
# test_shmem(); we must clean the full set.

set -u

DAEMONS=(linuxcncsvr milltask halui rtapi_app)
SHM_KEYS=(0x00000064 0x48414c32 0x48484c34 0x90280a48 0x130cf406 0x434c522b)

for proc in "${DAEMONS[@]}"; do
    pkill -KILL -x "$proc" 2>/dev/null || true
done

rm -f /tmp/linuxcnc.lock
halrun -U 2>/dev/null || true

for key in "${SHM_KEYS[@]}"; do
    shmid=$(LC_ALL=C ipcs -m | awk -v k="$key" 'tolower($1)==k {print $2}')
    if [ -n "$shmid" ]; then
        ipcrm -m "$shmid" 2>/dev/null || true
    fi
done

exit 0
