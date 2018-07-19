#!/usr/bin/env python
# qtVcp library audio player
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# This library is used to play sounds launched from STATUS messages
# it has uses OS builtin sounds or can play arbritray files
#######################################################################

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

# try to add ability for audio feedback to user.
PY_LIB_GOOD = GST_LIB_GOOD = True
try:
    import pygst
    pygst.require("0.10")
    import gst
except:
    PY_LIB_GOOD = False
    import subprocess
    p = subprocess.Popen('gst-launch-1.0', shell=True,
          stdin=subprocess.PIPE, stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, close_fds=True)
    output, error = p.communicate()
    if p.returncode > 1:
        log.warning('audio alerts - Is python-gst0.10 installed?')
        log.warning('no audio alerts available - Is gstreamer-1.0-tools installed?')
        GST_LIB_GOOD = False

import os
from qtvcp.core import Status
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
# Instaniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
STATUS = Status()

# the player class does the work of playing the audio hints
# http://pygstdocs.berlios.de/pygst-tutorial/introduction.html
class Player:
    def __init__(self):
        self.set_sounds()
        if PY_LIB_GOOD:
            self.player = gst.element_factory_make("playbin", "player")
            self.player.set_property("uri", "file://" + self.error)
            #Enable message bus to check for errors in the pipeline
            bus = self.player.get_bus()
            bus.add_signal_watch()
            bus.connect("message", self.on_message)

    # set sounds based on distribution
    def set_sounds(self):
        import platform
        if 'mint' in platform.linux_distribution()[0].lower():
            self.error = '/usr/share/sounds/LinuxMint/stereo/dialog-error.ogg'
            self.ready = '/usr/share/sounds/LinuxMint/stereo/dialog-information.ogg'
            self.done = '/usr/share/sounds/LinuxMint/stereo/system-ready.ogg'
            self.attention = '/usr/share/sounds/LinuxMint/stereo/dialog-question.wav'
            self.ring = '/usr/share/sounds/LinuxMint/stereo/phone-incoming-call.ogg'
            self.login = '/usr/share/sounds/LinuxMint/stereo/desktop-login.ogg'
            self.logout = '/usr/share/sounds/LinuxMint/stereo/desktop-logout.ogg'
            if not  os.path.exists(self.error):
                log.error('Audio player - Mint sound File not found {}'.format(self.error))
            return
        self.error = '/usr/share/sounds/freedesktop/stereo/dialog-error.oga'
        self.ready = '/usr/share/sounds/freedesktop/stereo/message.oga'
        self.done = '/usr/share/sounds/freedesktop/stereo/complete.oga'
        self.attention = '/usr/share/sounds/freedesktop/stereo/suspend-error.oga'
        self.ring = '/usr/share/sounds/freedesktop/stereo/phone-incoming-call.oga'
        self.login = '/usr/share/sounds/freedesktop/stereo/service-login.oga'
        self.logout = '/usr/share/sounds/freedesktop/stereo/service-logout.oga'
        if not  os.path.exists(self.error):
            log.error('Audio player - Default sound File not found {}'.format(self.error))

    # play sounds on these messages from GStat
    # play-sound allows an arbrtrary absolute file name
    # play-alert uses internal builtins
    # use the function name les 'play_'
    def _register_messages(self):
        if PY_LIB_GOOD:
            STATUS.connect('play-sound', lambda w,f: self.run(f))
            STATUS.connect('play-alert', self.jump)
        elif GST_LIB_GOOD:
            STATUS.connect('play-sound', lambda w,f: self.os_run(f))
            STATUS.connect('play-alert', self.os_jump)

    # jump to a builtin alert sound
    # This uses the system to play the sound because gst is not available
    # this can still fail if gstreamer/tools are not available 
    def os_jump(self,w,f):
        if 'beep' in f.lower():
            self[f.lower()]()
            return
        elif 'speak' in f.lower():
            self.os_speak(f)
            return
        cmd = 'gst-launch-1.0 playbin uri=file://%s '% self[f.lower()]
        p = subprocess.Popen(cmd, shell=True,
          stdin=subprocess.PIPE, stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, close_fds=True)

    def os_run(self,f):
        try:
            cmd = 'gst-launch-1.0 playbin uri=file://%s '% f
            p = subprocess.Popen(cmd, shell=True,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, 
                stderr=subprocess.PIPE, close_fds=True)
        except:
            log.error('Audio player using system - file not found {}'.format(f))

    # jump to a builtin alert sound
    # we use this so we can trap errors easily
    def jump(self,w,f):
        if 'beep' in f.lower():
            self[f.lower()]()
            return
        elif 'speak' in f.lower():
            self.os_speak(f)
            return
        if self.player.get_state()[1] == gst.STATE_PLAYING:
            self.player.set_state(gst.STATE_NULL)
        try:
            self['play_%s'%f.lower()]()
        except:
            log.error('Audio player - Alert not found {}'.format(f))

    # check if a file exists then play it
    def run(self,sfile):
        if self.player.get_state()[1] == gst.STATE_PLAYING:
            self.player.set_state(gst.STATE_NULL)
        sfile = os.path.expanduser(sfile)
        if os.path.exists(sfile):
            self.player.set_property("uri", "file://" + sfile)
            self.player.set_state(gst.STATE_PLAYING)
        else:
            log.error('Audio player - File not found {}'.format(sfile))

    # this gets messages back from GStreamer to control playback
    def on_message(self, bus, message):
        t = message.type
        if t == gst.MESSAGE_EOS:
            #file ended, stop
            self.player.set_state(gst.STATE_NULL)
        elif t == gst.MESSAGE_ERROR:
            #Error ocurred, print and stop
            self.player.set_state(gst.STATE_NULL)
            err, debug = message.parse_error()
            log.error('Audio player - Error {}'.format(err, debug))
            #print "Error: %s" % err, debug

    # play builtin alert sounds
    def play_error(self):
        self.player.set_property("uri", "file://" + self.error)
        self.player.set_state(gst.STATE_PLAYING)

    def play_ready(self):
        self.player.set_property("uri", "file://" + self.ready)
        self.player.set_state(gst.STATE_PLAYING)

    def play_attention(self):
        self.player.set_property("uri", "file://" + self.attention)
        self.player.set_state(gst.STATE_PLAYING)

    def play_ring(self):
        self.player.set_property("uri", "file://" + self.ring)
        self.player.set_state(gst.STATE_PLAYING)

    def play_done(self):
        self.player.set_property("uri", "file://" + self.done)
        self.player.set_state(gst.STATE_PLAYING)

    def play_login(self):
        self.player.set_property("uri", "file://" + self.logout)
        self.player.set_state(gst.STATE_PLAYING)

    def play_logout(self):
        self.player.set_property("uri", "file://" + self.login)
        self.player.set_state(gst.STATE_PLAYING)

    def beep_ring(self):
        os.system('''for n in 1 2 3 ; do
            for f in 1 2 1 2 1 2 1 2 1 2 ; do
            beep -f ${f}000 -l 20
            done
            done''')

    def beep_start(self):
        os.system('''beep -f165.4064 -l100 \-n -f130.813 -l100 -n 
            -f261.626 -l100 -n -f523.251 -l100 -n -f1046.50 -l100 
            -n -f2093.00 -l100 -n -f4186.01 -l10''')

    def beep(self):
        os.system("beep -f 555 ")

    def os_speak(self,f):
        cmd = f.lower().lstrip('speak')
        os.system('''espeak -s 160 -v m3 -p 1 "%s" &'''% cmd)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    import gobject
    try:
        test = Player()
        test.play_error()
        print 'done'
        gobject.threads_init()
        G = gobject.MainLoop()
        G.run()

    except Exception as e:
        print e

 
