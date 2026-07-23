#!/bin/bash
# Sourced by the gmoccapy ui-smoke tests (smoke and quit) to run gmoccapy
# against a writable copy of its sim config. Sets GMOCCAPY_INI to the
# mirrored ini path; the caller then execs run-gui.sh or quit-launch.sh
# with "$GMOCCAPY_INI". Must be sourced with LIB_DIR already set.
#
# gmoccapy writes its preferences file next to the config: with no
# PREFERENCE_FILE_PATH in the ini, getiniinfo falls back to
# <config-dir>/<MACHINE>.pref. CI mounts the workspace read-only for the
# runtime user, so that write raises PermissionError partway through
# __init__ (during _get_pref_data, before the MDIHistory widget's
# _hal_init runs). gmoccapy pops an error dialog and limps on in a
# half-initialised state: the interp-idle handler then hits a widget with
# no .stat and throws a second dialog. Both vanish once the config dir is
# writable. Mirror it to tmp, same fix qtdragon-prepare.sh uses.

: "${LIB_DIR:?gmoccapy-prepare.sh must be sourced with LIB_DIR set}"

SRC_DIR="$(cd "$LIB_DIR/../../../configs/sim/gmoccapy" && pwd)"

WORK_DIR="$(mktemp -d -t ui-smoke-gmoccapy.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT
cp -r "$SRC_DIR/." "$WORK_DIR/"

# Seed the preference file (config dir + <MACHINE>.pref; MACHINE=gmoccapy)
# so the first-run "Important change(s)" modal stays hidden. That dialog
# runs a nested gtk loop, so under xvfb it never gets dismissed: it sits
# on top of the UI in the confirm shot and, worse, swallows the SIGTERM
# in the quit test (the loop keeps running after main_quit). A real user
# ticks "Don't show this again" once; hide_startup_messsage replicates
# that. The triple-s key matches gmoccapy's own (sic).
cat >"$WORK_DIR/gmoccapy.pref" <<'PREF'
[DEFAULT]
hide_startup_messsage = 99
PREF

# Consumed by the sourcing test.sh, which execs the launcher with it.
# shellcheck disable=SC2034
GMOCCAPY_INI="$WORK_DIR/gmoccapy.ini"
