#!/bin/bash
# Sourced by the qtdragon ui-smoke tests (smoke and quit) to build a
# config qtvcp can actually run under CI. Sets QTDRAGON_INI to the
# patched ini path and exports the headless env; the caller then execs
# run-gui.sh or quit-launch.sh with "$QTDRAGON_INI". Must be sourced
# with LIB_DIR already set.
#
# qtdragon's qtvcp logger writes its log file (path from INI [DISPLAY]
# LOG_FILE) into the config directory. CI mounts the workspace read-
# only for the runtime user, so a relative LOG_FILE like 'qtdragon.log'
# resolves to a path qtvcp cannot create, hal_bridge then exits, and
# linuxcnc tears down before our driver can do anything. Mirror the
# config dir to a writable tmp location and patch LOG_FILE to be
# rooted at $HOME so the log lands in a directory we can write to.
#
# Force the Qt offscreen platform plugin. qtvcp under xvfb + xcb on
# Ubuntu 24.04 segfaults during widget construction (no backtrace);
# Debian containers in the same CI matrix do not. Offscreen renders
# entirely in memory, no X server needed (xvfb-run still wraps the
# call so the rest of scripts/linuxcnc's X-display assumptions hold).
# scripts/linuxcnc itself forces QT_QPA_PLATFORM=xcb unless
# LINUXCNC_OPENGL_PLATFORM is set to something other than glx, so we
# pin both env vars.
#
# qtdragon embeds a QWebEngineView (Chromium). Under offscreen + xvfb
# with no GPU and no user namespaces in the CI runner sandbox,
# QtWebEngine browser-process init segfaults even with --no-sandbox
# --single-process --disable-gpu (Chromium logs "Sandboxing disabled
# by user." then crashes inside the same qtvcp PID). Rather than keep
# tuning Chromium flags for a widget the smoke test never touches,
# we shim qtpy.QtWebEngineWidgets to raise ImportError; web_widget.py
# already has a fallback path that swaps the QWebEngineView for a
# plain QWidget when the import fails (its "fail safe - mostly for
# designer" branch). No Chromium spawn = no crash.

: "${LIB_DIR:?qtdragon-prepare.sh must be sourced with LIB_DIR set}"

SRC_DIR="$(cd "$LIB_DIR/../../../configs/sim/qtdragon/qtdragon_xyz" && pwd)"

WORK_DIR="$(mktemp -d -t ui-smoke-qtdragon.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT
cp -r "$SRC_DIR/." "$WORK_DIR/"
sed -i 's|^LOG_FILE = qtdragon\.log$|LOG_FILE = ~/qtdragon.log|' \
    "$WORK_DIR/qtdragon_metric.ini"

export LINUXCNC_OPENGL_PLATFORM=offscreen
export QT_QPA_PLATFORM=offscreen

# sitecustomize.py is auto-imported by Python from any sys.path entry
# at interpreter startup. Drop a meta_path finder that blocks the
# qtpy.QtWebEngineWidgets import so WebWidget falls back to QWidget.
SHIM_DIR="$WORK_DIR/_pyshim"
mkdir -p "$SHIM_DIR"
cat >"$SHIM_DIR/sitecustomize.py" <<'PY'
import sys
from importlib.abc import MetaPathFinder, Loader
from importlib.util import spec_from_loader

_BLOCK = {'qtpy.QtWebEngineWidgets', 'PyQt5.QtWebEngineWidgets'}

class _BlockLoader(Loader):
    def create_module(self, spec):
        raise ImportError('QtWebEngineWidgets blocked for ui-smoke CI')
    def exec_module(self, module):
        pass

class _BlockFinder(MetaPathFinder):
    def find_spec(self, name, path, target=None):
        if name in _BLOCK:
            return spec_from_loader(name, _BlockLoader())
        return None

sys.meta_path.insert(0, _BlockFinder())
PY
export PYTHONPATH="$SHIM_DIR${PYTHONPATH:+:$PYTHONPATH}"

# Consumed by the sourcing test.sh, which execs the launcher with it.
# shellcheck disable=SC2034
QTDRAGON_INI="$WORK_DIR/qtdragon_metric.ini"
