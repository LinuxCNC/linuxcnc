#!/bin/bash
# Shared headless environment for the UI smoke launchers. Sourced by
# launch.sh and quit-launch.sh so the two stay in lockstep; a knob added
# here reaches both. The caller must set LIB_DIR before sourcing (it
# locates asound.conf). This file only exports; it runs no commands.

# Force software OpenGL (Mesa llvmpipe). CI runners have no GPU and
# Qt/GL widgets segfault under hardware GL with no display. The Qt-
# specific knobs cover qtdragon's QtQuick + RHI paths.
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
export QT_QUICK_BACKEND=software
export QSG_RHI_BACKEND=software
export QT_OPENGL=software
# Dodge a long-known xcb_glx integration crash that hits QtWebEngine
# and related Qt5 widgets under xvfb (Launchpad #1761708, QTBUG-67537).
# Forces the egl path which is what software-GL stacks expect anyway.
export QT_XCB_GL_INTEGRATION=xcb_egl

# Silence audio: xvfb covers X but not sound. Demote every Gst
# Audio/Sink and disable canberra/SDL/pulse/ALSA-default paths.
export ALSA_CONFIG_PATH="$LIB_DIR/asound.conf"
export CANBERRA_DRIVER=null
export GST_PLUGIN_FEATURE_RANK="pulsesink:NONE,alsasink:NONE,osssink:NONE,oss4sink:NONE,jackaudiosink:NONE,pipewiresink:NONE,openalsink:NONE"
export PULSE_SERVER=/dev/null
export SDL_AUDIODRIVER=dummy

# Dump a Python traceback on a fatal signal. For a pure-Python crash this
# names the line; for a C/C++ crash (Qt, dbus, GL) it shows the Python
# frame that called in. The native side is captured by crashdump.sh.
export PYTHONFAULTHANDLER=1
